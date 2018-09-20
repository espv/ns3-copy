
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/processing-module.h"
#include "ns3/data-rate.h"

#include <fstream>
#include <iostream>
#include "ns3/gnuplot.h"
#include <string.h>
#include <time.h>
#include <ctime>

#include "ns3/internet-module.h"
#include "ns3/cc2420-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

#include <sstream>

#include "trex.h"

#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()


using namespace ns3;

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE("TRex");
    // For debug
    extern bool debugOn;
}

static ProgramLocation *dummyProgramLoc;

// ScheduleInterruptTRex schedules an interrupt on the node.
// interruptId is the service name of the interrupt, such as HIRQ-123
void ScheduleInterruptTRex(Ptr<Node> node, Ptr<Packet> packet, const char* interruptId, Time time) {
  Ptr<ExecEnv> ee = node->GetObject<ExecEnv>();

  // TODO: Model the interrupt distribution somehow
  static int cpu = 0;

  dummyProgramLoc = new ProgramLocation();
  dummyProgramLoc->tempvar = tempVar();
  dummyProgramLoc->curPkt = packet;
  dummyProgramLoc->localStateVariables = std::map<std::string, Ptr<StateVariable> >();
  dummyProgramLoc->localStateVariableQueue2s = std::map<std::string, Ptr<StateVariableQueue2> >();

  Simulator::Schedule(time,
                      &InterruptController::IssueInterruptWithServiceOnCPU,
                      ee->hwModel->m_interruptController,
                      cpu,
                      ee->m_serviceMap[interruptId],
                      dummyProgramLoc);

}

TRex::TRex(Ptr<Node> node, Address src, Ptr<CC2420InterfaceNetDevice> netDevice, TRexProtocolStack *ps) : TRexMote() {
  TRex::node = node;
  TRex::number_forwarded_and_acked = 0;
  TRex::packets_in_send_queue = 0;
  TRex::receivingPacket = false;
  TRex::src = src;
  TRex::netDevice = netDevice;
  TRex::ps = ps;
}

TRex::TRex(Ptr<Node> node, Address src, Address dst, Ptr<CC2420InterfaceNetDevice> netDevice, TRexProtocolStack *ps) : TRexMote() {
  TRex::node = node;
  TRex::number_forwarded_and_acked = 0;
  TRex::packets_in_send_queue = 0;
  TRex::receivingPacket = false;
  TRex::src = src;
  TRex::dst = dst;
  TRex::netDevice = netDevice;
  TRex::ps = ps;
}

TRex::TRex(Ptr<Node> node, TRexProtocolStack *ps) : TRexMote() {
  TRex::node = node;
  TRex::number_forwarded_and_acked = 0;
  TRex::packets_in_send_queue = 0;
  TRex::receivingPacket = false;
  TRex::ps = ps;
}

// Models the radio's behavior before the packets are processed by the microcontroller.
void TRex::ReceiveEvent(Ptr<Packet> packet) {
  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  packet->m_executionInfo.executedByExecEnv = false;

  NS_LOG_INFO (Simulator::Now() << " ReceiveEvent");

  execenv->Proceed(packet, "do_send_event", &TRex::ComplexEventTriggered, this, packet);

  ScheduleInterruptTRex(node, packet, "HIRQ-1", NanoSeconds(10));
  execenv->queues["h1-h2"]->Enqueue(packet);
  receivingPacket = true;
}


// Called when done writing packet into TXFIFO, and radio is ready to send
void TRex::ComplexEventTriggered(Ptr<Packet> packet) {
  packet->m_executionInfo.executedByExecEnv = false;

  NS_LOG_INFO (Simulator::Now() << " ComplexEventTriggered");

  Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize ()+36 + 5) + MicroSeconds (192), &TRex::finishedTransmitting, this, packet);
  ++radio.nr_send_recv;
}

// Radio is finished transmitting packet, and packet can now be removed from the send queue as there is no reason to ever re-transmit it.
// If acks are enabled, the ack has to be received before that can be done.
void TRex::finishedTransmitting(Ptr<Packet> packet) {
  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  ++ps->nr_packets_forwarded;

  // I believe it's here that the packet gets removed from the send queue, but it might be in sendDoneTask
  ip_radioBusy = false;
  packet->m_executionInfo.timestamps.push_back(Simulator::Now());
  if (ns3::debugOn)
    NS_LOG_INFO (Simulator::Now() << " " << id << ": finishedTransmitting: DELTA: " << packet->m_executionInfo.timestamps[3] - packet->m_executionInfo.timestamps[0] << ", UDP payload size: " << packet->GetSize () << ", seq no: " << packet->m_executionInfo.seqNr);
  --packets_in_send_queue;
  --radio.nr_send_recv;
  if (--cur_nr_packets_processing == 0) {
    ScheduleInterruptTRex (node, packet, "HIRQ-12", Seconds(0));
  }

  if (radio.collision) {
    if (ns3::debugOn)
      NS_LOG_INFO (Simulator::Now() << " finishedTransmitting: Collision occured, destroying packet to be forwarded, radio.nr_send_recv: " << radio.nr_send_recv << ", receivingPacket: " << receivingPacket);
    if (radio.nr_send_recv == 0) {
      radio.collision = false;
    }
  }

  // In the jitter experiment, we send the same packet three times.
  if (jitterExperiment) {
    packet->attemptedSent = false;
    packet->m_executionInfo.timestamps.pop_back ();
    packet->m_executionInfo.timestamps.pop_back ();
  }

  execenv->queues["rcvd-send"]->Enqueue(packet);
  //ScheduleInterruptTRex(node, packet, "HIRQ-6", NanoSeconds(0));
}

