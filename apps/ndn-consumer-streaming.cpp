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
 *
 *
 * Created by Giulio Grassi on 7/22/15.
**/



/**
 *
 * In order to distinguish among first transmission and retransmission, ndn-consumer must initialize
 * m_seqRetxCounts[seq] before sending the pkt. The current version of ndn-consumer, doesn't do such thing.
 * By adding:
 *  if(m_seqRetxCounts.find(seq)==m_seqRetxCounts.end()){
 *      m_seqRetxCounts[seq]=0;
 *   }
 * before calling WillSendOutInterest in ndn-consumer, the problem is solved. 
 * Other applications however might not expect this change in the consumer behaviour, thus the change is not active
 * todo: find another way to distinguish among first transmission and retransmission
 *
 */

#include "ndn-consumer-streaming.hpp"

#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include <assert.h>

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerStreaming");

namespace ns3 {
namespace ndn {
    
NS_OBJECT_ENSURE_REGISTERED(ConsumerStreaming);

TypeId
ConsumerStreaming::GetTypeId(void)
{
    static TypeId tid =
    TypeId("ns3::ndn::ConsumerStreaming")
    .SetGroupName("Ndn")
    .SetParent<Consumer>()
    .AddConstructor<ConsumerStreaming>()
    
    .AddTraceSource ("DownloadFailure",  "DownloadFailure",  MakeTraceSourceAccessor (&ConsumerStreaming::m_onDownloadFailure))
    .AddTraceSource ("DownloadSuccess",  "DownloadSuccess",  MakeTraceSourceAccessor (&ConsumerStreaming::m_onDownloadSuccess))
    
    
    .AddAttribute("Window", "Initial size of the window", StringValue("2"),
                  MakeUintegerAccessor(&ConsumerStreaming::GetWindow, &ConsumerStreaming::SetWindow),
                  MakeUintegerChecker<uint32_t>())
    
    .AddAttribute("PayloadSize",
                  "Average size of content object size (to calculate interest generation rate)",
                  UintegerValue(1040), MakeUintegerAccessor(&ConsumerStreaming::GetPayloadSize,
                                                            &ConsumerStreaming::SetPayloadSize),
                  MakeUintegerChecker<uint32_t>())
    
    .AddAttribute("Size", "Amount of data in megabytes to request, relying on PayloadSize "
                  "parameter (alternative to MaxSeq attribute)",
                  DoubleValue(-1), // don't impose limit by default
                  MakeDoubleAccessor(&ConsumerStreaming::GetMaxSize, &ConsumerStreaming::SetMaxSize),
                  MakeDoubleChecker<double>())
    
    .AddAttribute("MaxSeq", "Maximum sequence number to request (alternative to Size attribute, "
                  "would activate only if Size is -1). "
                  "The parameter is activated only if Size negative (not set)",
                  IntegerValue(std::numeric_limits<uint32_t>::max()),
                  MakeUintegerAccessor(&ConsumerStreaming::GetSeqMax, &ConsumerStreaming::SetSeqMax),
                  MakeUintegerChecker<uint32_t>())
    
    .AddAttribute("InitialWindowOnTimeout", "Set window to initial value when timeout occurs",
                  BooleanValue(true),
                  MakeBooleanAccessor(&ConsumerStreaming::m_setInitialWindowOnTimeout),
                  MakeBooleanChecker())
    .AddAttribute("SecondsToBuffer", "How many seconds of data the consumer can download in advance",
                  UintegerValue(1),
                  MakeUintegerAccessor(&ConsumerStreaming::m_secondsOfBuffer),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("PlayBitRate", "Video data rate",
                  UintegerValue(1000000), //480p
                  MakeUintegerAccessor(&ConsumerStreaming::m_playBitRate),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("Payload", "Data packet size (bytes)",
                  UintegerValue(1040),
                  MakeUintegerAccessor(&ConsumerStreaming::m_payloadSize),
                  MakeUintegerChecker<uint32_t>())
    .AddAttribute("PathForTrace", "The path to a folder where you want to keep your trace, default .",
                  StringValue("none"), MakeStringAccessor(&ConsumerStreaming::m_filepath),
                  MakeStringChecker())
    ;
    return tid;
}

ConsumerStreaming::ConsumerStreaming()
: m_stop_tracing(false)
, m_payloadSize(1040)
, m_playBitRate(1000000)//1000000 <-> 480p //2500000 <-> 720p video
, m_secondsOfBuffer(1)
, m_inFlight(0)
, m_numberOfFailure(0)
, m_ssthresh(50)
, m_nextIncrement(0)
, m_inFlightCongestion(0)
{
    NS_LOG_FUNCTION_NOARGS();
}


void
ConsumerStreaming::StartApplication(){
    m_playPktPerSecond = (int) (m_playBitRate/8)/m_payloadSize;
    m_buffer = std::vector<uint32_t>(m_secondsOfBuffer*m_playPktPerSecond);
    for(uint32_t i=0; i<m_buffer.size(); i++){
        m_buffer[i]=std::numeric_limits<uint32_t>::max();
    }
    m_playIndex = 0;
    //m_maxSortIndex = 0;
    m_bufferFull = false;
    m_dataBufferCounter=0;
    m_offset = 0;
    //m_didItFailed = false;
  
    m_dataPkt = 0;
    m_dataPlayed = 0;
    m_dataPkOld = 0;
    m_dataPlayedOld = 0;
    m_interestSent = 0;
    m_interestSentOld = 0;
    m_interestNoRetxSent = 0;
    m_interestNoRetxSent = 0;
  
    Simulator::Schedule(Seconds(m_secondsOfBuffer), &ConsumerStreaming::Play, this);
    Consumer::StartApplication();
    m_startingTime = Simulator::Now();
    NS_LOG_INFO("Second of buffer: " << m_secondsOfBuffer << " -> pkt/sec " << m_playPktPerSecond);
  
    if(m_filepath != "none")
        SetTraceFileName(m_filepath, "ConsumerStreaming.plotme");
}


void
ConsumerStreaming::Reset(){
    //todo removing pending events and clear queue
    /*if (m_retxEvent.IsRunning()) {
        // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
        Simulator::Remove(m_retxEvent); // slower, but better for memory
    }*/
    if (m_sendEvent.IsRunning()) {
        // m_retxEvent.Cancel (); // cancel any scheduled cleanup events
        Simulator::Remove(m_sendEvent); // slower, but better for memory
    }
    m_retxSeqs.clear();
    m_seqTimeouts.clear();
    m_seqLastDelay.clear();
    m_seqFullDelay.clear();
    m_seqRetxCounts.clear();
    
    m_playPktPerSecond = (int) (m_playBitRate/8)/m_payloadSize;
    m_buffer = std::vector<uint32_t>(m_secondsOfBuffer*m_playPktPerSecond);
    for(uint32_t i=0; i<m_buffer.size(); i++){
        m_buffer[i]=std::numeric_limits<uint32_t>::max();
    }
    m_playIndex += m_buffer.size()+1000; //we won't ask data we already sent interest for
    m_offset = m_playIndex;
    m_seq = m_playIndex;
    
    //m_maxSortIndex = 0;
    m_bufferFull = false;
    m_dataBufferCounter=0;
    m_window = m_initialWindow;
    m_inFlight=0;
    //m_didItFailed = false;
    
    m_ssthresh = 50;
    m_nextIncrement = Seconds(0);
    m_inFlightCongestion = 0;
    
    Ptr<UniformRandomVariable> delay=CreateObject<UniformRandomVariable>();
    double nexRestart = delay->GetValue(1.0, 5.0);
    
    Simulator::Schedule(Seconds(nexRestart+m_secondsOfBuffer), &ConsumerStreaming::Play, this);
    //Consumer::StartApplication();
    
    Simulator::Schedule(Seconds(nexRestart), &ConsumerStreaming::ScheduleNextPacket, this); //we will start in 1 second
    m_startingTime = Seconds(nexRestart);//Simulator::Now();
    //NS_LOG_INFO("Second of buffer: " << m_secondsOfBuffer << " -> pkt/sec " << m_playPktPerSecond);
  
    /*m_dataPkt = 0;
    m_dataPlayed = 0;
    m_dataPkOld = 0;
    m_dataPlayedOld = 0;
    m_interestSent = 0;
    m_interestSentOld = 0;*/
}

void
ConsumerStreaming::Play()
{
    if(!m_active){
        return;
    }

    
    if(m_buffer[m_playIndex%m_buffer.size()] == m_playIndex){
        NS_LOG_INFO("Playing " << m_buffer[m_playIndex%m_buffer.size()]);
        // std::cout<<GetId()<< "Playing " << m_buffer[m_playIndex%m_buffer.size()] << std::endl;
        m_buffer[m_playIndex%m_buffer.size()] = std::numeric_limits<uint32_t>::max();
        m_dataPlayed++;
    } else  if (m_buffer[m_playIndex%m_buffer.size()]<m_playIndex){
        //it should'n happen. something wrong (it never happend so far)
        std::cout<<"something wrong, we have data already played in the buffer"<<std::endl;
    }
    else{
        //failure!
        NS_LOG_INFO("Download failed, we were too slow. Data pkts played till now: " << m_playIndex << " " <<m_buffer[m_playIndex%m_buffer.size()] );
        //NS_LOG_INFO("Statistics: Total bytes downloaded before stopping " << (m_playIndex+m_dataBufferCounter)*m_payloadSize);
        //NS_LOG_INFO("Statistics: Bytes/sec downloaded before stopping " << (m_playIndex+m_dataBufferCounter)*m_payloadSize/(Simulator::Now().GetSeconds() - m_startingTime.GetSeconds()));
        
        m_onDownloadFailure(GetId(),
                            (m_playIndex+m_dataBufferCounter-m_offset)*m_payloadSize,
                            (m_playIndex+m_dataBufferCounter-m_offset)*m_payloadSize/(Simulator::Now().GetSeconds() - m_startingTime.GetSeconds()));
        
        //m_didItFailed = true;
        m_numberOfFailure++;
        
        Reset();
        
        return;
    }
    m_playIndex++;
    m_dataBufferCounter--;
    
    if(m_bufferFull){
        NS_LOG_DEBUG("The buffer was full " << m_dataBufferCounter);
        Simulator::ScheduleNow(&ConsumerStreaming::ScheduleNextPacket, this);
    } else {
        NS_LOG_DEBUG("The buffer was NOT full " << m_dataBufferCounter);
    }
    m_bufferFull = false;
    
    Simulator::Schedule(Seconds(1.0/m_playPktPerSecond), &ConsumerStreaming::Play, this);
}

void
ConsumerStreaming::SetWindow(uint32_t window)
{
    m_initialWindow = window;
    m_window = m_initialWindow;
}

uint32_t
ConsumerStreaming::GetWindow() const
{
    return m_initialWindow;
}

uint32_t
ConsumerStreaming::GetPayloadSize() const
{
    return m_payloadSize;
}

void
ConsumerStreaming::SetPayloadSize(uint32_t payload)
{
    m_payloadSize = payload;
}

double
ConsumerStreaming::GetMaxSize() const
{
    if (m_seqMax == 0)
        return -1.0;
    
    return m_maxSize;
}

void
ConsumerStreaming::SetMaxSize(double size)
{
    m_maxSize = size;
    if (m_maxSize < 0) {
        m_seqMax = 0;
        return;
    }
    
    m_seqMax = floor(1.0 + m_maxSize * 1024.0 * 1024.0 / m_payloadSize);
}

uint32_t
ConsumerStreaming::GetSeqMax() const
{
    return m_seqMax;
}

void
ConsumerStreaming::SetSeqMax(uint32_t seqMax)
{
    if (m_maxSize < 0)
        m_seqMax = seqMax;
    
    // ignore otherwise
}

bool
ConsumerStreaming::CheckBufferSpace(){
    //std::cout<<GetId()<< " m_dataBufferCounter " << m_dataBufferCounter  << " m_retxSeqs.size " << m_retxSeqs.size() << " m_inFlight " << m_inFlight  << " m_buffer.size() " << m_buffer.size()<< std::endl;
    if(m_dataBufferCounter+m_retxSeqs.size()+m_inFlight+1>=m_buffer.size()){
        //buffer is full
        NS_LOG_DEBUG("Buffer full inBuffer" << m_dataBufferCounter <<
                     " Retx queue: " << m_retxSeqs.size() <<
                     " inFlight " << m_inFlight <<
                     " Total: "<< m_dataBufferCounter+m_retxSeqs.size()+m_inFlight);
        m_bufferFull = true;
    } else {
        NS_LOG_DEBUG("Buffer has still some space inBuffer" << m_dataBufferCounter <<
                     " Retx queue: " << m_retxSeqs.size() <<
                     " inFlight " << m_inFlight <<
                     " Total: "<< m_dataBufferCounter+m_retxSeqs.size()+m_inFlight);
        m_bufferFull = false;
    }
    return m_bufferFull;
}

void
ConsumerStreaming::ScheduleNextPacket()
{
    NS_LOG_FUNCTION_NOARGS();
    
    if (CheckBufferSpace() && m_retxSeqs.size()==0){
        //we have to wait
        return;
    }
    if(m_retxSeqs.size()>0){
        Consumer::SendPacket();

    }
    else if (m_window == static_cast<uint32_t>(0)) {
        Simulator::Remove(m_sendEvent);
        
        NS_LOG_DEBUG(
                     "Next event in " << (std::min<double>(0.5, m_rtt->RetransmitTimeout().ToDouble(Time::S)))
                     << " sec");
        
        m_sendEvent =
        Simulator::Schedule(Seconds(
                                    std::min<double>(0.5, m_rtt->RetransmitTimeout().ToDouble(Time::S))),
                            &ConsumerStreaming::ScheduleNextPacket, this); //not sure is needed
    }
    /*        m_sendEvent =
     Simulator::Schedule(Seconds(
     std::min<double>(0.5, m_rtt->RetransmitTimeout().ToDouble(Time::S))),
     &Consumer::SendPacket, this);
     }*/
    else if (m_inFlightCongestion/*m_inFlight*/ >= m_window && m_retxSeqs.size()==0) {
        // simply do nothing
        return;
    }
    else {
        if (m_sendEvent.IsRunning()) {
            NS_LOG_DEBUG("Removing event");
            Simulator::Remove(m_sendEvent);
        }
        Consumer::SendPacket();
        
        //        m_sendEvent = Simulator::ScheduleNow(&Consumer::SendPacket, this);
    }
}

///////////////////////////////////////////////////
//          Process incoming packets             //
///////////////////////////////////////////////////

void
ConsumerStreaming::OnData(shared_ptr<const Data> contentObject)
{
    NS_LOG_FUNCTION_NOARGS();
    m_dataPkt++;
  
    Consumer::OnData(contentObject);
  
    uint32_t seq = contentObject->getName().at(-1).toSequenceNumber();
    if(seq<m_playIndex){
        //      if (m_buffer[seq%m_buffer.size()]<seq){
        return; //data has been already received
    }
    
    if (m_buffer[seq%m_buffer.size()]!=std::numeric_limits<uint32_t>::max() && m_buffer[seq%m_buffer.size()] > seq){
        return;
    }
    
    //!=std::numeric_limits<uint32_t>::max()){
    //             std::cout<< GetId()<< " Buffer failed " << m_buffer[seq%m_buffer.size()] << " seq " << seq << "  playout index " << m_playIndex <<  "m_dataBufferCounter " << m_dataBufferCounter << " +m_retxSeqs.size() " << m_retxSeqs.size() << " m_inFlight " << m_inFlight<< std::endl;
    //        }
    if(m_buffer[seq%m_buffer.size()] < seq){
        std::cout<<"We have data but no space to store it " << seq << std::endl; //just check, it never happened
    }
    if(m_buffer[seq%m_buffer.size()]!=std::numeric_limits<uint32_t>::max()){
        NS_LOG_ERROR("the buffer should be empty in location " << seq%m_buffer.size() << " but it stores " <<m_buffer[seq%m_buffer.size()]);
        return;
    }
    NS_LOG_DEBUG("previous value " << m_buffer[seq%m_buffer.size()] << " compared against " << std::numeric_limits<uint32_t>::max());
    m_buffer[seq%m_buffer.size()] = seq;
    m_dataBufferCounter++;
    NS_LOG_INFO("Data: " << m_buffer[seq%m_buffer.size()]<< " total: " << m_dataBufferCounter << " prec " <<m_buffer[(seq-1)%m_buffer.size()]);
    
    //if we want to make the playback start again after a failure, we should do something here
    
    if(m_window<m_ssthresh){
        m_window = m_window + 1;
        if(m_window==m_ssthresh) {
            m_nextIncrement = Simulator::Now()+Seconds(0.05);//m_rtt->RetransmitTimeout();
                    }
    } else {
        if (Simulator::Now()>m_nextIncrement){
            m_window+=1;
            m_nextIncrement = Simulator::Now()+Seconds(0.05);// m_rtt->RetransmitTimeout();
        }
    }
    if (m_inFlight > static_cast<uint32_t>(0))
        m_inFlight--;
    if (m_inFlightCongestion > static_cast<uint32_t>(0))
        m_inFlightCongestion--;
    NS_LOG_DEBUG("Window: " << m_window << ", InFlight: " << m_inFlight);
    
    ScheduleNextPacket();
}

void
ConsumerStreaming::OnTimeout(uint32_t sequenceNumber)
{
    if (m_inFlight > static_cast<uint32_t>(0))
        m_inFlight--;
    if (m_inFlightCongestion > static_cast<uint32_t>(0))
        m_inFlightCongestion--;
    
    
    m_ssthresh = m_window/2;
    // m_inFlight=0;
    m_inFlightCongestion=0;
    m_window = m_initialWindow;
    /*
     if (m_setInitialWindowOnTimeout) {
     // m_window = std::max<uint32_t> (0, m_window - 1);
     //m_window = m_initialWindow;
     if(m_window>m_initialWindow)
     m_window = (uint32_t) ((m_window - m_initialWindow)/2);
     else
     m_window = m_initialWindow;
     }
     */
    NS_LOG_DEBUG("Window: " << m_window << ", InFlight: " << m_inFlight);
    Consumer::OnTimeout(sequenceNumber);
}

void
ConsumerStreaming::WillSendOutInterest(uint32_t sequenceNumber)
{
    if(m_seqRetxCounts.find(sequenceNumber)==m_seqRetxCounts.end()){
      m_seqRetxCounts[sequenceNumber]=0; //ndn-consumer doesn't initialize m_seqRetxCounts
    }
  
    m_interestSent++;
    //std::cout <<GetId()<< " sequenceNumber " << m_seqRetxCounts[sequenceNumber]<< " - " << sequenceNumber << " inflight " << m_inFlight << " buffer " <<m_dataBufferCounter <<std::endl;
    m_inFlight++;
    if(m_seqRetxCounts[sequenceNumber]==0){
        m_interestNoRetxSent++;
        m_inFlightCongestion++;
    }
    Consumer::WillSendOutInterest(sequenceNumber);
}

void
ConsumerStreaming::StopApplication()
{
    NS_LOG_INFO("StopApplication Failueres: " <<m_numberOfFailure);
    if(m_numberOfFailure==0){
        m_onDownloadSuccess(GetId(),
                            (m_playIndex+m_dataBufferCounter-m_offset)*m_payloadSize,
                            (m_playIndex+m_dataBufferCounter-m_offset)*m_payloadSize/(Simulator::Now().GetSeconds() - m_startingTime.GetSeconds()));
    }
    Consumer::StopApplication();
}
  
  
void ConsumerStreaming::SetTraceFileName(std::string path, std::string name)
{
    char ss[20];
    sprintf(ss,"%d", this->GetNode()->GetId());
    name.insert (name.find("."), ss);
    m_filePlot << path <<"/"<< name;// <<"_"<<this->GetId();
    remove (m_filePlot.str ().c_str ());
    WriteConsumerTrace(true);
}

  
void
ConsumerStreaming::WriteConsumerTrace(bool isInitialize)
{
  if (!m_active)
    return;
  if(!isInitialize && !m_stop_tracing)
  {
    //double rate = 0.0, av_rate = 0.0;
    //double elapsed = Simulator::Now().GetSeconds() - m_start_period.GetSeconds();
    //double total_elapsed = Simulator::Now().GetSeconds() - m_start_time.GetSeconds();
    
    /*intmax_t packets = m_total_bytes - m_old_npackets;
    rate = packets*8.0/elapsed/1000000;
    av_rate = m_total_bytes*8.0/total_elapsed/1000000;
    m_old_npackets = m_total_bytes;
    */
    
    double played = m_dataPlayed - m_dataPlayedOld;
    m_dataPlayedOld = m_dataPlayed;
    
    double dataReceived = m_dataPkt - m_dataPkOld;
    m_dataPkOld = m_dataPkt;
    
    double interestSent = m_interestSent - m_interestSentOld;
    m_interestSentOld = m_interestSent;
    
    double interestSentNoRetx = m_interestNoRetxSent - m_interestNoRetxSentOld;
    m_interestNoRetxSentOld = m_interestNoRetxSent;
    
    std::ofstream fPlot (m_filePlot.str ().c_str (), std::ios::out|std::ios::app);
    fPlot << Simulator::Now ().GetSeconds ()
        <<"\tName\t"<<m_interestName.toUri()
        <<"\tData [Pkt]\t" << dataReceived
        <<"\tPlayed\t"<<played
        <<"\tIntSent\t"<<interestSent
        <<"\tIntSentNoRetx\t"<<interestSentNoRetx
        << std::endl;
    fPlot.close ();
  }
  if(!m_stop_tracing)
  {
    //m_start_period = Simulator::Now();
    Simulator::Schedule (Seconds (1.0), &ConsumerStreaming::WriteConsumerTrace, this, false);
    return;
  }
  else if(m_stop_tracing)
  {
    return;
  }
}

        
}
}