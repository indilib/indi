/*******************************************************************************
  Copyright(c) 2026 WandererAstro. All rights reserved.

  RASA FilterCube Filter Wheel Driver

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License version 2 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indifilterwheel.h"

static constexpr int FILTERCUBE_MAX_FILTERS = 5;

class RasaFilterCube : public INDI::FilterWheel
{
    public:
        RasaFilterCube();

    protected:

        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool Handshake() override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

        int QueryFilter() override;
        bool SelectFilter(int position) override;
        bool GetFilterNames() override;
        bool SetFilterNames() override;

    private:
        bool readCurrentFilterFromStatus(int &position);
        bool sendCommand(const char *command, char *response, int responseLen, int timeoutSeconds);

        char mFirmware[32] {};
        char mModel[16] {};
        int mDeviceID {0};
        char mFilterLetters[FILTERCUBE_MAX_FILTERS] {};

        INumber DeviceIDN[1] {};
        INumberVectorProperty DeviceIDNP {};

        IText DeviceInfoT[2] {};
        ITextVectorProperty DeviceInfoTP {};
};

