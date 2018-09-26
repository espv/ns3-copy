/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 University of Washington
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
 */

#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "drop-tail-queue2.h"

NS_LOG_COMPONENT_DEFINE ("DropTailQueue2");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (DropTailQueue2)
  ;

TypeId DropTailQueue2::GetTypeId (void) 
{
  static TypeId tid = TypeId ("ns3::DropTailQueue2")
    .SetParent<Queue2> ()
    .AddConstructor<DropTailQueue2> ()
    .AddAttribute ("Mode", 
                   "Whether to use bytes (see MaxBytes) or packets (see MaxPackets) as the maximum queue size metric.",
                   EnumValue (QUEUE_MODE_PACKETS),
                   MakeEnumAccessor (&DropTailQueue2::SetMode),
                   MakeEnumChecker (QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                    QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets accepted by this DropTailQueue2.",
                   UintegerValue (100),
                   MakeUintegerAccessor (&DropTailQueue2::m_maxPackets),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxBytes", 
                   "The maximum number of bytes accepted by this DropTailQueue2.",
                   UintegerValue (100 * 65535),
                   MakeUintegerAccessor (&DropTailQueue2::m_maxBytes),
                   MakeUintegerChecker<uint32_t> ())
  ;

  return tid;
}

DropTailQueue2::DropTailQueue2 () :
  Queue2 (),
  m_packets (),
  m_bytesInQueue2 (0)
{
  NS_LOG_FUNCTION (this);
}

DropTailQueue2::~DropTailQueue2 ()
{
  NS_LOG_FUNCTION (this);
}

void
DropTailQueue2::SetMode (DropTailQueue2::Queue2Mode mode)
{
  NS_LOG_FUNCTION (this << mode);
  m_mode = mode;
}

DropTailQueue2::Queue2Mode
DropTailQueue2::GetMode (void)
{
  NS_LOG_FUNCTION (this);
  return m_mode;
}

bool 
DropTailQueue2::DoEnqueue (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  if (m_mode == QUEUE_MODE_PACKETS && (m_packets.size () >= m_maxPackets))
    {
      NS_LOG_LOGIC ("Queue2 full (at max packets) -- droppping pkt");
      Drop (p);
      return false;
    }

  if (m_mode == QUEUE_MODE_BYTES && (m_bytesInQueue2 + p->GetSize () >= m_maxBytes))
    {
      NS_LOG_LOGIC ("Queue2 full (packet would exceed max bytes) -- droppping pkt");
      Drop (p);
      return false;
    }

  m_bytesInQueue2 += p->GetSize ();
  m_packets.push (p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue2);

  return true;
}

Ptr<Packet>
DropTailQueue2::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (m_packets.empty ())
    {
      NS_LOG_LOGIC ("Queue2 empty");
      return 0;
    }

  Ptr<Packet> p = m_packets.front ();
  m_packets.pop ();
  m_bytesInQueue2 -= p->GetSize ();

  NS_LOG_LOGIC ("Popped " << p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue2);

  return p;
}

Ptr<Packet>
DropTailQueue2::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  if (m_packets.empty ())
    {
      NS_LOG_LOGIC ("Queue2 empty");
      return 0;
    }

  Ptr<Packet> p = m_packets.front ();

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue2);

  return p;
}

} // namespace ns3

