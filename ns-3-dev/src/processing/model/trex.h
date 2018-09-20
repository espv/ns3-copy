
#ifndef TREX_MODEL_H
#define TREX_MODEL_H

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

using namespace ns3;

class TRex;

class TRexProtocolStack {
public:
    void GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TRex *m1, TRex *m2, TRex *m3);
    void GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TRex *m1, TRex *m2, TRex *m3);
    void GeneratePacket(uint32_t pktSize, uint32_t curSeqNr, TRex *m1, TRex *m2, TRex *m3);

    uint32_t seed = 3;
    double duration = 10;
    int pps = 1;
    int packet_size = 125;
    std::string deviceFile = "device-files/trex-six-cores.device";
    std::string trace_fn = "trace-inputs/packets-received.txt";
    std::string kbps = "65kbps";

    int nr_packets_collision_missed = 0;
    int nr_rxfifo_flushes = 0;
    int nr_packets_dropped_bad_crc = 0;
    int nr_packets_forwarded = 0;
    int nr_packets_dropped_ip_layer;
    int total_intra_os_delay = 0;
    int nr_packets_total = 0;
    bool firstNodeSendingtal = false;
    std::vector<int> forwarded_packets_seqnos;
    std::vector<int> time_received_packets;
    std::vector<int> all_intra_os_delays;
};

class TRexCC2420 {
public:
    bool rxfifo_overflow = false;
    int bytes_in_rxfifo = 0;
    DataRate datarate;
    int nr_send_recv = 0;
    bool collision = false;

    TRexCC2420() {
      datarate = DataRate("250kbps");
    }
};

class TRexMote {
protected:
    int id;
    Ptr<Node> node;

    TRexMote() {
      static int cnt;
      id = cnt++;
    }

public:
    Ptr<Node> GetNode() {
      return node;
    }
};

class TRex : public TRexMote {
private:
    // Components of the mote
    TRexCC2420 radio;
    int number_forwarded_and_acked = 0;
    int packets_in_send_queue = 0;
    bool receivingPacket = false;
    std::vector<Ptr<Packet> > receive_queue;
    TRexProtocolStack *ps;

    Address src;
    Address dst;
    Ptr<CC2420InterfaceNetDevice> netDevice;

public:
    bool ip_radioBusy = false;
    int cur_nr_packets_processing = 0;
    bool jitterExperiment = false;
    bool ccaOn = true;
    bool fakeSending = false;
    int number_forwarded = 0;
    bool use_device_model = true;
    int seqNr = 0;

    TRex(Ptr<Node> node, Address src, Ptr<CC2420InterfaceNetDevice> netDevice, TRexProtocolStack *ps);

    TRex(Ptr<Node> node, Address src, Address dst, Ptr<CC2420InterfaceNetDevice> netDevice, TRexProtocolStack *ps);

    TRex(Ptr<Node> node, TRexProtocolStack *ps);

    // Models the radio's behavior before the packets are processed by the microcontroller.
    void ReceiveEvent(Ptr<Packet> packet);

    // Called when done writing packet into TXFIFO, and radio is ready to send
    void ComplexEventTriggered(Ptr<Packet> packet);

    // Radio is finished transmitting packet, and packet can now be removed from the send queue as there is no reason to ever re-transmit it.
    // If acks are enabled, the ack has to be received before that can be done.
    void finishedTransmitting(Ptr<Packet> packet);

    void SendPacket(Ptr<Packet> packet, TRex *to_mote, TRex *third_mote);

    bool HandleRead (Ptr<CC2420Message> msg);
};

#endif //TELOSB_CSW_MODEL_TELOSB_H
