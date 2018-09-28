
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/processing-module.h"
#include "ns3/data-rate.h"

#include <fstream>
#include <iostream>
#include <string.h>
#include <time.h>
#include <ctime>
#include <random>

#include "ns3/gnuplot.h"
#include "ns3/internet-module.h"
#include "ns3/cc2420-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

#include <sstream>

#include "telosb.h"

#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()


using namespace ns3;

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE("TelosB");
}

// TODO: Replace some of the ad-hoc variables with proper queues and STATECOND variables that can be used by the execenv
// TODO: Get the mote to drop packets at 65kbps with packet size 125 bytes, which worked before the recent changes
// TODO: Tidy up telosb-example.cc

void
TelosB::Configure(Ptr<Node> node, ProtocolStack *ps, Ptr<CC2420InterfaceNetDevice> netDevice) {
    node->GetObject<ExecEnv>()->cpuScheduler->allowNestedInterrupts = true;
    this->node = node;
    this->number_forwarded_and_acked = 0;
    this->TelosB::receivingPacket = false;
    this->netDevice = netDevice;
    this->ps = ps;
}

// Models the radio's behavior before the packets are processed by the microcontroller.
void TelosB::ReceivePacket(Ptr<Packet> packet) {
  ps->firstNodeSending = false;
  --radio.nr_send_recv;
  packet->m_executionInfo.timestamps.push_back(Simulator::Now());
  packet->collided = radio.collision;
  if (radio.collision && radio.nr_send_recv == 0)
    radio.collision = false;

  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  packet->m_executionInfo.executedByExecEnv = false;

  if (radio.rxfifo_overflow) {
    NS_LOG_INFO ("Dropping packet " << packet->m_executionInfo.seqNr << " due to RXFIFO overflow");
    return;
  }

  radio.bytes_in_rxfifo += packet->GetSize ();
  NS_LOG_INFO ("radio.bytes_in_rxfifo: " << radio.bytes_in_rxfifo << ", packet size: " << packet->GetSize());
  if (radio.bytes_in_rxfifo > 128) {
    radio.bytes_in_rxfifo -= packet->GetSize ();
    NS_LOG_INFO (id << " RXFIFO overflow");
    packet->collided = true;
    // RemoveAtEnd removes the number of bytes from the received packet that were not received due to overflow.
    packet->RemoveAtEnd(radio.bytes_in_rxfifo - 128);
    radio.bytes_in_rxfifo = 128;
    radio.rxfifo_overflow = true;
  }

  execenv->Proceed(packet, "readdonepayload", &TelosB::readDone_payload, this, packet);
  execenv->queues["rxfifo"]->Enqueue(packet);
  if (receivingPacket) {
    NS_LOG_INFO ("Delaying writing the packet into RAM; length of receive_queue: " << execenv->queues["rxfifo"]->GetNPackets());
    return;
  }

  NS_LOG_INFO (Simulator::Now() << " " << id << ": CC2420ReceivePacket, packet nr " << packet->m_executionInfo.seqNr);

  execenv->ScheduleInterrupt (packet, "HIRQ-1", NanoSeconds(10));
  receivingPacket = true;
}

