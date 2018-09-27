/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Stein Kristiansen <steikr@ifi.uio.no>
 */

#include "ns3/simulator.h"
#include "ns3/random-variable.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/execenv.h"

#include "program.h"
#include "thread.h"
#include "peu.h"
#include "sem.h"

#include <vector>
#include <iostream>
#include <limits>

namespace ns3 {

std::map<std::string, std::vector<ExecutionEvent *> > programs;

Program::Program()
{
	  hasInternalLoop = false;
	  hasDequeue = false;
}

std::string ExecutionEvent::typeStrings[] = {
		  "LOOP",
		  "EXECUTE",
		  "PROCESS",
		  "SCHEDULER",
		  "SYNCHRONIZATION",
		  "QUEUE",
		  "INTERRUPT",
		  "CONDITION",
		  "TEMPSYNCH",
		  "END",
		  "DEBUG",
          "MEASURE"
};



//////////
// STATEMENTS
//////////
ExecutionEvent::~ExecutionEvent() = default;

ExecutionEvent::ExecutionEvent()
{
  checkpoint = "";
  hasDebug = false;
  debug = "";
  this->lineNr = -1;
  this->line = "";
}

ResourceConsumption::ResourceConsumption()
  : consumption(NormalVariable(0.0,0.0))
{
}

Condition::Condition()
{
  ExecutionEvent::type = CONDITION;
}

Condition::~Condition() {}

TempCompletion::TempCompletion()
{
	ExecutionEvent::type = TEMPSYNCH;
	global = false;
}

TempCompletion::~TempCompletion() = default;


Queue2Condition::Queue2Condition()
{
  Condition::condType = QUEUECONDITION;
}
Queue2Condition::~Queue2Condition() = default;


ServiceQueue2Condition::ServiceQueue2Condition()
{
  Condition::condType = SERVICEQUEUECONDITION;
}

ServiceQueue2Condition::~ServiceQueue2Condition() = default;

ThreadCondition::ThreadCondition()
{
  Condition::condType = THREADCONDITION;
}
ThreadCondition::~ThreadCondition() = default;


PacketCharacteristic::PacketCharacteristic()
{
  Condition::condType = PACKETCHARACTERISTIC;
}
PacketCharacteristic::~PacketCharacteristic() = default;


StateCondition::StateCondition()
{
  Condition::condType = STATECONDITION;
  hasSetterFunction = false;
  hasGetterFunction = false;
}
StateCondition::~StateCondition() = default;


Queue2ExecutionEvent::Queue2ExecutionEvent()
{
	ExecutionEvent::type = QUEUE;
	semToEnqueue = nullptr;
	valueToEnqueue = 0;
	serviceQueue2 = false;
	stateQueue2 = false;
	local = false;
	servQueue2 = nullptr;
	queue = nullptr;
	queueName = "";
}
Queue2ExecutionEvent::~Queue2ExecutionEvent() = default;


LoopCondition::LoopCondition()
{
  Condition::condType = LOOPCONDITION;
  emptyQueue2s = nullptr;
  hasAdditionalCondition = false;
  perQueue2 = false;
  serviceQueue2s = false;
  stateQueue2s = false;
}
LoopCondition::~LoopCondition() = default;


void
Condition::insertEntry(uint32_t entry, Program *p) {
  // Create the new pair to insert
  std::pair<uint32_t, Program *> newPair(entry, p);
  newPair.second = p;

  // Insert new program
  auto it = programs.begin();
  for(; it != programs.end() && entry >= (*it).first; it++);
  programs.insert(it, newPair);

}

// Function used to obtain closest entry
std::pair<uint32_t, Program *>
Condition::getClosestEntryValue(uint32_t value)
{
  // Locate
  auto it = programs.begin();


  // If we dont have any entries yet
  if(it == programs.end()) {
    std::pair<uint32_t, Program *> nullPair(0, nullptr);
    return nullPair;
  }

  // Find the first one larger than entry
  for(; it != programs.end() && value > (*it).first; it++);

  // If we hit the end, we are the largest one,
  // so return the difference to the one below (at
  // the end of the list)
  if(it == programs.end())  // This results in wrong SEM
    return *(--it);

  // If were at programs.begin(), we are the smallest
  else if (it == programs.begin())  // This results in correct SEM
	  return *it;

  // Elsewise, we are between two programs. Returns the
  // closest one
  else {
    uint32_t belowdiff = value - (*(--it)).first;
    uint32_t abovediff = (*++it).first - value;
    return belowdiff < abovediff ? (*(--it)) : (*it);
  }
}

// Function used to obtain closest entry
std::pair<uint32_t, Program *>
Condition::getClosestEntry(Ptr<Thread> t)
{
	// Obtain value. Pass packet, queues or thread-id,
	// depending on the type of condition we have.
	uint32_t value = 0;
	Ptr<ProgramLocation> loc = t->m_currentLocation;
	Ptr<ExecEnv> ee = loc->program->sem->peu->hwModel->node->GetObject<ExecEnv>();
	if(condType == STATECONDITION) {
		auto sc = (StateCondition *) this;
		if(sc->hasGetterFunction)
			value = sc->getConditionState(t);
		else
			if(sc->scope == CONDITIONLOCAL)
				value = loc->getLocalStateVariable(sc->name)->value;
			else
				value = ee->globalStateVariables[sc->name];
	}
	else if (condType == STATEQUEUECONDITION) {
		value = t->m_currentLocation->localStateVariables["dequeuedStateVariable"]->value;
	}
	else if(condType == PACKETCHARACTERISTIC)
		value = getConditionState(t);
	else if(condType == QUEUECONDITION)
		value = getConditionQueue2s(((Queue2Condition *)this)->firstQueue2, ((Queue2Condition *)this)->lastQueue2);
	else if(condType == SERVICEQUEUECONDITION) {
		value = getServiceConditionQueue2s(((ServiceQueue2Condition *)this)->firstQueue2,
				                           ((ServiceQueue2Condition *)this)->lastQueue2);
        //std::cout << "SERVICEQUEUECONDITION: empty: " << (value == QUEUEEMPTY) << std::endl;
	} else if(condType == THREADCONDITION)
		value = getConditionThread(((ThreadCondition *)this)->threadId);


	// Obtain the closest value and program, and return
	std::pair<uint32_t, Program *> returnValue = getClosestEntryValue(value);
    /*if (value == QUEUENOTEMPTY && !prev_program)
	    prev_program = returnValue.second;
	else if (value == QUEUENOTEMPTY)
        returnValue.second = prev_program;*/

    /*if ( value == QUEUENOTEMPTY) {
		returnValue.second->hasDequeue = true;
		returnValue.first = 1;
        returnValue.second->events = prev_program->events;  // THIS IS WHERE IT'S AT.
	    //returnValue.second->sem = ((SEM*)0x699ed0);
    }*/

	return returnValue;
}


ProcessingStage::ProcessingStage()
{
  // Set the resources used to false
  for(int i = 0; i < LASTRESOURCE; i++)
    resourcesUsed[i].defined = false;

  ExecutionEvent::type = PROCESS;
  interrupt = NULL;
}
ProcessingStage::~ProcessingStage() = default;

// Draws samples from the random distributions available,
// and returns one instance from this.
ProcessingInstance
ProcessingStage::Instantiate(Ptr<Packet> packet) {
  // The object to return
  ProcessingInstance toReturn;

  // We fill in "ourself" as the processing stage
  toReturn.source = this;

  // For the PERBYTE statement
  if (this->perByte) {
        this->factor = packet->GetSize ();
  }

  // Iterate all resources used, select a random
  // sample and store in toReturn.
  for(int i = 0; i < LASTRESOURCE; i++) {
    if(!resourcesUsed[i].defined) {
      toReturn.remaining[i].defined = false;
      continue;
    }

    double sample = resourcesUsed[i].consumption.GetValue() * this->factor;
    sample = (sample > 0 ? sample : 0);
    toReturn.remaining[i].amount = sample;
    toReturn.remaining[i].defined = true;
  }

  return toReturn;
}

InsertEventIntoCEPOp::InsertEventIntoCEPOp() {
    ExecutionEvent::type = INCOMINGCEPEVENT;
}

InsertEventIntoFSM::InsertEventIntoFSM() = default;

InterruptExecutionEvent::InterruptExecutionEvent(int IRQNr)
  : number(IRQNr)
{
}
InterruptExecutionEvent::~InterruptExecutionEvent() = default;


SchedulerExecutionEvent::SchedulerExecutionEvent(int t, std::vector<uint32_t> arguments, std::string threadName)
{
  schedType = t;
  ExecutionEvent::type = SCHEDULER;
  args = std::move(arguments);
  this->threadName = std::move(threadName);
}
SchedulerExecutionEvent::~SchedulerExecutionEvent() = default;


ExecuteExecutionEvent::ExecuteExecutionEvent()
{
  ExecutionEvent::type = EXECUTE;
  lc = nullptr;
  service = "";
  sem = nullptr;
}
ExecuteExecutionEvent::~ExecuteExecutionEvent() = default;


DebugExecutionEvent::DebugExecutionEvent()
{
  ExecutionEvent::type = DEBUG;
  this->tag = "";
}
DebugExecutionEvent::~DebugExecutionEvent() = default;

MeasureExecutionEvent::MeasureExecutionEvent()
{
  ExecutionEvent::type = MEASURE;
}
MeasureExecutionEvent::~MeasureExecutionEvent() = default;

SynchronizationExecutionEvent::SynchronizationExecutionEvent()
{
	  id = "";
	  temp = false;
	  ExecutionEvent::type = SYNCHRONIZATION;
	  global = false;
}
SynchronizationExecutionEvent::~SynchronizationExecutionEvent() = default;

StateQueue2Condition::StateQueue2Condition() {
	Condition::condType = STATEQUEUECONDITION;
	queueName = "";
}
StateQueue2Condition::~StateQueue2Condition() = default;


/************************************************************** */
/********************** << overloads ************************** */
/************************************************************** */

std::ostream& operator<<(std::ostream& out, ExecutionEvent& event)
{
	out << event.lineNr << " ";

	switch(event.type) {
	case LOOP: {
		out << "LOOP";
		break;
	}
	case EXECUTE: {
		out << "EXECUTE";
		break;
	}
	case PROCESS: {
		out << "PROCESS";
		break;
	}
	case INCOMINGCEPEVENT: {
		out << "INCOMINGCEPEVENT";
		break;
	}
	case SCHEDULER: {
		out << "SCHED";
		break;
	}
	case SYNCHRONIZATION: {
		out << "SYNCH";
		break;
	}
	case QUEUE: {
		out << "QUEUE";
		break;
	}
	case INTERRUPT: {
		out << "INT";
		break;
	}
	case CONDITION: {
		out << "COND";
		break;
	}
	case TEMPSYNCH: {
		out << "TEMPSYNCH";
		break;
	}
	case END: {
		out << "END";
		break;
	}
	case DEBUG: {
		out << "DEBUG";
		break;
	}
    case MEASURE: {
        out << "MEASURE";
        break;
    }
	case LASTTYPE: {
		out << "LASTTYPE";
		break;
	}
	}

	out << "(" << event.checkpoint << ")";
	return out;
}

std::ostream& operator<<(std::ostream& out, Condition& event)
{
	out << *((ExecutionEvent *) &event) << ": ";

	switch (event.condType) {
	case QUEUECONDITION: {
		out << "QUEUE";
		break;
	}
	case SERVICEQUEUECONDITION: {
		out << "SRVQUEUE";
		break;
	}
	case THREADCONDITION: {
		out << "THREAD";
		break;
	}
	case PACKETCHARACTERISTIC: {
		out << "PKTCHAR";
		break;
	}
	case STATECONDITION: {
		out << "STATE";
		break;
	}
	case STATEQUEUECONDITION: {
		out << "STATEQ";
		break;
	}
	case LOOPCONDITION: {
		out << "LOOP";
		break;
	}
	}

	// Print contents
	out << "(";
	for(auto program : event.programs)
		out << program.first << ":" << program.second->events[0]->lineNr << ",";
	out << ")";

	return out;
}

std::ostream& operator<<(std::ostream& out, LoopCondition& event){
	out << *((Condition *) &event) << ": ";
	unsigned long queueSize = event.serviceQueue2s ? event.serviceQueue2sServed.size() : event.queuesServed.size();
	out << "MI: " << event.maxIterations << ", PQ: " << event.perQueue2 << ", SQ: " << event.serviceQueue2s << ", QS: " << queueSize <<
			", EQ: " << event.emptyQueue2s << ", AC: " << event.hasAdditionalCondition;

	return out;
}

std::ostream& operator<<(std::ostream& out, InterruptExecutionEvent& event) {
	out << *((ExecutionEvent *) &event) << ": ";
	out << event.number << " " << event.service;

	return out;
}

std::ostream& operator<<(std::ostream& out, ProcessingStage& event) {
	out << *((ExecutionEvent *) &event) << ": ";

	// Run through resources
	for(int i = 0; i < LASTRESOURCE; i++) {
		if(event.resourcesUsed[i].defined) {
			switch (i) {
			case NANOSECONDS: {
				out << "NANOSECONDS(";
				break;
			}
			case CYCLES: {
				out << "CYCLES(";
				break;
			}
			case INSTRUCTIONS: {
				out << "INSTRUCTIONS(";
				break;
			}
			case CACHEMISSES: {
				out << "CACHEMISSES(";
				break;
			}
			case MEMORYACCESSES: {
				out << "MEMORYACCESSES(";
				break;
			}
			case MEMSTALLCYCLES: {
				out << "MEMSTALLCYCLES(";
				break;
			}
			default:
				NS_ASSERT(0);
			}

			out << event.samples << ", " << event.resourcesUsed[i].distributionType << "[" <<
			event.resourcesUsed[i].param1 << ", " <<
			event.resourcesUsed[i].param2 << ", " <<
			event.resourcesUsed[i].param3 <<"])";
		}
	}

	return out;
}

std::ostream& operator<<(std::ostream& out, SchedulerExecutionEvent& event) {
	out << *((ExecutionEvent *) &event) << ": ";
	out << event.schedType << "(";

	// Run through args
	for(auto arg : event.args)
		out << arg << ", ";
	out << ")";

	return out;
}

std::ostream& operator<<(std::ostream& out, SynchronizationExecutionEvent& event) {
	out << *((ExecutionEvent *) &event) << ": ";
	out << event.synchType << " " << event.id << " T: " << event.temp << "(";

	// Run through args
	for(auto arg : event.args)
		out << arg << ", ";
	out << ")";

	return out;
}

