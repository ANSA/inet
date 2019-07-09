#ifndef __INET_OSPFV3NEIGHBORSTATEATTEMPT_H_
#define __INET_OSPFV3NEIGHBORSTATEATTEMPT_H_

#include "inet/routing/ospfv3/neighbor/Ospfv3Neighbor.h"
#include "inet/routing/ospfv3/neighbor/Ospfv3NeighborState.h"
#include "inet/common/INETDefs.h"

namespace inet{

class INET_API Ospfv3NeighborStateAttempt : public Ospfv3NeighborState
{
    /*
     * Only for NBMA networks. No information received from neighbor but more concerted effort
     * should be made to establish connection. This is done by sending hello in hello intervals.
     */
  public:
    void processEvent(Ospfv3Neighbor* neighbor, Ospfv3Neighbor::Ospfv3NeighborEventType event) override;
    virtual Ospfv3Neighbor::Ospfv3NeighborStateType getState() const override { return Ospfv3Neighbor::ATTEMPT_STATE; }
    std::string getNeighborStateString(){return std::string("Ospfv3NeighborStateAttempt");};
    ~Ospfv3NeighborStateAttempt(){};

};

}//namespace inet
#endif
