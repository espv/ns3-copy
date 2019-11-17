/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   MANETSimulation.cc
 * Author: fabrice
 *
 * Created on February 22, 2018, 11:43 AM
 */

#include <queue>

#include <ns3/dcep-module.h>
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/dcep-app-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/stats-module.h"
#include "ns3/data-collector.h"
#include "ns3/time-data-calculators.h"
#include "ns3/trex.h"
#include "ns3/siddhitrexquery.h"

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("Siddhi/T-Rex throughput");


class SiddhiTRexTrace {
    std::string traceFileName;
    std::queue<int> indexQueue;

    /*std::queue<std::pair<Time, Ptr<CepEvent> > > rxCepEventTraceTuples;
    std::queue<Time> clearQueriesTraceTuples;
    std::queue<std::pair<Time, Ptr<Query> > > txQueryTraceTuples;
    std::queue<std::pair<Time, std::pair<Ptr<CepEvent>, Ptr<Query> > > > passedConstraintsTraceTuples;
    std::queue<std::pair<Time, Ptr<CepEvent> > > checkedConstraintsTraceTuples;
    std::queue<std::pair<Time, Ptr<CepEvent> > > rxFinalCepEventTraceTuples;*/

    std::queue<std::vector<long> > rxCepEventTraceTuples;
    std::queue<std::vector<long> > clearQueriesTraceTuples;
    std::queue<std::vector<long> > txQueryTraceTuples;
    std::queue<std::vector<long> > passedConstraintsTraceTuples;
    std::queue<std::vector<long> > checkedConstraintsTraceTuples;
    std::queue<std::vector<long> > rxFinalCepEventTraceTuples;
    std::queue<std::vector<long> > finishedProcessingCepEvent;

public:
    SiddhiTRexTrace(std::string traceFileName);

    void RxCepEventTrace(Ptr<CepEvent> e, Thread *t);
    void ClearQueriesTrace();
    void TxQueryTrace(Ptr<Query> q, Thread *t);
    void CheckedConstraintsTrace(Ptr<CepEvent> e, Thread *t);
    void PassedConstraintsTrace(Ptr<CepEvent> e, Ptr<Query> q, Thread *t);
    void RxFinalCepEventTrace(Ptr<CepEvent> e, Thread *t);
    void FinishedProcessingCepEvent(Ptr<CepEvent> e, Thread *t);

    void WriteTraceToFile();
};


SiddhiTRexTrace::SiddhiTRexTrace(std::string traceFileName)
{
    this->traceFileName = traceFileName + "_simulated";
}


void SiddhiTRexTrace::RxCepEventTrace(Ptr<CepEvent> e, Thread *t)
{
    std::cout << Simulator::Now() << ": Received CEP event " << e << std::endl;
    //auto pair = make_pair(Simulator::Now(), e);
    auto *args = new std::vector<long>();
    args->push_back((long)Simulator::Now().GetNanoSeconds());
    args->push_back(t->GetPid());
    args->push_back(e->m_seq);
    rxCepEventTraceTuples.push(*args);
    //rxCepEventTraceTuples.push(pair);
    indexQueue.push(1);
}


void SiddhiTRexTrace::ClearQueriesTrace()
{
    std::cout << Simulator::Now() << ": Cleared queries" << std::endl;
    auto *args = new std::vector<long>();
    args->push_back((long)Simulator::Now().GetNanoSeconds());
    clearQueriesTraceTuples.push(*args);
    //clearQueriesTraceTuples.push(Simulator::Now());
    indexQueue.push(222);
}


void SiddhiTRexTrace::TxQueryTrace(Ptr<Query> q, Thread *t)
{
    std::cout << Simulator::Now() << ": Transmitted query " << q->eventType << std::endl;
    auto *args = new std::vector<long>();
    args->push_back((long)Simulator::Now().GetNanoSeconds());
    args->push_back(q->query_base_id);
    txQueryTraceTuples.push(*args);
    //auto pair = make_pair(Simulator::Now(), q);
    //txQueryTraceTuples.push(pair);
    indexQueue.push(221);
}


