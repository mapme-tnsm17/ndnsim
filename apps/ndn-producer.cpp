/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ndn-producer.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "model/ndn-app-face.hpp"
#include "model/ndn-ns3.hpp"
#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include "ns3/wifi-net-device.h"

#include "utils/ndn-ns3-packet-tag.hpp"

#include "boost/lexical_cast.hpp"

#ifdef STRETCH_P
#include "utils/ndn-fw-from-tag.hpp"
#include "model/ndn-global-router.hpp"
#include "model/ndn-ns3.hpp"
#endif


#include <memory>

#ifdef WITH_HANDOVER_TRACING
#include "ns3/core-module.h" 
#endif

NS_LOG_COMPONENT_DEFINE("ndn.Producer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(Producer);

TypeId
Producer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::Producer")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<Producer>()
      .AddAttribute("Prefix", "Prefix, for which producer has the data", StringValue("/"),
                    MakeNameAccessor(&Producer::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&Producer::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&Producer::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&Producer::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&Producer::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&Producer::m_keyLocator), MakeNameChecker())
      .AddAttribute("EnableKite", "to kite mobility management",
				       BooleanValue(false),
				       MakeBooleanAccessor(&Producer::m_kiteEnabled),
				       MakeBooleanChecker())
#ifdef WITH_HANDOVER_TRACING
      .AddTraceSource("HandoverStatsTrace", "stats trace file name, by default  stats traces are not logged, if you don't specify a valid file name",
          MakeTraceSourceAccessor(&Producer::m_HandoverStatsTrace))
#endif
      ;
  return tid;
}

Producer::Producer()
: m_seq(0)
  ,m_rand(CreateObject<UniformRandomVariable>())
#ifdef WITH_HANDOVER_TRACING
  ,m_last_association_time(0)
  ,m_last_received_interest_time(0)
  ,m_last_received_interest_seq(0)
#endif
{
  NS_LOG_FUNCTION_NOARGS();
  
}

// inherited from Application base class.
void
Producer::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
}

void
Producer::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
Producer::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << interest);

  if (!m_active)
    return;

  Name dataName(interest->getName());
  // dataName.append(m_postfix);
  // dataName.appendVersion();

  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));
  
#ifdef PATH_LABELLING
  uint8_t pathId=0;
  data->setPathId(&pathId,1);
#endif // PATH_LABELLING

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::nonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName()<<", seq="<<(data->getName()).at(-1).toSequenceNumber());
  // to create real wire encoding
  data->wireEncode();
  
#ifdef STRETCH_P
  //NOTE: To compute the path stretch, producer tags every data with node Id of the AP that producer currently connects to.
  // The assumption the is that producer is NOT multi-homed which is the case in our simuations.
  // also we assume the global router has been installed on producer node.
  //This however, needs to be extended. we may consider move this operation to the forwarder class to eiminate the need of the assumptions.
  
  
   Ptr<GlobalRouter> gr = GetNode()->GetObject<GlobalRouter>();
   NS_ASSERT_MSG(gr, "cannot find global router on producer node");
   
   GlobalRouter::IncidencyList& producerNode_incidencies = gr->GetIncidencies();
   //NS_ASSERT_MSG(producerNode_incidencies.size()==1, "producer is connected to multiple base stations, not able to determine the base station");
   
   // if producer is NOT mulit-homed, we tag the data with prodcuer's AP's node Id:
   if(producerNode_incidencies.size()==1){
          GlobalRouter::IncidencyList::iterator i = producerNode_incidencies.begin();
          Ptr<GlobalRouter> gr_otherSide = std::get<2>(*i);
          Ptr<Node> ap= gr_otherSide ->GetObject<Node>();
          uint32_t fromHop = ap->GetId();
          Ptr<Packet> packet = Convert::ToPacket(*data);

          FwFromTag fromHopTag;   
          packet->RemovePacketTag(fromHopTag);
          fromHopTag.Set(fromHop);
   
          packet->AddPacketTag(fromHopTag);
          
          shared_ptr<const Data> d = Convert::FromPacket<Data>(packet);
          //make sure the data now has the fromHop packet tag included
          *data=*d;        
   } // else we leave the FwFromTag empty    
#endif

  m_transmittedDatas(data, this, m_face);
  m_face->onReceiveData(*data);
#ifdef WITH_HANDOVER_TRACING
   double now=Simulator::Now().ToDouble(Time::S);

  if(m_last_received_interest_time<m_last_association_time && m_last_received_interest_time>0)//last interest is received before association on layer
  {
    producerHandoverStats stats;   
    stats.l3_handoff_time=now-m_last_association_time;
    stats.first_interest_after_association_seq=(data->getName()).at(-1).toSequenceNumber();
    stats.last_interest_before_disassociation_seq=m_last_received_interest_seq;
    stats.producer_handoff_delay=now-m_last_received_interest_time;
    
    m_HandoverStatsTrace(stats);
    
  }
  m_last_received_interest_time=now;
  m_last_received_interest_seq=(data->getName()).at(-1).toSequenceNumber(); 
#endif
  

}

#ifdef WITH_HANDOVER_TRACING
void 
Producer::onAPassociation(std::string context, double time, char type, Ptr<Node> sta, Ptr<Node> ap)
{
  if (type == 'A') {
    m_last_association_time = Simulator::Now().ToDouble(Time::S);
  }
}

void 
Producer::setlogging()
{
    std::ostringstream onWifiAssocPath_fn;
    uint32_t myNodeId=GetNode()->GetId();
    onWifiAssocPath_fn<<"/NodeList/"<<myNodeId<<"/$ns3::ConnectivityManager/MobilityEvent";
     
  ns3::Config::Connect (onWifiAssocPath_fn.str(), MakeCallback(&Producer::onAPassociation,this));
}


std::string 
producerHandoverStats::getHeader()
{
   std::ostringstream oss;
    oss << "last_interest_before_disassociation_seq"
        << "\t" << "first_interest_after_association_seq"
        << "\t" << "l3_handoff_time"
        << "\t" << "producer_handoff_delay"
	;
    
    return oss.str();
}

std::string 
producerHandoverStats::toString()
{
     std::ostringstream oss;
    oss << last_interest_before_disassociation_seq
        << "\t" << first_interest_after_association_seq
        << "\t" << l3_handoff_time
        << "\t" << producer_handoff_delay
	;
    
    return oss.str();
}
#endif

} // namespace ndn
} // namespace ns3