void TelosB::readDone_payload(Ptr<Packet> packet) {
  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  packet->m_executionInfo.executedByExecEnv = false;

  radio.bytes_in_rxfifo -= packet->GetSize ();
  if (radio.rxfifo_overflow && radio.bytes_in_rxfifo <= 0) {
    NS_LOG_INFO ("RXFIFO gets flushed");
    radio.rxfifo_overflow = false;
    radio.bytes_in_rxfifo = 0;
    ps->nr_rxfifo_flushes++;
  }

  // Packets received and causing RXFIFO overflow get dropped.
  if (packet->collided) {
    execenv->globalStateVariables["packet-collided"] = 1;
    ps->nr_packets_dropped_bad_crc++;
    NS_LOG_INFO (Simulator::Now() << " " << id << ": collision caused packet nr " << packet->m_executionInfo.seqNr << "'s CRC check to fail, dropping it");
    if (!execenv->queues["rxfifo"]->IsEmpty()) {
      execenv->queues["rxfifo"]->Dequeue();
    } else {
      receivingPacket = false;
      if (radio.rxfifo_overflow && radio.bytes_in_rxfifo > 0) {
        NS_LOG_INFO ("RXFIFO gets flushed");
        radio.rxfifo_overflow = false;
        radio.bytes_in_rxfifo = 0;
        ps->nr_rxfifo_flushes++;
      }
    }
  } else {
    execenv->globalStateVariables["packet-collided"] = 0;
    NS_LOG_INFO ("readDone_payload seqno: " << packet->m_executionInfo.seqNr);
    execenv->Proceed(packet, "receivedone", &TelosB::receiveDone_task, this, packet);
  }

  NS_LOG_INFO (Simulator::Now() << " " << id << ": readDone_payload " << packet->m_executionInfo.seqNr << ", receivingPacket: " << receivingPacket << ", packet collided: " << packet->collided);
}

void TelosB::receiveDone_task(Ptr<Packet> packet) {
  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  packet->m_executionInfo.executedByExecEnv = false;
  NS_LOG_INFO ("Packets in send queue: " << execenv->queues["ipaq"]->GetNPackets());

  if (jitterExperiment) {
    /* In the jitter experiment, we fill the IP layer queue up by enqueueing the same packet three times instead of once.
     * That means we must increase the number of packets getting processed, which depends on how many packets are currently in the send queue.
     */
    execenv->queues["ipaq"]->Enqueue(packet);execenv->queues["ipaq"]->Enqueue(packet);execenv->queues["ipaq"]->Enqueue(packet);
    execenv->queues["rcvd-send"]->Enqueue(packet);execenv->queues["rcvd-send"]->Enqueue(packet);execenv->queues["rcvd-send"]->Enqueue(packet);
    execenv->globalStateVariables["ipaq-full"] = 0;
    execenv->Proceed(packet, "sendtask", &TelosB::sendTask, this, packet);
    NS_LOG_INFO (Simulator::Now() << " " << id << ": receiveDone " << packet->m_executionInfo.seqNr);
  } else if (execenv->queues["ipaq"]->GetNPackets() < 3) {
    execenv->queues["ipaq"]->Enqueue(packet);
    execenv->globalStateVariables["ipaq-full"] = 0;
    execenv->Proceed(packet, "sendtask", &TelosB::sendTask, this, packet);
    NS_LOG_INFO (Simulator::Now() << " " << id << ": receiveDone " << packet->m_executionInfo.seqNr);
  } else {
    ++ps->nr_packets_dropped_ip_layer;
    execenv->globalStateVariables["ipaq-full"] = 1;
    NS_LOG_INFO (Simulator::Now() << " " << id << ": receiveDone_task, queue full, dropping packet " << packet->m_executionInfo.seqNr);
  }

  if (execenv->queues["rxfifo"]->IsEmpty()) {
    receivingPacket = false;
    if (radio.rxfifo_overflow && radio.bytes_in_rxfifo > 0) {
      NS_LOG_INFO ("RXFIFO gets flushed");
      radio.rxfifo_overflow = false;
      radio.bytes_in_rxfifo = 0;
      ps->nr_rxfifo_flushes++;
    }
  }
}

void TelosB::sendTask(Ptr<Packet> packet) {
  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  packet->m_executionInfo.executedByExecEnv = false;
  execenv->Proceed(packet, "writtentotxfifo", &TelosB::writtenToTxFifo, this, packet);
  execenv->globalStateVariables["ip-radio-busy"] = 1;

  NS_LOG_INFO (Simulator::Now() << " " << id << ": sendTask " << packet->m_executionInfo.seqNr);
}

void TelosB::sendViaCC2420(Ptr<Packet> packet) {
  uint8_t nullBuffer[packet->GetSize()];
  wmemset((wchar_t*)nullBuffer, 0, sizeof(uint8_t)*packet->GetSize());

  // send with CCA
  Ptr<CC2420Send> msg = CreateObject<CC2420Send>(nullBuffer, packet->GetSize(), true);

  netDevice->descendingSignal(msg);
}

