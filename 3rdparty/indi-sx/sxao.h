/*
 Starlight Xpress Active Optics INDI Driver

 Copyright (c) 2012 Cloudmakers, s. r. o.
 All Rights Reserved.

 Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#pragma once

#include <defaultdevice.h>
#include <indiguiderinterface.h>

class SXAO : public INDI::DefaultDevice, INDI::GuiderInterface
{
    public:
        SXAO();
        ~SXAO() = default;

        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        bool initProperties();
        bool updateProperties();

        IPState GuideNorth(uint32_t ms);
        IPState GuideSouth(uint32_t ms);
        IPState GuideEast(uint32_t ms);
        IPState GuideWest(uint32_t ms);

        bool AONorth(int steps);
        bool AOSouth(int steps);
        bool AOEast(int steps);
        bool AOWest(int steps);

        bool AOCenter();
        bool AOUnjam();

        void CheckLimit(bool force);

        const char *getDefaultName();

    private:
        int aoCommand(const char *request, char *response, int nbytes);
        bool Handshake();

        INumber AONS[2];
        INumberVectorProperty AONSNP;

        INumber AOWE[2];
        INumberVectorProperty AOWENP;

        ISwitch Center[2];
        ISwitchVectorProperty CenterP;

        IText FWT[1] {};
        ITextVectorProperty FWTP;

        ILight AtLimitL[4];
        ILightVectorProperty AtLimitLP;

        char lastLimit = -1;

        Connection::Serial *serialConnection { nullptr };

        int PortFD { -1 };

        static constexpr const char *GUIDE_CONTROL_TAB  = "Guider Control";
};
