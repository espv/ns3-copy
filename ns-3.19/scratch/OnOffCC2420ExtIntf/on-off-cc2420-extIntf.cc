/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/cc2420-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OnOffCC2420ExtIntfExample");

/*
 * Simulates an OnOffApplication, which sends traffic to a PacketSink over a
 * CC2420 medium. The applications communicates directly with a
 * CC2420InterfaceNetDevice and use the extended message interface.
 * Since CC2420 has a fixed data rate of 250 kbps, this can
 * be used to demonstrate a bottleneck of the medium.
 */
int
main(int argc, char *argv[])
{
	LogComponentEnable("OnOffCC2420Application", LOG_LEVEL_INFO);
	LogComponentEnable("PacketSinkCC2420", LOG_LEVEL_INFO);
	//LogComponentEnable("CC2420NetDevice", LOG_LEVEL_INFO);

	NodeContainer nodes;
	nodes.Create(2);

	CC2420Helper cc2420;

	NetDeviceContainer devices;
	devices = cc2420.Install(nodes, true); // CC2420InterfaceNetDevice

	//InternetStackHelper stack;
	//stack.Install(nodes);

	MobilityHelper mobility;

	mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
	                               "MinX", DoubleValue (0.0),
	                               "MinY", DoubleValue (0.0),
	                               "DeltaX", DoubleValue (5.0),
	                               "DeltaY", DoubleValue (10.0),
	                               "GridWidth", UintegerValue (3),
	                               "LayoutType", StringValue ("RowFirst"));

	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (nodes);


	//Ipv4AddressHelper address;
	//address.SetBase("10.1.1.0", "255.255.255.0");

	//Ipv4InterfaceContainer interfaces = address.Assign(devices);

	// send packets to PacketSink (installed on node 1)
	OnOffCC2420Helper onoff;
	// 80kbps ist die "Grenze", bei der bei einer Paketgröße von 20 Bytes gerade noch alle Pakete ankommen

	// first simulation (DataRate and PacketSize ok)
	onoff.SetAttribute("DataRate", StringValue("70kbps"));
	onoff.SetAttribute("PacketSize", StringValue("20")); //default is 512, which is too much for CC2420

	// second simulation (DataRate is here also ok, because no Udp and Ip headers are used, PacketSize ok)
	//onoff.SetAttribute("DataRate", StringValue("90kbps"));
	//onoff.SetAttribute("PacketSize", StringValue("20"));

	// third simulation (DataRate ok, PacketSize too big)
	//onoff.SetAttribute("DataRate", StringValue("70kbps"));
	//onoff.SetAttribute("PacketSize", StringValue("150"));

	ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
	clientApps.Start(Seconds(2.0));
	clientApps.Stop(Seconds(5.0));

	PacketSinkCC2420Helper pktSink;

	ApplicationContainer serverApps = pktSink.Install(nodes.Get(1));
	serverApps.Start(Seconds(1.0));
	serverApps.Stop(Seconds(5.0));

	Simulator::Run();
	Simulator::Destroy();
	return 0;
}
