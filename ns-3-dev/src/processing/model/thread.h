#ifndef THREAD_H
#define THREAD_H

#include "ns3/executioninfo.h"
#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable.h"
#include "ns3/cep-engine.h"

#include "program.h"

#include "rrscheduler.h"

#include <vector>
#include <stack>
#include <map>
#include <iostream>
#include <fstream>

namespace ns3 {

class TaskScheduler;
class PEU;
class Program;
class ProcessingInstance;
class Packet;
class ExecutionEvent;

/* For now, we assume TEMPSYNCH means temporary completion in Linux.
 * As future work, we will extend this to allow specification of the
 * type of temporary synchronization variable. */
struct tempVar {
	std::vector<std::string> users;
	void *tempSynch = nullptr;
};

class ProgramLocation : public SimpleRefCount<ProgramLocation>  {
public:
	ProcessingInstance currentProcessing;
	Program *program;
	Program *rootProgram;
	int currentEvent;
	Ptr<Packet> curPkt;
	Ptr<CepOperator> curCepQuery;
	Ptr<CepEvent> curCepEvent;
	struct tempVar tempvar;

	// To keep track of queues
	LoopCondition *lc;
	uint32_t curIteration;
	int curServedQueue;

	// To store local state variables
	std::map<std::string, Ptr<StateVariable> > localStateVariables;
	std::map<std::string, Ptr<StateVariableQueue> > localStateVariableQueues;

	// Methods
	Ptr<StateVariableQueue> getLocalStateVariableQueue(std::string queueID);
	Ptr<StateVariable> getLocalStateVariable(std::string);

	// For execution statistics
	int64_t wasBlocked;

	ProgramLocation() {
		lc = nullptr;
		program = nullptr;
		rootProgram = nullptr;
		currentEvent = 0;
		curPkt = nullptr;
		curIteration = 0;
		curServedQueue = 0;
		curCepEvent = nullptr;
		curCepQuery = nullptr;
		wasBlocked = 0;
	}
};


class Thread : public Object
{
public:
	static TypeId GetTypeId ();

	void SetPid(int pid);
    void SetScheduler(Ptr<RoundRobinScheduler> scheduler);
	void Dispatch();

	void PreEmpt();

	int GetPid();

	Thread ();
	~Thread () override;

	// The PEU on which this thread executes
	Ptr<PEU> peu;

	// To keep track of location of thread of execution
	std::stack<Ptr<ProgramLocation> > m_programStack;

	ExecutionInfo m_executionInfo;

	void DoneProcessing();

	void ResumeProcessing(); // Called by Dispatch() when re-activated

	bool HandleExecutionEvent(ExecutionEvent *e);

	bool HandleEndEvent(ExecutionEvent* e);
	bool HandleProcessingEvent(ExecutionEvent* e);
	bool HandleExecuteEvent(ExecutionEvent* e);
	bool HandleQueueEvent(ExecutionEvent* e);
	bool HandleCopyQueueEvent(ExecutionEvent* e);
	bool HandleDuplicatePacketEvent(ExecutionEvent* e);
	bool HandleSchedulerEvent(ExecutionEvent* e);
	bool HandleSyncEvent(ExecutionEvent* e);
	bool HandleCondition(ExecutionEvent* e);

	// For debug
	void PrintGlobalTempvars(Ptr<ExecEnv> ee);

	/* When the root program encounters
	 * END, we must terminate the thread
	 */
	void Terminate();

	// Pointer to scheduler object
    Ptr<RoundRobinScheduler> m_scheduler;

	// Program ID obtained from SchedSim
	int m_pid;

	/* Currently executing processing instance.
	 * We don't need it to be a stack, since
	 * the processing instances for interrupts
	 * are stored in the PEU.
	 */
	ProcessingInstance m_currentProcessing;

	// The program and event currently running
	Ptr<ProgramLocation> m_currentLocation;
};


} // namespace ns3

#endif /* THREAD_H */
