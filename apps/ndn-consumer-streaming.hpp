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

#ifndef NDN_CONSUMER_STREAMING_H
#define NDN_CONSUMER_STREAMING_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-consumer.hpp"
#include "ns3/traced-value.h"
#include <vector>
#include <fstream>

namespace ns3 {
namespace ndn {

class ConsumerStreaming : public Consumer{
public:
    static TypeId
    GetTypeId();
    
    /**
     * \brief Default constructor
     */
    ConsumerStreaming();
    
    // From App
    virtual void
    OnData(shared_ptr<const Data> contentObject);
    
    virtual void
    OnTimeout(uint32_t sequenceNumber);
    
    virtual void
    WillSendOutInterest(uint32_t sequenceNumber);
  
    void
    SetTraceFileName(std::string path, std::string name);
    
protected:
    /**
     * \brief Constructs the Interest packet and sends it using a callback to the underlying NDN
     * protocol
     */
    virtual void
    ScheduleNextPacket();
    
    virtual void
    StartApplication();
    
    virtual void
    StopApplication(); ///< @brief Called at time specified by Stop
    
    virtual void
    Reset(); //restart download after a failure
  
    void
    WriteConsumerTrace(bool isInitialize);
  
private:
    virtual void
    SetWindow(uint32_t window);
    
    uint32_t
    GetWindow() const;
    
    virtual void
    SetPayloadSize(uint32_t payload);
    
    uint32_t
    GetPayloadSize() const;
    
    double
    GetMaxSize() const;
    
    void
    SetMaxSize(double size);
    
    uint32_t
    GetSeqMax() const;
    
    void
    SetSeqMax(uint32_t seqMax);
    
    void
    Play();
    
    bool
    CheckBufferSpace();
    
    
protected:
    //  Name m_prefix;
    bool m_stop_tracing;
    std::stringstream m_filePlot;
    std::string m_filepath;

  
private:
    uint32_t m_payloadSize; // expected payload size
    double m_maxSize;       // max size to request
    
    uint32_t m_playBitRate;    // How many bytes per second are consumed by playing the video
    uint32_t m_secondsOfBuffer; // Max delay for playing the stream
    uint32_t m_playPktPerSecond;
    uint32_t m_playIndex;
    uint32_t m_maxSortIndex;
    bool m_bufferFull; //if true, the buffer is full, we have to wait to download more data
    uint32_t m_dataBufferCounter; //how many pkts we have received and not yet played
    
    std::vector<uint32_t> m_buffer; //Received data waiting to be played
    
    uint32_t m_initialWindow;
    bool m_setInitialWindowOnTimeout;
    
    TracedValue<uint32_t> m_window;
    TracedValue<uint32_t> m_inFlight;
    
    TracedCallback <uint32_t, double, double>
    m_onDownloadFailure;
    
    TracedCallback <uint32_t, double, double>
    m_onDownloadSuccess;
    
    
    Time m_startingTime;
    
    //bool m_didItFailed;
    uint32_t m_numberOfFailure;
    uint32_t m_offset;
    
    uint32_t m_ssthresh;
    Time m_nextIncrement;
    uint32_t m_inFlightCongestion;
  
    //rate stats
    uint32_t m_dataPkt;
    uint32_t m_dataPlayed;
    uint32_t m_dataPkOld;
    uint32_t m_dataPlayedOld;
    uint32_t m_interestSent;
    uint32_t m_interestSentOld;
    uint32_t m_interestNoRetxSent;
    uint32_t m_interestNoRetxSentOld;
};
}
}
#endif