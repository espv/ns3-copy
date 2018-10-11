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

#ifndef CEP_ENGINE_H
#define CEP_ENGINE_H

#include "ns3/object.h"
#include "ns3/traced-callback.h"
#include "ns3/ipv4-address.h"
#include "ns3/event-impl.h"
#include "ns3/packet.h"

namespace ns3 {

    class CepEvent;
    class CepEventPattern;
    class CepOperator;
    class SerializedCepEvent;
    class SerializedQuery;
    class Producer;
    
    class Window : public Object{
    public:
        static TypeId GetTypeId (void);
        std::vector<CepEvent> buffer;
        
    };
    
     class CepEvent : public Object{
    public:
        static TypeId GetTypeId (void);
        
        CepEvent(Ptr<CepEvent>);
        CepEvent();
        void operator=(Ptr<CepEvent>);
        SerializedCepEvent* serialize();
        void deserialize(uint8_t*, uint32_t);
        uint32_t getSize();
        void CopyCepEvent (Ptr<CepEvent> e);
        
        std::string type; //the type of the event
        uint64_t m_seq;
        uint64_t delay;
        uint32_t event_class;
        uint32_t hopsCount;
        uint32_t prevHopsCount;
        Ptr<Packet> pkt;
    };
    
    class CepEventPattern : public Object{
        
    public:
        static TypeId GetTypeId (void);
        std::vector<std::string> eventTypes;
        
        std::string op;
       // std::string temporalConstraintValue;
      
    };
//    
    class Query : public Object
    {
        
    public:
        static TypeId GetTypeId (void);
        
        Query(Ptr<Query> q);
        Query();
        uint32_t id;
        uint32_t actionType;
        std::string eventType;
        bool isAtomic;
        Ipv4Address output_dest;
        Ipv4Address inputStream1_address;
        Ipv4Address inputStream2_address;
        Ipv4Address currentHost;
        std::string inevent1;
        std::string inevent2;
        std::string parent_output;
        std::string op;
        /*
         * the event notification for the event of type above is the
         * one the sink is interested in.
         */ 
        bool isFinal;
        bool assigned;
        
        SerializedQuery* serialize();
        void deserialize(uint8_t *buffer, uint32_t);
        uint32_t getSerializedSize();
         
    };
    
class CEPEngine : public Object
    {
    public:
        static TypeId GetTypeId (void);
        CEPEngine();
        void Configure();
        void ProcessCepEvent(Ptr<CepEvent> e);
        void GetOpsByInputCepEventType(std::string eventType, std::vector<Ptr<CepOperator> >& ops);
        
        /**
         * this method instantiates the query and 
         * stores it in the query pool
         * @param 
         * the query to instantiate
         */
        void RecvQuery(Ptr<Query>);
        TracedCallback< Ptr<CepEvent> > nevent;
        
        
private:
    friend class Detector;
   
    
    void ForwardProducedCepEvent(Ptr<CepEvent>);
    void InstantiateQuery(Ptr<Query> q);
    void StoreQuery(Ptr<Query> q);
    Ptr<Query> GetQuery(uint32_t id);
    std::vector<Ptr<Query> > queryPool;
    std::vector<Ptr<CepOperator> > ops_queue;
      
    };
    class Forwarder  : public Object
    {
    public:
        static TypeId GetTypeId (void);
        Forwarder();
        void Configure();
    private:
        friend class Producer;
        virtual void ForwardNewCepEvent(Ptr<CepEvent> new_event);
        TracedCallback< Ptr<CepEvent> > new_event;
        
    };
    
    class Detector  : public Object
    {
    public:
        static TypeId GetTypeId (void);
        void ProcessCepEvent(Ptr<CepEvent> e);
        void CepOperatorProcessCepEvent(Ptr<CepEvent> e, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep, Ptr<Producer> producer);
       
    };
    
    class BufferManager : public Object{
    public:
        static TypeId GetTypeId (void);
        
        void Configure(Ptr<CepOperator> op);
        void read_events(std::vector<Ptr<CepEvent> >& event1, 
        std::vector<Ptr<CepEvent> >& event2);
        void put_event(Ptr<CepEvent>);
        void clean_up();
        uint32_t consumption_policy;
        uint32_t selection_policy;
        std::vector<Ptr<CepEvent> > events1;
        std::vector<Ptr<CepEvent> > events2;
        
    private:
        friend class CepOperator;
          
    };
    
    class CepOperator: public Object {
    public:
        static TypeId GetTypeId ();
        
        virtual void Configure (Ptr<Query>, Ptr<CEPEngine>) = 0;
        virtual bool Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p) = 0;
        virtual bool ExpectingCepEvent (std::string) = 0;
        uint32_t queryId;
    };
    
    class AndOperator: public CepOperator {
    public:
        static TypeId GetTypeId ();
        
        void Configure (Ptr<Query>, Ptr<CEPEngine>);
        bool Evaluate (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p);
        bool DoEvaluate(Ptr<CepEvent> newEvent, std::vector<Ptr<CepEvent>> events, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> bufmanEvents, Ptr<Query> q, Ptr<Producer> p);
        bool ExpectingCepEvent (std::string);
        std::string event1;
        std::string event2;
        
    private:
        Ptr<BufferManager> bufman;
        
    };
    
    class OrOperator: public CepOperator {
    public:
        static TypeId GetTypeId ();
        
        void Configure (Ptr<Query>, Ptr<CEPEngine>);
        bool Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p);
        bool ExpectingCepEvent (std::string);
        std::string event1;
        std::string event2;
    
    private:
        Ptr<BufferManager> bufman;
    };

    class ThenOperator: public CepOperator {
    public:
        static TypeId GetTypeId ();

        void Configure (Ptr<Query>, Ptr<CEPEngine>);
        bool Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p);
        bool DoEvaluate(Ptr<CepEvent> newEvent, std::vector<Ptr<CepEvent>> events, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> bufmanEvents, Ptr<Query> q, Ptr<Producer> p);
        bool ExpectingCepEvent(std::string);
        std::string event1;
        std::string event2;

    private:
        Ptr<BufferManager> bufman;
    };
    
    class Producer  : public Object
    {
    public:
        static TypeId GetTypeId (void);
        void HandleNewCepEvent(Ptr<Query> q, std::vector<Ptr<CepEvent> >&);
        
    private:
        friend class Detector;
        
    };
    
}

#endif /* CEP_ENGINE_H */

