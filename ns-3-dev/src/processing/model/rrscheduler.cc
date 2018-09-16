#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"

#include "ns3/local-state-variable-queue.h"
#include "ns3/local-state-variable.h"

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

#include "ns3/rrscheduler.h"

namespace ns3 {

// From Schedsim
enum SynchRequestType {
  SEM_UP = 0,
  SEM_DOWN,
  WAIT_COMPL,
  COMPL
};

static unsigned int g_nextPid = NUM_CPU + 1; // First pid after idle threads

NS_LOG_COMPONENT_DEFINE ("RoundRobinScheduler");
NS_OBJECT_ENSURE_REGISTERED (RoundRobinScheduler);

TypeId
RoundRobinScheduler::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::processing::RoundRobinScheduler")
    .SetParent<TaskScheduler> ()
    .AddConstructor<RoundRobinScheduler> ();
  return tid;
}

RoundRobinScheduler::RoundRobinScheduler() : TaskScheduler()
{
    // m_currentRunning = NULL;
    m_currentRunning.reserve(NUM_CPU);

    // Add idle threads
    for (int i = 1; i <= NUM_CPU; ++i) {
        m_currentRunning.push_back(i);
    }
}

RoundRobinScheduler::~RoundRobinScheduler () {
}

bool called_from_here = false;
void RoundRobinScheduler::Schedule() {
    //NS_LOG_INFO("Schedule()");
    //return;
    //
    // OYSTEDAL: TODO: Add handling for idle threads.


    /*int oldCr = m_runqueue.front();
    m_runqueue.push_back( oldCr );
    m_runqueue.pop_front();
    int newCr = m_runqueue.front();

    if (oldCr != newCr) {
		NS_LOG_INFO("Preempt()");
		NS_LOG_INFO("oldCr = " << oldCr << " newCr = " << newCr);
	TaskScheduler::PreEmpt(0, newCr);
    }*/

    /*if (m_runqueue.empty()) {
        // No other threads to run, no need to reschedule
    } else {*/
        for (unsigned int cpu = 0; cpu < TaskScheduler::peu->hwModel->cpus.size(); ++cpu) {
            // Don't interrupt an interrupt
            if (TaskScheduler::peu->hwModel->cpus[cpu]->inInterrupt) {
                NS_LOG_INFO("CPU " << cpu << " was in interrupt, not scheduling");
                //continue;  // We do this to wake a thread from the end of a HIRQ
            }

            if (m_runqueue.empty() || m_runqueue.front() == m_currentRunning[cpu]) {
                m_currentRunning[cpu] = cpu + 1; // Assign idle thread if nothing to schedule
            } else {
                int oldCr = m_currentRunning[cpu];
                int newCr = m_runqueue.front();

                m_runqueue.pop_front();
                if (oldCr > NUM_CPU) // Don't queue idle threads
                    m_runqueue.push_back(oldCr);

                m_currentRunning[cpu] = newCr;

                TaskScheduler::PreEmpt(cpu, newCr);
            }
        }
    //}

    // If there is more than one thread, they need to get preempted once in a while
    /*if (m_runqueue.size() > 1)
        Simulator::Schedule(MicroSeconds(150), // TODO: make property?
                &RoundRobinScheduler::Schedule,
                this
                );*/
}

void RoundRobinScheduler::WakeupIdle() {
    if (m_runqueue.empty()) return;

    for (unsigned int cpu = 0; cpu < TaskScheduler::peu->hwModel->cpus.size(); ++cpu) {
        if (m_currentRunning[cpu] <= NUM_CPU) {
            m_currentRunning[cpu] = m_runqueue.front();
            m_runqueue.pop_front();

            if (m_runqueue.empty()) return;
        }
    }
}

void RoundRobinScheduler::RescheduleCPU(int cpu) {
    if (!m_runqueue.empty()) {
        m_currentRunning[cpu] = m_runqueue.front();
        m_runqueue.pop_front();
    } else {
        m_currentRunning[cpu] = cpu + 1; // This equals the PID of the idle
                                         // thread for the CPU, which is set up
                                         // by the constructor.
    }
}

void RoundRobinScheduler::DoInitialize() {
    NS_LOG_INFO (TaskScheduler::peu->m_name << " DoInitialize()");
    Simulator::ScheduleNow(&RoundRobinScheduler::Schedule, this);
}

