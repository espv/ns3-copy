
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

#include "software-execution-model.h"
#include "trex.h"
#include "execenv.h"

enum FSM {
    PACKETTHREADRECVPACKET,
    HANDLETHENCEPOP,
    HANDLECEPOP,
    CHECKCONSTRAINTS,
    ASSIGNATTRIBUTES,
    SENDPACKET,
    FINISHEDPROCESSING,
};

using namespace ns3;

class TRex;

class TRexProtocolStack : public SoftwareExecutionModel {
public:
    /*
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);

    TRexProtocolStack();

    uint32_t seed = 3;
    double duration = 10;
    int pps = 1;
    int packet_size = 125;
    std::string trace_fn = "trace-inputs/packets-received.txt";
    std::string kbps = "65kbps";

    int nr_packets_collision_missed = 0;
    int nr_rxfifo_flushes = 0;
    int nr_packets_dropped_bad_crc = 0;
    int nr_packets_forwarded = 0;
    int nr_packets_dropped_ip_layer{};
    int total_intra_os_delay = 0;
    int nr_packets_total = 0;
    bool firstNodeSendingtal = false;
    std::vector<int> forwarded_packets_seqnos;
    std::vector<int> time_received_packets;
    std::vector<int> all_intra_os_delays;

    void FsmTriggerCallback(Ptr<ExecEnv> ee, std::string fsm);
};

#endif //TREX_CSE_MODEL_H
