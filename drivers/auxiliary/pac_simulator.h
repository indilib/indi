/*******************************************************************************
  PAC Simulator (Polar Alignment Correction)

  SPDX-FileCopyrightText: 2026 Joaquin Rodriguez
  SPDX-License-Identifier: LGPL-2.0-or-later
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indipacinterface.h"

class PACSimulator : public INDI::DefaultDevice, public INDI::PACInterface
{
    public:
        PACSimulator();
        virtual ~PACSimulator() override = default;

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        bool initProperties() override;
        bool updateProperties() override;
        bool saveConfigItems(FILE *fp) override;

        bool Connect() override
        {
            return true;
        }
        bool Disconnect() override
        {
            return true;
        }
        const char *getDefaultName() override
        {
            return "Alignment Correction Simulator";
        }

        // From PACInterface – single-axis movement
        // MoveAZ: positive = East, negative = West
        virtual IPState MoveAZ(double degrees) override;
        // MoveALT: positive = North, negative = South
        virtual IPState MoveALT(double degrees) override;

        // From PACInterface – abort, speed and reverse
        virtual bool AbortMotion() override;
        virtual bool SetPACSpeed(uint16_t speed) override;
        virtual bool ReverseAZ(bool enabled) override;
        virtual bool ReverseALT(bool enabled) override;

    private:
        INDI::PropertyNumber CorrectionGainNP {1};

        // Track how many axes are still moving so we can set the final state correctly.
        int m_MovingAxes {0};
};