// Called when done writing packet into TXFIFO, and radio is ready to send
void TelosB::writtenToTxFifo(Ptr<Packet> packet) {
  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  packet->m_executionInfo.executedByExecEnv = false;

  if (!packet->attemptedSent) {
    packet->attemptedSent = true;
    packet->m_executionInfo.timestamps.push_back(Simulator::Now());
    int64_t intra_os_delay = packet->m_executionInfo.timestamps[2].GetMicroSeconds() - packet->m_executionInfo.timestamps[1].GetMicroSeconds();
    ps->time_received_packets.push_back (packet->m_executionInfo.timestamps[1].GetMicroSeconds());
    ps->forwarded_packets_seqnos.push_back (packet->m_executionInfo.seqNr);
    ps->all_intra_os_delays.push_back(intra_os_delay);
    ps->total_intra_os_delay += intra_os_delay;
    NS_LOG_INFO (Simulator::Now() << " " << id << ": writtenToTxFifo " << packet->m_executionInfo.seqNr);
    NS_LOG_INFO (id << " writtenToTxFifo: DELTA: " << intra_os_delay << ", UDP payload size (36+payload bytes): " << packet->GetSize () << ", seq no " << packet->m_executionInfo.seqNr);
    NS_LOG_INFO (Simulator::Now() << " " << id << ": writtenToTxFifo, number forwarded: " << ++number_forwarded_and_acked << ", seq no " << packet->m_executionInfo.seqNr);
  }

  // TODO: Use only the CC2420 model to transmit packets
  // DO NOT SEND
  if (fakeSending) {
    ++radio.nr_send_recv;
    Simulator::Schedule(Seconds(0), &TelosB::finishedTransmitting, this, packet);
    return;
  }

  if (radio.nr_send_recv > 0) {
    if (ccaOn) {  // 2500 comes from traces
      Simulator::Schedule(MicroSeconds(2400 + rand() % 200), &TelosB::writtenToTxFifo, this, packet);
      return;
    }
    radio.collision = true;
    NS_LOG_INFO ("Forwarding packet " << packet->m_executionInfo.seqNr << " causes collision");
  }

  Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize () + 5) + MicroSeconds (192), &TelosB::finishedTransmitting, this, packet);
  ++radio.nr_send_recv;
}

// Radio is finished transmitting packet, and packet can now be removed from the send queue as there is no reason to ever re-transmit it.
// If acks are enabled, the ack has to be received before that can be done.
void TelosB::finishedTransmitting(Ptr<Packet> packet) {
  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  ++ps->nr_packets_forwarded;

  execenv->globalStateVariables["ip-radio-busy"] = 0;
  packet->m_executionInfo.timestamps.push_back(Simulator::Now());
  NS_LOG_INFO (Simulator::Now() << " " << id << ": finishedTransmitting: DELTA: " << packet->m_executionInfo.timestamps[3] - packet->m_executionInfo.timestamps[0] << ", UDP payload size: " << packet->GetSize () << ", seq no: " << packet->m_executionInfo.seqNr);
  execenv->queues["ipaq"]->Dequeue();
  --radio.nr_send_recv;

  if (radio.collision) {
    NS_LOG_INFO (Simulator::Now() << " Collision occured, destroying packet to be forwarded, radio.nr_send_recv: " << radio.nr_send_recv << ", receivingPacket: " << receivingPacket);
    if (radio.nr_send_recv == 0)
      radio.collision = false;
  }

  // In the jitter experiment, we send the same packet three times.
  if (jitterExperiment) {
    packet->attemptedSent = false;
    packet->m_executionInfo.timestamps.pop_back ();
    packet->m_executionInfo.timestamps.pop_back ();
  }

  // Re-scheduling sendTask in case there is a packet waiting to be sent
  execenv->ScheduleInterrupt (packet, "HIRQ-6", NanoSeconds(0));
}

