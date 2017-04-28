/*
    USB Focus V3
    Copyright (C) 2016 G. Schmidt

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

#ifndef USBFOCUSV3_H
#define USBFOCUSV3_H

#include "indibase/indifocuser.h"

/***************************** USB Focus V3 Commands **************************/

#define UFOCREADPARAM	"SGETAL"
#define UFOCDEVID	"SWHOIS"
#define UFOCREADPOS	"FPOSRO"
#define UFOCREADTEMP	"FTMPRO"
#define UFOCMOVEOUT	"O"
#define UFOCMOVEIN	"I"
#define UFOCABORT	"FQUITx"
#define UFOCSETMAX	"M"
#define UFOCSETSPEED	"SMO"
#define UFOCSETTCTHR	"SMA"
#define UFOCSETSDIR	"SMROTH"
#define UFOCSETRDIR	"SMROTT"
#define UFOCSETFSTEPS	"SMSTPF"
#define UFOCSETHSTEPS	"SMSTPD"
#define UFOCSETSTDEG	"FLA"
#define UFOCGETSIGN	"FTAXXA"
#define UFOCSETSIGN	"FZAXX"
#define UFOCSETAUTO	"FAMODE"
#define UFOCSETMANU	"FMMODE"
#define UFOCRESET	"SEERAZ"

/**************************** USB Focus V3 Constants **************************/

#define UFOID		"UFO"

#define UFORSACK	"*"
#define UFORSEQU	"="
#define UFORSAUTO	"AP"
#define UFORSDONE	"DONE"
#define UFORSERR	"ER="
#define UFORSRESET	"EEPROM RESET"

#define UFOPSDIR	0	// standard direction
#define UFOPRDIR	1	// reverse direction
#define UFOPFSTEPS	0	// full steps
#define UFOPHSTEPS	1	// half steps
#define UFOPPSIGN	0	// positive temp. comp. sign
#define UFOPNSIGN	1	// negative temp. comp. sign

#define UFOPSPDERR	0	// invalid speed
#define UFOPSPDAV	2	// average speed
#define UFOPSPDSL	3	// slow speed
#define UFOPSPDUS	4	// ultra slow speed

#define UFORTEMPLEN	8	// maximum length of returned temperature string
#define UFORSIGNLEN	3	// maximum length of temp. comp. sign string
#define UFORPOSLEN	7 	// maximum length of returned position string
#define UFORSTLEN	26	// maximum length of returned status string
#define UFORIDLEN	3	// maximum length of returned temperature string
#define UFORDONELEN	4	// length of done response 

#define UFOCTLEN	6	// length of temp parameter setting commands
#define UFOCMLEN	6	// length of move commands
#define UFOCMMLEN	6	// length of max. move commands
#define UFOCSLEN	6	// length of speed commands
#define UFOCDLEN	6	// length of direction commands
#define UFOCSMLEN	6	// length of step mode commands
#define UFOCTCLEN	6	// length of temp compensation commands

/******************************************************************************/

class USBFocusV3 : public INDI::Focuser
{
public:
    USBFocusV3();
    ~USBFocusV3();

    typedef enum { FOCUS_HALF_STEP, FOCUS_FULL_STEP } FocusStepMode;

    virtual bool Handshake();
    virtual bool getControllerStatus();
    const char * getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual IPState MoveAbsFocuser(uint32_t ticks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
    virtual bool SetFocuserSpeed(int speed);
    virtual bool AbortFocuser();
    virtual void TimerHit();

private:

    unsigned int direction; // 0 standard, 1 reverse
    unsigned int stepmode;  // 0 full steps, 1 half steps
    unsigned int speed;     // 2 average, 3 slow, 4 ultra slow
    unsigned int stepsdeg;  // steps per degree for temperature compensation
    unsigned int tcomp_thr; // temperature compensation threshold
    unsigned int firmware;  // firmware version
    unsigned int maxpos;    // maximum step position (0..65535)

    double targetPos, lastPos, lastTemperature;
    unsigned int currentSpeed;

    struct timeval focusMoveStart;
    float focusMoveRequest;

    bool oneMoreRead(char* response, unsigned int maxlen);
    
    void GetFocusParams();
    bool reset();
    bool updateStepMode();
    bool updateRotDir();
    bool updateTemperature();
    bool updatePosition();
    bool updateMaxPos();
    bool updateTempCompSettings();
    bool updateTempCompSign();
    bool updateSpeed();
    bool updateFWversion();

    bool isMoving();
    bool Ack();

    bool MoveFocuser(FocusDirection dir, unsigned int ticks);
    bool setStepMode(FocusStepMode mode);
    bool setRotDir(unsigned int dir);
    bool setMaxPos(unsigned int dir);
    bool setSpeed(unsigned short drvspeed);
    bool setAutoTempCompThreshold(unsigned int thr);
    bool setTemperatureCoefficient(unsigned int coefficient);
    bool setTempCompSign(unsigned int sign);
    bool setTemperatureCompensation(bool enable);
    float CalcTimeLeft(timeval,float);

    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    ISwitch StepModeS[2];
    ISwitchVectorProperty StepModeSP;

    ISwitch RotDirS[2];
    ISwitchVectorProperty RotDirSP;

    INumber MaxPositionN[1];
    INumberVectorProperty MaxPositionNP;

    INumber TemperatureSettingN[2];
    INumberVectorProperty TemperatureSettingNP;

    ISwitch TempCompSignS[2];
    ISwitchVectorProperty TempCompSignSP;

    ISwitch TemperatureCompensateS[2];
    ISwitchVectorProperty TemperatureCompensateSP;

    ISwitch ResetS[1];
    ISwitchVectorProperty ResetSP;

    INumber FWversionN[1];
    INumberVectorProperty FWversionNP;

};

#endif // USBFOCUSV3_H 
