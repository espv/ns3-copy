//
// Created by espen on 26.09.2019.
//

#ifndef NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXEXECUTIONTIME_H
#define NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXEXECUTIONTIME_H

#include <cstdlib>
#include <ns3/point-to-point-module.h>
#include "ns3/dcep.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/olsr-helper.h"
#include "ns3/dcep-app-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/cep-engine.h"
#include "ns3/cep-engine.h"
#include "ns3/stats-module.h"
#include "ns3/data-collector.h"
#include "ns3/time-data-calculators.h"
#include "ns3/trex.h"
#include "ns3/execenv.h"
#include "ns3/common.h"
#include "ns3/application.h"


class SiddhiTRexThroughputSink : public Sink {
public:
    static TypeId GetTypeId (void);
    void BuildTRexQueries(Ptr<Dcep> dcep) final;
    void receiveFinalCepEvent(Ptr<CepEvent> e) final;
};

class SiddhiTRexThroughputDataSource : public DataSource {
public:
    static TypeId GetTypeId (void);
    void GenerateAtomicCepEvents(Ptr<Query> q) final;
};

class SiddhiTRexThroughputDcep : public Dcep {
public:
    static TypeId GetTypeId (void);

    void ScheduleEventsFromTrace(Ptr<Query> q) final;
    void ActivateDatasource(Ptr<Query> q) final;
    void StartApplication() final;
};

#endif //NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_SIDDHITREXEXECUTIONTIME_H
