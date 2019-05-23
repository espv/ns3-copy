//
// Created by espen on 21.05.19.
//
#include "ns3/executioninfo.h"
#include "ns3/packet.h"


namespace ns3 {

    ns3::ExecutionInfo::ExecutionInfo() = default;


    ns3::ExecutionInfo::ExecutionInfo(ExecutionInfo *ei) {
        this->packet = ei->packet;
        this->executedByExecEnv = ei->executedByExecEnv;
        this->targets = ei->targets;
        this->timestamps = ei->timestamps;
        this->seqNr = ei->seqNr;
    }


    void ns3::ExecutionInfo::ExecuteTrigger(std::string &checkpoint) {
        auto it = targets.find(checkpoint);
        if (it != targets.end() && !it->second.empty()) {
            executedByExecEnv = true;
            auto toInvoke = it->second.front();
            toInvoke->Invoke();
            toInvoke->Unref();
            it->second.erase(it->second.begin());
        }
    }

    uint32_t ns3::ExecutionInfo::GetSize() const {
        return packet->GetSize();
    }

}
