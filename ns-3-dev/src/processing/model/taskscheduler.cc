#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/program.h"
#include "ns3/uinteger.h"

#include "taskscheduler.h"
#include "thread.h"
#include "peu.h"
#include "hwmodel.h"
#include "execenv.h"
#include "interrupt-controller.h"
#include "sem.h"

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TaskScheduler");
NS_OBJECT_ENSURE_REGISTERED (TaskScheduler);

TypeId 
TaskScheduler::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::processing::TaskScheduler")
    .SetParent<Object> ()
    .AddConstructor<TaskScheduler> ();
  return tid;
}

TaskScheduler::~TaskScheduler () = default;

TaskScheduler::TaskScheduler() 
{
    for (int i = 0; i < NUM_CPU; ++i) {
        m_currentRunning[i] = -1;
    }
}

Ptr<Thread>
TaskScheduler::GetCurrentRunningThread(int cpu) {
    return m_threads[m_currentRunning[cpu]];
}

/*
 * CheckForPreemptions checks the vector of current running pids, and issues any 
 * neccessary preemptions. Returns false if the specified cpu has a new CR,
 * ie. the currently running thread should stop executing.
 * Returns true if the thread on the specified cpu should continue executing.
 * m_currentRunning is updated in PreEmpt().
 */
bool
TaskScheduler::CheckAllCPUsForPreemptions(int cpu, std::vector<int> previouslyRunning) {
    bool ret = true;
    std::vector<int> currentRunning = DoCurrentRunning();
    for(unsigned int i = 0; i < peu->hwModel->cpus.size(); i++) {
        if (currentRunning[i] != previouslyRunning[i]) {
            PreEmpt(i, currentRunning[i]);
            if (i == (unsigned int)cpu) ret = false;
        }
    }

    return ret;
}

void
TaskScheduler::Terminate(Ptr<PEU> peu, unsigned int pid)
{
    NS_LOG_INFO("Terminating " << pid);
    Ptr<CPU> cpu = peu->GetObject<CPU>();
    if (cpu->inInterrupt) {
        cpu->inInterrupt = false;
        cpu->hwModel->m_interruptController->Proceed(cpu->GetId());
    } else {
        NS_ASSERT(0);
    }
}

// Must be implemented by a SimSched-specific sub-class
bool
TaskScheduler::Request(int cpu, int type, std::vector<uint32_t> arguments)
{
    std::vector<int> previouslyRunning = std::vector<int>(m_currentRunning, m_currentRunning + NUM_CPU);
    DoRequest(cpu, type, std::move(arguments));

    return CheckAllCPUsForPreemptions(cpu, previouslyRunning);
}

// Must be implemented by a SimSched-specific sub-class
std::vector<int>
TaskScheduler::CurrentRunning()
{
  return std::vector<int>();
}

void
TaskScheduler::HandleSchedulerEvent()
{
  // Obtain iterator
  //  Simulator::Schedule(m_timeslice, &TaskScheduler::HandleSchedulerEvent, this);
}

// Creates and initializes data structures for this node as well as 
// creating the idle thread.
void
TaskScheduler::Initialize(Ptr<PEU> cpuPEU)
{
  // Set the CPU
  peu = cpuPEU;
  Simulator::ScheduleNow(&TaskScheduler::SetupIdleThreads, this);

  // Call SchedSim-specific DoStart
  DoInitialize();
}

void
TaskScheduler::SetupIdleThreads()
{
  // Create resource consumption for idle thread
  ResourceConsumption idleResources;
  idleResources.consumption = NormalVariable(10000000000000, 0);
  idleResources.defined = true;

  // Create processing stage
  auto idleProcessing = new ProcessingStage();

  idleProcessing->resourcesUsed[NANOSECONDS] = idleResources;

  // Create idle program and store in program:
  // - Infinite loop start
  // - process for one million seconds
  // - infinite loop end
  auto idleProgram = new Program();
  idleProgram->events.push_back(idleProcessing);
  auto end = new ExecutionEvent();
  end->type = END;
  idleProgram->events.push_back(end);

  // Create and register a SEM, in which we only store the CPU as
  // the PEU. This is because the thread calls Consume()
  // of the peu on which the program runs. We do not need
  // to create any measurements, though, as the thread
  // should (!) never call execute, i.e., it is never
  // allowed to execute the idle program from another
  // thread.
  Ptr<SEM> idleSEM= Create<SEM> ();
  idleSEM->name = "idlethread";
  idleSEM->peu = peu;
  peu->hwModel->node->GetObject<ExecEnv> ()->m_serviceMap["idle"] = idleSEM;
//  std::cout << "idleProgram->sem before setting it: " << idleProgram->sem << std::endl;
  idleProgram->sem = idleSEM;
//  std::cout << "idleProgram->sem after setting it: " << idleProgram->sem << std::endl;

  Ptr<ProgramLocation> toe = Create<ProgramLocation>();
  toe->program = idleProgram;
  toe->currentEvent = -1; // Will be incremented by Dispatch()
  toe->lc = new LoopCondition();
  toe->lc->emptyQueues = idleProgram;
  toe->lc->maxIterations = 0;
  toe->lc->perQueue = false;

    std::vector<int> idleThreadPids = DoCurrentRunning();
    for (unsigned int i = 0; i < this->peu->hwModel->cpus.size(); i++) {
        Ptr<Thread> idleThread = CreateObject<Thread>();
        idleThread->SetPid(idleThreadPids[i]);
        idleThread->m_programStack.push(toe);

        m_currentRunning[i] = idleThreadPids[i];

        m_threads[idleThreadPids[i]] = idleThread;

        idleThread->peu = this->peu->hwModel->cpus[i];
        idleThread->Dispatch();
    }
}

