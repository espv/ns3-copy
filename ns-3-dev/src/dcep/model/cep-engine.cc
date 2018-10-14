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

/*
 * Implementation of a CEP engine including models for:
 *  1. CepEvent and Query models
 *  2. Detector and Producer models to process CEP events
 *  3. A forwarder model to forward composite events produced by the CEP engine
 */

#include "cep-engine.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/log.h"
#include "ns3/config.h"
#include "placement.h"
#include "message-types.h"
#include "ns3/abort.h"
#include "ns3/placement.h"
#include "ns3/dcep.h"
#include "ns3/cep.h"
#include "ns3/processing-module.h"
#include "ns3/event-impl.h"


namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(CEPEngine);
NS_LOG_COMPONENT_DEFINE ("Detector");

/**************** CEP CORE *******************
 * *************************************************
 * ***************************************************/
/* ... */
    TypeId
    CEPEngine::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CEPEngine")
        .SetParent<Object> ()
        .AddConstructor<CEPEngine> ()
        .AddTraceSource ("CepEvent",
                       "Final event.",
                       MakeTraceSourceAccessor (&CEPEngine::nevent))
        
        ;
        
        return tid;
    }
    
    
    CEPEngine::CEPEngine()
    {
        Ptr<Forwarder> forwarder = CreateObject<Forwarder>();
        Ptr<Detector> detector = CreateObject<Detector>();
        Ptr<Producer> producer = CreateObject<Producer>();

        AggregateObject(forwarder);
        AggregateObject(detector);
        AggregateObject(producer);
        
        
    }
    
    void
    CEPEngine::Configure()
    {
        GetObject<Forwarder>()->TraceConnectWithoutContext("new event", 
                MakeCallback(&CEPEngine::ForwardProducedCepEvent, this));
    }
    
    void
    CEPEngine::ForwardProducedCepEvent(Ptr<CepEvent> e)
    {
        GetObject<Placement>()->ForwardProducedCepEvent(e);
    }
    
    void
    CEPEngine::ProcessCepEvent(Ptr<CepEvent> e)
    {
        GetObject<Detector>()->ProcessCepEvent(e);
    }
    
    
    void
    CEPEngine::GetOpsByInputCepEventType(std::string eventType, std::vector<Ptr<CepOperator>>& ops)
    {
        for(auto op : ops_queue)
        {
           // Ptr<CepEventPattern> ep = q->GetObject<CepEventPattern>();
            //Ptr<CepEventPattern> ep = q->ep;
            if(op->ExpectingCepEvent(eventType))
            {
                NS_LOG_INFO("found query expecting event type " << eventType);
                ops.push_back(op);
            }
        }
    }
    
    Ptr<Query> 
    CEPEngine::GetQuery(uint32_t id)
    {
        for(auto q : queryPool) {

            if(q->id == id)
            {
                return q;
            }
        }
        
        return NULL;
    }
    
    void
    CEPEngine::RecvQuery(Ptr<Query> q)
    {
        InstantiateQuery(q);
        StoreQuery(q);
    }
    
    void
    CEPEngine::InstantiateQuery(Ptr<Query> q){
        
        if(q->isAtomic)
        {
            /* instantiate atomic event */
            GetObject<Dcep>()->ActivateDatasource(q);
        }
        else
        {
            Ptr<CepOperator> cepOp;
            if(q->op == "and")
            {
                cepOp = CreateObject<AndOperator>();
            }
            else if(q->op == "or")
            {
                cepOp = CreateObject<OrOperator>();
            }
            else if (q->op == "then")
            {
                cepOp = CreateObject<ThenOperator>();
            }
            else
            {
                NS_ABORT_MSG ("UNKNOWN OPERATOR");
            }
            cepOp->Configure(q, this);
            this->ops_queue.push_back(cepOp);
        }

        std::cout << "Adding operator to CEPEngine" << std::endl;
        GetObject<Dcep>()->node->GetObject<ProcessCEPEngine>()->AddOperator(q->op, {q->inevent1, q->inevent2});
    }

    void
    CEPEngine::StoreQuery(Ptr<Query> q){
        queryPool.push_back(q);
    }


    /**************** DETECTOR ****************
     ***********************************************
     **********************************************/
    
    TypeId
    Detector::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Detector")
        .SetParent<Object> ()
        .AddConstructor<Detector> ()
        ;
        
        return tid;
    }


    void
    Detector::CepOperatorProcessCepEvent(Ptr<CepEvent> e, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep, Ptr<Producer> producer)
    {
        if (ops.begin() == ops.end())
            return;
        auto op = *ops.begin();

        std::vector<Ptr<CepEvent> > returned;

        Ptr<ExecEnv> ee = GetObject<Dcep>()->GetNode()->GetObject<ExecEnv>();
        ops.erase(ops.begin());

        e->pkt->m_executionInfo.executedByExecEnv = false;
        ee->Proceed(e->pkt, "handle-cepops", &Detector::CepOperatorProcessCepEvent, this, e, ops, cep, producer);
        // Not optimal that the cepops queue contains packets, because each packet might require multiple CepOperators to process it
        ee->queues["cepops"]->Enqueue(e->pkt);

        e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
        op->Evaluate(e, returned, cep->GetQuery(op->queryId), producer, ops, cep);
    }
    
    void
    Detector::ProcessCepEvent(Ptr<CepEvent> e)
    {
        auto cep = GetObject<CEPEngine>();

        std::vector<Ptr<CepOperator>> ops;
        cep->GetOpsByInputCepEventType(e->type, ops);
        Ptr<Producer> producer = GetObject<Producer>();

        auto node = GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();

        e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
        CepOperatorProcessCepEvent(e, ops, cep, producer);
    }
    
    
    TypeId
    CepOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CepOperator")
            .SetParent<Object> ()
        ;
        
        return tid;
    }
    
    TypeId
    AndOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::AndOperator")
            .SetParent<CepOperator> ()
        ;
        
        return tid;
    }

    TypeId
    ThenOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::ThenOperator")
                .SetParent<CepOperator> ()
        ;

        return tid;
    }
    
    TypeId
    OrOperator::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::OrOperator")
            .SetParent<CepOperator> ()
        ;
        
        return tid;
    }
    
    void
    AndOperator::Configure(Ptr<Query> q, Ptr<CEPEngine> cep)
    {
        this->queryId = q->id;
        this->event1 = q->inevent1;
        this->event2 = q->inevent2;
            
        Ptr<BufferManager> bufman = CreateObject<BufferManager>();
        
        bufman->consumption_policy = SELECTED_CONSUMPTION; //default
        bufman->selection_policy = SINGLE_SELECTION; //default
        bufman->Configure(this);
        this->bufman = bufman;
        AggregateObject(cep);
    }

    void
    ThenOperator::Configure(Ptr<Query> q, Ptr<CEPEngine> cep)
    {
        this->queryId = q->id;
        this->event1 = q->inevent1;
        this->event2 = q->inevent2;

        Ptr<BufferManager> bufman = CreateObject<BufferManager>();

        bufman->consumption_policy = SELECTED_CONSUMPTION; //default
        bufman->selection_policy = SINGLE_SELECTION; //default
        bufman->Configure(this);
        this->bufman = bufman;
        AggregateObject(cep);
    }
    
    void
    OrOperator::Configure(Ptr<Query> q, Ptr<CEPEngine> cep)
    {
        this->queryId = q->id;
        this->event1 = q->inevent1;
        this->event2 = q->inevent2;

        Ptr<BufferManager> bufman = CreateObject<BufferManager>();
        
        bufman->consumption_policy = SELECTED_CONSUMPTION; //default
        bufman->selection_policy = SINGLE_SELECTION; //default
        bufman->Configure(this);
        this->bufman = bufman;
        AggregateObject(cep);
    }

    bool
    AndOperator::DoEvaluate(Ptr<CepEvent> newEvent, std::vector<Ptr<CepEvent>> *events, std::vector<Ptr<CepEvent> > &returned, std::vector<Ptr<CepEvent>> *bufmanEvents, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) {
        Ptr<Node> node = GetObject<CEPEngine>()->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();

        if (events->empty()) {
            // No sequences left
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
            newEvent->pkt->m_executionInfo.executedByExecEnv = false;
            ee->Proceed(newEvent->pkt, "handle-cepops", &Detector::CepOperatorProcessCepEvent, cep->GetObject<Detector>(), newEvent, ops, cep, p);
            return false;
        }

        Ptr<CepEvent> existingEvent = *events->begin();
        events->erase(events->begin());
        if(newEvent->m_seq == existingEvent->m_seq) {
            Ptr<CepEvent> e1 = CreateObject<CepEvent>();
            Ptr<CepEvent> e2 = CreateObject<CepEvent>();
            newEvent->CopyCepEvent(e1);
            // Here we insert the incoming event into the sequence
            // Split loop into recursion.
            // Return a recursive call to some function

            existingEvent->CopyCepEvent(e2);

            for (auto it = bufmanEvents->begin(); it != bufmanEvents->end(); it++) {
                auto e = *it;
                if (e->m_seq == newEvent->m_seq) {
                    bufmanEvents->erase(it);
                    break;
                }
            }
            returned.push_back(e1);
            returned.push_back(e2);

            p->HandleNewCepEvent(q, returned);
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
            //return true;
        } else {
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
        }

        newEvent->pkt->m_executionInfo.executedByExecEnv = false;
        ee->Proceed(newEvent->pkt, "handle-and-cepop", &AndOperator::DoEvaluate, this, newEvent, events, returned, bufmanEvents, q, p, ops, cep);
        return false;
    }
    
    bool
    AndOperator::Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        auto events1 = new std::vector<Ptr<CepEvent>>();
        auto events2 = new std::vector<Ptr<CepEvent>>();
        bufman->put_event(e);//wait for event with corresponding sequence number
        bufman->read_events(*events1, *events2);

        e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpType")->value = 1;

        if (!events1->empty() && !events2->empty())
        {
            Ptr<Node> node = GetObject<CEPEngine>()->GetObject<Dcep>()->GetNode();
            auto ee = node->GetObject<ExecEnv>();
            e->pkt->m_executionInfo.executedByExecEnv = false;
            if (e->type == events1->front()->type)
            {
                delete events1;  // Not going to use events1
                ee->Proceed(e->pkt, "handle-and-cepop", &AndOperator::DoEvaluate, this, e, events2, returned, &bufman->events2, q, p, ops, cep);
                //return DoEvaluate(e, events2, returned, &bufman->events2, q, p, ops, cep);
            }
            else
            {
                delete events2;  // Not going to use events2
                ee->Proceed(e->pkt, "handle-and-cepop", &AndOperator::DoEvaluate, this, e, events1, returned, &bufman->events1, q, p, ops, cep);
                //return DoEvaluate(e, events1, returned, &bufman->events1, q, p, ops, cep);
            }
            
        } else {
            // No sequences left
            e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
            e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
        }
        return false;
    }

    bool
    ThenOperator::DoEvaluate(Ptr<CepEvent> newEvent, std::vector<Ptr<CepEvent>> *events, std::vector<Ptr<CepEvent> >& returned, std::vector<Ptr<CepEvent>> *bufmanEvents, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep) {
        Ptr<Node> node = GetObject<CEPEngine>()->GetObject<Dcep>()->GetNode();
        auto ee = node->GetObject<ExecEnv>();
        if (events->empty()) {
            // No sequences left
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
            newEvent->pkt->m_executionInfo.executedByExecEnv = false;
            ee->Proceed(newEvent->pkt, "handle-cepops", &Detector::CepOperatorProcessCepEvent, cep->GetObject<Detector>(), newEvent, ops, cep, p);
            return false;
        }

        Ptr<CepEvent> existingEvent = *events->begin();
        events->erase(events->begin());
        if(newEvent->m_seq == existingEvent->m_seq) {
            Ptr<CepEvent> e1 = CreateObject<CepEvent>();
            Ptr<CepEvent> e2 = CreateObject<CepEvent>();
            newEvent->CopyCepEvent(e1);
            // Here we insert the incoming event into the sequence
            // Split loop into recursion.
            // Return a recursive call to some function

            existingEvent->CopyCepEvent(e2);

            for (auto it = bufmanEvents->begin(); it != bufmanEvents->end(); it++) {
                auto e = *it;
                if (e->m_seq == newEvent->m_seq) {
                    bufmanEvents->erase(it);
                    break;
                }
            }
            returned.push_back(e1);
            returned.push_back(e2);

            p->HandleNewCepEvent(q, returned);
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
            //return true;
        } else {
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 0;
            newEvent->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 0;
        }

        newEvent->pkt->m_executionInfo.executedByExecEnv = false;
        ee->Proceed(newEvent->pkt, "handle-then-cepop", &ThenOperator::DoEvaluate, this, newEvent, events, returned, bufmanEvents, q, p, ops, cep);
    }

    bool
    ThenOperator::Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        auto *events1 = new std::vector<Ptr<CepEvent>>();
        auto *events2 = new std::vector<Ptr<CepEvent>>();
        bufman->put_event(e);//wait for event with corresponding sequence number
        bufman->read_events(*events1, *events2);

        e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpType")->value = 2;

        if(!events1->empty() && !events2->empty() && e->type == event2)
        {
            delete events2;  // Not going to use events2
            Ptr<Node> node = GetObject<CEPEngine>()->GetObject<Dcep>()->GetNode();
            auto ee = node->GetObject<ExecEnv>();
            e->pkt->m_executionInfo.executedByExecEnv = false;
            ee->Proceed(e->pkt, "handle-then-cepop", &ThenOperator::DoEvaluate, this, e, events1, returned, &bufman->events1, q, p, ops, cep);
            //return DoEvaluate(e, events1, returned, &bufman->events1, q, p, ops, cep);
        } else {
            // No sequences left
            e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
            e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("InsertedSequence")->value = 1;
            delete events1;  // Not going to use events1
            delete events2;  // Not going to use events2
        }
        return false;
    }
    
    bool
    OrOperator::Evaluate(Ptr<CepEvent> e, std::vector<Ptr<CepEvent> >& returned, Ptr<Query> q, Ptr<Producer> p, std::vector<Ptr<CepOperator>> ops, Ptr<CEPEngine> cep)
    {
        /* everything is a match*/
        returned.push_back(e);
        // Here we insert the incoming event into the sequence
        Ptr<Node> node = GetObject<CEPEngine>()->GetObject<Dcep>()->GetNode();
        Ptr<ExecEnv> ee = node->GetObject<ExecEnv>();
        p->HandleNewCepEvent(q, returned);
        e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpType")->value = 0;
        e->pkt->m_executionInfo.curThread->m_currentLocation->getLocalStateVariable("CepOpDoneYet")->value = 1;
        e->pkt->m_executionInfo.executedByExecEnv = false;
        ee->Proceed(e->pkt, "handle-cepops", &Detector::CepOperatorProcessCepEvent, cep->GetObject<Detector>(), e, ops, cep, p);
        return true; 
    }
    
    bool
    AndOperator::ExpectingCepEvent(std::string eType)
    {
       return event1 == eType || event2 == eType;
    }
    
    bool
    OrOperator::ExpectingCepEvent(std::string eType)
    {
        return event1 == eType || event2 == eType;
    }

    bool
    ThenOperator::ExpectingCepEvent(std::string eType)
    {
        return event1 == eType || event2 == eType;
    }
    

     
    /*********** BUFFER MANAGEMENT******************
     *****************************************************
     ************************************************************ */
    
    TypeId
    BufferManager::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::WindowManager")
        .SetParent<Object> ()
        .AddConstructor<CEPEngine> ()
        ;
        
        return tid;
    }
    
    void
    BufferManager::Configure(Ptr<CepOperator> op)
    {
        /*
         * setup the buffers with their corresponding event types
         */
        
    }
    
    void
    BufferManager::read_events(std::vector<Ptr<CepEvent> >& event1,
            std::vector<Ptr<CepEvent> >& event2)
    {
        //apply selection policy
        switch(selection_policy)
        {
            case SINGLE_SELECTION:
                event1 = events1;
                event2 = events2;
                break;
                
            default:
                NS_LOG_INFO("Not applying any selection policy");
        }
        
    }
    
    void
    BufferManager::put_event(Ptr<CepEvent> e)
    {
        if(events1.empty() && events2.empty())
        {
            events1.push_back(e);
        }
        else if ((events1.empty()) && (!events2.empty()))
        {
            if(events2.front()->type == e->type)
            {
                events2.push_back(e);
            }
            else
            {
                events1.push_back(e);
            }
        }
        else if ((!events1.empty()) && (events2.empty()))
        {
            if(events1.front()->type == e->type)
            {
                events1.push_back(e);
            }
            else
            {
                events2.push_back(e);
            }
        }
        else
        {
            NS_LOG_INFO("unknown type");
        }

    }
    
    void
    BufferManager::clean_up()
    {
        switch(consumption_policy)
        {
            case SELECTED_CONSUMPTION:
                NS_LOG_INFO("Applying consumption policy " << SELECTED_CONSUMPTION);
                events1.clear();
                events2.clear();
                break;
                
            default:
                NS_LOG_INFO("Not applying any consumption policy");
                
        }
        
    }
      
    /***************************PRODUCER **************
     * ***************************************************
     * *************************************************************/
    
    TypeId
    Producer::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Producer")
        .SetParent<Object> ()
        .AddConstructor<Producer> ()
        ;
        
        return tid;
    }
    
    void
    Producer::HandleNewCepEvent(Ptr<Query> q, std::vector<Ptr<CepEvent> > &events){
        if(q->actionType == NOTIFICATION)
        {
            
            Ptr<CepEvent> new_event = CreateObject<CepEvent>();
            uint64_t delay = 0;
            uint32_t hops = 0;
            for(auto e : events)
            {
                delay = std::max(delay, e->delay);
                hops = hops + e->hopsCount;
            }
            
            new_event->type = q->eventType;
            
            new_event->delay = delay; 
            new_event->hopsCount = hops;
            
            
            if(q->isFinal)
            {
                new_event->event_class = FINAL_EVENT;
            }
            else
            {
                new_event->event_class = COMPOSITE_EVENT;
            }
            
            new_event->m_seq = events.back()->m_seq;
            
            Ptr<Forwarder> forwarder = GetObject<Forwarder>();
            forwarder->ForwardNewCepEvent(new_event);
        }
    }

    
    
    /*************************** FORWARDER **************
     * ***************************************************
     * *************************************************************/
    
    TypeId
    Forwarder::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Forwarder")
        .SetParent<Object> ()
        .AddConstructor<Forwarder> ()
        .AddTraceSource ("new event",
                       "a new event is produced by the CEP engine.",
                       MakeTraceSourceAccessor (&Forwarder::new_event))
        ;
        
        return tid;
    }
    
    Forwarder::Forwarder()
    {}
    
    void
    Forwarder::Configure()
    {}
    
    void
    Forwarder::ForwardNewCepEvent(Ptr<CepEvent> event)
    {
        new_event(event);
    }
    
    
    TypeId
    Query::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::Query")
        .SetParent<Object> ()
        .AddConstructor<Query> ()
        ;
        
        return tid;
    }
    
    
    /*********** EVENT PATTERN IMPLEMENTATION ******************
     *****************************************************
     ************************************************************ */
    
    TypeId
    CepEventPattern::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CepEventPattern")
        .SetParent<Object> ()
        .AddConstructor<CepEventPattern> ()
        ;
        return tid;
    }
    
    
    /************** EVENT **************
     * ***********************************************
     * *************************************************/
    TypeId
    CepEvent::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::CepEvent")
        .SetParent<Object> ()
        .AddConstructor<CepEvent> ()
        ;
        
        return tid;
    }
    
    CepEvent::CepEvent(Ptr<CepEvent> e)
    {
        type = e->type;
        event_class = e->event_class;
        delay = e->delay;
        hopsCount = e->hopsCount;
        e->m_seq = m_seq;
    }
    
    CepEvent::CepEvent()
    {}
    
    void 
    CepEvent::operator=(Ptr<CepEvent> e)
    {
        this->type = e->type;
        
    }
    uint32_t
    CepEvent::getSize()
    { 
        return type.size()+sizeof(uint64_t)+sizeof(uint32_t);
    }
    
    SerializedCepEvent*
    CepEvent::serialize()
    {
        
        auto message = new SerializedCepEvent();
        message->type = this->type;
        message->event_class = this->event_class;
        message->size = sizeof(SerializedCepEvent);
        message->delay = this->delay;
        message->hopsCount = this->hopsCount;
        message->prevHopsCount = this->prevHopsCount;
        message->m_seq = this->m_seq;
        NS_LOG_INFO("serialized type " << message->type);
        
        return message;
       
    }
    
    void
    CepEvent::deserialize(uint8_t *buffer, uint32_t size)
    {
        auto message = new SerializedCepEvent;
        
        memcpy(message, buffer, size);
        this->type = message->type;
        this->delay = message->delay;
        this->m_seq = message->m_seq;
        this->hopsCount = message->hopsCount;
        this->prevHopsCount = message->prevHopsCount;
        this->event_class = message->event_class;
    }
    
    void
    CepEvent::CopyCepEvent(Ptr<CepEvent> e)
    {
        e->event_class = event_class;
        e->hopsCount = hopsCount;
        e->m_seq = m_seq;
        e->prevHopsCount = prevHopsCount;
        e->delay = delay;
    }
    
    
    
    /************** QUERY **************
     * ***********************************************
     * *************************************************/
    Query::Query()
    {
    }
    Query::Query(Ptr<Query> q)
    {
        this->actionType = q->actionType;
        this->eventType = q->eventType;
        this->id = q->id;
        this->isAtomic = q->isAtomic;
        this->isFinal = q->isFinal;
        this->output_dest = q->output_dest;
        this->assigned = q->assigned;
        this->currentHost = q->currentHost;
    }
    
    

    uint32_t 
    Query::getSerializedSize()
    {
        uint32_t size = eventType.size()+inevent1.size()+inevent2.size()+
                (sizeof(uint32_t)*3)+sizeof(bool);
        return size;
    }
    
    SerializedQuery*
    Query::serialize()
    {
        SerializedQuery * message = new SerializedQuery();
        
        
        output_dest.Serialize(message->output_dest);
        inputStream1_address.Serialize(message->inputStream1_address);
        inputStream2_address.Serialize(message->inputStream2_address);
        currentHost.Serialize(message->currentHost);
        message->actionType = this->actionType;
        message->eventType = this->eventType;
        message->q_id = this->id;
        message->isFinal = this->isFinal;
        message->isAtomic = this->isAtomic;
        message->inevent1 = this->inevent1;
        message->inevent2 = this->inevent2;
        message->op = this->op;
        message->assigned = this->assigned;
        
        message->parent_output = this->parent_output;
        message->size = sizeof(SerializedQuery);
        
        return message;
    }
    
    void
    Query::deserialize(uint8_t *buffer, uint32_t size)
    {
        NS_LOG_INFO ("1");
       SerializedQuery *message = new SerializedQuery();
        memcpy(message, buffer, size);
        NS_LOG_INFO("DESERIALIZED MESSAGE " << message->eventType);
        this->actionType = message->actionType;
        NS_LOG_INFO ("1");
        this->id = message->q_id;
        NS_LOG_INFO ("1");
        this->isFinal = message->isFinal;
        NS_LOG_INFO ("1");
        this->isAtomic = message->isAtomic;
        NS_LOG_INFO ("1");
        this->eventType = message->eventType;
        NS_LOG_INFO ("1");
        
        this->output_dest = Ipv4Address::Deserialize(message->output_dest);
        this->inputStream1_address = Ipv4Address::Deserialize(message->inputStream1_address);
        this->inputStream2_address = Ipv4Address::Deserialize(message->inputStream2_address);
        this->currentHost = Ipv4Address::Deserialize(message->currentHost);
        NS_LOG_INFO ("1");
        this->inevent1 = message->inevent1;
        this->inevent2 = message->inevent2;
        this->parent_output = message->parent_output;
        NS_LOG_INFO ("1");
        this->op = message->op;
        this->assigned = message->assigned;
        NS_LOG_INFO ("1");
    }
    
}