void SiddhiTRexTrace::CheckedConstraintsTrace(Ptr<CepEvent> e, Thread *t)
{
    std::cout << Simulator::Now() << ": " << e << " Checked constraints of event " << e->type << std::endl;
    auto *args = new std::vector<long>();
    args->push_back((long)Simulator::Now().GetNanoSeconds());
    args->push_back(t->GetPid());
    args->push_back(e->m_seq);
    checkedConstraintsTraceTuples.push(*args);
    //auto pair = make_pair(Simulator::Now(), e);
    //checkedConstraintsTraceTuples.push(pair);
    indexQueue.push(4);
}

void SiddhiTRexTrace::PassedConstraintsTrace(Ptr<CepEvent> e, Ptr<Query> q, Thread *t)
{
    std::cout << Simulator::Now() << ": Event " << e->type << " passed constraints of query " << q->eventType << std::endl;
    auto *args = new std::vector<long>();
    args->push_back((long)Simulator::Now().GetNanoSeconds());
    args->push_back(t->GetPid());
    args->push_back(e->m_seq);
    args->push_back(q->id);
    passedConstraintsTraceTuples.push(*args);
    //auto pair = make_pair(Simulator::Now(), make_pair(e, q));
    //passedConstraintsTraceTuples.push(pair);
    indexQueue.push(5);
}


void SiddhiTRexTrace::RxFinalCepEventTrace(Ptr<CepEvent> e, Thread *t)
{
    std::cout << Simulator::Now() << ": Created complex event " << e->type << std::endl;
    auto *args = new std::vector<long>();
    args->push_back((long)Simulator::Now().GetNanoSeconds());
    args->push_back(t->GetPid());
    args->push_back(e->m_seq);
    rxFinalCepEventTraceTuples.push(*args);
    //auto pair = make_pair(Simulator::Now(), e);
    //rxFinalCepEventTraceTuples.push(pair);
    indexQueue.push(6);
}

void SiddhiTRexTrace::FinishedProcessingCepEvent(Ptr<CepEvent> e, Thread *t)
{
    std::cout << Simulator::Now() << ": Finished processing CEP event " << e->type << std::endl;
    auto *args = new std::vector<long>();
    args->push_back((long)Simulator::Now().GetNanoSeconds());
    args->push_back(t->GetPid());
    args->push_back(e->m_seq);
    finishedProcessingCepEvent.push(*args);
    //auto pair = make_pair(Simulator::Now(), e);
    //finishedProcessingCepEvent.push(pair);
    indexQueue.push(100);
}


void writeTupleToFile(ofstream& myfile, int index, const std::vector<long>& items) {
    myfile << index;
    for (auto item : items) {
        myfile << "\t" << item;
    }
    myfile << "\n";
}


void SiddhiTRexTrace::WriteTraceToFile() {
    ofstream myfile;
    myfile.open (traceFileName);

    NS_LOG_INFO("\n\nPrinting out the simulation and milestone execution events:");

    while (!indexQueue.empty()) {
        auto index = indexQueue.front();
        indexQueue.pop();
        std::vector<long> items;
        switch (index) {
            case 1: {
                //std::pair<Time, Ptr<CepEvent> > item = rxCepEventTraceTuples.front();
                items = rxCepEventTraceTuples.front();
                rxCepEventTraceTuples.pop();
                NS_LOG_INFO(items[0] << ": Received event " << items[1]);
                break;
            } case 222: {
                //Time t = clearQueriesTraceTuples.front();
                items = clearQueriesTraceTuples.front();
                clearQueriesTraceTuples.pop();
                NS_LOG_INFO(items[0] << ": ClearQueries");
                //myfile << index << "\t" << items[0] << "\n";
                break;
            } case 221: {
                items = txQueryTraceTuples.front();
                txQueryTraceTuples.pop();
                NS_LOG_INFO(items[0] << ": Transmitted query " << items[2]);
                //myfile << index << "\t" << items[0] << "\t" << items[1] << "\n";
                break;
            } case 4: {
                items = checkedConstraintsTraceTuples.front();
                checkedConstraintsTraceTuples.pop();
                NS_LOG_INFO(items[0] << ": Checked constraints of event " << items[2]);
                //myfile << index << "\t" << items[0] << "\t" << items[1] << "\n";
                break;
            } case 5: {
                items = passedConstraintsTraceTuples.front();
                passedConstraintsTraceTuples.pop();
                NS_LOG_INFO(items[0] << ": Event " << items[2] << " passed the constraints of query " << items[3]);

                //myfile << index << "\t" << items[0] << "\t" << items[1] << "\n";
                break;
            } case 6: {
                items = rxFinalCepEventTraceTuples.front();
                rxFinalCepEventTraceTuples.pop();
                NS_LOG_INFO(items[0] << ": Created complex event " << items[2]);
                //myfile << index << "\t" << items[0] << "\t" << items[1] << "\n";
                break;
            } case 100: {
                items = finishedProcessingCepEvent.front();
                finishedProcessingCepEvent.pop();
                NS_LOG_INFO(items[0] << ": Finished processing CEP event " << items[2]);
                //myfile << index << "\t" << items[0] << "\t" << items[1] << "\n";
                break;
            } default: {
                break;
            }
        }

        writeTupleToFile(myfile, index, items);
    }

    NS_LOG_INFO("Finished writing trace file " << traceFileName);
    myfile.close();
}


