#ifndef __INET_OSPFV3INTERFACESTATEPOINTTOPOINT_H_
#define __INET_OSPFV3INTERFACESTATEPOINTTOPOINT_H_

#include <omnetpp.h>
#include <string>

#include "inet/routing/ospfv3/interface/Ospfv3InterfaceState.h"
#include "inet/common/INETDefs.h"

namespace inet{

/*
 * Interface is operational and it is connected to either a physical point-to-point interface
 * or it is a virtual link. When this state is entered, the interface is trying to form an adjacency
 * with the neighbor. Hello is sent normally every Hello interval.
 */

class INET_API Ospfv3InterfaceStatePointToPoint : public Ospfv3InterfaceState
{
  public:
    ~Ospfv3InterfaceStatePointToPoint() {};
    virtual void processEvent(Ospfv3Interface* intf, Ospfv3Interface::Ospfv3InterfaceEvent event) override;
    Ospfv3Interface::Ospfv3InterfaceFaState getState() const override { return Ospfv3Interface::INTERFACE_STATE_POINTTOPOINT; }
    std::string getInterfaceStateString() const {return std::string("Ospfv3InterfaceStatePointToPoint");};
};

}//namespace inet

#endif
