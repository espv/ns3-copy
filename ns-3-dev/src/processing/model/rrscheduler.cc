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
RoundRobinScheduler::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::processing::RoundRobinScheduler")
    .SetParent<TaskScheduler> ()
    .AddConstructor<RoundRobinScheduler> ();
  return tid;
}

RoundRobinScheduler::RoundRobinScheduler() : TaskScheduler()
{
    m_currentRunning.reserve(NUM_CPU);

    // Add idle threads
    for (int i = 1; i <= NUM_CPU; ++i) {
        m_currentRunning.push_back(i);
    }
}

RoundRobinScheduler::~RoundRobinScheduler () = default;

void RoundRobinScheduler::Schedule() {
    // OYSTEDAL: TODO: Add handling for idle threads.

    for (unsigned int cpu = 0; cpu < peu->hwModel->cpus.size(); ++cpu) {
        // Don't interrupt an interrupt
        if (!allowNestedInterrupts && peu->hwModel->cpus[cpu]->inInterrupt) {
            NS_LOG_INFO("CPU " << cpu << " was in interrupt, not scheduling");
            Simulator::Schedule(MicroSeconds(100), // TODO: make property?
                                &RoundRobinScheduler::Schedule,
                                this
            );
            continue;  // We do this to wake a thread from the end of a HIRQ
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

            PreEmpt(cpu, newCr);
        }
    }

    // If there is more than one thread, they need to get preempted once in a while
    if (!m_runqueue.empty())
        Simulator::Schedule(MicroSeconds(150), // TODO: make property?
                &RoundRobinScheduler::Schedule,
                this
                );
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
        m_currentRunning[cpu] = cpu + 1; /* This equals the PID of the idle
                                          * thread for the CPU, which is set up
                                          * by the constructor. */
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

void RoundRobinScheduler::DoTerminate() {
    NS_ASSERT_MSG(0, "Process terminated in RRSched, not supported");
}

std::vector<int> RoundRobinScheduler::DoCurrentRunning() {
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
                auto c = new Completion();
                return (void*)c;
            }
            break;
        default:
            break; // Fall though to the assert
    }

    NS_LOG_ERROR(type << " is an unknown temp. synchronization type");
    NS_ASSERT(0);
    return nullptr;
}

void RoundRobinScheduler::DoDeallocateTempSynch(void* var) {
    NS_LOG_INFO("Deallocating tempsync");
    delete (Completion*)var;
}

    int RoundRobinScheduler::DoRequest(int cpu, int type, std::vector<uint32_t> arguments) {
        int pid;
        switch (type) {
            case AWAKE:
            {
                pid = arguments[0];
                if (!m_blocked.erase(pid)) {
                    NS_LOG_ERROR( TaskScheduler::peu->m_name << " Tried to awake thread not present in the blocking queue! PID: " << pid);
                    break;
                }
                NS_LOG_INFO (TaskScheduler::peu->m_name << " Waking up " << pid);
                m_runqueue.push_back(pid);
                WakeupIdle();
                break;
            }
            case SLEEPTHREAD: {
                // We insert the pid into the blocking queue, to be awakened later.
                pid = arguments[0];
                m_blocked.insert(pid);
                RescheduleCPU(cpu);
                return 1;
            }
            default: {
                NS_ASSERT(0);
                break;
            }
        }
        return m_runqueue.front();
    }

void RoundRobinScheduler::MigrateThread(int pid, Ptr<Thread> thread) {
    NS_ASSERT(0); // This should not be used.
}

int RoundRobinScheduler::DoSynchRequest(int cpu, int type, std::string id, std::vector<uint32_t> arguments) {
    switch (type) {
        case SEM_UP:
            {
                NS_LOG_INFO("semaphore_up");
                auto it = m_semaphores.find(id);
                if (it == m_semaphores.end()) {
                    NS_LOG_ERROR("Unable to find semaphore named" << id);
                    NS_ASSERT(0);
                }

                Ptr<Semaphore> semaphore = it->second;

                if (semaphore->Up()) {
                    /* Someone was unblocked, which means that we're passing
                     * the condition to the new process.
                     * The new process won't call down themselves, so we must
                     * do it ourself.
                     */
                    const int unblocked = semaphore->Unblock();
                    NS_LOG_INFO("Unblocked " << unblocked << " on semaphore " << id);
                    m_runqueue.push_back(unblocked);

                    WakeupIdle();

                    // We are in big trouble if we weren't supposed to unblocked on this semaphore after all.
                    NS_ASSERT_MSG(semaphore->Down(), "Unblocked on a semaphore that is not available!");
                }
            }
            break;
        case SEM_DOWN:
            {
                NS_LOG_INFO("semaphore_down");
                auto it = m_semaphores.find(id);
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
                auto c = (Completion*)var;

                /* When we arrive here, the completion may be either completed
                 * or not. If it is completed, just keep running.
                 */
                if (!c->IsCompleted()) {
                    c->SetWaiter(m_currentRunning[cpu]);

                    RescheduleCPU(cpu);
                }

                return 0;
            }
            break;
        case COMPL:
            {
                auto c = (Completion*)var;
                int pid = c->Complete();
                if (pid >= 0) {
                    // TODO: Maybe check if the pid isn't already in the runqueue?
                    m_runqueue.push_back(pid);

                    WakeupIdle();
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
    if(name == "SEMUP")
        return SEM_UP;
    else if(name == "SEMDOWN")
        return SEM_DOWN;
    else if(name == "WAITCOMPL")
        return WAIT_COMPL;
    else if(name == "COMPL")
        return COMPL;

    NS_ASSERT(0); // Not implemented
    return 0;
}

}
