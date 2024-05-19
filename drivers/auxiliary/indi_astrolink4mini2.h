/*******************************************************************************
 Copyright(c) 2022 astrojolo.com
 .
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

#ifndef ASTROLINK4_H
#define ASTROLINK4_H

#include <string>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <memory>
#include <regex>
#include <cstring>
#include <map>
#include <sstream>

#include <defaultdevice.h>
#include <indifocuserinterface.h>
#include <indiweatherinterface.h>
#include <connectionplugins/connectionserial.h>

#define Q_DEVICE_CODE 0
#define Q_FOC1_POS 1
#define Q_FOC1_TO_GO 2
#define Q_FOC2_POS 3
#define Q_FOC2_TO_GO 4
#define Q_ITOT 5
#define Q_SENS1_PRESENT 6
#define Q_SENS1_TEMP 7
#define Q_SENS1_HUM 8
#define Q_SENS1_DEW 9
#define Q_SENS2_PRESENT 10
#define Q_SENS2_TEMP 11
#define Q_PWM1 12
#define Q_PWM2 13
#define Q_OUT1 14
#define Q_OUT2 15
#define Q_OUT3 16
#define Q_VIN 17
#define Q_VREG 18
#define Q_AH 19
#define Q_WH 20
#define Q_FOC1_COMP 21
#define Q_FOC2_COMP 22
#define Q_OVERTYPE 23
#define Q_OVERVALUE 24
#define Q_MLX_TEMP 25
#define Q_MLX_AUX 26
#define Q_SENS2_HUM 30
#define Q_SENS2_DEW 31
#define Q_SBM_PRESENT 32
#define Q_SBM 33

#define U_BUZZER 1
#define U_MANUAL 2
#define U_FOC1_CUR 3
#define U_FOC2_CUR 4
#define U_FOC1_HOLD 5
#define U_FOC2_HOLD 6
#define U_FOC1_SPEED 7
#define U_FOC2_SPEED 8
#define U_FOC1_ACC 9
#define U_FOC2_ACC 10
#define U_FOC1_MODE 11
#define U_FOC2_MODE 12
#define U_FOC1_MAX 13
#define U_FOC2_MAX 14
#define U_FOC1_REV 15
#define U_FOC2_REV 16
#define U_FOC1_STEP 17
#define U_FOC2_STEP 18
#define U_FOC1_COMPSTEPS 19
#define U_FOC2_COMPSTEPS 20
#define U_FOC_COMP_CYCLE 21
#define U_FOC1_COMPTRIGGER 22
#define U_FOC2_COMPTRIGGER 23
#define U_FOC1_COMPAUTO 24
#define U_FOC2_COMPAUTO 25
#define U_PWM_PRESC 26
#define U_OUT1_DEF 27
#define U_OUT2_DEF 28
#define U_OUT3_DEF 29
#define U_PWM1_DEF 30
#define U_PWM2_DEF 31
#define U_HUM_SENSOR 32
#define U_HUM_START 33
#define U_HUM_FULL 34
#define U_TEMP_PRESET 35
#define U_VREF 36
#define U_OVERVOLTAGE 37
#define U_OVERCURRENT 38
#define U_OVERTIME 39
#define U_COMPSENSOR 40

namespace Connection
{
    class Serial;
}

class IndiAstroLink4mini2 : public INDI::DefaultDevice, public INDI::FocuserInterface, public INDI::WeatherInterface
{

public:
    IndiAstroLink4mini2();
    virtual bool initProperties() override;
    virtual bool updateProperties() override;

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

protected:
    virtual const char *getDefaultName() override;
    virtual void TimerHit() override;
    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool sendCommand(const char *cmd, char *res);

    // Focuser Overrides
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool AbortFocuser() override;
    virtual bool ReverseFocuser(bool enabled) override;
    virtual bool SyncFocuser(uint32_t ticks) override;

    virtual bool SetFocuserBacklash(int32_t steps) override;
    virtual bool SetFocuserBacklashEnabled(bool enabled) override;
    virtual bool SetFocuserMaxPosition(uint32_t ticks) override;

    // Weather Overrides
    virtual IPState updateWeather() override
    {
        return IPS_OK;
    }

private:
    virtual bool Handshake();
    int PortFD = -1;
    Connection::Serial *serialConnection{nullptr};
    char stopChar{0xA}; // new line
    int focuserIndex {0};
    int getFindex();
    void setFindex(int index);
    bool readDevice();
    bool updateSettings(const char *getCom, const char *setCom, int index, const char *value);
    bool updateSettings(const char *getCom, const char *setCom, std::map<int, std::string> values);
    std::vector<std::string> split(const std::string &input, const std::string &regex);
    std::string doubleToStr(double val);
    std::string intToStr(double val);

    INDI::PropertySwitch FocuserSelectSP {2};
    enum
    {
        FOC_SEL_1,
        FOC_SEL_2
    };

    INDI::PropertyNumber Focuser1SettingsNP {6};
    enum
    {
        FS1_SPEED,
        FS1_CURRENT,
        FS1_HOLD,
        FS1_STEP_SIZE,
        FS1_COMPENSATION,
        FS1_COMP_THRESHOLD
    };
    INDI::PropertyNumber Focuser2SettingsNP {6};
    enum
    {
        FS2_SPEED,
        FS2_CURRENT,
        FS2_HOLD,
        FS2_STEP_SIZE,
        FS2_COMPENSATION,
        FS2_COMP_THRESHOLD
    };

    INDI::PropertySwitch Focuser1ModeSP {3};
    enum
    {
        FS1_MODE_UNI,
        FS1_MODE_MICRO_L,
        FS1_MODE_MICRO_H
    };

    INDI::PropertySwitch Focuser2ModeSP {3};
    enum
    {
        FS2_MODE_UNI,
        FS2_MODE_MICRO_L,
        FS2_MODE_MICRO_H
    };

    INDI::PropertyNumber PowerDataNP {5};
    enum
    {
        POW_VIN,
        POW_REG,
        POW_ITOT,
        POW_AH,
        POW_WH
    };

    INDI::PropertySwitch Power1SP {2};
    enum
    {
        PWR1BTN_ON,
        PWR1BTN_OFF
    };
    INDI::PropertySwitch Power2SP {2};
    enum
    {
        PWR2BTN_ON,
        PWR2BTN_OFF
    };
    INDI::PropertySwitch Power3SP {2};
    enum
    {
        PWR3BTN_ON,
        PWR3BTN_OFF
    };
    INDI::PropertyNumber PWMNP {2};
    enum
    {
        PWM1_VAL,
        PWM2_VAL
    };
    INDI::PropertySwitch PowerDefaultOnSP {3};
    enum
    {
        POW_DEF_ON1,
        POW_DEF_ON2,
        POW_DEF_ON3
    };

    static constexpr const char *POWER_TAB{"Power"};
    static constexpr const char *ENVIRONMENT_TAB{"Environment"};
    static constexpr const char *SETTINGS_TAB{"Settings"};
    static constexpr const char *FOC2_SETTINGS_TAB{"Focuser 2 Settings"};
    static constexpr const char *FOC1_SETTINGS_TAB{"Focuser 1 Settings"};
};

#endif
