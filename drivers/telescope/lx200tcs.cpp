/** \file indi_tcs.cpp
    \brief Driver for the AstroAlliance TCS GOTO telescope mount controller.
    \author Ilia Platone

    \example indi_tcs.cpp
    Driver for the AstroAlliance TCS GOTO telescope mount controller.
    See https://www.astro-alliance.com for more information
*/

#include "lx200tcs.h"
#include "indicom.h"

#include <cmath>
#include <memory>

TCSBase::TCSBase() : LX200_OnStep()
{
}

bool TCSBase::initProperties()
{
    LX200_OnStep::initProperties();
    for (auto oneProperty : *getProperties())
    {
        oneProperty.setDeviceName(getDeviceName());
    }

    TCSRAEncoderErrorNP[0].fill("TCS_RA_ENCODER_ERROR", "Error (as)", "%.0f", 1, 360*3600/(1<<24), 1, 0);
    TCSRAEncoderErrorNP.fill(getDeviceName(), "TCS_RA_ENCODER", "RA Parameters", CONFIGURATION_TAB,
                             IP_RO, 60, IPS_IDLE);
    TCSRAEncoderTrackingAssistantSP[0].fill("TCS_RA_ENCODER_ON", "Enable RA Assistant", ISS_OFF);
    TCSRAEncoderTrackingAssistantSP.fill(getDeviceName(), "TCS_RA_ENCODER_ENABLE", "Invert DE Axis", CONFIGURATION_TAB,
                          IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    TCSDEEncoderErrorNP[0].fill("TCS_DE_ENCODER_ERROR", "Error (as)", "%.0f", 1, 360*3600/(1<<24), 1, 0);
    TCSDEEncoderErrorNP.fill(getDeviceName(), "TCS_DE_ENCODER", "DE Parameters", CONFIGURATION_TAB,
                             IP_RO, 60, IPS_IDLE);
    TCSDEEncoderTrackingAssistantSP[0].fill("TCS_DE_ENCODER_ON", "Enable DE Assistant", ISS_OFF);
    TCSDEEncoderTrackingAssistantSP.fill(getDeviceName(), "TCS_DE_ENCODER_ENABLE", "Invert DE Axis", CONFIGURATION_TAB,
                          IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    return true;
}

void TCSBase::ISGetProperties(const char *dev)
{
    LX200_OnStep::ISGetProperties(dev);
    if (isConnected())
    {
        defineProperty(TCSRAEncoderErrorNP);
        defineProperty(TCSRAEncoderTrackingAssistantSP);
        defineProperty(TCSDEEncoderErrorNP);
        defineProperty(TCSDEEncoderTrackingAssistantSP);
    }
}

bool TCSBase::updateProperties()
{
    LX200_OnStep::updateProperties();

    if (isConnected())
    {
        defineProperty(TCSRAEncoderErrorNP);
        defineProperty(TCSRAEncoderTrackingAssistantSP);
        defineProperty(TCSDEEncoderErrorNP);
        defineProperty(TCSDEEncoderTrackingAssistantSP);
        TCSRAEncoderErrorNP[0].setValue(0);
        TCSRAEncoderTrackingAssistantSP[0].setState(ISS_OFF);
        TCSDEEncoderErrorNP[0].setValue(0);
        TCSDEEncoderTrackingAssistantSP[0].setState(ISS_OFF);
        TCSRAEncoderErrorNP.apply();
        TCSRAEncoderTrackingAssistantSP.apply();
        TCSDEEncoderErrorNP.apply();
        TCSDEEncoderTrackingAssistantSP.apply();
    }
    else
    {
        deleteProperty(TCSRAEncoderErrorNP.getName());
        deleteProperty(TCSRAEncoderTrackingAssistantSP.getName());
        deleteProperty(TCSDEEncoderErrorNP.getName());
        deleteProperty(TCSDEEncoderTrackingAssistantSP.getName());
    }
    return true;
}

bool TCSBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        if(!strcmp(TCSRAEncoderTrackingAssistantSP.getName(), name))
        {
            bool enable = TCSRAEncoderTrackingAssistantSP[0].getState() == ISS_ON;
            if(enable)
                sendOnStepCommand(":SX44,0");
            else
                sendOnStepCommand(":SX44,1");
        }
        if(!strcmp(TCSDEEncoderTrackingAssistantSP.getName(), name))
        {
            bool enable = TCSDEEncoderTrackingAssistantSP[0].getState() == ISS_ON;
            if(enable)
                sendOnStepCommand(":SX45,1");
            else
                sendOnStepCommand(":SX45,0");
        }
    }
    return LX200_OnStep::ISNewSwitch(dev, name, states, names, n);
}

const char *TCSBase::getDefaultName()
{
    return "LX200 TCS";
}