void TelosB::SendPacket(Ptr<Packet> packet, TelosB *to_mote, TelosB *third_mote) {
  Ptr<ExecEnv> execenv = node->GetObject<ExecEnv>();
  NS_LOG_INFO (Simulator::Now() << " " << id << ": SendPacket " << packet->m_executionInfo.seqNr);

  // Finish this, also change ReceivePacket to also accept acks
  if (!to_mote->radio.rxfifo_overflow) {
    if (ps->firstNodeSending) {
      Simulator::Schedule(MicroSeconds(100), &TelosB::SendPacket, this, packet, to_mote, third_mote);
      return;
    }
    if (to_mote->radio.nr_send_recv > 0) {
        if (ccaOn && to_mote->radio.nr_send_recv > 0) {
            NS_LOG_INFO ("CCA, delaying sending packet");
            Simulator::Schedule(MicroSeconds(2400 + rand() % 200), &TelosB::SendPacket, this, packet, to_mote,
                                third_mote);
            return;
        }
        ++ps->nr_packets_collision_missed;
        to_mote->radio.collision = packet->collided = true;
    }

    ps->firstNodeSending = true;
    ++ps->nr_packets_total;
    ++to_mote->radio.nr_send_recv;
    packet->m_executionInfo.timestamps.push_back(Simulator::Now());
    Simulator::Schedule(radio.datarate.CalculateBytesTxTime(packet->GetSize () + 5/* 5 is preamble + SFD */) + MicroSeconds (192) /* 12 symbol lengths before sending packet */, &TelosB::ReceivePacket, to_mote, packet);
    NS_LOG_INFO ("SendPacket, sending packet " << packet->m_executionInfo.seqNr);
  } else {
    NS_LOG_INFO ("SendPacket, failed to send because of RXFIFO overflow");
  }
}

bool TelosB::HandleRead (Ptr<CC2420Message> msg)
{
  //NS_LOG_INFO ("Received message from CC2420InterfaceNetDevice");

  // What does it mean that it has not been received correctly? Bad CRC?
  if(msg==nullptr){
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
    packet->m_executionInfo.seqNr = seqNr++;
    if (use_device_model)
      ReceivePacket (packet);
    else
      sendViaCC2420 (packet);
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
          Simulator::Schedule(Seconds(0.0025), &TelosB::writtenToTxFifo, this, packet);
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
void ProtocolStack::GeneratePacket(uint32_t pktSize, uint32_t curSeqNr, TelosB *m1, TelosB *m2, TelosB *m3) {
  Ptr<Packet> toSend = Create<Packet>(pktSize);
  toSend->m_executionInfo.seqNr = curSeqNr;
  toSend->m_executionInfo.executedByExecEnv = false;

  NS_LOG_INFO ("Generating packet " << curSeqNr);

  m1->SendPacket(toSend, m2, m3);
}

// GenerateTraffic schedules the generation of packets according to the duration
// of the experinment and the specified (static) rate.
void ProtocolStack::GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TelosB *m1, TelosB *m2, TelosB *m3) {
  static uint32_t curSeqNr = 0;

  static std::random_device r;

  // Choose a random mean between 1 and 6
  std::default_random_engine e1(r());
  std::uniform_int_distribution<uint64_t> uniform_dist(0, 99);

  GeneratePacket(pktSize, curSeqNr++, m1, m2, m3);
  if (Simulator::Now().GetSeconds() + (1.0 / (double) pps) < duration - 0.02)
    Simulator::Schedule(Seconds(1.0 / (double) pps) + MicroSeconds(uniform_dist(e1)),
                        &ProtocolStack::GenerateTraffic, this, n, pktSize, m1, m2, m3);
}


// GenerateTraffic schedules the generation of packets according to the duration
// of the experiment and the specified (static) rate.
void ProtocolStack::GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TelosB *m1, TelosB *m2, TelosB *m3) {
  Simulator::Schedule(time, &ProtocolStack::GenerateTraffic, this, n, pktSize, m1, m2, m3);
}
