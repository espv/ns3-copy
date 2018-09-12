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

#include <cstdlib>
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/olsr-helper.h"
#include "ns3/dcep-app-helper.h"
#include "ns3/netanim-module.h"
#include "src/dcep/model/cep-engine.h"
#include "ns3/cep-engine.h"
#include "ns3/stats-module.h"
#include "ns3/data-collector.h"
#include "ns3/time-data-calculators.h"
#include "ns3/trex.h"
using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("MANETSimulation");

static TRexProtocolStack ps;

int main(int argc, char** argv) {
    
    std::string phyMode ("DsssRate1Mbps");
    double rss = -80;  // -dBm
    std::string mobilityTraceFile ("bonn-motion/bonnmotion-3.0.1/bin/mobility4adaptation.ns_movements");
    
    CommandLine cmd;

    LogComponentEnable ("MANETSimulation", LOG_LEVEL_INFO);
    LogComponentEnable ("Placement", LOG_LEVEL_INFO);
    LogComponentEnable ("Dcep", LOG_LEVEL_INFO);
    LogComponentEnable ("Communication", LOG_LEVEL_INFO);
    LogComponentEnable ("Detector", LOG_LEVEL_INFO);
    LogComponentEnable ("ResourceManager", LOG_LEVEL_INFO);
    
    std::string placementPolicy ("centralized");
    std::string adaptationMechanism ("FastAdaptationMechanism");
    uint32_t numberOfEvents = 1;
    uint32_t numStationary = 3;
    uint32_t numMobile = 50;
    uint32_t allNodes = numMobile+numStationary;
    uint64_t stateSize = 100;
    uint32_t eventRate = 1;/* # per second*/
    
    std::string format ("OMNet++");
    std::string experiment ("dcep-performance-test"); //the current study
    std::string strategy ("event-workload");//parameters being examined
    std::string runID("defaultID");//unique identifier for this trial
    
    
    cmd.AddValue ("PlacementPolicy", "the structure of the placement mechanism", placementPolicy);
    cmd.AddValue ("AdaptationMechanism", "the adaptation mechanism to be applied", adaptationMechanism);
    cmd.AddValue ("NumberOfEvents", "the number of events to be generated by each datasource", numberOfEvents);
    cmd.AddValue("EventRate", "the rate at which events are produced by data sources", eventRate);
    cmd.AddValue ("StateSize", "Size of the operator state ", stateSize);
    cmd.AddValue("RunID", "", runID);    
    cmd.Parse (argc, argv);
    
    NodeContainer allNodesContainer;
    allNodesContainer.Create (allNodes);
    
    WifiHelper wifi;
    
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    wifiPhy.Set ("RxGain", DoubleValue (0) ); 
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
    wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel",
			"MaxRange", StringValue("350.0"));
    wifiPhy.SetChannel (wifiChannel.Create ());

    QosWifiMacHelper wifiMac;
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode",StringValue (phyMode),
                                  "ControlMode",StringValue (phyMode));
    
    wifiMac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodesContainer);
    
    
    Ns2MobilityHelper ns2 = Ns2MobilityHelper (mobilityTraceFile);
    
    MobilityHelper staticMobility;
    Ptr<ListPositionAllocator> positionAlloc2 = CreateObject<ListPositionAllocator> ();
    
    
    positionAlloc2->Add (Vector (300.0, 300.0, 0.0));//sink
    positionAlloc2->Add (Vector (1275.0, 50.0, 0.0));
    positionAlloc2->Add (Vector (1275.0, 400.0, 0.0));
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
    
    NodeContainer::Iterator itr = allNodesContainer.Begin();
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
    NS_LOG_INFO ("Assigning IP Addresses......");
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    wifiInterfaces = ipv4.Assign (devices);
    
    DcepAppHelper dcepApphelper;
    ApplicationContainer dcepApps = dcepApphelper.Install (allNodesContainer);
    Ipv4Address sinkAddress = wifiInterfaces.GetAddress (0);
    
    for(uint32_t i = 0; i < numStationary; i++)
    {
        dcepApps.Get(i)->SetAttribute("SinkAddress", Ipv4AddressValue (sinkAddress));
        dcepApps.Get(i)->SetAttribute("placement policy", StringValue(placementPolicy));
        if(i == 0)
        {
            NS_LOG_INFO("sink...");
            dcepApps.Get(i)->SetAttribute("IsSink", BooleanValue(true));
        }
        else//data generator
        {
            NS_LOG_INFO("generator...");
            dcepApps.Get(i)->SetAttribute("IsGenerator", BooleanValue(true));
            dcepApps.Get(i)->SetAttribute("event code", UintegerValue (i));
            dcepApps.Get(i)->SetAttribute("number of events", UintegerValue (numberOfEvents));

        }
    }

    // Espen
    Ptr<ExecEnvHelper> eeh = CreateObjectWithAttributes<ExecEnvHelper>(
            "cacheLineSize", UintegerValue(64), "tracingOverhead",
            UintegerValue(0));
    // Espen
    
    for(uint32_t i = numStationary; i < allNodesContainer.GetN(); i++)
    {
        dcepApps.Get(i)->SetAttribute("SinkAddress", Ipv4AddressValue (sinkAddress));
        dcepApps.Get(i)->SetAttribute("placement policy", StringValue(placementPolicy));
        new TRex(allNodesContainer.Get(i), &ps);
        eeh->Install(ps.deviceFile, allNodesContainer.Get(i));
    }
    
    
    NS_LOG_INFO ("Starting applications .....");
    dcepApps.Start (Seconds (50.0));//make some time for olsr to stabilise
    dcepApps.Stop (Seconds (1000));

    Simulator::Stop (Seconds (1300.0));
     
    Simulator::Run ();
    Simulator::Destroy ();
    
    return 0;
}

