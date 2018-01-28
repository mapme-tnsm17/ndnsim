/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 SEBASTIEN DERONNE
 *
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
 *
 * Author: Sebastien Deronne <sebastien.deronne@gmail.com>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ndnSIM/apps/ndn-consumer-cbr.hpp"
#include "ns3/ndnSIM/helper/ndn-global-routing-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-app-helper.hpp"

#include "boost/lexical_cast.hpp"
// This is a simple example in order to show how to configure an IEEE 802.11ac Wi-Fi network.
//
// It ouputs the UDP or TCP goodput for every VHT bitrate value, which depends on the MCS value (0 to 9, where 9 is
// forbidden when the channel width is 20 MHz), the channel width (20, 40, 80 or 160 MHz) and the guard interval (long
// or short). The PHY bitrate is constant over all the simulation run. The user can also specify the distance between
// the access point and the station: the larger the distance the smaller the goodput.
//
// The simulation assumes a single station in an infrastructure network:
//
//  STA     AP
//    *     *
//    |     |
//   n1     n2
//
//Packets in this simulation aren't marked with a QosTag so they are considered
//belonging to BestEffort Access Class (AC_BE).

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ndn-vht-wifi-network-ConsumerCbr");

int nReceivedData=0;


void
onReceiveData(ns3::ndn::shared_ptr<const ns3::ndn::Data> data, Ptr<ns3::ndn::App> app, ns3::ndn::shared_ptr<ns3::ndn::Face> face)
{
 nReceivedData++;
}

int main (int argc, char *argv[])
{
  double simulationTime = 10.0; //seconds
  double distance = 1.0; //meters
  //to be deleted:
  int i=9; int j=160; int k=1;
  CommandLine cmd;
  cmd.AddValue ("distance", "Distance in meters between the station and the access point", distance);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("i", "Simulation time in seconds", i);
  cmd.AddValue ("j", "Simulation time in seconds", j);
  cmd.AddValue ("k", "Simulation time in seconds", k);
  cmd.Parse (argc,argv);

  std::cout << "MCS value" << "\t\t" << "Channel width" << "\t\t" << "short GI" << "\t\t" << "Throughput" << '\n';
  /*
  for (int i = 0; i <= 9; i++) //MCS
    {
      for (int j = 20; j <= 160; ) //channel width
        {
          if (i == 9 && j == 20)
            {
              j *= 2;
              continue;
            }
          for (int k = 0; k < 2; k++) //GI: 0 and 1
            {
            */
  

              int payloadSize=1472; //to compare with UDP throughput
            
              NodeContainer wifiStaNode;
              wifiStaNode.Create (1);
              NodeContainer wifiApNode;
              wifiApNode.Create (1);

              YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
              YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
              phy.SetChannel (channel.Create ());

              // Set guard interval
              phy.Set ("ShortGuardEnabled", BooleanValue (k));

              WifiHelper wifi = WifiHelper::Default ();
              wifi.SetStandard (WIFI_PHY_STANDARD_80211ac);
              VhtWifiMacHelper mac = VhtWifiMacHelper::Default ();
                
              StringValue DataRate = VhtWifiMacHelper::DataRateForMcs (i);
              wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", DataRate,
                                            "ControlMode", DataRate);
                
              Ssid ssid = Ssid ("ns3-80211ac");

              mac.SetType ("ns3::StaWifiMac",
                           "Ssid", SsidValue (ssid),
                           "ActiveProbing", BooleanValue (false));

              NetDeviceContainer staDevice;
              staDevice = wifi.Install (phy, mac, wifiStaNode);

              mac.SetType ("ns3::ApWifiMac",
                           "Ssid", SsidValue (ssid));

              NetDeviceContainer apDevice;
              apDevice = wifi.Install (phy, mac, wifiApNode);

              // Set channel width
              Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (j));

              // mobility.
              MobilityHelper mobility;
              Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

              positionAlloc->Add (Vector (0.0, 0.0, 0.0));
              positionAlloc->Add (Vector (distance, 0.0, 0.0));
              mobility.SetPositionAllocator (positionAlloc);

              mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

              mobility.Install (wifiApNode);
              mobility.Install (wifiStaNode);
	      
	       /* NDN stack */
	       ns3::ndn::StackHelper ndnHelper;
	       ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1");
	       ndnHelper.InstallAll(); 
	       ns3::ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");
	       
	        /* Routing */
		ns3::ndn::GlobalRoutingHelper myGlobalRoutingHelper;
		// This compute routes
		myGlobalRoutingHelper.InstallAll();
		
		
		//**************install applications******************
	
		std::string prefix="/prefix";
		//for consumer
    ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    consumerHelper.SetAttribute("Frequency",StringValue("100000.0"));
    consumerHelper.SetPrefix(prefix);
    ApplicationContainer myApps = consumerHelper.Install (wifiApNode);


  
    
   myApps.Start (Seconds (1.0));
   myApps.Stop (Seconds (simulationTime + 1));

    
    //install producer
       //set origin of prefix to producer NodeContainer
    myGlobalRoutingHelper.AddOrigins(prefix, wifiStaNode);
    
    ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix(prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue(boost::lexical_cast<std::string>(payloadSize)));
    ApplicationContainer producer=producerHelper.Install(wifiStaNode);
    producer.Start (Seconds (1.0));
    producer.Stop (Seconds (simulationTime + 1));
    
    
      Config::ConnectWithoutContext("/NodeList/" + boost::lexical_cast<std::string>(wifiApNode.Get(0)->GetId())
                                  + "/ApplicationList/*/$ns3::ndn::App/ReceivedDatas",
                                MakeCallback(&onReceiveData));

    // This is only required if there are static prefixes.
    ns3::ndn::GlobalRoutingHelper::CalculateRoutes();

              Simulator::Stop (Seconds (simulationTime+2));
              Simulator::Run ();
              Simulator::Destroy ();

           //TODO: print-satistics:
	       double throughput = 0;
          
                  throughput = nReceivedData * payloadSize * 8 / (simulationTime * 1000000.0); //Mbit/s
		  std::cout << i << "\t\t\t" << j << " MHz\t\t\t" << k << "\t\t\t" << throughput << " Mbit/s" << std::endl;
		  nReceivedData=0;


/*            }
          j *= 2;
        }
    }
    */
  return 0;
}