int main(int argc, char** argv) {

    // Real-world experiment limits bandwidth to 6mpbs by running "sudo wondershaper eth0 6000 6000" where 6000 kbps for up and download
    std::string phyMode ("OfdmRate36Mbps");
    double rss = 0;  // -dBm
    std::string mobilityTraceFile ("bonn-motion/bonnmotion-3.0.1/bin/mobility4adaptation.ns_movements");
    
    CommandLine cmd;

    LogComponentEnable ("Siddhi/T-Rex throughput", LOG_LEVEL_INFO);
    LogComponentEnable ("Placement", LOG_LEVEL_INFO);
    LogComponentEnable ("Dcep", LOG_LEVEL_INFO);
    LogComponentEnable ("Communication", LOG_LEVEL_INFO);
    LogComponentEnable ("Detector", LOG_LEVEL_INFO);
    LogComponentEnable ("ResourceManager", LOG_LEVEL_INFO);
    
    std::string placementPolicy ("centralized");
    std::string adaptationMechanism ("FastAdaptationMechanism");
    uint32_t numberOfCepEvents = 100;
    uint32_t numberOfCepQueries = 100;
    uint32_t numStationary = 4;  // Four stationaries where two are data source, one is a T-Rex server and the last is a subscriber
    uint32_t numMobile = 0;
    uint32_t allNodes = numMobile+numStationary;
    uint64_t stateSize = 100;

    Ptr<TRexProtocolStack> pss[allNodes];

    std::string format ("OMNet++");
    std::string experiment ("dcep-performance-test"); //the current study
    std::string strategy ("event-workload");//parameters being examined
    std::string runID("defaultID");//unique identifier for this trial
    std::string trace_fn = "";
    std::string experiment_metadata_fn = "";

    
    cmd.AddValue ("PlacementPolicy", "the structure of the placement mechanism", placementPolicy);
    cmd.AddValue ("AdaptationMechanism", "the adaptation mechanism to be applied", adaptationMechanism);
    cmd.AddValue ("NumberOfCepEvents", "the number of events to be generated by each datasource", numberOfCepEvents);
    cmd.AddValue ("NumberOfCepQueries", "the number of complex queries to be placed by sink", numberOfCepQueries);
    cmd.AddValue ("TraceFileName", "Trace to be used for scheduling events", trace_fn);
    cmd.AddValue ("ExperimentMetadataFileName", "Metadata json file for the experiment that produced the trace file", experiment_metadata_fn);
    //cmd.AddValue ("CepEventInterval", "the interval in between each event that is produced by data sources", eventInterval);
    cmd.AddValue ("StateSize", "Size of the operator state ", stateSize);
    cmd.AddValue ("RunID", "", runID);
    cmd.Parse (argc, argv);

    uint64_t eventInterval = numberOfCepQueries*1000;  // Interval in seconds

    uint64_t simulationLength = 100+numberOfCepEvents*eventInterval/1000+numberOfCepQueries*3*10;  // Time to stop the simulation

    NodeContainer allNodesContainer;
    allNodesContainer.Create (allNodes);
    NodeContainer n0n1(allNodesContainer.Get(0), allNodesContainer.Get(1));
    NodeContainer n1n2(allNodesContainer.Get(1), allNodesContainer.Get(2));

    std::string bandwidth = "5Mbps";
    std::string delay = "5ms";

    /*PointToPointHelper pointToPoint1;
    PointToPointHelper pointToPoint2;
    pointToPoint1.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
    pointToPoint2.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
    pointToPoint1.SetChannelAttribute ("Delay", StringValue (delay));
    pointToPoint2.SetChannelAttribute ("Delay", StringValue (delay));

    NetDeviceContainer devices1;
    NetDeviceContainer devices2;
    devices1 = pointToPoint1.Install (n0n1);
    devices2 = pointToPoint2.Install (n1n2);*/

    WifiHelper wifi;
    
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.Set ("RxGain", DoubleValue (0) ); 
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
    wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel",
			"MaxRange", StringValue("350.0"));
    wifiPhy.SetChannel (wifiChannel.Create ());

    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode",StringValue (phyMode),
                                  "ControlMode",StringValue (phyMode));
    
    wifiMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodesContainer);
    
    
    Ns2MobilityHelper ns2 = Ns2MobilityHelper (mobilityTraceFile);
    
    MobilityHelper staticMobility;
    Ptr<ListPositionAllocator> positionAlloc2 = CreateObject<ListPositionAllocator> ();

    positionAlloc2->Add (Vector (150.0, 150.0, 0.0));
    positionAlloc2->Add (Vector (150.0, 250.0, 0.0));
    positionAlloc2->Add (Vector (350.0, 200.0, 0.0));//sink
    positionAlloc2->Add (Vector (550.0, 200.0, 0.0));
