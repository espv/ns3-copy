//
// Created by espen on 21.05.19.
//
#include "ns3/packet.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/log.h"

#include "execenv.h"


namespace ns3 {

    NS_OBJECT_TEMPLATE_CLASS_DEFINE(Queue, ExecutionInfo);
    NS_OBJECT_TEMPLATE_CLASS_DEFINE(DropTailQueue, ExecutionInfo);

    ns3::ExecutionInfo::ExecutionInfo() = default;

    ns3::ExecutionInfo::ExecutionInfo(Ptr<ExecutionInfo> ei) {
        this->packet = ei->packet;
        this->executedByExecEnv = ei->executedByExecEnv;
        this->targets = ei->targets;
        this->timestamps = ei->timestamps;
        this->seqNr = ei->seqNr;
    }

    void ExecutionInfo::ExecuteTrigger(std::string &checkpoint) {
        if (!checkpoint.empty()) {
          //std::cout << checkpoint << std::endl;
        }
        auto it = targets.find(checkpoint);
        if (it != targets.end() && !it->second.empty()) {
            executedByExecEnv = true;
            auto toInvoke = it->second.front();
            toInvoke->event->Invoke();
            it->second.erase(it->second.begin());
        }
    }

    uint32_t ns3::ExecutionInfo::GetSize() const {
        return packet->GetSize();
    }

}