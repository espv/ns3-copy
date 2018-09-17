//
// Created by espen on 17.09.18.
//

#include "automata.h"

namespace ns3 {

Automata::Automata() = default;

Automata::~Automata() = default;

bool Automata::IsEmpty() {
    return finiteAutomata.empty();
}

}