 std::ostream& operator<<(std::ostream& out, Queue2ExecutionEvent& event) {
	 out << *((ExecutionEvent *) &event) << ": ";
	 out << (event.enqueue ? "ENQUEUE " : "DEQUEUE ") << (event.serviceQueue2 ? "SERVICE(" : "PACKET(") <<
			 (event.serviceQueue2 ? (event.semToEnqueue == nullptr ? "0" : event.semToEnqueue->name) : "") << ") from/to " <<
					 (event.serviceQueue2 ? (void *) event.servQueue2 : (void *) PeekPointer(event.queue));

	 return out;
 }

 std::ostream& operator<<(std::ostream& out, ExecuteExecutionEvent& event) {
	 out << *((ExecutionEvent *) &event) << ": ";
	 out << (event.lc == nullptr ? "SERVICE(" : "LOOP(") << (event.sem == nullptr ? "0" : event.sem->name) << ")";

	 if(event.lc)
	 {
		 out << *(event.lc);
	 }

	 return out;
 }

 std::ostream& operator<<(std::ostream& out, DebugExecutionEvent& event) {
	 out << *((ExecutionEvent *) &event) << ": ";
	 out << "DEBUG: " << event.tag;

	 return out;
 }


 std::ostream& operator<<(std::ostream& out, TempCompletion& event) {
	 out << *((ExecutionEvent *) &event) << ": ";
	 out << "tempcompl";

	 return out;
 }
}

// namespace ns3