void RoundRobinScheduler::DoHandleSchedulerEvent() {

}

int RoundRobinScheduler::DoFork(int priority) {
    // Priority is unused for now.

    const int pid = g_nextPid++;
    NS_LOG_INFO (TaskScheduler::peu->m_name << " Forked a new thread with pid " << pid);
    m_runqueue.push_back(pid);
    return pid;
}

void RoundRobinScheduler::DoTerminate(void) {
    NS_ASSERT_MSG(0, "Process terminated in RRSched, not supported");
    // m_runqueue.pop_front();
}

std::vector<int> RoundRobinScheduler::DoCurrentRunning(void) {
    /*
    if (m_runqueue.empty()) return 1;
    return m_runqueue.front();
    */
    return m_currentRunning;
}

void RoundRobinScheduler::DoAllocateSynch(int type, std::string id, std::vector<uint32_t> arguments) {
    switch (type) {
        case 0: // Semaphore
            {
                Ptr<Semaphore> sem = new Semaphore(arguments[0]);
                m_semaphores[id] = sem;
            }
            break;
        default: // Unknown sync primitive
            NS_LOG_ERROR(id << "is of unknown synchronization type " << type);
            NS_ASSERT(0);
    }
}

void* RoundRobinScheduler::DoAllocateTempSynch(int type, std::vector<uint32_t> arguments) {
    switch (type) {
        case 1: // Completion
            {
                NS_LOG_INFO("Allocating tempsync");
                // No arguments for completions
                // Ptr<Completion> c = new Completion();
                // c->Ref();
                Completion* c = new Completion();
                return (void*)c;
            }
            break;
        default:
            break; // Fall though to the assert
    }

    NS_LOG_ERROR(type << " is an unknown temp. synchronization type");
    NS_ASSERT(0);
    return NULL;
}

void RoundRobinScheduler::DoDeallocateTempSynch(void* var) {
    // This could be dangerous if var is not a pointer to a completion, beware...
    // Ptr<Completion> c = Ptr<Completion>((Completion*)var);
    // c->Unref();

    NS_LOG_INFO("Deallocating tempsync");
    delete (Completion*)var;
}

int RoundRobinScheduler::DoRequest(int cpu, int type, std::vector<uint32_t> arguments) {
    int pid;
    switch (type) {
        case AWAKE:
            {
                //std::cout << "Awakening" << std::endl;
                //return 1;
                pid = arguments[0];
                if (!m_blocked.erase(pid)) {
                    //std::cout << "But not really" << std::endl;
                    NS_LOG_ERROR( TaskScheduler::peu->m_name << " Tried to awake thread not present in the blocking queue! PID: " << pid);
                    goto end;
                }
#if 0
                // OYSTEDAL: This is just a sanity check, in most cases this can be disabled
                // to avoid the additional complexity.
                std::list<int>::iterator it = std::find(m_runqueue.begin(), m_runqueue.end(), pid);
                if (it != m_runqueue.end()) {
                    NS_LOG_ERROR("Tried to awake pid already present in runqueue!");
                    break;
                }
#endif
                NS_LOG_INFO (TaskScheduler::peu->m_name << " Waking up " << pid);
                m_runqueue.push_back(pid);
                this->need_scheduling = true;

                //WakeupIdle();
                //this->Schedule();
                //std::cout << "AWAKING" << std::endl;

                break;
            }
        case SLEEP:
            // We insert the pid into the blocking queue, to be awakened later.
            pid = arguments[0];
            m_blocked.insert( pid );
            this->Schedule();
            //std::cout << "Going to sleep" << std::endl;
            return 1;
            NS_LOG_INFO("SLEEP PID " << pid << " " << m_currentRunning[0] << " " <<  m_currentRunning[1]);

            //std::cout << "Going to sleep" << std::endl;
            if (m_runqueue.empty()) {
                // TODO: Assign an idle thread instead
                NS_ASSERT(0);
            }

            int i;
            for(i = 0; i < NUM_CPU; i++) {
                if (m_currentRunning[i] == pid) {
                    break;
                }
            }

            NS_LOG_INFO("Removed from cpu" << i);

            // Shocks, couldn't find this pid amongst any of the currently
            // running pids. Better take it out of the run queue some day
            if (i == NUM_CPU) NS_ASSERT(0);

            m_blocked.insert( m_currentRunning[i] );

            // TODO: Add support for idle threads when nothing else is available
            //m_currentRunning[i] = m_runqueue.front();
            //m_runqueue.pop_front();
            //this->Schedule();
            WakeupIdle();

            return 1;
    }
end:
    // return m_runqueue.size() == 1;
	return m_runqueue.front();
}

