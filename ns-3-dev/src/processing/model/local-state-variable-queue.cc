/*
 * local-state-variable-queue.cc
 *
 *  Created on: Jul 14, 2014
 *      Author: stein
 */

#include "ns3/local-state-variable-queue.h"
#include <iostream>

namespace ns3 {

StateVariableQueue2::StateVariableQueue2() = default;

StateVariableQueue2::~StateVariableQueue2() = default;

bool StateVariableQueue2::empty() {
	return stateVariableQueue2.empty();
}

}
