/*******************************************************************************
  Copyright(c) 2018 Franck Le Rhun. All rights reserved.
  Copyright(c) 2018 Christian Liska. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indifocuser.h"

class lacerta_mfoc : public INDI::Focuser
{
    public:
        lacerta_mfoc();

        bool initProperties() override;
        void ISGetProperties(const char *dev) override;
        bool updateProperties() override;

        const char *getDefaultName() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool Handshake() override;
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
        virtual bool SetFocuserBacklash(int32_t steps) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        //virtual bool SetBacklash(double values[], char *names[], int n);
        virtual bool SetTempComp(double values[], char *names[], int n);
        virtual uint32_t GetAbsFocuserPosition();

        // Backlash
        //        INumberVectorProperty BacklashNP;
        //        INumber BacklashN[1];

        // Temperature Compensation
        INumberVectorProperty TempCompNP;
        INumber TempCompN[1];

        enum
        {
            MODE_TDIR_BOTH,
            MODE_TDIR_IN,
            MODE_TDIR_OUT,
            MODE_COUNT_TEMP_DIR
        };
        ISwitchVectorProperty TempTrackDirSP;
        ISwitch TempTrackDirS[MODE_COUNT_TEMP_DIR];

        enum
        {
            MODE_SAVED_ON,
            MODE_SAVED_OFF,
            MODE_COUNT_SAVED
        };
        ISwitchVectorProperty StartSavedPositionSP;
        ISwitch StartSavedPositionS[MODE_COUNT_SAVED];

};
