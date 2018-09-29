#ifndef RRSCHEDULER_H
#define RRSCHEDULER_H

// TODO: Move to taskscheduler
enum RequestType {
  AWAKE = 0,
  SLEEPTHREAD
};

#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/local-state-variable-queue.h"
#include "ns3/local-state-variable.h"

#include <ns3/sync.h>

#include <vector>
#include <map>
#include <list>
#include <set>

namespace ns3 {

class RoundRobinScheduler : public TaskScheduler
{
public:
  static TypeId GetTypeId (void);
  bool need_scheduling = false;

  RoundRobinScheduler();
  virtual ~RoundRobinScheduler();

  void Schedule();

  /* To keep track of mapping between PID and thread name.
   * The latter is used with scheduler requests.
   * std::map<std::string, int> threadPids;
   */
  std::list<int> m_runqueue;
  std::set<int> m_blocked;
  std::vector<int> m_currentRunning;

protected:
  void RescheduleCPU(int);
  void WakeupIdle();

  // The functions below must be implemented by a subclass for a specific SchedSim
  virtual void DoInitialize();
  virtual void DoHandleSchedulerEvent();
  virtual int DoFork(int priority);
  virtual void DoTerminate();
  virtual std::vector<int> DoCurrentRunning();
  virtual void DoAllocateSynch(int type, std::string id, std::vector<uint32_t> arguments);
  virtual void* DoAllocateTempSynch(int type, std::vector<uint32_t> arguments);
  virtual void DoDeallocateTempSynch(void* var);
  virtual int DoRequest(int cpu, int type, std::vector<uint32_t> arguments);
  virtual int DoSynchRequest(int cpu, int type, std::string id, std::vector<uint32_t> arguments);
  virtual int DoTempSynchRequest(int cpu, int type, void *var, std::vector<uint32_t> arguments);
  virtual uint32_t DoGetSynchReqType(std::string name);

  void MigrateThread(int pid, Ptr<Thread> thread);
private:

  std::map<std::string, Ptr<Semaphore> > m_semaphores;
};

}

#endif
