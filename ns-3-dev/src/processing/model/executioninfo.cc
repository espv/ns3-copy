//
// Created by espen on 21.05.19.
//
#include "ns3/executioninfo.h"

using namespace ns3;


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
