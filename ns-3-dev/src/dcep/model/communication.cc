/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (C) 2018, Fabrice S. Bigirimana
 * Copyright (c) 2018, University of Oslo
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * 
 */

#include "communication.h"
#include "ns3/drop-tail-queue2.h"
#include "ns3/type-id.h"
#include "dcep.h"
#include "dcep-header.h"
//#include "seq-ts-header.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"
#include "cep-engine.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/names.h"
#include "ns3/uinteger.h"
#include "ns3/nstime.h"
#include "placement.h"
#include "common.h"
#include "message-types.h"
#include "ns3/socket-factory.h"
#include "ns3/ipv4-header.h"
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <ns3/tcp-header.h>
#include "ns3/abort.h"
#include "dcep-header.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(Communication);
NS_LOG_COMPONENT_DEFINE("Communication");


/* ... */
    TypeId
    Communication::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Communication")
        .SetParent<Object> ()
        .AddConstructor<Communication> ()
        .AddAttribute ("SinkAddress",
                       "The destination Address of the outbound packets",
                       Ipv4AddressValue (),
                       MakeIpv4AddressAccessor (&Communication::m_sinkAddress),
                       MakeIpv4AddressChecker ())
        
        ;
        
        return tid;
    }
 
    Communication::Communication()
   // : m_lossCounter (0)
    {
        numRetransmissions = 0;
        m_sent=0;
        m_sendQueue2 = CreateObject<DropTailQueue2> ();
        
    }
    
    
    Communication::~Communication()
    {
      //  m_lossCounter.~PacketLossCounter();
        this->m_sendQueue2->DequeueAll();
    }
    
    void
    Communication::setNode(Ptr<Node> node)
    {
        this->disnode = node;
    }
    
    void
    Communication::setPort(uint16_t cepPort)
    {
        this->m_port = cepPort;
    }
    
    void
    Communication::Configure()
    {
        Ptr<Dcep> dcep = GetObject<Dcep>();
        Ipv4AddressValue adr;
        dcep->GetAttribute("SinkAddress", adr);
        m_sinkAddress = adr.Get();
        /* setup cep server socket */
        if (m_socket == 0)
        {
          TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");  // If using UDP
          //TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");  // If using TCP
          m_socket = Socket::CreateSocket (disnode, tid);
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
                                                       m_port);
          m_socket->Bind (local);
          
        }
        
        if(this->GetAggregateIterator().HasNext())
        {
            Ptr<Ipv4> ipv4 = GetObject<Dcep>()->GetNode()->GetObject<Ipv4> (); // Get Ipv4 instance of the node
            host_address = ipv4->GetAddress (1, 0).GetLocal (); // Get Ipv4InterfaceAddress of xth interface
        }
       
        m_socket->SetRecvCallback (MakeCallback (&Communication::HandleRead, this));
    }
    
    
    void
    Communication::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_FUNCTION (this << socket);
        Ptr<Packet> packet, pcopy;
        Ptr<Dcep> dcep = GetObject<Dcep>();
        
        Address from;
        while ((packet = socket->RecvFrom (from)))
          {
            if (packet->GetSize () > 0)
              {
                //SeqTsHeader seqTs;
                DcepHeader dcepHeader;
                Ipv4Header ipv4;
                
                //packet->RemoveHeader(seqTs);
                packet->RemoveHeader(ipv4);
                packet->RemoveHeader(dcepHeader);
                
                Time delay = Simulator::Now(); //- seqTs.GetTs();
                
                if (InetSocketAddress::IsMatchingType (from))
                {
                       NS_LOG_INFO (Simulator::Now() << " At time " << Simulator::Now ().GetMilliSeconds()
                       << " ms packet of type " << dcepHeader.GetContentType()
                       << " from "
                       << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                           << " destination was "
                           << ipv4.GetDestination()
                           << " local address "
                           << this->host_address
                               << " packet size "
                               << packet->GetSize()
                           << " delay "
                           << delay.GetMilliSeconds()
                               );
                      
                       auto buffer = new uint8_t[dcepHeader.GetContentSize()];
                       packet->CopyData(buffer, dcepHeader.GetContentSize());
                       dcep->rcvRemoteMsg(buffer,(uint32_t)dcepHeader.GetContentSize(),dcepHeader.GetContentType(), (uint64_t)delay.GetMilliSeconds());
                  
                }
               }   
          }
    }

    void Communication::ScheduleSend(Ptr<Packet> p, Ipv4Address addr)
    {
        DcepHeader dcepHeader;
        p->PeekHeader(dcepHeader);
        Ipv4Header ipv4;
        ipv4.SetDestination(addr);
        ipv4.SetProtocol(123);
        p->AddHeader(ipv4);

        m_sendQueue2->Enqueue(p);

        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        //p->m_executionInfo.timestamps.emplace_back(Simulator::Now());
        ee->currentlyExecutingThread->m_executionInfo.timestamps.emplace_back(Simulator::Now());
        auto contentType = dcepHeader.GetContentType();
        if (contentType == EVENT) {
            Ptr<ExecEnv> ee = disnode->GetObject<ExecEnv>();
            ee->currentlyExecutingThread->m_executionInfo.executedByExecEnv = false;
            ee->Proceed(1, p, "send-packet", &Communication::send, this);
            ee->queues["packets-to-be-sent"]->Enqueue(p);
        } else if (contentType == QUERY) {
            send();
        } else {
            NS_FATAL_ERROR("Wrong content type of packet being sent.");
        }
    }
    
    void
    Communication::send()
    {
        if(m_sendQueue2->GetNPackets() > 0)
        {
            Ptr<Packet> p = m_sendQueue2->Dequeue();
            //NS_LOG_INFO(Simulator::Now() << " Time to process packet " << p->GetUid() << ": " << (Simulator::Now() - p->m_executionInfo.timestamps[0]).GetMicroSeconds());
            DcepHeader dcepHeader;
            Ipv4Header ipv4;
            p->RemoveHeader(ipv4);
            p->RemoveHeader(dcepHeader);
            
            Ptr<Packet> pp = p->Copy();

//            SeqTsHeader sth;
//            sth.SetSeq(m_sent);

            pp->AddHeader(dcepHeader);
            pp->AddHeader(ipv4);
//            pp->AddHeader(sth);
            bool itemSent = false;

            m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(ipv4.GetDestination()), m_port));
            if ((m_socket->Send (pp)) >= 0)
            {

                NS_LOG_INFO (Simulator::Now() << " SUCCESSFUL TX from : " << host_address
                        << " to : " << ipv4.GetDestination()
                        << " packet size "
                        << pp->GetSize());
                itemSent = true;
                m_sent++;
            }
            else
            {
              NS_LOG_INFO (Simulator::Now() << " Error " << m_socket->GetErrno());

            }

            Ptr<Packet> item = p; //m_sendQueue2->Dequeue();

            if (!itemSent) //we push it back at the rear of the queue
            {
                NS_LOG_INFO (Simulator::Now() << " COMMUNICATION: Rescheduling item!");
                m_sendQueue2->Enqueue(item);
            }

            if(m_sendQueue2->GetNPackets() != 0)//only schedule when there are more packet to send
            {
                NS_LOG_INFO (Simulator::Now() << ": SCHEDULING TRANSMISSION");
                Simulator::Schedule (Seconds(1), &Communication::send, this);
            }
        }
    }
    
    
    Ipv4Address
    Communication::GetSinkAddress()
    {
        return m_sinkAddress;
    }
    
    Ipv4Address
    Communication::GetLocalAddress() {
        return host_address;
    }
    
  
}

