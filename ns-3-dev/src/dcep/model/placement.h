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

#ifndef PLACEMENT_H
#define PLACEMENT_H

#include <stdint.h>
#include "ns3/object-factory.h"
#include "ns3/object.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/traced-callback.h"

namespace ns3 {

/* ... */
  
    class Query;
    class CepEvent;
    class Ipv4Address;
    class CentralizedPlacementPolicy;
    class Detector;
    class Forwarder;
    class Dcep;
   
    class PlacementPolicy : public Object
    {
    public:
        static TypeId GetTypeId (void);
        
        virtual void configure(void)= 0;
        virtual void DoPlacement(void)= 0;
        virtual bool doAdaptation(std::string eType)= 0;
        /**
         * This function is used to determine where the event produced 
         * by query q should be sent. 
         * In a centralized CEP: all events produced by the engine should be 
         * forwarded on the local node: the sink.
         * In a decentralized CEP there a re two cases to consider:
         *  1. only datasources and the sink do the processing: in this case,
         *     all event produced are forwarded to the sink.
         *  2. we have a scenario with brokers which process unpinned operators:
         *     in this case, a more advance placement mechanism is implemented. 
         * 
         *  In a sense, this function implements whatever placement_policy one
         *  wants to apply. It therefore needs to be configured by the user.
         * the function return true if an event routing table entry has been 
         * successfully created: the processor for q has been determined.
         */
        virtual bool PlaceQuery(Ptr<Query> q)= 0;
        
        /**
         * A query is used to produce an event of a specific type when 
         * an event pattern aggregated to it matches. The event pattern captures
         * relationships between events of specific types. The following model
         * reflect what a query is:
         * ----- event type 1 ---->  | query |---> event type 3
         * ----- event type 2 ---->  |       | 
         * 
         * the query depicted here is a set of operators which can match event
         *  of types 1 and 2 and produce events of type 3. 
         * 
         * this function is used to setup an event-query mapping table which is
         * used by the cep engine to determine which operator(s) to apply to
         * incoming events.
         */
        
    protected:
        TracedCallback<std::string > newHostFound;
        TracedCallback<std::string > newLocalPlacement;
        
    };
    
    class CentralizedPlacementPolicy : public PlacementPolicy
    {
    public:
        static TypeId GetTypeId (void);
        
        virtual void configure(void);
        virtual void DoPlacement(void);
        virtual bool doAdaptation(std::string eType);
        virtual bool PlaceQuery(Ptr<Query> q);
        
    
    };
    
    /**
     * The placement component can be seen as an extension for 
     * a CEP engine which enables distribution.
     * This means that the primary goal of this component is to hide distribution
     * to the CEP engine. To achieve this, the placement component takes care of:
     *      1. operator placement assignment on network nodes, essentially building an
     *          operator network.
     *      2. event forwarding through the operator network
     * The design of the placement component applies the separation of 
     * policies and mechanisms. The aim is to allow different placement assignment policies 
     * which can be loaded at compile or runtime. This should make this component reusable across
     * different system scenarios. 
     * A placement object implements mechanisms for building the operator network and forwarding events
     * through it. And instance of a PlacementPolicy (defined above) object implements a placement 
     * assignment algorithm to determine where an operator should be placed. 
     */
    
class Placement : public Object
    {
    public:
        static TypeId GetTypeId (void);
        
        void configure();
        
        
        /*
         * All CEP events from remote nodes are received here
         * 
         */
        void RcvCepEvent(Ptr<CepEvent> e);

        /*
         * All CEP events produced by this node are forwarded from here.
         */
        void ForwardProducedCepEvent(Ptr<CepEvent> e);
        
        /* Called when the Placement Policy has determined where a 
         * given query should be sent
         */
        void ForwardQuery(Ptr<Query> q);
        void SendQueryToCepEngine (Ptr<Query> q);
        
        void RecvQuery(Ptr<Query> q);

        Ipv4Address SinkAddressForEvent(Ptr<CepEvent> e);
        
        
        
        TracedCallback< Ptr<CepEvent> > m_systemCepEvent;
        
        
    private:
        
        friend class CentralizedPlacementPolicy;
        friend class Detector;
        friend class Forwarder;
        friend class Dcep;
        friend class ResourceManager;
        /*
         * All events to be processed by remote CEP engine(s)
         * are sendt from here.
         */
        void SendCepEvent (Ptr<CepEvent> e, Ipv4Address dest);
        /*
         * events to be processed by the local CEP engine are
         * sendt from here
         */
        void SendCepEventToCepEngine (Ptr<CepEvent> e);
        /*
         * events expected by the local sink are sendt from here.
         */
        void SendCepEventToSink (Ptr<CepEvent> e);
        
        
        void ForwardRemoteQuery(std::string eType);
        uint32_t RemoveQuery(Ptr<Query> q);
        
        uint16_t deploymentModel;
        std::vector<Ptr<CepEvent> > eventsList;
        
        bool centralized_mode;
        uint16_t operator_counter;
        
        
        
        std::vector<Ptr<Query> > q_queue;//queries awaiting to be placed
        TracedCallback<> activateDatasource;
        TracedCallback<Ptr<CepEvent> > remoteCepEventReceived;
        TracedCallback<Ptr<CepEvent> > m_newCepEventProduced;
    };
    
}

#endif /* PLACEMENT_H */

