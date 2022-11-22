#include "indibase.h"
#include "basedevice.h"

namespace INDI
{

// device

void BaseMediator::newDevice(INDI::BaseDevice)
{ }

void BaseMediator::removeDevice(INDI::BaseDevice)
{ }

// property

void BaseMediator::newProperty(INDI::Property)
{ }

void BaseMediator::updateProperty(INDI::Property)
{ }

void BaseMediator::removeProperty(INDI::Property)
{ }

// message

void BaseMediator::newMessage(INDI::BaseDevice, int)
{ }

// server

void BaseMediator::serverConnected()
{ }

void BaseMediator::serverDisconnected(int)
{ }

// deprecated
#if INDI_VERSION_MAJOR < 2
void BaseMediator::newDevice(INDI::BaseDevice *)
{ }

void BaseMediator::removeDevice(INDI::BaseDevice *)
{ }

void BaseMediator::newProperty(INDI::Property *)
{ }

void BaseMediator::removeProperty(INDI::Property *)
{ }

void BaseMediator::newSwitch(ISwitchVectorProperty *)
{ }

void BaseMediator::newNumber(INumberVectorProperty *)
{ }

void BaseMediator::newText(ITextVectorProperty *)
{ }

void BaseMediator::newLight(ILightVectorProperty *)
{ }

void BaseMediator::newBLOB(IBLOB *)
{ }

void BaseMediator::newMessage(INDI::BaseDevice *, int)
{ }
#endif

}
