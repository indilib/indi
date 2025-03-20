/*******************************************************************************
  Dust Cover Simulator

  SPDX-FileCopyrightText: 2025 Jasem Mutlaq
  SPDX-License-Identifier: LGPL-2.0-or-later
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indidustcapinterface.h"

class DustCoverSimulator : public INDI::DefaultDevice, public INDI::DustCapInterface
{
    public:
        DustCoverSimulator();
        virtual ~DustCoverSimulator() override = default;

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
            return "Dust Cover Simulator";
        }

        // From Dust Cap Interface
        virtual IPState ParkCap() override;
        virtual IPState UnParkCap() override;

    private:
        // Duration of park/unpark operations
        INDI::PropertyNumber OperationDurationNP {1};
};
