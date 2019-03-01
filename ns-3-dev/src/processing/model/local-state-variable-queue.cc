/*
 * local-state-variable-queue.cc
 *
 *  Created on: Jul 14, 2014
 *      Author: stein
 */

#include "ns3/local-state-variable-queue.h"
#include <iostream>

namespace ns3 {

StateVariableQueue::StateVariableQueue() = default;

StateVariableQueue::~StateVariableQueue() = default;

bool StateVariableQueue::empty() {
	return stateVariableQueue.empty();
}

}
