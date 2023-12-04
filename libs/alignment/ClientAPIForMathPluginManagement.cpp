/*!
 * \file ClientAPIForMathPluginManagement.cpp
 *
 * \author Roger James
 * \date 13th November 2013
 *
 */

#include "ClientAPIForMathPluginManagement.h"

#include <cstring>

namespace INDI
{
namespace AlignmentSubsystem
{
// Public methods

bool ClientAPIForMathPluginManagement::EnumerateMathPlugins(MathPluginsList &AvailableMathPlugins)
{
    // Wait for driver to initialise if necessary
    WaitForDriverCompletion();

    AvailableMathPlugins.clear();

    auto pPlugins = MathPlugins->getSwitch();

    for (int i = 0; i < pPlugins->count(); i++)
        AvailableMathPlugins.emplace_back(std::string(pPlugins->at(i)->getLabel()));

    return true;
}

void ClientAPIForMathPluginManagement::Initialise(INDI::BaseClient *BaseClient)
{
    ClientAPIForMathPluginManagement::BaseClient = BaseClient;
}

void ClientAPIForMathPluginManagement::ProcessNewDevice(INDI::BaseDevice *DevicePointer)
{
    Device = DevicePointer;
}

void ClientAPIForMathPluginManagement::ProcessNewProperty(INDI::Property *PropertyPointer)
{
    bool GotOneOfMine = true;

    if (strcmp(PropertyPointer->getName(), "ALIGNMENT_SUBSYSTEM_MATH_PLUGINS") == 0)
        MathPlugins = PropertyPointer;
    else if (strcmp(PropertyPointer->getName(), "ALIGNMENT_SUBSYSTEM_MATH_PLUGIN_INITIALISE") == 0)
        PluginInitialise = PropertyPointer;
    else
        GotOneOfMine = false;

    // Tell the client when all the database properties have been set up
    if (GotOneOfMine && (nullptr != MathPlugins) && (nullptr != PluginInitialise))
    {
        // The DriverActionComplete state variable is initialised to false
        // So I need to call this to set it to true and signal anyone
        // waiting for the driver to initialise etc.
        SignalDriverCompletion();
    }
}

void ClientAPIForMathPluginManagement::ProcessNewSwitch(ISwitchVectorProperty *SwitchVectorProperty)
{
    if (strcmp(SwitchVectorProperty->name, "ALIGNMENT_SUBSYSTEM_MATH_PLUGINS") == 0)
    {
        if (IPS_BUSY != SwitchVectorProperty->s)
            SignalDriverCompletion();
    }
    else if (strcmp(SwitchVectorProperty->name, "ALIGNMENT_SUBSYSTEM_MATH_PLUGIN_INITIALISE") == 0)
    {
        if (IPS_BUSY != SwitchVectorProperty->s)
            SignalDriverCompletion();
    }
}

bool ClientAPIForMathPluginManagement::SelectMathPlugin(const std::string &MathPluginName)
{
    // Wait for driver to initialise if necessary
    WaitForDriverCompletion();

    auto pPlugins = MathPlugins->getSwitch();

    int i;
    for (i = 0; i < pPlugins->count(); i++)
    {
        if (0 == strcmp(MathPluginName.c_str(), pPlugins->at(i)->getLabel()))
            break;
    }
    if (i >= pPlugins->count())
        return false;

    pPlugins->reset();
    pPlugins->at(i)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pPlugins);
    WaitForDriverCompletion();
    if (IPS_OK != pPlugins->getState())
    {
        IDLog("SelectMathPlugin - Bad MathPlugins switch state %s\n", pPlugins->getStateAsString());
        return false;
    }
    return true;
}

bool ClientAPIForMathPluginManagement::ReInitialiseMathPlugin()
{
    // Wait for driver to initialise if necessary
    WaitForDriverCompletion();

    auto pPluginInitialise = PluginInitialise->getSwitch();

    pPluginInitialise->reset();
    pPluginInitialise->at(0)->setState(ISS_ON);
    SetDriverBusy();
    BaseClient->sendNewSwitch(pPluginInitialise);
    WaitForDriverCompletion();
    if (IPS_OK != pPluginInitialise->getState())
    {
        IDLog("ReInitialiseMathPlugin - Bad PluginInitialise switch state %s\n", pPluginInitialise->getStateAsString());
        return false;
    }
    return true;
}

// Private methods

bool ClientAPIForMathPluginManagement::SetDriverBusy()
{
    int ReturnCode = pthread_mutex_lock(&DriverActionCompleteMutex);

    if (ReturnCode != 0)
        return false;
    DriverActionComplete = false;
    IDLog("SetDriverBusy\n");
    ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
    return ReturnCode == 0;
}

bool ClientAPIForMathPluginManagement::SignalDriverCompletion()
{
    int ReturnCode = pthread_mutex_lock(&DriverActionCompleteMutex);

    if (ReturnCode != 0)
        return false;
    DriverActionComplete = true;
    ReturnCode           = pthread_cond_signal(&DriverActionCompleteCondition);
    if (ReturnCode != 0)
    {
        ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
        return false;
    }
    IDLog("SignalDriverCompletion\n");
    ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
    return ReturnCode == 0;
}

bool ClientAPIForMathPluginManagement::WaitForDriverCompletion()
{
    int ReturnCode = pthread_mutex_lock(&DriverActionCompleteMutex);

    while (!DriverActionComplete)
    {
        IDLog("WaitForDriverCompletion - Waiting\n");
        ReturnCode = pthread_cond_wait(&DriverActionCompleteCondition, &DriverActionCompleteMutex);
        IDLog("WaitForDriverCompletion - Back from wait ReturnCode = %d\n", ReturnCode);
        if (ReturnCode != 0)
        {
            ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
            return false;
        }
    }
    IDLog("WaitForDriverCompletion - Finished waiting\n");
    ReturnCode = pthread_mutex_unlock(&DriverActionCompleteMutex);
    return ReturnCode == 0;
}

} // namespace AlignmentSubsystem
} // namespace INDI
