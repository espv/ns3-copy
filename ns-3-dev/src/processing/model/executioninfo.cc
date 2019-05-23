//
// Created by espen on 21.05.19.
//
#include "ns3/executioninfo.h"
#include "ns3/packet.h"

using namespace ns3;


ExecutionInfo::ExecutionInfo() = default;


ExecutionInfo::ExecutionInfo(ExecutionInfo *ei) {
    this->packet = ei->packet;
    this->curThread = ei->curThread;
    this->executedByExecEnv = ei->executedByExecEnv;
    this->targets = ei->targets;
    this->timestamps = ei->timestamps;
    this->seqNr = ei->seqNr;
}


void ExecutionInfo::ExecuteTrigger(std::string &checkpoint) {
  auto it = targets.find(checkpoint);
  if (it != targets.end() && !it->second.empty()) {
    executedByExecEnv = true;
    auto toInvoke = it->second.front();
    toInvoke->Invoke();
    toInvoke->Unref();
    it->second.erase(it->second.begin());
  }
}
