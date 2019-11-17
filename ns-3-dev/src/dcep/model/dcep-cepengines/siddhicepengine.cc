//
// Created by espen on 17.11.2019.
//

#include <ns3/cep-engine.h>
#include "siddhicepengine.h"
#include "ns3/type-id.h"

namespace ns3 {

    TypeId
    SiddhiCepEngine::GetTypeId() {
        static TypeId tid = TypeId("ns3::TRexCepEngine")
                .SetParent<CEPEngine>()
                .AddConstructor<SiddhiCepEngine>();

        return tid;
    }

    SiddhiCepEngine::SiddhiCepEngine() = default;

}