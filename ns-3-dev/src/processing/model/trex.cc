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

    fsms["packet-thread-recv-packet"] = PACKETTHREADRECVPACKET;
    fsms["handle-then-cepop"] = HANDLETHENCEPOP;
    fsms["handle-cepop"] = HANDLECEPOP;
    fsms["check-constraints"] = CHECKCONSTRAINTS;
    fsms["assign-attributes"] = ASSIGNATTRIBUTES;
    fsms["send-packet"] = SENDPACKET;
    fsms["finished-processing"] = FINISHEDPROCESSING;
}

void TRexProtocolStack::FsmTriggerCallback(Ptr<ExecEnv> ee, std::string fsm) {
    auto dcep = GetObject<Dcep>();
    auto enum_fsm = fsms[fsm];
    switch (enum_fsm) {
        case PACKETTHREADRECVPACKET: {
            auto evs = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.find(
                    "DCEP-Sim");
            if (evs == ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.end()) {
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables["DCEP-Sim"] = new DcepSimExecutionVariables();
                evs = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.find(
                        "DCEP-Sim");
            }

            dcep->GetObject<CEPEngine>()->PacketThreadRecvPacket(
                    ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->curCepEvent);
            break;
        } case HANDLETHENCEPOP: {
            break;
        } case HANDLECEPOP: {
            auto evs = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.find("DCEP-Sim");
            if (evs == ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.end()) {
                ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables["DCEP-Sim"] = new DcepSimExecutionVariables();
                evs = ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->executionVariables.find("DCEP-Sim");
            }
            auto devs = (DcepSimExecutionVariables *)evs->second;

            /*dcep->GetObject<CEPEngine>()->GetObject<Detector>()->CepOperatorProcessCepEvent(
                    ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->curCepEvent,
                    devs->cepOperatorProcessCepEvent_ops,
                    devs->cepOperatorProcessCepEvent_cep,
                    devs->cepOperatorProcessCepEvent_producer);*/

            dcep->GetObject<CEPEngine>()->GetObject<Detector>()->CepQueryComponentProcessCepEvent(
                    ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->curCepEvent,
                    devs->cepOperatorProcessCepEvent_cep);
            break;
        } case CHECKCONSTRAINTS: {
            dcep->GetObject<CEPEngine>()->CheckConstraints(ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->curCepEvent);
            break;
        } case ASSIGNATTRIBUTES: {
            break;
        } case SENDPACKET: {
            dcep->GetObject<Communication>()->send();
            break;
        } case FINISHEDPROCESSING: {
            dcep->GetObject<CEPEngine>()->FinishedProcessingEvent(ee->currentlyExecutingThread->m_currentLocation->m_executionInfo->curCepEvent);
            break;
        } default: {
            break;
        }
    }
}
