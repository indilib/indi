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

        // From PACInterface
        virtual IPState StartCorrection(double azError, double altError) override;
        virtual IPState AbortCorrection() override;

    private:
        INDI::PropertyNumber OperationDurationNP {1};
        INDI::PropertyNumber CorrectionGainNP {1};
};
