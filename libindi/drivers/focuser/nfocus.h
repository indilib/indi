/*
    NFocus
    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2013 Felix Krämer (rigelsys@felix-kraemer.de)
    Copyright (C) 2006 Markus Wildi (markus.wildi@datacomm.ch)

    Thanks to Rigel Systems, especially Gene Nolan and Leon Palmer,
    for their support in writing this driver.

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

#include "indifocuser.h"

class NFocus : public INDI::Focuser
{
    public:
        NFocus();
        virtual ~NFocus() override = default;

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;

    protected:
        bool saveConfigItems(FILE *fp) override;
        bool SyncFocuser(uint32_t ticks) override;
        bool SetFocuserMaxPosition(uint32_t ticks) override;

    private:
        unsigned char CalculateSum(char *rf_cmd);
        bool SendCommand(const char *cmd, char *res);
        bool GetFocusParams();

        int updateNFTemperature(double *value);
        int updateNFInOutScalar(double *value);
        int updateNFMotorSettings(double *onTime, double *offTime, double *fastDelay);
        int moveNFInward(const double *value);
        int moveNFOutward(const double *value);
        int getNFAbsolutePosition(double *value);
        int setNFAbsolutePosition(const double *value);
        int setNFMaxPosition(double *value);
        //int syncNF(const double *value);

        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;

        INumber SettingsN[3];
        INumberVectorProperty SettingsNP;
        enum
        {
            SETTING_ON_TIME,
            SETTING_OFF_TIME,
            SETTING_MODE_DELAY,
        };

        INumber InOutScalarN[1];
        INumberVectorProperty InOutScalarNP;

        static constexpr const char * SETTINGS_TAB = "Settings";
};