void TRex::SendPacket(Ptr<Packet> packet, TRex *to_mote, TRex *third_mote) {
  if (ns3::debugOn)
    NS_LOG_INFO (Simulator::Now() << " " << id << ": SendPacket " << packet->m_executionInfo.seqNr);

  Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize ()+36 + 5) + MicroSeconds (192), &TRex::ReceiveEvent, to_mote, packet);
}

bool TRex::HandleRead (Ptr<CC2420Message> msg)
{
  //NS_LOG_INFO ("Received message from CC2420InterfaceNetDevice");

  // What does it mean that it has not been received correctly? Bad CRC?
  if(msg==NULL){
    NS_LOG_INFO ("Message not correctly received!");
    return false;
  }


  Ptr<CC2420Recv> recvMsg = DynamicCast<CC2420Recv>(msg);
  if(recvMsg){
    //NS_LOG_INFO ("THIS is the place where the device model gets involved and forwards the packet to mote 3");
    //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
    //        << "s mote " << GetId() << " received " << recvMsg->getSize()
    //        << " bytes with CRC=" << (recvMsg->getCRC()?"true":"false")
    //        << " and RSSI=" << recvMsg->getRSSI() << " bytes");

    Ptr<Packet> packet = Create<Packet>(ps->packet_size);
    ps->nr_packets_total++;
    packet->m_executionInfo.timestamps.push_back (Simulator::Now());
    packet->src = src;
    packet->dst = dst;
    packet->m_executionInfo.seqNr = seqNr++;
    ReceiveEvent (packet);
    return true;

  } else {
    Ptr<CC2420Cca> ccaMsg = DynamicCast<CC2420Cca>(msg);
    if(ccaMsg){
      //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
      //        << "s mote " << GetId() << " received CC2420Cca message with channel free = "
      //        << (ccaMsg->getCcaValue()?"true":"false"));
      return true;

    } else {
      Ptr<CC2420Sending> sendingMsg = DynamicCast<CC2420Sending>(msg);
      if(sendingMsg){
        //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
        //        << "s mote " << GetId() << " received CC2420Sending message with can send = "
        //        << (sendingMsg->getSending()?"true":"false"));
        if (!sendingMsg->getSending ()) {
          // This means we failed to send packet because channel is busy
          //NS_LOG_INFO ("recvMsg->getSize (): " << recvMsg->getSize ());
          Ptr<Packet> packet = Create<Packet>(ps->packet_size);
          packet->attemptedSent = true;
          Simulator::Schedule(Seconds(0.0025), &TRex::ComplexEventTriggered, this, packet);
        }
        return true;

      } else {
        Ptr<CC2420SendFinished> sfMsg = DynamicCast<CC2420SendFinished>(msg);
        if(sfMsg){
          //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
          //        << "s mote " << GetId() << " received CC2420SendFinished message");

          finishedTransmitting (Create<Packet>(ps->packet_size));
          return true;

        } else {
          Ptr<CC2420StatusResp> respMsg = DynamicCast<CC2420StatusResp>(msg);
          if(respMsg){
            /*NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                    << "s mote " << GetId() << " received CC2420StatusResp message with values"
                    << " CCA mode=" << (int) respMsg->getCcaMode()
                    << ", CCA hysteresis=" << (int) respMsg->getCcaHysteresis()
                    << ", CCA threshold=" << (int) respMsg->getCcaThreshold()
                    << ", long TX turnaround=" << (respMsg->getTxTurnaround()?"true":"false")
                    << ", automatic CRC=" << (respMsg->getAutoCrc()?"true":"false")
                    << ", preamble length=" << (int) respMsg->getPreambleLength()
                    << ", sync word=0x" << std::hex << (int) respMsg->getSyncWord() << std::dec
                    << ", channel=" << (int) respMsg->getChannel()
                    << ", power=" << (int) respMsg->getPower());*/
            return true;
          } else {
            //unknown message or NULL-Pointer
            NS_LOG_INFO ("CC2420Message is of an unknown type!");
            return false;
          } //unknown
        } //status response
      } // send finished
    } // sending
  } // receive

  //return false; // something went wrong
}

// GeneratePacket creates a packet and passes it on to the NIC
void TRexProtocolStack::GeneratePacket(uint32_t pktSize, uint32_t curSeqNr, TRex *m1, TRex *m2, TRex *m3) {
  Ptr<Packet> toSend = Create<Packet>(pktSize);
  toSend->m_executionInfo.seqNr = curSeqNr;
  toSend->m_executionInfo.executedByExecEnv = false;

  if (ns3::debugOn)
    NS_LOG_INFO ("Generating packet " << curSeqNr);

  //m1->SendPacket(toSend, m2, m3);
}

// GenerateTraffic schedules the generation of packets according to the duration
// of the experinment and the specified (static) rate.
void TRexProtocolStack::GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TRex *m1, TRex *m2, TRex *m3) {
  static int curSeqNr = 0;

  GeneratePacket(pktSize, curSeqNr++, m1, m2, m3);
  if (Simulator::Now().GetSeconds() + (1.0 / (double) pps) < duration - 0.02)
    Simulator::Schedule(Seconds(1.0 / (double) pps) + MicroSeconds(rand() % 100),
                        &TRexProtocolStack::GenerateTraffic, this, n, pktSize, m1, m2, m3);
}


// GenerateTraffic schedules the generation of packets according to the duration
// of the experiment and the specified (static) rate.
void TRexProtocolStack::GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TRex *m1, TRex *m2, TRex *m3) {
  Simulator::Schedule(time, &TRexProtocolStack::GenerateTraffic, this, n, pktSize, m1, m2, m3);
}
