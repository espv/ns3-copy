//
// Created by espen on 29.09.18.
//

#ifndef NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_EXECUTIONINFO_H
#define NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_EXECUTIONINFO_H

#include "ns3/event-impl.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include <vector>
#include <map>

namespace ns3 {
    class Thread;
    class SEM;

    class ExecutionInfo : public SimpleRefCount<ExecutionInfo> {
    public:
        ExecutionInfo();

        explicit ExecutionInfo(ExecutionInfo *ei);

        /* Used to identify whether a service is allready
         * called by the ExecEnv. If this is the same as
         * the event executed by the scheduler, return false.
         */
        bool executedByExecEnv;
        Ptr<Packet> packet;

        // Name and arguments for target service
        std::string target;
        //EventImpl *targetFPM;
        std::map<std::string, std::vector<EventImpl*> > targets;

        // Used for temporary synchronization primitives
        //void *tempSynch;

        /* Used to indicate which service to execute when
         * the packet is within a service queue
         */
        //SEM *queuedService;

        // EXPERIMENTATION:
        std::vector <Time> timestamps;
        int seqNr;

        void ExecuteTrigger(std::string &checkpoint);

        /**
          * \brief Returns the the size in bytes of the packet (including the zero-filled
          * initial payload).
          *
          * \returns the size in bytes of the packet
          */
        inline uint32_t GetSize () const;
    };
}

#endif //NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_EXECUTIONINFO_H
