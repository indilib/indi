/*
    Giotto driver
    Copyright (C) 2023 Jasem Mutlaq

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include "indilightboxinterface.h"
#include "defaultdevice.h"
#include "../focuser/primalucacommandset.h"

class GIOTTO : public INDI::DefaultDevice, public INDI::LightBoxInterface
{
    public:
        GIOTTO();
        virtual ~GIOTTO();

        const char *getDefaultName() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

    protected:
        bool Handshake();

        bool Disconnect() override;

        // From Light Box
        virtual bool SetLightBoxBrightness(uint16_t value) override;
        virtual bool EnableLightBox(bool enable) override;

        virtual bool saveConfigItems(FILE *fp) override;


    private:
        Connection::Serial *serialConnection{ nullptr };
        int PortFD{ -1 };
        std::unique_ptr<PrimalucaLabs::GIOTTO> m_GIOTTO;
};
