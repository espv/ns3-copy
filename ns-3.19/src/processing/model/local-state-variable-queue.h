/*
 * local-state-variable-queue.h
 *
 *  Created on: Jul 14, 2014
 *      Author: stein
 */

#ifndef LOCAL_STATE_VARIABLE_QUEUE_H_
#define LOCAL_STATE_VARIABLE_QUEUE_H_

#include <queue>
#include "ns3/simple-ref-count.h"


namespace ns3 {

class StateVariableQueue2 : public SimpleRefCount<StateVariableQueue2>{
public:
	StateVariableQueue2();
	virtual ~StateVariableQueue2();

	std::queue<uint32_t> stateVariableQueue2;
	bool empty(void);
};

}
#endif /* LOCAL_STATE_VARIABLE_QUEUE_H_ */
