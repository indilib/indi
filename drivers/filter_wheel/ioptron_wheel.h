/*******************************************************************************
  Copyright(c) 2024 Joe Zhou. All rights reserved.

  iOptron Filter Wheel

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

class iEFW : public INDI::FilterWheel
{
    public:
        iEFW();
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool Handshake() override;
        int QueryFilter() override;
        bool SelectFilter(int) override;
//        void TimerHit() override;
	int getFilterPos();
        // Firmware of the iEFW
        INDI::PropertyText FirmwareTP {1};
        INDI::PropertyText WheelIDTP  {1};;

    private:

        bool getiEFWID();
	bool getiEFWInfo();
        bool getiEFWfirmwareInfo();
	void initOffset();
  // Calibration offset
        bool getOffset(int filter);
        bool setOffset(int filter, int shift);
	INDI::PropertySwitch HomeSP{1};
};
