#include "sem.h"
#include "program.h"

namespace ns3 {

SEM::SEM()
  : peu(nullptr), rootProgram(nullptr), lc(nullptr), numExec(0), numStmtExec(0), cpuProcessing(0), peuProcessing(0), blocked(0)
{
	trigger = "";
}

}
