
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
#include "ns3/telosb.h"
#include "ns3/gnuplot.h"

#include <sstream>

#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE("TelosBExample");
    // For debug
    extern bool debugOn;
}

using namespace ns3;

static ProtocolStack ps;

Gnuplot *ppsPlot = nullptr;
Gnuplot *delayPlot = nullptr;
Gnuplot *numberForwardedPlot = nullptr;
Gnuplot *packetOutcomePlot = nullptr;
Gnuplot *numberBadCrcPlot = nullptr;
Gnuplot *numberRxfifoFlushesPlot = nullptr;
Gnuplot *numberCollidedPlot = nullptr;
Gnuplot *numberIPDroppedPlot = nullptr;
Gnuplot *intraOsDelayPlot = nullptr;
Gnuplot2dDataset *ppsDataSet = nullptr;
Gnuplot2dDataset *delayDataSet = nullptr;
Gnuplot2dDataset *numberForwardedDataSet = nullptr;
Gnuplot2dDataset *numberForwardedDataSet2 = nullptr;
Gnuplot2dDataset *numberBadCrcDataSet = nullptr;
Gnuplot2dDataset *numberRxfifoFlushesDataSet = nullptr;
Gnuplot2dDataset *numberCollidedDataSet = nullptr;
Gnuplot2dDataset *numberIPDroppedDataSet = nullptr;
Gnuplot2dDataset *intraOsDelayDataSet = nullptr;

