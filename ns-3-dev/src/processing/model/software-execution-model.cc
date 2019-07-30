//
// Created by espen on 30.07.2019.
//

#include "software-execution-model.h"
#include "execenv.h"
#include "ns3/ptr.h"

using namespace ns3;

namespace ns3 {

TypeId
SoftwareExecutionModel::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::SoftwareExecutionModel")
            .SetParent<Object>();
    return tid;
}

void SoftwareExecutionModel::FsmTriggerCallback(Ptr<ExecEnv> ee, std::string fsm) {}

}
