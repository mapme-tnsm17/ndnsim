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

#include "ndn-consumer-cbr.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "model/ndn-app-face.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerCbr");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerCbr);

TypeId
ConsumerCbr::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerCbr")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerCbr>()

      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
                    MakeDoubleAccessor(&ConsumerCbr::m_frequency), MakeDoubleChecker<double>())

      .AddAttribute("Randomize",
                    "Type of send time randomization: none (default), uniform, exponential",
                    StringValue("none"),
                    MakeStringAccessor(&ConsumerCbr::SetRandomize, &ConsumerCbr::GetRandomize),
                    MakeStringChecker())

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&ConsumerCbr::m_seqMax), MakeIntegerChecker<uint32_t>())

      .AddAttribute("PathForTrace", "The path to a folder where you want to keep your trace, default .",
                     StringValue("none"), MakeStringAccessor(&ConsumerCbr::m_filepath),
                     MakeStringChecker())

    ;

  return tid;
}

ConsumerCbr::ConsumerCbr()
  : m_frequency(1.0)
  , m_firstTime(true)
  , m_random(0)
  , m_total_packets(0)
  , m_total_bytes(0)
  , m_old_npackets(0)
  , m_stop_tracing(false)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
}

ConsumerCbr::~ConsumerCbr()
{
}

void
ConsumerCbr::StartApplication()
{
	m_start_period = Simulator::Now();
	m_start_time = Simulator::Now();
	std::cout<<"StartTime "<<m_start_time<<" StartPeriod "<<m_start_period<<std::endl;
	Consumer::StartApplication();
	if(m_filepath != "none")
		SetTraceFileName(m_filepath, "ConsumerCbr.plotme");
}

void
ConsumerCbr::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  // std::cout << "next: " << Simulator::Now().ToDouble(Time::S) + mean << "s\n";

  if (m_firstTime) {
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &Consumer::SendPacket, this);
    m_firstTime = false;
  }
  else if (!m_sendEvent.IsRunning())
    m_sendEvent = Simulator::Schedule((m_random == 0) ? Seconds(1.0 / m_frequency)
                                                      : Seconds(m_random->GetValue()),
                                      &Consumer::SendPacket, this);
}

void
ConsumerCbr::SetRandomize(const std::string& value)
{
  if (value == "uniform") {
    m_random = CreateObject<UniformRandomVariable>();
    m_random->SetAttribute("Min", DoubleValue(0.0));
    m_random->SetAttribute("Max", DoubleValue(2 * 1.0 / m_frequency));
  }
  else if (value == "exponential") {
    m_random = CreateObject<ExponentialRandomVariable>();
    m_random->SetAttribute("Mean", DoubleValue(1.0 / m_frequency));
    m_random->SetAttribute("Bound", DoubleValue(50 * 1.0 / m_frequency));
  }
  else
    m_random = 0;

  m_randomType = value;
}

std::string
ConsumerCbr::GetRandomize() const
{
  return m_randomType;
}

void
ConsumerCbr::OnData(shared_ptr<const Data> contentObject)
{
App::OnData(contentObject);//for logging
	m_total_packets++;
	m_total_bytes += contentObject->wireEncode().size();

/*	uint64_t seq = name.at(-1).toSequenceNumber(); // c'est la bonne maniere de faire
	 ndn::Name prefix = contentObject->getName().getPrefix(2);
	 ndn::Name seq = prefix.getSubName(1,1);

	if(seq == "/%FEc")
	{
		WriteConsumerTrace(false);
		m_stop_tracing = true;
	}*/
}

void ConsumerCbr::SetTraceFileName(std::string path, std::string name)
{

	char ss[20];
	sprintf(ss,"%d", this->GetNode()->GetId());
	name.insert (name.find("."), ss);
	m_filePlot << path <<"/"<< name;// <<"_"<<this->GetId();
	remove (m_filePlot.str ().c_str ());
	WriteConsumerTrace(true);
}

void
ConsumerCbr::WriteConsumerTrace(bool isInitialize)
{
	if(!isInitialize && !m_stop_tracing)
	{
		double rate = 0.0, av_rate = 0.0;
		double elapsed = Simulator::Now().GetSeconds() - m_start_period.GetSeconds();
		double total_elapsed = Simulator::Now().GetSeconds() - m_start_time.GetSeconds();
		intmax_t packets = m_total_bytes - m_old_npackets;
		rate = packets*8.0/elapsed/1000000;
		av_rate = m_total_bytes*8.0/total_elapsed/1000000;
		m_old_npackets = m_total_bytes;
		std::ofstream fPlot (m_filePlot.str ().c_str (), std::ios::out|std::ios::app);
		fPlot << Simulator::Now ().GetSeconds ()
				  <<"\tName\t"<<m_interestName.toUri()
				  <<"\tRate [Mb/s]\t" << rate
				  <<"\tAvRate\t"<<av_rate
				  <<"\tGotPackets\t"<<m_total_packets
				  <<"\tGotData\t"<<packets
				  <<"\tLastSeqN\t"<<m_seq
				  << std::endl;
		fPlot.close ();
	}
	if(!m_stop_tracing)
	{
		m_start_period = Simulator::Now();
		Simulator::Schedule (Seconds (1.0), &ConsumerCbr::WriteConsumerTrace, this, false);
		return;
	}
	else if(m_stop_tracing)
	{
		return;
	}
}

} // namespace ndn
} // namespace ns3
