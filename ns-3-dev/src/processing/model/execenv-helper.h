#ifndef PROCESSING_HELPER_H
#define PROCESSING_HELPER_H

#include "ns3/node-container.h"
#include "ns3/object.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "software-execution-model.h"

#include <string>

namespace ns3 {

class SEM;

class ExecEnvHelper : public Object {
 public:
  static TypeId GetTypeId();
  ExecEnvHelper();

  // Install and configure processing models on a set of nodes with a given configuration script
  void Install(Ptr<SoftwareExecutionModel> softwareExecutionModel, Ptr<Node> n);
  void Install(Ptr<SoftwareExecutionModel> softwareExecutionModel, NodeContainer nc);

 private:
  // Average overhead of one trace function execution
  uint32_t m_traceOverhead;
  // CPU frequency
  uint32_t m_cpuFreq;
  // Memory bus frequency 
  uint32_t m_memFreq;
  // Cache line size
  uint32_t m_cacheLineSize;
};

} // namespace ns3

#endif // PROCESSING_HELPER