void createPlot(Gnuplot** plot, const std::string &filename, const std::string &title, Gnuplot2dDataset** dataSet) {
  *plot = new Gnuplot(filename);
  (*plot)->SetTitle(title);
  (*plot)->SetTerminal("png");

  *dataSet = new Gnuplot2dDataset();
  (*dataSet)->SetTitle(title);
  (*dataSet)->SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void createPlot2(Gnuplot** plot, const std::string &filename, const std::string &title, Gnuplot2dDataset** dataSet, const std::string &dataSetTitle) {
  *plot = new Gnuplot(filename);
  (*plot)->SetTitle(title);
  (*plot)->SetTerminal("png");

  *dataSet = new Gnuplot2dDataset();
  (*dataSet)->SetTitle(dataSetTitle);
  (*dataSet)->SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

void writePlot(Gnuplot* plot, const std::string &filename, Gnuplot2dDataset* dataSet) {
  plot->AddDataset(*dataSet);
  std::ofstream plotFile(filename.c_str());
  plot->GenerateOutput(plotFile);
  plotFile.close();
}

void writePlot2Lines(Gnuplot* plot, const std::string &filename, Gnuplot2dDataset* dataSet1, Gnuplot2dDataset* dataSet2) {
  plot->AddDataset(*dataSet1);
  plot->AddDataset(*dataSet2);
  std::ofstream plotFile(filename.c_str());
  plot->GenerateOutput(plotFile);
  plotFile.close();
}

int main(int argc, char *argv[])
{
  LogComponentEnable("TelosBExample", LOG_LEVEL_ALL);

  // Fetch from command line
  CommandLine cmd;
  cmd.AddValue("seed", "seed for the random generator", ps.seed);
  cmd.AddValue("duration", "The number of seconds the simulation should run", ps.duration);
  cmd.AddValue("pps", "Packets per second", ps.pps);
  cmd.AddValue("ps", "Packet size", ps.packet_size);
  cmd.AddValue("device", "Device file to use for simulation", ps.deviceFile);
  cmd.AddValue("trace_file", "Trace file including times when packets should get sent", ps.trace_fn);
  cmd.Parse(argc, argv);

  SeedManager::SetSeed(ps.seed);

  createPlot(&ppsPlot, "testplot.png", "pps", &ppsDataSet);
  createPlot(&delayPlot, "delayplot.png", "intra-os delay", &delayDataSet);

#define READ_TRACES 0
#define ONE_CONTEXT 0
#define SIMULATION_OVERHEAD_TEST 0
#define ALL_CONTEXTS 1
#define CC2420_MODEL 0

#if ALL_CONTEXTS
    debugOn = false;
#else
    debugOn = true;
#endif

    if (debugOn) {
        LogComponentEnable("TelosB", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffCC2420Application", LOG_LEVEL_INFO);
    }

#if CC2420_MODEL
    CC2420Helper cc2420;

    NodeContainer nodes;
    nodes.Create(3);

    NetDeviceContainer devices = cc2420.Install(nodes, true); // regular CC2420NetDevice

    InternetStackHelper stack;
    stack.Install(nodes);

    MobilityHelper mobility;

    // The way we want to configure this: mote 1 receives the packet from mote 2, but mote 3 does not receive it.
    // Mote 3 receives the packet from mote 2.
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (30.0),
                                   "DeltaY", DoubleValue (10.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    auto netDevice1 = nodes.Get(0)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
    auto netDevice2 = nodes.Get(1)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
    auto netDevice3 = nodes.Get(2)->GetDevice(0)->GetObject<CC2420InterfaceNetDevice>();
    auto mote1 = new TelosB(); mote1->Configure(nodes.Get(0), &ps, netDevice1);
    auto mote2 = new TelosB(); mote2->Configure(nodes.Get(1), &ps, netDevice2);
    auto mote3 = new TelosB(); mote3->Configure(nodes.Get(2), &ps, netDevice3);

    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    eeh->Install(ps.deviceFile, mote2->GetNode());

    // send packets to PacketSink (installed on node 1)
    OnOffCC2420Helper onoff;
    // 80kbps ist die "Grenze", bei der bei einer Paketgröße von 20 Bytes gerade noch alle Pakete ankommen

    // first simulation (DataRate and PacketSize ok)
    onoff.SetAttribute("DataRate", StringValue(ps.kbps));
    onoff.SetAttribute("PacketSize", StringValue(SSTR( ps.packet_size ))); //default is 512, which is too much for CC2420

    ApplicationContainer clientApps = onoff.Install(nodes.Get(0));

    netDevice2->SetMessageCallback(MakeCallback(&TelosB::HandleRead, mote2));

    uint8_t channelNo = 12; // channel number is changed from default value 11 to 12
    uint8_t power = 31; // power remains on default value of 31
    Ptr<CC2420Message> msg = CreateObject<CC2420Setup>(channelNo, power);
    //construct Config message (default power, changed channel)
    netDevice2->descendingSignal(msg);

    // request current status
    netDevice2->descendingSignal(CreateObject<CC2420StatusReq>());

    PacketSinkCC2420Helper pktSink;

    ApplicationContainer serverApps = pktSink.Install(nodes.Get(2));
    //serverApps.Start(Seconds(1.0));
    //serverApps.Stop(Seconds(5.0));

    ps.duration = 8.01;
    Simulator::Stop(Seconds(ps.duration));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO ("Packet size: " << ps.packet_size << ", pps: " << ps.pps << ", RXFIFO flushes: " << ps.nr_rxfifo_flushes <<
                 ", bad CRC: " << ps.nr_packets_dropped_bad_crc << ", radio collision: " << ps.nr_packets_collision_missed <<
                 ", ip layer drop: " << ps.nr_packets_dropped_ip_layer << ", successfully forwarded: " <<
                 ps.nr_packets_forwarded << " / " << ps.nr_packets_total << " = " <<
                 (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100 << "% in " << (ps.duration/2 + (int)ps.duration % 2) <<
                 " seconds, actual pps=" << (ps.nr_packets_forwarded/(ps.duration/2 + (int)ps.duration % 2)));

#elif READ_TRACES
  Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));

    // Create node with ExecEnv
    NodeContainer c;
    memset(&c, 0, sizeof(NodeContainer));
    c.Create(3);

    std::string line;
    std::ifstream trace_file;
    trace_file.open (ps.trace_fn.c_str());

    eeh->Install(ps.deviceFile, c.Get(0));
    eeh->Install(ps.deviceFile, c.Get(1));
    eeh->Install(ps.deviceFile, c.Get(2));

    auto ee1 = c.Get(0)->GetObject<ExecEnv>();
    auto ee2 = c.Get(1)->GetObject<ExecEnv>();
    auto ee3 = c.Get(2)->GetObject<ExecEnv>();
    ProtocolStack *protocolStack = &ps;

    auto mote1 = new TelosB(); mote1->Configure(c.Get(0), &ps, nullptr);
    auto mote2 = new TelosB(); mote2->Configure(c.Get(1), &ps, nullptr);
    auto mote3 = new TelosB(); mote3->Configure(c.Get(2), &ps, nullptr);

    ps.pps = 0;  // Need to disable pps here
    bool next_is_packet_size = false;
    Time first_time = MicroSeconds(0);
    Time next_time;
    while (!trace_file.eof()) {
        getline(trace_file, line);
        if (next_is_packet_size) {
          next_is_packet_size = false;
          ps.packet_size = atoi(line.c_str());
        } else {
          next_is_packet_size = true;
          next_time = MicroSeconds(atoi(line.c_str()));
          if (first_time == MicroSeconds(0))
            first_time = next_time;
          continue;
        }
        protocolStack->GenerateTraffic2(c.Get(0), ps.packet_size-36, MicroSeconds
                                        ((next_time-first_time).GetMicroSeconds()*0.87), mote1, mote2, mote3);
        //Simulator::Schedule(MicroSeconds(atoi(line.c_str())),
        //                    &ProtocolStack::GeneratePacket, protocolStack, ps.packet_size, curSeqNr++, mote1, mote2, mote3);
        NS_LOG_INFO ("Sending packet at " << ps.packet_size-36 << " or in microseconds " <<
                     (next_time-first_time).GetMicroSeconds());
    }
    Simulator::Stop(Seconds(ps.duration));
    Simulator::Run();
    Simulator::Destroy();

    createPlot(&intraOsDelayPlot, "intraOsDelay.png", "Intra-OS delay", &intraOsDelayDataSet);
    for (int i = 0; i < ps.all_intra_os_delays.size(); ++i) {
        NS_LOG_INFO ("3 " << ps.all_intra_os_delays[i]);
        intraOsDelayDataSet->Add(ps.forwarded_packets_seqnos[i], ps.all_intra_os_delays[i]);
    }
    NS_LOG_INFO ("Packet size: " << ps.packet_size << ", ps.pps: " << ps.pps << ", RXFIFO flushes: " << ps.nr_rxfifo_flushes <<
                 ", bad CRC: " << ps.nr_packets_dropped_bad_crc << ", radio collision: " << ps.nr_packets_collision_missed <<
                 ", ip layer drop: " << ps.nr_packets_dropped_ip_layer << ", successfully forwarded: " <<
                 ps.nr_packets_forwarded << " / " << ps.nr_packets_total << " = " <<
                 (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100 << "% Intra OS median: " <<
                 ps.all_intra_os_delays.at(ps.all_intra_os_delays.size()/2));

    writePlot(intraOsDelayPlot, "plots/intraOsDelay.gnu", intraOsDelayDataSet);

#elif ONE_CONTEXT
  Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
          "cacheLineSize", UintegerValue(64), "tracingOverhead",
          UintegerValue(0));

  // Create node with ExecEnv
  NodeContainer c;
  memset(&c, 0, sizeof(NodeContainer));
  c.Create(3);

  eeh->Install(ps.deviceFile, c.Get(0));
  eeh->Install(ps.deviceFile, c.Get(1));
  eeh->Install(ps.deviceFile, c.Get(2));

  ProtocolStack *protocolStack = &ps;

  auto mote1 = new TelosB(); mote1->Configure(c.Get(0), &ps, nullptr);
  auto mote2 = new TelosB(); mote2->Configure(c.Get(1), &ps, nullptr);
  auto mote3 = new TelosB(); mote3->Configure(c.Get(2), &ps, nullptr);

  protocolStack->GenerateTraffic(c.Get(0), ps.packet_size, mote1, mote2, mote3);
  Simulator::Stop(Seconds(ps.duration));
  clock_t t;
  t = clock();
  Simulator::Run();
  t = clock() - t;
  Simulator::Destroy();
  NS_LOG_INFO ("Packet size: " << ps.packet_size << ", ps.pps: " << ps.pps << ", RXFIFO flushes: " << ps.nr_rxfifo_flushes <<
                               ", bad CRC: " << ps.nr_packets_dropped_bad_crc << ", radio collision: " << ps.nr_packets_collision_missed <<
                               ", ip layer drop: " << ps.nr_packets_dropped_ip_layer << ", successfully forwarded: " <<
                               ps.nr_packets_forwarded << " / " << ps.nr_packets_total << " = " <<
                               (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100 << "%");
  NS_LOG_INFO ("1 " << ps.packet_size << " " << ps.pps << " " << (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100 << "\n");
  NS_LOG_INFO ("2 " << ps.packet_size << " " << ps.pps << " " <<
                    (ps.nr_packets_dropped_ip_layer/(float)ps.nr_packets_total)*100 << "\n");
  NS_LOG_INFO ("3 " << ps.packet_size << " " << ps.pps << " " << ps.total_intra_os_delay/(float)ps.nr_packets_total << "\n");
  NS_LOG_INFO ("Milliseconds it took to simulate: " << t);
#elif SIMULATION_OVERHEAD_TEST
    NodeContainer c;
    int numberMotes = 1;
    ps.pps = 10;
    ps.duration = 6000;
    memset(&c, 0, sizeof(NodeContainer));
    c.Create(numberMotes);

    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    /*Ptr<ExecEnvHelper> eeh2 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh3 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh4 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh5 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh6 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh7 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh8 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh9 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    Ptr<ExecEnvHelper> eeh10 = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));

    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));*/

   // eeh1->Install(ps.deviceFile, c.Get(10));
    /*eeh2->Install(ps.deviceFile, c.Get(11));
    eeh3->Install(ps.deviceFile, c.Get(12));
    eeh4->Install(ps.deviceFile, c.Get(13));
    eeh5->Install(ps.deviceFile, c.Get(14));
    eeh6->Install(ps.deviceFile, c.Get(15));
    eeh7->Install(ps.deviceFile, c.Get(16));
    eeh8->Install(ps.deviceFile, c.Get(17));
    eeh9->Install(ps.deviceFile, c.Get(18));
    eeh10->Install(ps.deviceFile, c.Get(19));*/

    ProtocolStack *protocolStack = &ps;

    clock_t install_time;
    install_time = clock();
    eeh->Install(ps.deviceFile, c);
    for (int i = 0; i < numberMotes; i++) {
        auto mote1 = new TelosB(); mote1->Configure(c.Get(i), &ps, nullptr);
        auto mote2 = new TelosB(); mote1->Configure(c.Get(i), &ps, nullptr);
        auto mote3 = new TelosB(); mote1->Configure(c.Get(i), &ps, nullptr);
auto
        protocolStack->GenerateTraffic(c.Get(i), ps.packet_size, mote1, mote2, mote3);
    }
    install_time = clock() - install_time;

    Simulator::Stop(Seconds(ps.duration));
    clock_t t;
    NS_LOG_INFO ("Before");
    t = clock();
    Simulator::Run();
    t = clock() - t;
    Simulator::Destroy();
    NS_LOG_INFO ("Packet size: " << ps.packet_size << ", pps: " << ps.pps << ", RXFIFO flushes: " << ps.nr_rxfifo_flushes <<
                 ", bad CRC: " << ps.nr_packets_dropped_bad_crc << ", radio collision: " << ps.nr_packets_collision_missed <<
                 ", ip layer drop: " << ps.nr_packets_dropped_ip_layer << ", successfully forwarded: " <<
                 ps.nr_packets_forwarded << " / " << ps.nr_packets_total << " = " <<
                 (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100 << "%");
    NS_LOG_INFO ("1 " << ps.packet_size << " " << ps.pps << " " << (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100 << "\n");
    NS_LOG_INFO ("2 " << ps.packet_size << " " << ps.pps << " " <<
                 (ps.nr_packets_dropped_ip_layer/(float)ps.nr_packets_total)*100 << "\n");
    NS_LOG_INFO ("3 " << ps.packet_size << " " << ps.pps << " " << ps.total_intra_os_delay/(float)ps.nr_packets_total << "\n");
    NS_LOG_INFO ("Microseconds to simulate " << numberMotes << " motes for " << ps.duration << " seconds: " <<
                 t << ", install time in microseconds: " << install_time);
#elif ALL_CONTEXTS

    std::ofstream numberForwardedFile ("NumberPoints.txt");
    if (!numberForwardedFile.is_open()) {
        NS_LOG_INFO ("Failed to open NumberPoints.txt, exiting");
        exit(-1);
    }
    for (uint32_t i = 125; i >= 36; i-=8) {
        ps.packet_size = i;
        std::ostringstream os;
        os << ps.packet_size;
        createPlot(&numberForwardedPlot, "numberForwarded"+os.str()+".png", "Forwarded at packet size: "+os.str(),
                   &numberForwardedDataSet);
        createPlot2(&packetOutcomePlot, "packetOutcome"+os.str()+".png", "Packet outcome at packet size: "+os.str(),
                    &numberForwardedDataSet2, "Forwarded");
        createPlot(&numberCollidedPlot, "numberCollided"+os.str()+".png", "Collided at packet size: "+os.str(),
                   &numberCollidedDataSet);
        createPlot(&numberRxfifoFlushesPlot, "numberRxfifoFlushes"+os.str()+".png",
                   "RXFIFO flushes at packet size: "+os.str(), &numberRxfifoFlushesDataSet);
        createPlot(&numberBadCrcPlot, "numberBadCrc"+os.str()+".png", "Bad CRC at packet size: "+os.str(),
                   &numberBadCrcDataSet);
        createPlot(&numberIPDroppedPlot, "numberIPdropped"+os.str()+".png",
                   "Dropped at IP layer - packet size: "+os.str(), &numberIPDroppedDataSet);
        createPlot(&intraOsDelayPlot, "intraOsDelay"+os.str()+".png", "Intra-OS delay - packet size: "+os.str(),
                   &intraOsDelayDataSet);
        numberForwardedPlot->AppendExtra ("set xrange [37:]");
        numberCollidedPlot->AppendExtra ("set xrange [37:]");
        numberRxfifoFlushesPlot->AppendExtra ("set xrange [37:]");
        numberBadCrcPlot->AppendExtra ("set xrange [37:]");
        numberIPDroppedPlot->AppendExtra ("set xrange [37:]");

        for (int j = 0; j <= 150; j++) {
            ps.pps = j;

            ps.nr_packets_forwarded = 0;
            ps.nr_packets_total = 0;
            ps.nr_packets_dropped_bad_crc = 0;
            ps.nr_packets_collision_missed = 0;
            ps.nr_packets_dropped_ip_layer = 0;
            ps.nr_rxfifo_flushes = 0;
            ps.total_intra_os_delay = 0;
            ps.all_intra_os_delays.clear();

            // Create node with ExecEnv
            NodeContainer node_container;
            node_container.Create(3);

            Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
                    "cacheLineSize", UintegerValue(64), "tracingOverhead",
                    UintegerValue(0));

            eeh->Install(ps.deviceFile, node_container.Get(0));
            eeh->Install(ps.deviceFile, node_container.Get(1));
            eeh->Install(ps.deviceFile, node_container.Get(2));

            ProtocolStack *protocolStack = &ps;

            auto mote1 = new TelosB(); mote1->Configure(node_container.Get(0), &ps, nullptr);
            auto mote2 = new TelosB(); mote2->Configure(node_container.Get(1), &ps, nullptr);
            auto mote3 = new TelosB(); mote3->Configure(node_container.Get(2), &ps, nullptr);

            protocolStack->GenerateTraffic(node_container.Get(0), ps.packet_size, mote1, mote2, mote3);
            Simulator::Stop(Seconds(ps.duration));
            Simulator::Run();
            Simulator::Destroy();
            numberForwardedDataSet->Add(ps.pps, (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100);
            numberForwardedDataSet2->Add(ps.pps, (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100);
            numberBadCrcDataSet->Add(ps.pps, (ps.nr_packets_dropped_bad_crc/(float)ps.nr_packets_total)*100);
            numberCollidedDataSet->Add(ps.pps, (ps.nr_packets_collision_missed/(float)ps.nr_packets_total)*100);
            numberRxfifoFlushesDataSet->Add(ps.pps, ps.nr_rxfifo_flushes);
            numberIPDroppedDataSet->Add(ps.pps, (ps.nr_packets_dropped_ip_layer/(float)ps.nr_packets_total)*100);
            intraOsDelayDataSet->Add(ps.pps, ps.all_intra_os_delays.at(ps.all_intra_os_delays.size()/2));

            numberForwardedFile << std::flush;
            numberForwardedFile << "1 " << i << " " << ps.pps << " " << (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100
                                << "\n";
            numberForwardedFile << std::flush;
            numberForwardedFile << "2 " << i << " " << ps.pps << " "
                                << (ps.nr_packets_dropped_ip_layer/(float)ps.nr_packets_total)*100 << "\n";
            numberForwardedFile << std::flush;
            numberForwardedFile << "3 " << i << " " << ps.pps << " "
                                << ps.all_intra_os_delays.at(ps.all_intra_os_delays.size()/2) << "\n";
            numberForwardedFile << std::flush;

            NS_LOG_INFO ("Packet size: " << ps.packet_size << ", pps: " << ps.pps << ", RXFIFO flushes: " <<
                         ps.nr_rxfifo_flushes << ", bad CRC: " << ps.nr_packets_dropped_bad_crc << ", radio collision: " <<
                         ps.nr_packets_collision_missed <<
                         ", ip layer drop: " << ps.nr_packets_dropped_ip_layer << ", successfully forwarded: " <<
                         ps.nr_packets_forwarded << " / " << ps.nr_packets_total << " = " <<
                         (ps.nr_packets_forwarded/(float)ps.nr_packets_total)*100 <<
                         "% Intra OS median: " << ps.all_intra_os_delays.at(ps.all_intra_os_delays.size()/2));

            delete mote1;
            delete mote2;
            delete mote3;
        }

        writePlot2Lines(packetOutcomePlot, "plots/numberForwardedNumberBadCrc" + os.str() + ".gnu",
                        numberForwardedDataSet2, numberIPDroppedDataSet);
        writePlot(numberForwardedPlot, "plots/numberForwarded" + os.str() + ".gnu", numberForwardedDataSet);
        writePlot(numberIPDroppedPlot, "plots/numberIPDropped" + os.str() + ".gnu", numberIPDroppedDataSet);
        writePlot(numberCollidedPlot, "plots/numberCollided" + os.str() + ".gnu", numberCollidedDataSet);
        writePlot(numberRxfifoFlushesPlot, "plots/numberRxfifoFlushes" + os.str() + ".gnu", numberRxfifoFlushesDataSet);
        writePlot(numberBadCrcPlot, "plots/numberBadCrc" + os.str() + ".gnu", numberBadCrcDataSet);
        writePlot(intraOsDelayPlot, "plots/intraOsDelay" + os.str() + ".gnu", intraOsDelayDataSet);

    }

    numberForwardedFile.close();
#endif

  return 0;
}
