#include "indibase.h"
#include "basedevice.h"

namespace INDI
{

void BaseMediator::newDevice(INDI::BaseDevice)
{ }

void BaseMediator::removeDevice(INDI::BaseDevice)
{ }

void BaseMediator::newProperty(INDI::Property)
{ }

void BaseMediator::removeProperty(INDI::Property)
{ }

void BaseMediator::newBLOB(IBLOB *)
{ }

void BaseMediator::newSwitch(INDI::PropertySwitch)
{ }

void BaseMediator::newNumber(INDI::PropertyNumber)
{ }

void BaseMediator::newText(INDI::PropertyText)
{ }

void BaseMediator::newLight(INDI::PropertyLight)
{ }

void BaseMediator::newMessage(INDI::BaseDevice, int)
{ }

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

void BaseMediator::newMessage(INDI::BaseDevice *, int)
{ }
#endif

}
