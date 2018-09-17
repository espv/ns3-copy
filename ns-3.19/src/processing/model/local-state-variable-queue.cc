/*
 * local-state-variable-queue.cc
 *
 *  Created on: Jul 14, 2014
 *      Author: stein
 */

#include "ns3/local-state-variable-queue.h"
#include <iostream>

namespace ns3 {

StateVariableQueue2::StateVariableQueue2() {
//	std::cout << "Creating local state variable queue " << this << std::endl;
}

StateVariableQueue2::~StateVariableQueue2() {
//	std::cout << "Destroying local state variable queue " << this << std::endl;
}

bool StateVariableQueue2::empty(void) {
	return stateVariableQueue2.empty();
}

}
