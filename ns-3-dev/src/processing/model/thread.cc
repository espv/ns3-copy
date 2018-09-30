#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>

#include "peu.h"
#include "program.h"

#include "thread.h"
#include "hwmodel.h"
#include "execenv.h"
#include "ns3/log.h"
#include "interrupt-controller.h"
#include "sem.h"
#include "ns3/local-state-variable-queue.h"
#include "ns3/rrscheduler.h"

#include <queue>

namespace ns3 {
// For debug
bool debugOn;
bool traceOn;
bool withBlockingIO;

std::map<int, int> m_fifo_debugfiles;

NS_LOG_COMPONENT_DEFINE("Thread");
NS_OBJECT_ENSURE_REGISTERED(Thread);

TypeId Thread::GetTypeId() {
	static TypeId tid = TypeId("ns3::processing::Thread").SetParent<Object>().AddConstructor<Thread>();
	return tid;
}

Thread::~Thread() = default;

Thread::Thread() {
	m_currentProcessing.done = true;
}

void Thread::SetPid(int pid) {
	m_pid = pid;

	std::ostringstream stringStream;
	stringStream << "/tmp/ns3leu" << pid;
	m_fifo_debugfiles[pid] = open(stringStream.str().c_str(),
			O_WRONLY | O_NONBLOCK);
}

void Thread::DoneProcessing() {
	// TODO: notify memory bus
	m_currentProcessing.done = true;
	Dispatch();
}

bool recordExecStats;

Ptr<StateVariableQueue2> ProgramLocation::getLocalStateVariableQueue2(const std::string &queueID) {
	// Find the requested queue
	auto it = localStateVariableQueue2s.find(queueID);

	/* If the queue does not exist, create it
	 * NOTE: this is the only mechanism by which new local state queues are created, i.e.,
	 * upon the first appearance of the queueID in the signature. Since these queues
	 *       are referenced via smart pointers, and pointers are copied upon each entry
	 *       copy of a ProgramLocation, these queues will exist as long as they are by at least
	 *       one ProgramLocation.
	 */
	if (it == localStateVariableQueue2s.end())
		localStateVariableQueue2s[queueID] = Create<StateVariableQueue2>();

	return localStateVariableQueue2s[queueID];
}

Ptr<StateVariable> ProgramLocation::getLocalStateVariable(std::string svID) {
	// Find the requested queue
	auto it = localStateVariables.find(svID);

	/* If the queue does not exist, create it
	 * NOTE: this is the only mechanism by which new local state queues are created, i.e.,
	 *       upon the first appearance of the queueID in the signature. Since these queues
	 *       are referenced via smart pointers, and pointers are copied upon each entry
	 *       copy of a ProgramLocation, these queues will exist as long as they are by at least
	 *       one ProgramLocation.
	 */
	if (it == localStateVariables.end())
		localStateVariables[svID] = Create<StateVariable>();

	return localStateVariables[svID];
}

void Thread::PrintGlobalTempvars(Ptr<ExecEnv> execEnv) {
	for (auto tv : execEnv->tempVars) {
		std::cout << "Tempvar " << tv.tempSynch << std::endl;
		for (auto &user : tv.users) {
			std::cout << user << " " << std::endl;
		}
	}
}

bool Thread::HandleExecutionEvent(ExecutionEvent *e) {
	// If we have a trigger on this event,
	// call it.
	if (recordExecStats)
		m_currentLocation->program->sem->numStmtExec++;

	if (e->hasDebug) {
		if (e->debug == "on")
			debugOn = true;
		if (e->debug == "off")
			debugOn = false;
	}

	switch (e->type) {
		/* When ending programs. Causes the program to be popped from the stack.
		 * If the stack is empty when we return from this function, Dispatch() will
		 * call Terminate() on the scheduler hosting this thread, causing this thread
		 * to be removed from the system.
		 */
		case END:
			return HandleEndEvent(e);
		case PROCESS: 
			// PROCESS: Passed to the HWModels for handling.
			return HandleProcessingEvent(e);
		case INCOMINGCEPEVENT:
			return HandleIncomingCEPEvent(e);
		case EXECUTE:
			return HandleExecuteEvent(e);
		case QUEUE: 
			// Used to enqueue or dequeue packets into/from queues.
			return HandleQueue2Event(e);
		case SCHEDULER: 
			return HandleSchedulerEvent(e);
		case SYNCHRONIZATION: 
			return HandleSyncEvent(e);
		case CONDITION: 
			return HandleCondition(e);
		case TEMPSYNCH:
			{
				auto tc = dynamic_cast<TempCompletion *>(e);
				struct tempVar tv;
				tv.users = tc->users;
				std::vector<uint32_t> dummyArgs;
				tv.tempSynch = m_scheduler->AllocateTempSynch(1, dummyArgs);

				if (tc->global) {
					auto execEnv = peu->hwModel->node->GetObject<ExecEnv>();
					execEnv->tempVars.push_back(tv);
					if(traceOn)
						PrintGlobalTempvars(execEnv);
				} else
					m_currentLocation->tempvar = tv;

				return true;
			}

		case DEBUG: 
			{
				std::cout << " Executing debug statement: "
					<< *((DebugExecutionEvent *) e) << std::endl;
				return true;
			}
        case MEASURE:
            {
                // OYSTEDAL: Used to measure time at specific points in the signature
                if (m_currentLocation->curPkt != nullptr)
                    m_currentLocation->curPkt->m_executionInfo.timestamps.push_back(Simulator::Now());

                return true;
            }

			/* Here, we have encountered an unrecognized statement type.
			 * Complain.
			 */
		default: 
			{
				NS_LOG_INFO("Encountered unhandled statement of type" << e->type << "\n");
				return true;
			}
	}
}

bool Thread::HandleEndEvent(ExecutionEvent* e) {
	// Handle prospective loop
	if (m_currentLocation->lc) {
		// Fetch info about the loop
		long maxIterations = m_currentLocation->lc->maxIterations;
        long curIteration = ++(m_currentLocation->curIteration);
        long queueServedIndex = m_currentLocation->curServedQueue2;
        long numQueue2s =
			m_currentLocation->lc->serviceQueue2s ?
			m_currentLocation->lc->serviceQueue2sServed.size() :
			(m_currentLocation->lc->stateQueue2s ?
			 m_currentLocation->lc->stateQueue2sServed.size() :
			 m_currentLocation->lc->queuesServed.size());
		bool perQueue2 = m_currentLocation->lc->perQueue2;
		Ptr<Packet> curPacket = m_currentLocation->curPkt;
		bool condPassed = !m_currentLocation->lc->hasAdditionalCondition ? true :
                          (bool)m_currentLocation->lc->additionalCondition(this);

		// If we did not pass the condition, break loop
		if (!condPassed) {
			m_programStack.pop();
			return true;
		}

		/* If we have set rootProgram, we know we have encountered
		 * a condition during execution of the loop body, exchanging
		 * the program pointer with the one in the condition(s). Thus,
		 * when re-starting the loop, we need to re-set the program
		 * pointer to the root program.
		 */
		if (m_currentLocation->rootProgram) {
			m_currentLocation->program = m_currentLocation->rootProgram;
			m_currentLocation->rootProgram = nullptr;
		}

		bool infiniteLoop = (maxIterations == 0);
		bool iterationsToGo = infiniteLoop || (curIteration < maxIterations);

		// If we have no queues, continue if maxIterations allows
		if (numQueue2s == 0) {
			if (iterationsToGo)
				m_currentLocation->currentEvent = -1; // Incremented to 0 by dispatch
			else {
				m_programStack.pop();
			}

			return true;
		}

		/* Else wise, if the body does not have a de-queue event,
		 * i.e., an internal loop is service the queues (see
		 * comments on the field hadDequeue in program.h),
		 * this loop is here only to continue to iterate
		 * until the queues are empty or we hit a maximum
		 * number of iterations.
		 */
		auto curLc = m_currentLocation->lc;
		if (!m_currentLocation->program->hasDequeue && m_currentLocation->program->hasInternalLoop) {
			if (!iterationsToGo) {
				m_programStack.pop();
				return true;
			}

			if (curLc->serviceQueue2s) {
				auto it = curLc->serviceQueue2sServed.begin();
				for (;
						it != curLc->serviceQueue2sServed.end()
						&& (*it)->empty(); it++)
					;

				// If all queues were empty, pop program and return.
				if (it == curLc->serviceQueue2sServed.end()) {
					// We do not pop thread of execution, as we did not push it yet
					m_programStack.pop();
					return true;
				}
			} else if (curLc->stateQueue2s) {
				auto it = curLc->stateQueue2sServed.begin();
				for (;
						it != curLc->stateQueue2sServed.end()
						&& !(*it)->empty(); it++)
					;

				// If all queues were empty, pop program and return.
				if (it == curLc->stateQueue2sServed.end()) {
					// We do not pop thread of execution, as we did not push it yet
					m_programStack.pop();
					return true;
				}
			} else {
				auto it = curLc->queuesServed.begin();
				for (; it != curLc->queuesServed.end() && (*it)->IsEmpty();
						it++)
					;

				// If all queues were empty, pop program and return.
				if (it == curLc->queuesServed.end()) {
					// We do not pop thread of execution, as we did not push it yet
					m_programStack.pop();
					return true;
				}
			}
		}

		////////////////////////////////////////////////
		// Here we know the loop body serves queues ////
		////////////////////////////////////////////////

		// If we don't have any more iterations to
		// execute, break loop
		if (!iterationsToGo && !perQueue2) {
			m_programStack.pop();
			return true;
		}

		bool curEmptyProgram = (m_currentLocation->program
				== m_currentLocation->lc->emptyQueue2s);
		bool goToNextQueue2 = curEmptyProgram || !iterationsToGo;

		// If go to next queue, do that. If no more queues, break loop.
		if (goToNextQueue2) {
			/* Calculate next index. If -1, i.e., no more queues, break if not infinite loop.
			 * NOTE 09.08.14: We do NOT re-start on the first queue, even if we have an infinite
			 * loop. REASON: this is what we have outer loops for, e.g., as with softirqs!
			 */
			int nextQueue2 = (queueServedIndex + 1) < numQueue2s ? queueServedIndex + 1 : -1;
			if (nextQueue2 == -1) {
				m_programStack.pop();
				return true;
			}

			// Else wise, update m_currentLocation wrt. queue served
			else {
				queueServedIndex = m_currentLocation->curServedQueue2 = nextQueue2;
				m_currentLocation->curIteration = 0;
			}
		}

		// If if THIS ONE queue is empty, set program to empty queues (should be called empty queue) program
		bool queueEmpty =
			m_currentLocation->lc->serviceQueue2s ?
			m_currentLocation->lc->serviceQueue2sServed[queueServedIndex]->empty() :
			(m_currentLocation->lc->stateQueue2s ?
			 m_currentLocation->lc->stateQueue2sServed[queueServedIndex]->empty() :
			 m_currentLocation->lc->queuesServed[queueServedIndex]->IsEmpty());
		if (queueEmpty) {
			if (curLc->emptyQueue2s != nullptr)
				m_currentLocation->program = curLc->emptyQueue2s;
			else { // See commends in program.h on LoopCondition members hasInternalLoop and hasDequeue
				m_currentLocation->program = m_currentLocation->program->sem->rootProgram;
				if (!m_currentLocation->program->hasInternalLoop) {
					/* iterate queues until we find one which is not
					 * empty. If all are empty, return from LOOP.
					 */
                    uint32_t index = 0;
					if (curLc->serviceQueue2s) {
						auto it = curLc->serviceQueue2sServed.begin();
						for (;
								it != curLc->serviceQueue2sServed.end()
								&& (*it)->empty(); it++)
							index++;

						// If all queues were empty, pop program and return.
						if (it == curLc->serviceQueue2sServed.end()) {
							// We do not pop thread of execution, as we did not push it yet
							m_programStack.pop();
							return true;
						} else
							m_currentLocation->curServedQueue2 = index;
					} else if (curLc->stateQueue2s) {
						auto it = curLc->stateQueue2sServed.begin();
						for (;
								it != curLc->stateQueue2sServed.end()
								&& (*it)->empty(); it++)
							index++;

						// If all queues were empty, pop program and return.
						if (it == curLc->stateQueue2sServed.end()) {
							// We do not pop thread of execution, as we did not push it yet
							m_programStack.pop();
							return true;
						} else
							m_currentLocation->curServedQueue2 = index;
					} else {
						auto it = curLc->queuesServed.begin();
						for (;
								it != curLc->queuesServed.end()
								&& (*it)->IsEmpty(); it++)
							index++;

						// If all queues were empty, pop program and return.
						if (it == curLc->queuesServed.end()) {
							// We do not pop thread of execution, as we did not push it yet
							m_programStack.pop();
							return true;
						} else
							m_currentLocation->curServedQueue2 = index;
					}
				}
			}
		} else
			m_currentLocation->program = m_currentLocation->program->sem->rootProgram;

		/* At this point, we know we should continue from the beginning of the loop.
		 * Set event index to -1 (incremented to 0 by dispatch) and return true.
		 */
		m_currentLocation->currentEvent = -1;
		return true;
	}

	/* Else wise, we have a regular non-loop service. We pop the stack to
	 * exit the service and resume the calling service.
	 */
	else {
		m_programStack.pop();
		return true;
	}
}

bool Thread::HandleProcessingEvent(ExecutionEvent* e) {
	auto ps = dynamic_cast<ProcessingStage *>(e);

	if (ps->interrupt != nullptr) { // OYSTEDAL: We are in an interrupt
		/* TODO: consume from a PEU. For now, use CPU.
		 * Dirty hack for now: sample cycles, and calculate based on CPU frequency
		 */
		auto pi = ps->Instantiate(m_currentLocation->curPkt);
		int cpu = peu->GetObject<CPU>()->GetId();

		long m_freq = peu->hwModel->cpus[cpu]->m_freq;
		uint32_t m_tracingOverhead = peu->hwModel->cpus[cpu]->m_tracingOverhead;

		double nanoseconds = (((pi.remaining[CYCLES].amount * 1000) / m_freq)) - ((m_tracingOverhead * 1000.0) / m_freq);
		nanoseconds = (nanoseconds > 0 ? nanoseconds : 0);
		if(withBlockingIO) {
			Simulator::Schedule(NanoSeconds((uint64_t) nanoseconds),
					&InterruptController::IssueInterruptWithServiceOnCPU,
					peu->hwModel->m_interruptController, 
					cpu,
					ps->interrupt,
					m_currentLocation);
		}
		else
			Simulator::ScheduleNow(
					&InterruptController::IssueInterruptWithServiceOnCPU,
					peu->hwModel->m_interruptController, 
					cpu,
					ps->interrupt,
					m_currentLocation);

		if (recordExecStats)
			m_currentLocation->program->sem->peuProcessing += (uint64_t)pi.remaining[CYCLES].amount;

		return true;
	} else {
		/* First, obtain a processing instance with all sample
		 * values for all resources consumed filled in.
		 */
		m_currentProcessing = ps->Instantiate(m_currentLocation->curPkt);
		m_currentProcessing.thread = this;

		/* Pass this instance to the PEU responsible of
		 * calculating and scheduling a completion event. We always
		 * return false, as the completion event will anyway
		 * call this threads ProcessingComplete().
		 */

        if (this->peu && m_currentLocation->program->sem->peu->IsCPU()) {
            this->peu->Consume(&m_currentProcessing);
        } else {
            m_currentLocation->program->sem->peu->Consume(&m_currentProcessing);
        }
        m_currentProcessing.done = false;

		if (recordExecStats)
			m_currentLocation->program->sem->cpuProcessing += (uint64_t)m_currentProcessing.remaining[CYCLES].amount;

		return false;
	}
}

bool Thread::HandleIncomingCEPEvent(ExecutionEvent* e) {
	auto ieiceop = dynamic_cast<InsertEventIntoCEPOp *>(e);
	std::cout << "In HandleIncomingCEPEvent()" << std::endl;
	auto ee = peu->hwModel->node->GetObject<ExecEnv>();
	if (ee->eventqueues["event-queue"].empty())
		return false;
	std::string event = ee->eventqueues["event-queue"].at(0);
	ee->eventqueues["event-queue"].erase(ee->eventqueues["event-queue"].begin());

    ieiceop->ieifsm->ps->factor = 0;
    ieiceop->ps->factor = 0;
    for (auto it = ieiceop->pCEPEngine->operators.begin(); it != ieiceop->pCEPEngine->operators.end(); ++it) {
        Ptr<CEPOp> op = *it;
        op->InsertEvent(event);
		for (int i = 0; i != op->helper->GetNumberSequences(); ++i) {
            //HandleProcessingEvent(ieiceop->ieifsm->ps);
            // The factor determines how much processing happens
            ++ieiceop->ieifsm->ps->factor;
		}
		// The factor determines how much processing happens
		++ieiceop->ps->factor;
        //HandleProcessingEvent(ieiceop->ps);
    }

    return true;
}

bool Thread::HandleExecuteEvent(ExecutionEvent* e) {
	auto ee = dynamic_cast<ExecuteExecutionEvent *>(e);

	// Obtain target. If it is an empty string, the target must be in the packet.
	std::string eeTarget = ee->service;

	Ptr<SEM> newSem;
	if (ee->sem != nullptr) {
		newSem = ee->sem;

		if (m_currentLocation->curPkt != nullptr) {
			ExecutionInfo *pktEI =
				&(m_currentLocation->curPkt->m_executionInfo);
			if (newSem->trigger.length() != 0 && pktEI->target == newSem->trigger && pktEI->targetFPM != nullptr) {
				pktEI->executedByExecEnv = true;
				EventImpl *toInvoke = pktEI->targetFPM;
				toInvoke->Invoke();
				toInvoke->Unref();
			}
		}
	} else {
		auto execEnv = peu->hwModel->node->GetObject<ExecEnv>();
		// Here, we assume there is an active packet specifying the target
		auto it = execEnv->serviceTriggerMap.find(m_currentLocation->curPkt->m_executionInfo.target);
		newSem = it->second;


        if (it == execEnv->serviceTriggerMap.end()) {
            // Check that the service you're trying to call is in the signature.
			NS_LOG_ERROR("Failed to find signature " << m_currentLocation->curPkt->m_executionInfo.target);
			std::cout << "Failed to find signature " << m_currentLocation->curPkt->m_executionInfo.target << std::endl;
            NS_ASSERT(0);
        }

		// Here, we know the target is that of this packet
		auto pktEI = &(m_currentLocation->curPkt->m_executionInfo);
		if (newSem->trigger.length() != 0 && pktEI->target == newSem->trigger && pktEI->targetFPM != nullptr) {
			pktEI->executedByExecEnv = true;
			EventImpl *toInvoke = pktEI->targetFPM;
			toInvoke->Invoke();
			toInvoke->Unref();
		}
	}


	/* We must determined whether the PEU on which we should
	 * execute this program is the same as the one were currently
	 * running on. If it is, we simply push the program onto the
	 * program stack of the current thread. If not, we fork a new
	 * thread on the destination PEU and run the program there.
	 * Note that this is the only way for one PEU to start work
	 * on another PEU, if max threads are activated, nothing
	 * happends. But note also that it is possible to pass the
	 * packet via a queue, and instantiate the thread with a
	 * program from a SEM that is not sensitive to conditions
	 * from the packet.
	 */
    // OYSTEDAL: Don't Fork() when we're executing on a diff CPU core.
    if (newSem->peu->IsCPU()) {
		Ptr<ProgramLocation> newProgramLocation = Create<ProgramLocation>();
		newProgramLocation->program = newSem->rootProgram;
		newProgramLocation->currentEvent = -1; // incremented to 0 in Dispatch()
		newProgramLocation->curPkt = m_programStack.top()->curPkt;
		newProgramLocation->localStateVariables = m_programStack.top()->localStateVariables;
		newProgramLocation->localStateVariableQueue2s = m_programStack.top()->localStateVariableQueue2s;
		newProgramLocation->tempvar = m_programStack.top()->tempvar;

		// Set up loop state if this is a loop statement
		LoopCondition *lcPtr = ee->lc;
		if (lcPtr != nullptr) {
			newProgramLocation->curIteration = 0;
			newProgramLocation->lc = lcPtr;

			/* Since regular services can be called as loops,
			 * we need to act according to whether the target
			 * is a loop or not.
			 */
			if (newSem->lc) {
				LoopCondition *newLc = newSem->lc;

				/* If target is a loop, but no queues are set,
				 * we don't have the "empty queues" program. This
				 * means we should use the regular root program
				 */
				uint64_t queueSize = newLc->serviceQueue2s ? newLc->serviceQueue2sServed.size() :
					                 (newLc->stateQueue2s ? newLc->stateQueue2sServed.size() :
					                  newLc->queuesServed.size());

				if (queueSize == 0)
					newProgramLocation->program = newSem->rootProgram;

				/* If the target has queues, we always start at the
				 * first queue. If its empty, set the program to the
				 * empty queues program.
				 */
				else {
					newProgramLocation->curServedQueue2 = 0;
					bool firstQueue2Empty =
						newLc->serviceQueue2s ?
						newLc->serviceQueue2sServed[0]->empty() :
						(newLc->stateQueue2s ?
						 newLc->stateQueue2sServed[0]->empty() :
						 newLc->queuesServed[0]->IsEmpty());

					if (!firstQueue2Empty)
						newProgramLocation->program = newSem->rootProgram;
					else {
						if (newLc->emptyQueue2s != nullptr)
							newProgramLocation->program = newLc->emptyQueue2s;
						else { // See commends in program.h on LoopCondition members hasInternalLoop and hasDequeue
							newProgramLocation->program =
								newSem->rootProgram;
							if (!newSem->rootProgram->hasInternalLoop) {
								/* iterate queues until we find one which is not empty.
								 * If all are empty, return from LOOP.
								 */
								if (newLc->serviceQueue2s) {
									uint32_t index = 0;
									auto it = newLc->serviceQueue2sServed.begin();
									for (;
											it
											!= newLc->serviceQueue2sServed.end()
											&& (*it)->empty(); it++)
										index++;

									// If all queues were empty, pop program and return.
									if (it
											== newLc->serviceQueue2sServed.end())
										// We do not pop thread of execution, as we did not push it yet
										return true;
									else
										m_currentLocation->curServedQueue2 =
											index;
								} else if (newLc->stateQueue2s) {
									uint32_t index = 0;
									auto it = newLc->stateQueue2sServed.begin();
									for (;
											it
											!= newLc->stateQueue2sServed.end()
											&& (*it)->empty(); it++)
										index++;

									// If all queues were empty, pop program and return.
									if (it
											== newLc->stateQueue2sServed.end())
										// We do not pop thread of execution, as we did not push it yet
										return true;
									else
										m_currentLocation->curServedQueue2 = index;
								} else {
									uint32_t index = 0;
									auto it = newLc->queuesServed.begin();
									for (;
											it != newLc->queuesServed.end()
											&& (*it)->IsEmpty();
											it++)
										index++;

									// If all queues were empty, pop program and return.
									if (it == newLc->queuesServed.end())
										// We do not pop thread of execution, as we did not push it yet
										return true;
									else
										m_currentLocation->curServedQueue2 = index;
								}
							}
						}
					}
				}

				// Set the loopcondition into the new program location
				newProgramLocation->lc = lcPtr;
			}
		}

		m_programStack.push(newProgramLocation);
	} else {
		newSem->peu->taskScheduler->Fork("", newSem->rootProgram, 0,
				m_programStack.top()->curPkt,
				m_programStack.top()->localStateVariables,
				m_programStack.top()->localStateVariableQueue2s, false);
	}

	// We should immediately continue with the next event in the called program
	return true;
}

bool Thread::HandleQueue2Event(ExecutionEvent* e) {
    auto qe = dynamic_cast<Queue2ExecutionEvent *>(e);
    if (qe->enqueue) {
		/* If we have an en-queue event, simply insert into queue.
		 * We assume that the queue extist, as it should have been
		 * created during parsing of the header in the device-file
		 * initialization.
		 */
		auto ee = peu->hwModel->node->GetObject<ExecEnv>();
        if (qe->serviceQueue2) {
            /* Here, we want to push a service onto the service
			 * queue specified in the event. This may however
			 * either be a service specified in the event OR
			 * a service specified in the current packet. In
			 * the latter case, the service in the event is nullptr,
			 * meaning that we need to obtain the service from
			 * the packet.
             */
			Ptr<SEM> semToEnqueue = nullptr;
			if (qe->semToEnqueue == nullptr) {
				semToEnqueue = ee->serviceTriggerMap[m_currentLocation->curPkt->m_executionInfo.target];
			} else
				semToEnqueue = qe->semToEnqueue;

			auto toBeEnqueued = std::pair<Ptr<SEM>, Ptr<ProgramLocation> >(semToEnqueue, m_programStack.top());
			qe->servQueue2->push(toBeEnqueued);

		} else if (qe->stateQueue2) {
			ee->stateQueue2s[qe->queueName]->stateVariableQueue2.push(qe->valueToEnqueue);
		} else
			qe->queue->Enqueue(m_currentLocation->curPkt);
	} else
		// Check if we are dealing with a service queue
        if (qe->serviceQueue2) {
			// Obtain queue from encapsulated loop if not defined in the event
			auto queueToServe = (qe->servQueue2 == nullptr) ?
					m_currentLocation->lc->serviceQueue2sServed[m_currentLocation->curServedQueue2] :
				    qe->servQueue2;

			/* Here, we want to dequeue the service, then (below) execute it.
			 * Note that we resolved which sem to enqueue (which may be "0")
			 * in the insertion above, so we don't need to resolve this again.
			 */
            Ptr<SEM> toExecute = queueToServe->front().first;
			Ptr<ProgramLocation> newPl = queueToServe->front().second;
            queueToServe->pop();

            NS_LOG_INFO("Dequeueing service " << toExecute->name);

			/* Now, its time to execute the de-queued service
			 * Note that it is not possible to en-queue loop services
			 * into service queues, so its safe to assume the
			 * enqueued service is a regular service.
			 * COMMENTS ON THIS is in the EXECUTE events above - we do
			 * almost the same here.
			 */
            if (toExecute->peu->IsCPU()) {
				Ptr<ProgramLocation> newProgramLocation = Create<ProgramLocation>();
				newProgramLocation->program = toExecute->rootProgram;
				newProgramLocation->currentEvent = -1; // incremented to 0 in Dispatch()
				newProgramLocation->curPkt = newPl->curPkt;  // Added by Espen
				newProgramLocation->lc = newPl->lc;
				newProgramLocation->localStateVariables = newPl->localStateVariables;
				newProgramLocation->localStateVariableQueue2s = newPl->localStateVariableQueue2s;
				newProgramLocation->tempvar = m_currentLocation->tempvar;
				m_programStack.push(newProgramLocation);
			} else
				toExecute->peu->taskScheduler->Fork("", toExecute->rootProgram,
						0, newPl->curPkt, newPl->localStateVariables,
						newPl->localStateVariableQueue2s, false);

			// We should immediately continue with the next event in the called program
			return true;
		} else if (qe->stateQueue2) {
			/*
DEPRECATED: Do nothing. Reason: ExecEnv inserted a subsequent StateQueue2Condition statement,
which will need the first value in the queue, and thus the value is dequeued
below where statements of type Condition are handled.

210814: It is important that dequeue statements reside before their condition satatement
because of packet queues: their condition function (e.g., "ip::packet::size") needs the
packet to be available as m_currentLocation->curPkt to be able to work. We don't want
to go through the hassle of discriminating between state and packet queues during
parsing in ExecEnv, and instead have dequeue statements before their conditions
for all types of queues, including state queues. We use a special local variable
"dequeuedStateVariable" to propagate the dequeued value to the condition function executed
during interpretation of a condition statement following a dequeue statement that
works on a state queue.

Ptr<LocalStateVariableQueue2> queueToServe = (!qe->queueName.compare("")) ?
m_currentLocation->lc->stateQueue2sServed[m_currentLocation->curServedQueue2] :
m_currentLocation->getLocalStateQueue2(qe->queueName);

m_currentLocation->localStateVariableQueue2s[qe->queueName]->stateVariableQueue2.pop(); */
			Ptr<ExecEnv> ee = peu->hwModel->node->GetObject<ExecEnv>();

			m_currentLocation->getLocalStateVariable("dequeuedStateVariable")->value =
				ee->stateQueue2s[qe->queueName]->stateVariableQueue2.front();
			ee->stateQueue2s[qe->queueName]->stateVariableQueue2.pop();

			return true;
		} else {
			// Obtain queue from encapsulated loop if not defined in the event
			Ptr<Queue2> queueToServe = (qe->queue == nullptr) ?
					m_currentLocation->lc->queuesServed[m_currentLocation->curServedQueue2] :
					qe->queue;

            Ptr<ExecEnv> ee = peu->hwModel->node->GetObject<ExecEnv>();
            Ptr<Queue2> rxfifo_queue = ee->queues["rxfifo"];
            if (rxfifo_queue == queueToServe && queueToServe->IsEmpty()) {
                std::cout << "Dequeueing from empty rxfifo" << std::endl;
            }
            m_currentLocation->curPkt = queueToServe->Dequeue();

			// We need call activate any prospective triggers on the queue
			Ptr<ExecEnv> execEnv = peu->hwModel->node->GetObject<ExecEnv>();
			std::string queueTarget = execEnv->dequeueTriggers[execEnv->queueNames[queueToServe]];
			if (queueTarget.length() != 0 && m_currentLocation->curPkt->m_executionInfo.target == queueTarget) {
				Ptr<Packet> curPkt = m_currentLocation->curPkt;
				curPkt->m_executionInfo.executedByExecEnv = true;
				EventImpl *toInvoke = curPkt->m_executionInfo.targetFPM;
				toInvoke->Invoke();
				toInvoke->Unref();
			}
		}

	return true;

	/* For now we ignore these statements and simply
	 * continue executing the program.
	 */
}

bool Thread::HandleSchedulerEvent(ExecutionEvent* e) {
	auto se = dynamic_cast<SchedulerExecutionEvent *>(e);
	std::vector<uint32_t> arguments;

	if (se->schedType == AWAKE) {
		arguments.push_back((uint32_t)this->m_scheduler->threadPids[se->threadName]);
	} else
	    arguments.push_back((uint32_t)m_pid);

	const int cpu = peu->GetObject<CPU>()->GetId();
	bool reqReturn = m_scheduler->Request(cpu, se->schedType, arguments);
	if (recordExecStats && !reqReturn)
        m_currentLocation->wasBlocked = Simulator::Now().GetNanoSeconds();

	return this->peu->hwModel->cpus[cpu]->inInterrupt ? true : reqReturn;
}

bool Thread::HandleSyncEvent(ExecutionEvent* e) {
	auto se = dynamic_cast<SynchronizationExecutionEvent *>(e);

    const int cpu = peu->GetObject<CPU>()->GetId();

	if (se->temp) {
		/* Check if temp synch is empty. If not, carry out command and remove ourself
		 * from the list of users */

		bool toReturn;
		if (!se->global) {
			for (unsigned int i = 0; i < m_currentLocation->tempvar.users.size(); i++)
				// Check all users in local tempvar
				if (m_currentLocation->tempvar.users[i] == m_currentLocation->program->sem->name) {
					toReturn = m_scheduler->TempSynchRequest(cpu, se->synchType, m_currentLocation->tempvar.tempSynch, se->args);
					m_currentLocation->tempvar.users.erase(m_currentLocation->tempvar.users.begin() + i);
					if (m_currentLocation->tempvar.users.empty())
						m_scheduler->DeallocateTempSynch(m_currentLocation->tempvar.tempSynch);

					if (recordExecStats)
						if (!toReturn)
							m_currentLocation->wasBlocked = Simulator::Now().GetNanoSeconds();

					// If there was a context switch, only return false (blocked) if we're not in an interrupt handler
					return this->peu->hwModel->cpus[cpu]->inInterrupt ? true : toReturn;
				}
		} else {
			// Check all users among global tempvars
			Ptr<ExecEnv> execEnv = peu->hwModel->node->GetObject<ExecEnv>();
			if(traceOn)
				PrintGlobalTempvars(execEnv);
			for (auto it = execEnv->tempVars.begin(); it != execEnv->tempVars.end(); ++it) {
				struct tempVar *tv = &(*it);
				for (unsigned int i = 0; i < tv->users.size(); i++) {
					if(traceOn) std::cout << tv->users[i] << std::endl;
					if (tv->users[i] == m_currentLocation->program->sem->name) {
						toReturn = m_scheduler->TempSynchRequest(cpu, se->synchType, tv->tempSynch, se->args);
						tv->users.erase(tv->users.begin() + i);
						if (tv->users.empty()) {
							m_scheduler->DeallocateTempSynch(tv->tempSynch);
							execEnv->tempVars.erase(it);
						}

						if (recordExecStats)
							if (!toReturn)
								m_currentLocation->wasBlocked = Simulator::Now().GetNanoSeconds();

						// If there was a context switch, only return false (blocked) if we're not in an interrupt handler
						return this->peu->hwModel->cpus[cpu]->inInterrupt ?
							true : toReturn;
					}
				}
			}
		}
	}

	/* Execute the request. This will be routed through the
	 * implementation specific SchedSim, potentially next via
	 * wrappers to C-code as in LinSched, and finally to execute
	 * the proper function in the SchedSim code.
	 *
	 * Synchrequest returns true if there was a task switch,
	 * in which case we must return false. The next thread is
	 * already scheduled to execute.
	 */
	bool synchReturn = m_scheduler->SynchRequest(cpu, se->synchType, se->id, se->args);
	if (recordExecStats)
		if (!synchReturn)
			m_currentLocation->wasBlocked = Simulator::Now().GetNanoSeconds();

	return this->peu->hwModel->cpus[cpu]->inInterrupt ? true : synchReturn;
}

bool Thread::HandleCondition(ExecutionEvent* e) {
	auto ce = dynamic_cast<Condition *>(e);
    Ptr<ExecEnv> execEnv = peu->hwModel->node->GetObject<ExecEnv>();

	/* If state condition AND write, write to local or global
	 * variable according to scope of the variable. Elsewise, change
	 * the currently running program by ce->getClosestEntry.
	 */
	if (ce->condType == STATECONDITION) {
		auto sce = (StateCondition *) ce;
		if (sce->operation == CONDITIONWRITE) {
			if (sce->scope == CONDITIONLOCAL) {
				if (sce->hasSetterFunction)
					ce->setConditionState(this, sce->value);
				else
					m_currentLocation->getLocalStateVariable(sce->name)->value = sce->value;
			} else if (sce->scope == CONDITIONGLOBAL) {
				if (sce->hasSetterFunction)
					ce->setConditionState(this, sce->value);
				else
					execEnv->globalStateVariables[sce->name] = sce->value;
			}

			return true;
		}
	}

	/* We need to set the program pointer to the one obtained in teh
	 * condition. But first, store the root program in m_currentLocation
	 * so that if we are in a loop, and it re-starts, we can re-set
	 * the program pointer to the root program of that loop. But this
	 * should only happen if we have not already encountered a condition
	 * previously, which is indicated by rootCondition not being nullptr.
	 */
	if (m_currentLocation->rootProgram == nullptr)
		m_currentLocation->rootProgram = m_currentLocation->program;

	m_currentLocation->program = ce->getClosestEntry(this).second;
	m_currentLocation->currentEvent = -1; // will be set to 0 in Dispatch()

	return true;
}

void Thread::SetScheduler(Ptr<RoundRobinScheduler> scheduler) {
	m_scheduler = scheduler;
}

int Thread::GetPid() {
	return m_pid;
}

void Thread::Terminate() {

}

void Thread::ResumeProcessing() {
	peu->Consume(&m_currentProcessing);
}

void Thread::PreEmpt() {
	/* Re-calculate remaining resources based on
	 * remaining time until the completion event finishes:
	 *
	 * fractionConsumed = MILLISECONDS / remaining time
	 * newRemaining = total * (1 - fractionConsumed)
	 *
	 * Then cancel the event
	 */

	if(m_currentProcessing.remaining[NANOSECONDS].amount != 0) {
		int64_t timeLeft = Simulator::GetDelayLeft(m_currentProcessing.processingCompleted).GetMilliSeconds();
		double fractionCompleted = (double) timeLeft / m_currentProcessing.remaining[NANOSECONDS].amount;

		// Calculate remaining resource consumptions
		for (auto remaining : m_currentProcessing.remaining)
			if (remaining.defined)
                remaining.amount = remaining.amount * (1 - fractionCompleted);
	}

	/* Finally we cancel the event. If will be re-scheduled
	 * by PEU::Consume() at a later point.
	 */
	Simulator::Cancel(m_currentProcessing.processingCompleted);
}

// Dispatch is allways called when executing a service on a PEU
void Thread::Dispatch() {
	/* Make sure the stack is not empty, i.e., that we have
	 * at least a root program
	 */
	if (m_programStack.empty()) {
		NS_LOG_ERROR("Attempted to dispatch thread " << m_pid << " without a root program.");
	} else {

		/* In rare cases we might be interrupted between process statements,
		 * e.g., in the 310814-case where we where Simulator::ScheduleNow()
		 * scheduled an interrupt to be scheduled between context switches
		 * caused by a scheduler request (COMPL). In these cases, we must
		 * pre-empt ourselves once dispatches because at the point of
		 * dispatch, the CPU is actually interrupted. This pre-empt was not
		 * performed during interrupt, because there waS not processing
		 * statement executing due to the interrupt being issued between
		 * processing stages.
		 */
		if(this->peu) {
            const int cpu = peu->GetObject<CPU>()->GetId();
            NS_LOG_INFO("Dispatched thread on CPU" << cpu);
			if(this->peu->hwModel->cpus[cpu]->inInterrupt && this != this->peu->hwModel->cpus[cpu]->interruptThread) {
				this->PreEmpt();
				return;
			}
        }
		/* Are we returning from being interrupted from processing by an
		 * interrupt of task switch? If so, we simply resume processing.
		 */
		if (!m_currentProcessing.done) {
			ResumeProcessing();
			return;
		}

		/* Continue executing statements until handler returns true.
		 * In this case, we have one of the following cases:
		 * (1) We have encountered a processing stage, and the handler
		 *     has taken the approapriate action; either to schedule
		 *     Proceed() if the next scheduler event is scheduled at a
		 *     later point, or not if the next scheduler event preceeds
		 *     the end of the processing duration.
		 * (2) The handler have put this thread to sleep.
		 * (3) We have encountered the end of the root program. The handler
		 *     calls the scheduler wrapper to terminate the task. Note that
		 *     when we encounter the end of a child program, the handler
		 *     should pop the program stack, and set the state variables
		 *     accordingly, and thus not return true.
		 */
		bool proceed = true;
		while (proceed) {
			m_currentLocation = m_programStack.top();

			m_currentLocation->currentEvent++;
			int currentEvent = m_currentLocation->currentEvent;

            ExecutionEvent *e = m_currentLocation->program->events[currentEvent];
			proceed = HandleExecutionEvent(e);

			if (m_currentLocation->curPkt != nullptr) {
				ExecutionInfo *pktEI = &(m_currentLocation->curPkt->m_executionInfo);
				if (e->checkpoint.length() != 0 && pktEI->target == e->checkpoint && pktEI->targetFPM != nullptr) {
					pktEI->executedByExecEnv = true;
					EventImpl *toInvoke = pktEI->targetFPM;
					toInvoke->Invoke();
                    //toInvoke->Unref();  // TODO: Espen, figure out why this causes crashes and almost all packets dropping due to receive-queue overflow
				}
			}

			/* Must check if there are any more statements to execute. If not,
			 * terminate the thread. Note that this should never occur for
			 * single-threaded PEUs, as they should simply run one PEU in an
			 * infinite loop.
			 */
            if (m_programStack.empty()) {
                if (m_scheduler->need_scheduling) {
                    m_scheduler->need_scheduling = false;
                    m_scheduler->Schedule();
                }
                m_scheduler->Terminate(this->peu, (uint32_t)m_pid);
				break;
			}
		}
	}
}

} // namespace ns3

