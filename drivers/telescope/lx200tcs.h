/*
   INDI Developers Manual
   Tutorial #2

   "Simple Telescope Driver"

   We develop a simple telescope simulator.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file indi_ahp_gt.h
    \brief Driver for the TCS Telescope motor controllers.
    \author Ilia Platone

    AHP TCS Telescope stepper motor GOTO controllers.
*/

#pragma once

#include "lx200_OnStep.h"
#include <connectionplugins/connectionserial.h>
#include <connectionplugins/connectiontcp.h>

using namespace INDI;

class TCSBase : public LX200_OnStep
{
    public:
        TCSBase();
        bool initProperties() override;
        bool updateProperties() override;
        const char *getDefaultName() override;
        void ISGetProperties(const char *dev) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    private:

        // Telescope & guider aperture and focal length

        PropertyNumber TCSRAEncoderErrorNP{1};
        PropertySwitch TCSRAEncoderTrackingAssistantSP{1};

        PropertyNumber TCSDEEncoderErrorNP{1};
        PropertySwitch TCSDEEncoderTrackingAssistantSP{1};

        int progress { 0 };
        int write_finished { 1 };
        const char *CONFIGURATION_TAB = "Main Control";
};
