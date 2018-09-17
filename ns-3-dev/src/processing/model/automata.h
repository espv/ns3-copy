//
// Created by espen on 17.09.18.
//

#ifndef NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_AUTOMATA_H
#define NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_AUTOMATA_H

#include <queue>
#include "ns3/simple-ref-count.h"

namespace ns3 {

class Automata : public SimpleRefCount <Automata> {
public:
    Automata();

    virtual ~Automata();

    std::queue <uint32_t> finiteAutomata;

    bool IsEmpty();
};

}


#endif //NS_3_EXTENDED_WITH_EXECUTION_ENVIRONMENT_AUTOMATA_H
