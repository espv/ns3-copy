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
#include "ns3/nstime.h"
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace ns3 {

    class CepEvent;
    class CepEventPattern;
    class CepOperator;
    class SerializedCepEvent;
    class SerializedQuery;
    class Producer;
    class CEPEngine;
    class FogApplicationComponent;
    class Window;
    class CepQueryComponent;


    class FogEvent : public Object
    {
    public:
        static TypeId GetTypeId (void);
    };
    
    class CepEvent : public FogEvent
    {
    public:
        static TypeId GetTypeId (void);
        
        CepEvent(std::vector<Ptr<CepEvent> >);
        CepEvent(Ptr<CepEvent>);
        CepEvent();
        void operator=(Ptr<CepEvent>);
        SerializedCepEvent* serialize();
        void deserialize(uint8_t*, uint32_t);
        uint32_t getSize();
        void CopyCepEvent (Ptr<CepEvent> e);
        int GetStreamId();
        
        std::string type; //the type of the event
        uint64_t m_seq;
        uint64_t delay;
        uint32_t event_class;
        uint32_t event_base;
        uint32_t hopsCount;
        uint32_t prevHopsCount;
        Time timestamp;
        Ptr<Packet> pkt;
        std::map<std::string, double> numberValues;
        std::map<std::string, std::string> stringValues;

        // Complex event only start
        CepOperator *generatedByOp;
        std::vector<Ptr<CepEvent> > prevEvents;
        // Complex event only end

        std::vector<Ptr<CepEvent> > internalEvents;

        // Added by Espen to exempt certain generated complex events from being processed,
        // to be able to chain together queries
        bool skipProcessing;
    };
    
    class CepEventPattern : public Object{
        
    public:
        static TypeId GetTypeId (void);
        std::vector<std::string> eventTypes;
        
        std::string op;
       // std::string temporalConstraintValue;
      
    };

    enum ConstraintType {
        INVALIDCONSTRAINT,
        NUMBEREQCONSTRAINT,
        NUMBERINEQCONSTRAINT,
        NUMBERLTCONSTRAINT,
        NUMBERLTECONSTRAINT,
        NUMBERGTCONSTRAINT,
        NUMBERGTECONSTRAINT,
        JOINNUMBEREQCONSTRAINT,
        JOINNUMBERINEQCONSTRAINT,
        JOINNUMBERLTCONSTRAINT,
        JOINNUMBERLTECONSTRAINT,
        JOINNUMBERGTCONSTRAINT,
        JOINNUMBERGTECONSTRAINT,
        STRINGEQCONSTRAINT,
        STRINGINEQCONSTRAINT,
        JOINSTRINGEQCONSTRAINT,
        JOINSTRINGINEQCONSTRAINT
    };

    class Constraint : public Object {
    public:
        ConstraintType type;
        Ptr<CEPEngine> cepEngine;
        std::string var_name;
        double numberValue;
        std::string stringValue;
        int event_stream1;
        int event_stream2;
        bool Evaluate(Ptr<CepEvent> e);
        bool Evaluate(Ptr<CepEvent> e1, Ptr<CepEvent> e2);
        void SetType(json constraint);
    };

    class FogService : public Object
    {
    public:
        static TypeId GetTypeId ();

        /*
         * Builds a DAG of ApplicationComponent objects
         * @return Pointer to the last ApplicationComponent in the DAG
         */
        //virtual Ptr<FogApplicationComponent> buildComponentDAG();
    };

    class Query : public FogService
    {
        
    public:
        static TypeId GetTypeId (void);
        
        Query(Ptr<Query> q);
        Query();
        uint32_t id;
        uint32_t query_base_id;
        uint32_t actionType;
        std::string eventType;
        bool isAtomic;
        bool toBeProcessed;
        Ipv4Address output_dest;
        Ipv4Address inputStream1_address;
        Ipv4Address inputStream2_address;
        Ipv4Address currentHost;
        std::string inevent1;
        std::string inevent2;
        std::string parent_output;
        std::string op;
        std::vector<Ptr<Constraint> > constraints;
        Time window;
        Ptr<Query> nextQuery;
        Ptr<Query> prevQuery;
        json json_query;
        /*
         * the event notification for the event of type above is the
         * one the sink is interested in.
         */ 
        bool isFinal;
        bool isFinalWithinNode;
        bool assigned;
        
        SerializedQuery* serialize();
        void deserialize(uint8_t *buffer, uint32_t);
        uint32_t getSerializedSize();
        Ptr<CepQueryComponent> buildComponentDAG();
         
    };

    class FogApplicationEngine : public Object
    {
    public:
        static TypeId GetTypeId ();
    };

    class CEPEngine : public FogApplicationEngine
        {
        public:
            static TypeId GetTypeId (void);
            CEPEngine();
            void Configure();
            void PacketThreadRecvPacket(Ptr<CepEvent> e);
            void ProcessCepEvent(Ptr<CepEvent> e);
            void GetOpsByInputCepEventType(std::string eventType, std::vector<Ptr<CepOperator> >& ops);
            void CheckConstraints(Ptr<CepEvent> e);
            void DoCheckNumberConstraints(Ptr<CepEvent> e, std::map<std::string, Ptr<Constraint>> constraints, Ptr<CEPEngine> cep, Ptr<Producer> producer, std::map<std::string, double> values, std::map<std::string, std::string> stringValues);
            void DoCheckStringConstraints(Ptr<CepEvent> e, std::map<std::string, Ptr<Constraint>> constraints, Ptr<CEPEngine> cep, Ptr<Producer> producer, std::map<std::string, std::string> values);
            void FinishedProcessingEvent(Ptr<CepEvent> e);
            void ClearQueries();
            void ClearQueryComponents();

            Ptr<CepOperator> GetOperator(uint32_t queryId);
            Ptr<Query> GetQuery(uint32_t id);
            std::set<Ptr<CepQueryComponent> > GetCepQueryComponents(int stream_id);

            /**
             * this method instantiates the query and
             * stores it in the query pool
             * @param
             * the query to instantiate
             */
            void RecvQuery(Ptr<Query>);
            void RecvQueryComponent(Ptr<CepQueryComponent>);
            TracedCallback< Ptr<CepEvent> > nevent;


    private:
        friend class Detector;


        void ForwardProducedCepEvent(Ptr<CepEvent>);
        void InstantiateQuery(Ptr<Query> q);
        void InstantiateQueryComponent(Ptr<CepQueryComponent> queryComponent);
        void StoreQuery(Ptr<Query> q);
        void StoreQueryComponent(Ptr<CepQueryComponent> queryComponent);
        std::vector<Ptr<Query> > queryPool;
        std::vector<Ptr<CepQueryComponent> > queryComponentPool;
        std::vector<Ptr<CepOperator> > ops_queue;
        std::map<int, std::set<Ptr<CepQueryComponent> > > streamToQueryComponents;

        };
        class Forwarder  : public Object
        {
        public:
            static TypeId GetTypeId (void);
            Forwarder();
            void Configure();
            virtual void ForwardNewCepEvent(Ptr<CepEvent> new_event);
        private:
            friend class Producer;
            TracedCallback< Ptr<CepEvent> > new_event;

        };

        class Detector  : public Object
        {
        public:
            static TypeId GetTypeId (void);
            void ProcessCepEvent(Ptr<CepEvent> e);
            void CepOperatorProcessCepEvent(Ptr<CepEvent> e, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep, Ptr<Producer> producer);
            void CepQueryComponentProcessCepEvent(Ptr<CepEvent> e, Ptr<CEPEngine> cep);

        };

        class BufferManager : public Object{
        public:
            static TypeId GetTypeId (void);

            void Configure(Ptr<CepOperator> op);
            void read_events(std::vector<Ptr<CepEvent> >& event1,
            std::vector<Ptr<CepEvent> >& event2);
            void put_event(Ptr<CepEvent>, CepOperator *op);
            void consume(std::vector<Ptr<CepEvent> > &events);
            uint32_t consumption_policy;
            uint32_t selection_policy;
            std::vector<Ptr<CepEvent> > events1;
            std::vector<Ptr<CepEvent> > events2;

        private:
            friend class CepOperator;

        };

        class FogApplicationComponent : public Object
        {
        public:
            static TypeId GetTypeId ();
        private:
            Ptr<FogApplicationComponent> next;
        };

        class CepOperator: public Object {
        public:
            static TypeId GetTypeId ();

            virtual void Configure (Ptr<Query>, Ptr<CEPEngine>) = 0;
            virtual bool Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) = 0;
            virtual std::vector<Ptr<CepEvent> > Evaluate2(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<CEPEngine> cep) = 0;
            virtual void GetPartialResults(std::vector<Ptr<CepEvent> > &ret) = 0;
            void InsertCepEventIntoWindows(Ptr<CepEvent> e);
            bool ExpectingCepEvent (std::string);
            void Consume(std::vector<Ptr<CepEvent> > &events);
            uint32_t queryId;
            Ptr<CEPEngine> cepEngine;
            std::string event1;
            std::string event2;
            std::vector<Ptr<Constraint> > constraints;
            Ptr<CepOperator> prevOperator;
            Ptr<CepOperator> nextOperator;
            std::vector<Ptr<Window> > windows;
            // Maps event streams to windows; we require that events are in all the windows to move on
            std::map<int, std::vector<Ptr<Window> > > stream_windows;
            int stream_id;

        protected:
            Ptr<BufferManager> bufman;
        };

        class AtomicOperator: public CepOperator {
        public:
            static TypeId GetTypeId ();
            void Configure (Ptr<Query>, Ptr<CEPEngine>) override;
            bool Evaluate (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) override;
            std::vector<Ptr<CepEvent> > Evaluate2(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<CEPEngine> cep) override;
            void GetPartialResults(std::vector<Ptr<CepEvent> > &ret) override;
        };

        class JoinOperator: public CepOperator {
        public:
            static TypeId GetTypeId ();
            void Configure (Ptr<Query>, Ptr<CEPEngine>) override;
            bool Evaluate (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) override;
            std::vector<Ptr<CepEvent> > Evaluate2(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<CEPEngine> cep) override;
            void InsertAtomicOperator(Ptr<AtomicOperator> a);
            std::vector<Ptr<AtomicOperator> > GetAtomicOperators();
            void GetPartialResults(std::vector<Ptr<CepEvent> > &ret) override;
        private:
            std::vector<Ptr<AtomicOperator> > atomicOperators;
        };

        class AndOperator: public JoinOperator {
        public:
            static TypeId GetTypeId ();
            void Configure (Ptr<Query>, Ptr<CEPEngine>) override;
            bool Evaluate (Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) override;
            std::vector<Ptr<CepEvent> > Evaluate2(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<CEPEngine> cep) override;
            void GetPartialResults(std::vector<Ptr<CepEvent> > &ret) override;

            bool DoEvaluate(Ptr<CepEvent> newEvent2, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> *events1, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep);

        };

        class OrOperator: public JoinOperator {
        public:
            static TypeId GetTypeId ();
            void Configure (Ptr<Query>, Ptr<CEPEngine>) override;
            bool Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) override;
            std::vector<Ptr<CepEvent> > Evaluate2(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<CEPEngine> cep) override;
            void GetPartialResults(std::vector<Ptr<CepEvent> > &ret) override;
        };

        class ThenOperator: public CepOperator {
        public:
            static TypeId GetTypeId ();
            void Configure (Ptr<Query>, Ptr<CEPEngine>) override;
            bool Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) override;
            void GetPartialResults(std::vector<Ptr<CepEvent> > &ret) override;

            bool DoEvaluate(Ptr<CepEvent> newEvent, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> *bufmanEvents, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep);

            std::vector<Ptr<CepEvent> > Evaluate2(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >&, Ptr<CEPEngine> cep) override;
        };

        class CepQueryComponent : public FogApplicationComponent {
        public:
            static TypeId GetTypeId ();
            CepQueryComponent();
            void InsertThenOperator(Ptr<ThenOperator>);
            void SetFirstOperator(Ptr<CepOperator>);
            void SetLastOperator(Ptr<CepOperator>);
            std::vector<Ptr<ThenOperator> > GetThenOperators();
            Ptr<CepOperator> GetFirstOperator();
            Ptr<CepOperator> GetLastOperator();
            std::string GetEventType();
            void SetEventType(std::string);
            int GetId();

        private:
            int id;
            int lastLocalOperatorIndex;
            std::string eventType;;
            Ptr<CepOperator> firstOperator;
            Ptr<CepOperator> lastOperator;
            std::vector<Ptr<ThenOperator> > thenOperators;
        };

        class Producer  : public Object
        {
        public:
            static TypeId GetTypeId (void);
            void HandleNewCepEvent(Ptr<Query> q, std::vector<Ptr<CepEvent> >&, CepOperator *op);
            void HandleNewCepEvent2(Ptr<CepQueryComponent> cqc, std::vector<Ptr<CepEvent> > &events);
            void AddAttributesToNewEvent(Ptr<Query> q, std::vector<Ptr<CepEvent> > &events, Ptr<CepEvent> complex_event, CepOperator *op, int index);

        private:
            friend class Detector;

        };

        class Window : public Object {
        public:
            static TypeId GetTypeId(void);
            Window() = default;

            virtual void InsertEvent(Ptr<CepEvent> e) = 0;
            void RemoveEvent(Ptr<CepEvent> e);

            virtual std::vector<std::pair<Time, Ptr<CepEvent> > > GetCepEvents();

        protected:
            std::vector <std::pair<Time, Ptr<CepEvent> > > buffer;
        };

        class TimeWindow : public Window {
        public:
            static TypeId GetTypeId(void);
            TimeWindow() = default;
            TimeWindow(Time t) {
                this->size = t;
            }

        protected:
            Time size;
        };

        class TupleWindow : public Window {
        public:
            static TypeId GetTypeId(void);
            TupleWindow() = default;
            TupleWindow(int numberTuples) {
                this->size = numberTuples;
            }

        protected:
            int size;
        };

        class SlidingTimeWindow : public TimeWindow {
        public:
            static TypeId GetTypeId(void);
            SlidingTimeWindow(Time t);
            void InsertEvent(Ptr<CepEvent> e) final;
            std::vector<std::pair<Time, Ptr<CepEvent> > > GetCepEvents() final;
            void UpdateWindow();
        };

        class SlidingTupleWindow : public TupleWindow {
        public:
            static TypeId GetTypeId(void);
            SlidingTupleWindow(int numberTuples);
            void InsertEvent(Ptr<CepEvent> e) final;
        };

        class TumblingTimeWindow : public TimeWindow {
        public:
            static TypeId GetTypeId(void);
            TumblingTimeWindow(Time t);
            void InsertEvent(Ptr<CepEvent> e) final;
            std::vector<std::pair<Time, Ptr<CepEvent> > > GetCepEvents() final;
            void UpdateWindow();
        private:
            Time lastTumble;
        };

        class TumblingTupleWindow : public TupleWindow {
        public:
            static TypeId GetTypeId(void);
            TumblingTupleWindow(int numberTuples);
            void InsertEvent(Ptr<CepEvent> e) final;
        };
    
}

#endif /* CEP_ENGINE_H */

