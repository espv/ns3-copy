//
// Created by espen on 29.09.18.
//

#ifndef NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_EXECUTIONINFO_H
#define NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_EXECUTIONINFO_H

#include <ns3/queue-item.h>
#include "ns3/event-impl.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"

#include "ns3/cep-engine.h"

#include <vector>
#include <map>

namespace ns3 {

    class EventWrapper : public SimpleRefCount<EventWrapper> {
    public:
        explicit EventWrapper(EventImpl *event) {
          this->event = event;
        }

        EventImpl *event;
    };

    class UserDefinedVariables {

    };

    class DcepSimUserDefinedVariables : public UserDefinedVariables {
        std::map<std::string,Ptr<CepEvent>> cepEvents;
        std::map<std::string,Ptr<CepOperator>> cepOperators;
    };

    class ExecutionVariables {
    public:
        Ptr<Packet> packet;
        UserDefinedVariables *userDefinedVariables;
    };

    class DcepSimExecutionVariables : public ExecutionVariables {
    public:
        DcepSimExecutionVariables() {
            userDefinedVariables = new DcepSimUserDefinedVariables();
        }

        Ptr<CepEvent> curCepEvent;

        std::vector<Ptr<CepOperator>> cepOperatorProcessCepEvent_ops;
        Ptr<CEPEngine> cepOperatorProcessCepEvent_cep;
        Ptr<Producer> cepOperatorProcessCepEvent_producer;
    };

    class ExecutionInfo : public SimpleRefCount<ExecutionInfo> {
    public:
        ExecutionInfo();

        explicit ExecutionInfo(Ptr<ExecutionInfo> ei);

        /* Used to identify whether a service is allready
         * called by the ExecEnv. If this is the same as
         * the event executed by the scheduler, return false.
         */
        bool executedByExecEnv;
        Ptr<Packet> packet;
        Ptr<CepEvent> curCepEvent;

        // Name and arguments for target service
        std::string target;
        std::map<std::string, Ptr<EventWrapper> > targets;

        std::map<std::string, ExecutionVariables *> executionVariables;

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