void
TaskScheduler::AllocateSynch(int type, std::string id, std::vector<uint32_t> arguments)
{
  return DoAllocateSynch(type, std::move(id), std::move(arguments));
}


bool
TaskScheduler::SynchRequest(int cpu, int type, std::string id,  std::vector<uint32_t> arguments)
{
  std::vector<int> previouslyRunning = DoCurrentRunning();

  DoSynchRequest(cpu, type, std::move(id), std::move(arguments));

  return CheckAllCPUsForPreemptions(cpu, previouslyRunning);
}

uint32_t
TaskScheduler::GetSynchReqType(std::string name)
{
  return DoGetSynchReqType(std::move(name));
}

uint32_t
TaskScheduler::DoGetSynchReqType(std::string name)
{
  NS_LOG_ERROR("DoGetSynchRequest not implemented in SchedSim");
  return -1;  
}

int
TaskScheduler::DoSynchRequest(int cpu, int type, std::string id,  std::vector<uint32_t> arguments)
{
  NS_LOG_ERROR("DoSynchRequest not implemented in SchedSim");
  return -1;
}

int
TaskScheduler::DoTempSynchRequest(int cpu, int type, void *var, std::vector<uint32_t> arguments)
{
  NS_LOG_ERROR("DoTempSynchRequest not implemented in SchedSim");
  return -1;
}

bool TaskScheduler::TempSynchRequest(int cpu, int type, void* var, std::vector<uint32_t> arguments){
    std::vector<int> previouslyRunning = DoCurrentRunning();

    DoTempSynchRequest(cpu, type, var, std::move(arguments));

    return CheckAllCPUsForPreemptions(cpu, previouslyRunning);
}

void TaskScheduler::DeallocateTempSynch(void* var) {
	DoDeallocateTempSynch(var);
}

void
TaskScheduler::PreEmpt(int cpu, int new_pid)
{
    Ptr<Thread> cr = GetCurrentRunningThread(cpu);

    if (!this->peu->hwModel->cpus[cpu]->inInterrupt)
        if (!cr->m_currentProcessing.done) // TODO OYSTEDAL: We should possibly have a CR here
            cr->PreEmpt();

    int oldPid = m_currentRunning[cpu];
    m_currentRunning[cpu] = new_pid;

    NS_LOG_INFO("CPU" << cpu << ": Context switch " << oldPid << " -> " << new_pid);

    Ptr<Thread> new_cr = GetCurrentRunningThread(cpu);

    new_cr->peu = this->peu->hwModel->cpus[cpu];

    if (!this->peu->hwModel->cpus[cpu]->inInterrupt)
        Simulator::ScheduleNow(&Thread::Dispatch, new_cr);
}

Ptr<Thread>
TaskScheduler::Fork(
        std::string threadName,
		Program *program,
		int priority,
		Ptr<Packet> currentPacket,
		std::map<std::string, Ptr<StateVariable> > localVars,
		std::map<std::string, Ptr<StateVariableQueue> > localStateQueues,
		bool infinite) {
  Ptr<Thread> t = CreateObject<Thread> ();
  t->name = threadName;
  t->peu = peu;
  t->SetScheduler(peu->taskScheduler);

  // Create a new program location, and
  // push onto the new thread's stack
  Ptr<ProgramLocation> pl = Create<ProgramLocation>();
  pl->program = program;
  pl->currentEvent = -1;
  //pl->curPkt = currentPacket;
  pl->m_executionInfo->packet = currentPacket;
  pl->localStateVariableQueues = std::move(localStateQueues);
  pl->localStateVariables = std::move(localVars);


  // Generate infinite loop if requested
  if(infinite) {
	  pl->lc = new LoopCondition();
	  pl->lc->maxIterations = 0;
  }

  t->m_programStack.push(pl);

  // OYSTEDAL: 
  //
  // A fork may cause a reschedule, which might even update more than one core.
  // A rough sketch of the algo would be:
  // - Get the previously running pids
  // - Perform fork
  // - Perform any preemptions that is provoked by the fork
  std::vector<int> previouslyRunning = DoCurrentRunning();

  int pid = DoFork(priority);
  threadPids[threadName] = pid;

  t->SetPid(pid);
  m_threads[pid] = t;

  if (m_threads.size() <= NUM_CPU) {
      // t->Dispatch();
  } else {
      CheckAllCPUsForPreemptions(0, previouslyRunning);
  }

  return t;
}


