
#ifndef TELOSB_CSW_MODEL_TELOSB_H
#define TELOSB_CSW_MODEL_TELOSB_H

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

#include "telosb.h"

using namespace ns3;

class TelosB;

class ProtocolStack : public SoftwareExecutionModel {
public:
    ProtocolStack();

    void GenerateTraffic(Ptr<Node> n, uint32_t pktSize, TelosB *m1, TelosB *m2, TelosB *m3);
    void GenerateTraffic2(Ptr<Node> n, uint32_t pktSize, Time time, TelosB *m1, TelosB *m2, TelosB *m3);
    void GeneratePacket(uint32_t pktSize, uint32_t curSeqNr, TelosB *m1, TelosB *m2, TelosB *m3);

    uint32_t seed = 3;
    double duration = 10;
    int pps = 150;
    uint32_t packet_size = 124;
    std::string trace_fn = "trace-inputs/packets-received.txt";
    std::string kbps = "60kbps";

    int nr_packets_collision_missed = 0;
    int nr_rxfifo_flushes = 0;
    int nr_packets_dropped_bad_crc = 0;
    int nr_packets_forwarded = 0;
    int nr_packets_dropped_ip_layer = 0;
    int total_intra_os_delay = 0;
    int nr_packets_total = 0;
    bool firstNodeSending = false;
    std::vector<int64_t> forwarded_packets_seqnos;
    std::vector<int64_t> time_received_packets;
    std::vector<int64_t> all_intra_os_delays;
};

class CC2420 {
public:
    bool rxfifo_overflow = false;
    uint32_t bytes_in_rxfifo = 0;
    DataRate datarate;
    int nr_send_recv = 0;
    bool collision = false;

    CC2420() {
      datarate = DataRate("250kbps");
    }
};

class Mote {
protected:
    int id;
    Ptr<Node> node;

    Mote() {
      static int cnt = 1;
      id = cnt++;
    }

public:
    Ptr<Node> GetNode() {
      return node;
    }
};

class TelosB : public Mote {
private:
    // Components of the mote
    CC2420 radio;
    int number_forwarded_and_acked = 0;
    int packets_in_send_queue = 0;
    bool receivingPacket = false;
    std::vector<Ptr<Packet> > receive_queue;
    Ptr<ProtocolStack> ps;

    Address *src;
    Address *dst;
    Ptr<CC2420InterfaceNetDevice> netDevice;

public:
    bool channel_is_clear = true;
    Ptr<Packet> packet_waiting_to_send = nullptr;
    Time first_received_packet = Seconds(0);
    Time last_received_packet = Seconds(0);
    int total_packets_received = 0;
    bool ip_radioBusy = false;
    int cur_nr_packets_processing = 0;
    bool jitterExperiment = false;
    bool ccaOn = true;
    bool fakeSending = false;
    int number_forwarded = 0;
    bool use_device_model = true;
    int seqNr = 0;

    TelosB() = default;

    void Configure(Ptr<Node> node, Ptr<ProtocolStack> ps, Ptr<CC2420InterfaceNetDevice> netDevice);
    void Configure(Ptr<Node> node, Ptr<ProtocolStack> ps, Ptr<CC2420InterfaceNetDevice> netDevice, bool use_device_model);

    // Models the radio's behavior before the packets are processed by the microcontroller.
    void ReceivePacket(Ptr<Packet> packet);

    void read_done_length(Ptr<Packet> packet);

    void readDone_fcf(Ptr<Packet> packet);

    void readDone_payload(Ptr<Packet> packet);

    void sendViaCC2420(Ptr<Packet> packet);

    void receiveDone_task(Ptr<Packet> packet);

    void sendTask(Ptr<Packet> packet);

    // Called when done writing packet into TXFIFO, and radio is ready to send
    void writtenToTxFifo(Ptr<Packet> packet);

    /* Radio is finished transmitting packet, and packet can now be removed from the send queue as there is no reason to ever re-transmit it.
     * If acks are enabled, the ack has to be received before that can be done.
     */
    void finishedTransmitting(Ptr<Packet> packet);

    void SendPacket(Ptr<Packet> packet, TelosB *to_mote, TelosB *third_mote);

    bool HandleRead (Ptr<CC2420Message> msg);

    void ReSend(Ptr<Packet> packet);
};

#endif //TELOSB_CSW_MODEL_TELOSB_H
