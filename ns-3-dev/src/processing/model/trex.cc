//
// Created by espen on 30.07.2019.
//

#include "trex.h"
#include "execenv.h"
#include "ns3/dcep-module.h"

using namespace ns3;

std::map<std::string, FSM> fsms;

TypeId
TRexProtocolStack::GetTypeId (void)
{
    static TypeId tid = TypeId ("ns3::TRexProtocolStack")
            .SetParent<SoftwareExecutionModel> ()
    ;
    return tid;
}

TRexProtocolStack::TRexProtocolStack() {
    deviceFile = "device-files/trex.device";  // Required if we use gdb

    fsms["handle-then-cepop"] = HANDLETHENCEPOP;
    fsms["check-constraints"] = CHECKCONSTRAINTS;
    fsms["assign-attributes"] = ASSIGNATTRIBUTES;
    fsms["send-packet"] = SENDPACKET;
    fsms["finished-processing"] = FINISHEDPROCESSING;
}

void TRexProtocolStack::FsmTriggerCallback(Ptr<ExecEnv> ee, std::string fsm) {
    auto dcep = GetObject<Dcep>();
    auto enum_fsm = fsms[fsm];
    switch (enum_fsm) {
        case HANDLETHENCEPOP: {
            std::cout << "HANDLETHENCEPOP" << std::endl;
            break;
        } case CHECKCONSTRAINTS: {
            std::cout << "CHECKCONSTRAINTS" << std::endl;
            //dcep->GetObject<CEPEngine>()->CheckConstraints(ee->currentlyExecutingThread->m_currentLocation->curCepEvent);
            //auto c = ee->GetObject<Node>()->GetObject<Dcep>()->GetObject<CEPEngine>();
            //std::cout << c << std::endl;
            break;
        } case ASSIGNATTRIBUTES: {
            std::cout << "ASSIGNATTRIBUTES" << std::endl;
            break;
        } case SENDPACKET: {
            std::cout << "SENDPACKET" << std::endl;
            //dcep->GetObject<Communication>()->send();
            break;
        } case FINISHEDPROCESSING: {
            std::cout << "FINISHEDPROCESSING" << std::endl;
            break;
        } default: {
            break;
        }
    }
}