void *TaskScheduler::AllocateTempSynch(int type, std::vector<uint32_t> arguments){
	return DoAllocateTempSynch(type, std::move(arguments));
}



////////////
// The following should be implemented by the SchedSim-specific sub-class
////////////

void* TaskScheduler::DoAllocateTempSynch(int type, std::vector<uint32_t> arguments) {
	  NS_LOG_ERROR("DoAllocateTempSynch not specialized by SchedSim wrapper");
	  return 0;
}

void TaskScheduler::DoDeallocateTempSynch(void* var) {
	  NS_LOG_ERROR("DoDeallocateTempSynch not specialized by SchedSim wrapper");
}

void
TaskScheduler::DoTerminate()
{
  /* If we wind up here, no SchedSim is specified. This is the
   * case for PEUs other than the CPU.
   */

  // Nothing to do.
}

void
TaskScheduler::DoInitialize()
{
  NS_LOG_ERROR("DoStart not specialized by SchedSim wrapper");
}

int
TaskScheduler::DoFork(int priority) {
  /* If we wind up here, no SchedSim is specified. This is the
   * case for PEUs other than the CPU.
   */

  // Nothing to do. Simply return 0, this value will not be of importance.
  return 0;
}

void
TaskScheduler::DoHandleSchedulerEvent()
{
  NS_LOG_ERROR("DoHandleSchedulerEvent not specialized by SchedSim wrapper");
}

int
TaskScheduler::DoRequest(int cpu, int type, std::vector<uint32_t> arguments)
{
  NS_LOG_ERROR("DoRequest not specialized by SchedSim wrapper");
  return 0;
}

std::vector<int>
TaskScheduler::DoCurrentRunning()
{
  NS_LOG_ERROR("DoCurrentRunning not specialized by SchedSim wrapper");
  return std::vector<int>();
}

void
TaskScheduler::DoAllocateSynch(int type, std::string id, std::vector<uint32_t> arguments)
{
  NS_LOG_ERROR("DoAllocateSynch not implemented by SchedSim");
}

/* FOR INTERRUPTS */
void
InterruptScheduler::DoTerminate() {
  // Here, the PEU must be the cpu
  peu->hwModel->m_interruptController->Proceed(peu->GetObject<CPU>()->GetId());
}

ParallelThreadsScheduler::ParallelThreadsScheduler() = default;

NS_OBJECT_ENSURE_REGISTERED (ParallelThreadsScheduler);

/* FOR (current models of) PEUs */
TypeId 
ParallelThreadsScheduler::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::processing::ParallelThreadsScheduler")
    .SetParent<TaskScheduler> ()
    .AddConstructor<ParallelThreadsScheduler> ()
    .AddAttribute("maxthreads", "Maximum number of threads running in parallel",
                  UintegerValue(1),
		  MakeUintegerAccessor(&ParallelThreadsScheduler::m_maxThreads),
		  MakeUintegerChecker<uint32_t> ())
    ;

 return tid;
}

void
ParallelThreadsScheduler::Initialize(Ptr<PEU> nonCPUPEU)
{
  /* Initialize bitmap
   * Create N threads according to attribute
   */
  for(unsigned int i = 0; i < m_maxThreads; i++) {
    Ptr<Thread> t = CreateObject<Thread> ();
    t->SetPid(i);
    m_threads[i] = t;
  } 
}

/* We override Terminate, as threads should not
 * be removed, only be flagged as no longer active.
 * Can only be called by the running thread itself.
 */
void
ParallelThreadsScheduler::Terminate(Ptr<PEU> peu, unsigned int pid)
{
  m_freeList.push_back(pid);
}

/* Returns nullptr if no free thread, a smart pointer to thread elsewise.
 * Priority is not used, but must be there to override the base class
 * version of this function
 */
Ptr<Thread>
ParallelThreadsScheduler::Fork(
		std::string threadName,
		Program *program,
		int priority,
		Ptr<Packet> currentPacket,
		std::map<std::string, Ptr<StateVariable> > localVars,
		std::map<std::string, Ptr<StateVariableQueue> > localStateQueues,
		bool infinite)
{
  // If no more threads are free, return
  if (m_freeList.empty())
    return nullptr;

  // Find first non-active thread
  unsigned int freePid = m_freeList.front();
  Ptr<Thread> t = m_threads[freePid];

  // Remove this thread from the free list
  m_freeList.pop_front();

  // Create a new program location, and push onto the new thread's stack
  Ptr<ProgramLocation> pl;
  pl->program = program;
  pl->currentEvent = 0;
  //pl->curPkt = currentPacket;
  pl->m_executionInfo->packet = currentPacket;
  t->m_programStack.push(pl);
  pl->localStateVariables = localVars;
  pl->localStateVariableQueues = localStateQueues;

  // Dispatch thread
  t->Dispatch();

  return t;
}

} // namespace ns3
