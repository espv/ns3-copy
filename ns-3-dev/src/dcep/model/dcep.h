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

#ifndef DCEP_H
#define DCEP_H

#include <stdint.h>
#include <ns3/cep-engine.h>
#include "ns3/type-id.h"
#include "ns3/nstime.h"
#include "ns3/ipv4-address.h"
#include "ns3/event-id.h"
#include "ns3/application.h"
#include "ns3/traced-callback.h"
#include "resource-manager.h"
#include "ns3/execenv.h"

namespace ns3 {

    class Query;
    class CepEvent;
    class DataSource;
    class Sink;
    class Communication;
    class Placement;
/* ... */
class Dcep : public Application
    {
    public:
        Dcep();
        static TypeId GetTypeId (void);
        
        bool isGenerator();
        bool isDistributedExecution();
        uint32_t getNumCepEvents();
        uint16_t getCepEventCode();
        void SendPacket (Ptr<Packet> p, Ipv4Address addr);
        void DispatchQuery(Ptr<Query> q);

        virtual void CreateAtomicQueries();
        virtual void ActivateDatasource (Ptr<Query> q);
        virtual void ScheduleEventsFromTrace(Ptr<Query> q);
        void DispatchAtomicCepEvent (Ptr<CepEvent> e);
        void rcvRemoteMsg(uint8_t *data, uint32_t size, uint16_t msg_type, uint64_t delay);
        void SendFinalCepEventToSink(Ptr<CepEvent>);

        Ptr<Node> node;
protected:
    
        virtual void StartApplication (void);
        virtual void StopApplication (void);

        bool datasource_node;
        bool sink_node;
        bool distributed_execution;
        Ipv4Address m_sinkAddress;
        uint16_t m_cepPort; 
        uint16_t event_code;
        uint32_t events_load;
        uint32_t query_load;
        uint32_t event_interval;
        uint16_t operators_load;
        std::string placementPolicy;
        std::string routing_protocol;
        std::string trace_fn;
        std::string experiment_metadata_fn;
        
        TracedCallback<uint32_t> RxFinalCepEvent;
        TracedCallback<uint32_t> RxFinalCepEventHops;
        TracedCallback<uint64_t> RxFinalCepEventDelay;
    };
    
class Sink : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  Sink ();

  virtual ~Sink ();
 
    void BuildAndSendQuery(void);
    virtual void BuildTRexQueries(Ptr<Dcep> dcep);
    void receiveFinalCepEvent(Ptr<CepEvent> e);


protected:

  int number_received = 0;
  std::vector<Query> m_queries;
  TracedCallback<Ptr<Query> > nquery;
  
};


class DataSource : public Object
{
public:
    /**
    * \brief Get the type ID.
    * \return the object TypeId
    */
    static TypeId GetTypeId (void);

    DataSource ();

    virtual ~DataSource ();

    void Configure();
    virtual void GenerateAtomicCepEvents(Ptr<Query> q);
    bool IsActive();
    void Activate();

protected:

    std::string m_eventType;
    std::map<std::string, double> m_eventNumberValues;
    std::map<std::string, std::string> m_eventStringValues;
    uint32_t numCepEvents;
    uint32_t cepEventsInterval;
    uint32_t eventRate;
    uint32_t counter;
    uint32_t eventCode;
    std::string trace_fn;
    TracedCallback<Ptr<CepEvent>> nevent;
    bool active = false;

};

}

#endif /* DCEP_H */

