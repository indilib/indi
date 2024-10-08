/*******************************************************************************
 Copyright(c) 2019 astrojolo.com
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

#define Q_STEPPER_POS		1
#define Q_STEPS_TO_GO		2
#define Q_CURRENT			3
#define Q_SENS1_TYPE		4
#define Q_SENS1_TEMP		5
#define Q_SENS1_HUM			6
#define Q_SENS1_DEW			7
#define Q_SENS2_TYPE		8
#define Q_SENS2_TEMP		9
#define Q_PWM1				10
#define Q_PWM2				11
#define Q_OUT1				12
#define Q_OUT2				13
#define Q_OUT3				14
#define Q_VIN				15
#define Q_VREG				16
#define Q_AH				17
#define Q_WH				18
#define Q_DC_MOVE			19
#define Q_COMP_DIFF			20
#define Q_OP_FLAG			21
#define Q_OP_VALUE			22

#define U_MAX_POS			1
#define U_SPEED				2
#define U_PWMSTOP			3
#define U_PWMRUN			4
#define U_ACC				5
#define U_REVERSED			6
#define U_STEPPER_MODE		7
#define U_COMPSENS			8
#define U_STEPSIZE			9
#define U_PWMPRESC			10
#define U_STEPPRESC			11
#define U_BUZ_ENABLED		12
#define U_HUM_SENS			13
#define U_DC_REVERSED		14
#define U_OUT1_DEF			15
#define U_OUT2_DEF			16
#define U_OUT3_DEF			17

#define E_COMP_CYCLE		1
#define E_COMP_STEPS		2
#define E_COMP_SENSR		3
#define E_COMP_AUTO			4
#define E_COMP_TRGR			5

#define N_AREF_COEFF		1
#define N_OVER_VOLT			2
#define N_OVER_AMP			3
#define N_OVER_TIME			4

namespace Connection
{
class Serial;
}

class IndiAstrolink4 : public INDI::DefaultDevice, public INDI::FocuserInterface, public INDI::WeatherInterface
{

public:
    IndiAstrolink4();
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
	
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n) override;
	
protected:
    virtual const char *getDefaultName() override;
    virtual void TimerHit() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual bool sendCommand(const char * cmd, char * res);

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
    Connection::Serial *serialConnection { nullptr };
    bool updateSettings(const char * getCom, const char * setCom, int index, const char * value);
    bool updateSettings(const char * getCom, const char * setCom, std::map<int, std::string> values);
    std::vector<std::string> split(const std::string &input, const std::string &regex);
    std::string doubleToStr(double val);
    bool sensorRead();
    bool setAutoPWM();
    int32_t calculateBacklash(uint32_t targetTicks);
    char stopChar { 0xA };	// new line
    bool backlashEnabled = false;
    int32_t backlashSteps = 0;
    bool requireBacklashReturn = false;
    
    IText PowerControlsLabelsT[3];
    ITextVectorProperty PowerControlsLabelsTP;

    ISwitch Power1S[2];
    ISwitchVectorProperty Power1SP;
    ISwitch Power2S[2];
    ISwitchVectorProperty Power2SP;
    ISwitch Power3S[2];
    ISwitchVectorProperty Power3SP;

    INumber Sensor2N[1];
    INumberVectorProperty Sensor2NP;

    INumber PWMN[2];
    INumberVectorProperty PWMNP;

    ISwitch AutoPWMS[2];
    ISwitchVectorProperty AutoPWMSP;
    
    INumber PowerDataN[5];
    INumberVectorProperty PowerDataNP;
    enum
    {
    POW_VIN, POW_VREG, POW_ITOT, POW_AH, POW_WH
    };

    INumber FocusPosMMN[1];
    INumberVectorProperty FocusPosMMNP;

    INumber CompensationValueN[1];
    INumberVectorProperty CompensationValueNP;
    ISwitch CompensateNowS[1];
    ISwitchVectorProperty CompensateNowSP;

    INumber FocuserSettingsN[4];
    INumberVectorProperty FocuserSettingsNP;
    enum
    {
        FS_SPEED, FS_STEP_SIZE, FS_COMPENSATION, FS_COMP_THRESHOLD
    };
    ISwitch FocuserModeS[3];
    ISwitchVectorProperty FocuserModeSP;
    enum
    {
      FS_MODE_UNI, FS_MODE_BI, FS_MODE_MICRO
    };

    ISwitch FocuserCompModeS[2];
    ISwitchVectorProperty FocuserCompModeSP;
    enum
    {
    FS_COMP_AUTO, FS_COMP_MANUAL
    };
    
    ISwitch FocuserManualS[2];
    ISwitchVectorProperty FocuserManualSP;
    enum
    {
    FS_MANUAL_ON, FS_MANUAL_OFF
    };

    ISwitch PowerDefaultOnS[3];
    ISwitchVectorProperty PowerDefaultOnSP;
    
    ISwitch AutoPWMDefaultOnS[2];
    ISwitchVectorProperty AutoPWMDefaultOnSP;

    INumber OtherSettingsN[4];
    INumberVectorProperty OtherSettingsNP;
    enum
    {
        SET_AREF_COEFF, SET_OVER_TIME, SET_OVER_VOLT, SET_OVER_AMP
    };
    
    INumber DCFocTimeN[2];
    INumberVectorProperty DCFocTimeNP;
    enum
    {
        DC_PERIOD, DC_PWM
    };
    
    ISwitch DCFocDirS[2];
    ISwitchVectorProperty DCFocDirSP;

    ISwitch DCFocAbortS[1];
    ISwitchVectorProperty DCFocAbortSP;

    ISwitch BuzzerS[1];
    ISwitchVectorProperty BuzzerSP;
    
    static constexpr const char *POWER_TAB {"Power"};
    static constexpr const char *ENVIRONMENT_TAB {"Environment"};
    static constexpr const char *SETTINGS_TAB {"Settings"};
    static constexpr const char *DCFOCUSER_TAB {"DC Focuser"};
};

#endif
