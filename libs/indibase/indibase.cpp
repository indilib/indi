#include "indibase.h"

namespace INDI
{

void BaseMediator::newDevice(INDI::BaseDevice *)
{ }

void BaseMediator::removeDevice(INDI::BaseDevice *)
{ }

void BaseMediator::newProperty(INDI::Property *)
{ }

void BaseMediator::removeProperty(INDI::Property *)
{ }

void BaseMediator::newBLOB(IBLOB *)
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

void BaseMediator::serverConnected()
{ }

void BaseMediator::serverDisconnected(int)
{ }

}