//    positionAlloc2->Add (Vector (925.0, 310.0, 0.0));
//    positionAlloc2->Add (Vector (925.0, 510.0, 0.0));
//    positionAlloc2->Add (Vector (925.0, 710.0, 0.0));
//    positionAlloc2->Add (Vector (925.0, 910.0, 0.0));
//    positionAlloc2->Add (Vector (925.0, 1110.0, 0.0));
//    positionAlloc2->Add (Vector (925.0, 1310.0, 0.0));
    
    staticMobility.SetPositionAllocator (positionAlloc2);
    staticMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    for (uint32_t i = 0; i < numStationary; i++)
    {
	staticMobility.Install (allNodesContainer.Get(i));
    }
    auto itr = allNodesContainer.Begin();
    std::advance(itr, numStationary);// skip the first nodes which should be static
    ns2.Install (itr, allNodesContainer.End()); // configure movements for each node, while reading trace file
    
    OlsrHelper olsr;

    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;
    list.Add (staticRouting, 0);
    list.Add (olsr, 10);

    InternetStackHelper istack;
    istack.SetRoutingHelper (list); // has effect on the next Install ()
    istack.Install (allNodesContainer);

    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer wifiInterfaces;
    //Ipv4InterfaceContainer wifiInterfaces2;
    NS_LOG_INFO ("Assigning IP Addresses......");
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    wifiInterfaces = ipv4.Assign (devices);
    /*ipv4.SetBase ("10.0.1.0", "255.255.255.0");
    wifiInterfaces2 = ipv4.Assign (devices2);*/
    
    DcepAppHelper dcepApphelper;

    // Espen
    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    for (auto i = allNodesContainer.Begin (); i != allNodesContainer.End (); ++i)
    {
        Ptr<Node> node = *i;
        pss[node->GetId()] = CreateObject<TRexProtocolStack>();
        eeh->Install(pss[node->GetId()], node);
    }
    // Espen

    ApplicationContainer dcepApps = dcepApphelper.Install (allNodesContainer, "SiddhiTRexThroughput");
    Ipv4Address sinkAddress = wifiInterfaces.GetAddress (2);

    // Espen
    for (uint32_t i = 0; i < numStationary; i++)
    {
        pss[i]->AggregateObject(dcepApps.Get(i));
    }
    // Espen

    std::vector<SiddhiTRexTrace *> tracers;
    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
    uint32_t random_number = x->GetInteger (1,99999);
    for(uint32_t i = 0; i < numStationary; i++)
    {
        auto dcep = dcepApps.Get(i);
        dcep->SetAttribute("SinkAddress", Ipv4AddressValue (sinkAddress));
        dcep->SetAttribute("placement_policy", StringValue(placementPolicy));
        if(i == 2)
        {
            NS_LOG_INFO("sink...");
            dcep->SetAttribute("IsSink", BooleanValue(true));
            // In this simulation, this node produces atomic events and processes queries.
            dcep->SetAttribute("IsGenerator", BooleanValue(true));
            dcep->SetAttribute("number_of_queries", UintegerValue (numberOfCepQueries));
            dcep->SetAttribute("DistributedExecution", BooleanValue (false));
            dcep->SetAttribute("TraceFileName", StringValue(trace_fn));
            dcep->SetAttribute("ExperimentMetadataFileName", StringValue(experiment_metadata_fn));
            auto tracer = new SiddhiTRexTrace(trace_fn);
            tracers.push_back(tracer);
            dcep->TraceConnectWithoutContext ("RxCepEvent", MakeCallback(&SiddhiTRexTrace::RxCepEventTrace, tracer));
            dcep->TraceConnectWithoutContext ("TxQuery", MakeCallback(&SiddhiTRexTrace::TxQueryTrace, tracer));
            dcep->TraceConnectWithoutContext ("ClearQueries", MakeCallback(&SiddhiTRexTrace::ClearQueriesTrace, tracer));
            dcep->TraceConnectWithoutContext ("RxFinalCepEvent", MakeCallback(&SiddhiTRexTrace::RxFinalCepEventTrace, tracer));
            dcep->TraceConnectWithoutContext ("CheckedConstraints", MakeCallback(&SiddhiTRexTrace::CheckedConstraintsTrace, tracer));
            dcep->TraceConnectWithoutContext ("PassedConstraints", MakeCallback(&SiddhiTRexTrace::PassedConstraintsTrace, tracer));
            dcep->TraceConnectWithoutContext ("FinishedProcessingCepEvent", MakeCallback(&SiddhiTRexTrace::FinishedProcessingCepEvent, tracer));
        }
        //else if (i < 2)//data generator
        //{
            /*NS_LOG_INFO("generator...");
            dcepApps.Get(i)->SetAttribute("IsGenerator", BooleanValue(true));
            dcepApps.Get(i)->SetAttribute("event_code", UintegerValue (random_number % 20 + 2));
            dcepApps.Get(i)->SetAttribute("TraceFileName", StringValue(trace_fn));
            dcepApps.Get(i)->SetAttribute("ExperimentMetadataFileName", StringValue(experiment_metadata_fn));*/
        //}
    }
    
    /*for(uint32_t i = numStationary; i < allNodesContainer.GetN(); i++)
    {
        dcepApps.Get(i)->SetAttribute("SinkAddress", Ipv4AddressValue (sinkAddress));
        dcepApps.Get(i)->SetAttribute("placement_policy", StringValue(placementPolicy));
    }*/
    
    
    NS_LOG_INFO ("Starting applications .....");
    dcepApps.Start (Seconds (50.0));//make some time for olsr to stabilise
    dcepApps.Stop (Seconds (simulationLength));

    Simulator::Stop (Seconds (simulationLength*1.3));

    AnimationInterface anim("netanim-output.xml");
    anim.EnablePacketMetadata (true);
    uint32_t temperature_icon_resource = anim.AddResource ("/home/espen/Research/ns-3-extended-with-execution-environment/ns-3-dev/icons/thermometer.png");
    uint32_t humidity_icon_resource = anim.AddResource ("/home/espen/Research/ns-3-extended-with-execution-environment/ns-3-dev/icons/humidity.png");
    uint32_t raspberry_pi_icon_resource = anim.AddResource ("/home/espen/Research/ns-3-extended-with-execution-environment/ns-3-dev/icons/raspberry-pi.png");
    uint32_t control_center_icon_resource = anim.AddResource ("/home/espen/Research/ns-3-extended-with-execution-environment/ns-3-dev/icons/control-center.png");
    anim.UpdateNodeImage (0, temperature_icon_resource);
    anim.UpdateNodeImage (1, humidity_icon_resource);
    anim.UpdateNodeImage (2, raspberry_pi_icon_resource);
    anim.UpdateNodeImage (3, control_center_icon_resource);
    anim.SetBackgroundImage ("/home/espen/Research/ns-3-extended-with-execution-environment/ns-3-dev/icons/plain-white-background.jpg", -413, -275, 1, 1, 1);
    anim.UpdateNodeSize (0, 50, 50);
    anim.UpdateNodeSize (1, 50, 50);
    anim.UpdateNodeSize (2, 50, 50);
    anim.UpdateNodeSize (3, 50, 50);

    Simulator::Run ();
    Simulator::Destroy ();

    for (auto tracer : tracers) {
        tracer->WriteTraceToFile();
    }
    
    return 0;
}