void RoundRobinScheduler::MigrateThread(int pid, Ptr<Thread> thread) {
    NS_ASSERT(0); // This should not be used.
#if 0
	NS_LOG_INFO ("Thread " << pid << " was migrated to " << TaskScheduler::peu->m_name);

	m_runqueue.push_back(pid);
	m_threads[pid] = thread;
#endif
}

int RoundRobinScheduler::DoSynchRequest(int cpu, int type, std::string id, std::vector<uint32_t> arguments) {
    switch (type) {
        case SEM_UP:
            {
                NS_LOG_INFO("semaphore_up");
                std::map<std::string, Ptr<Semaphore> >::iterator it = m_semaphores.find(id);
                if (it == m_semaphores.end()) {
                    NS_LOG_ERROR("Unable to find semaphore named" << id);
                    NS_ASSERT(0);
                }

                Ptr<Semaphore> semaphore = it->second;

                if (semaphore->Up()) {
                    // Someone was unblocked, which means that we're passing
                    // the condition to the new process.
                    // The new process won't call down themselves, so we must
                    // do it ourself.
                    const int unblocked = semaphore->Unblock();
                    NS_LOG_INFO("Unblocked " << unblocked << " on semaphore " << id);
                    m_runqueue.push_back(unblocked);

                    WakeupIdle();

                    // We are in big trouble if we weren't supposed to unblocked
                    // on this semaphore after all.
                    NS_ASSERT_MSG(semaphore->Down(), "Unblocked on a semaphore that is not available!");
                }
            }
            break;
        case SEM_DOWN:
            {
                NS_LOG_INFO("semaphore_down");
                std::map<std::string, Ptr<Semaphore> >::iterator it = m_semaphores.find(id);
                if (it == m_semaphores.end()) {
                    std::cout << "Unable to find semaphore named " << id << std::endl;
                    NS_LOG_ERROR("Unable to find semaphore named" << id);
                    NS_ASSERT(0);
                }

                Ptr<Semaphore> semaphore = it->second;

                if (!semaphore->Down()) {
                    NS_LOG_INFO("Blocked " << m_currentRunning[cpu] << " on semaphore " << id);
                    semaphore->Block(m_currentRunning[cpu]);

                    RescheduleCPU(cpu);

                    return 1;
                } else {
                    NS_LOG_INFO("Past semaphore");
                }
            }
            break;
        default:
            NS_ASSERT(0); // Not implemented

    }

    return 0;
}

int RoundRobinScheduler::DoTempSynchRequest(int cpu, int type, void *var, std::vector<uint32_t> arguments) {
    switch (type) { // Completion
        case WAIT_COMPL:
            {
                Completion* c = (Completion*)var;
                
                // When we arrive here, the completion may be either completed
                // or not. If it is completed, just keep running.
                if (!c->IsCompleted()) {
                    c->SetWaiter(m_currentRunning[cpu]);

                    RescheduleCPU(cpu);
                }

                return 0;
            }
            break;
        case COMPL:
            {
                Completion* c = (Completion*)var;
                int pid = c->Complete();
                if (pid >= 0) {
                    // TODO: Maybe check if the pid isn't already in the runqueue?
                    m_runqueue.push_back(pid);

                    WakeupIdle();

                    // HACK: Because the dpc_thread runs with higher priority, 
                    // put it in the front of the runqueue, and enable immediately
                    // m_runqueue.push_front(pid);
                    // RescheduleCPU(cpu);
                } else {
                    NS_LOG_ERROR("Completion without waiting pid!");
                }

                return 0;
            }
            break;
    }
        
    NS_ASSERT(0); // Not implemented
    return 0;
}

uint32_t RoundRobinScheduler::DoGetSynchReqType(std::string name) {
    if(!name.compare("SEMUP"))
        return SEM_UP;
    else if(!name.compare("SEMDOWN"))
        return SEM_DOWN;
    else if(!name.compare("WAITCOMPL"))
        return WAIT_COMPL;
    else if(!name.compare("COMPL"))
        return COMPL;

    NS_ASSERT(0); // Not implemented
    return 0;
}

}