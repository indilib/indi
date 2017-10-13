/*
    USB_Dewpoint
    Copyright (C) 2017 Jarno Paananen

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

#include <defaultdevice.h>

/***************************** USB_Dewpoint Commands **************************/

#define UFOCREADPARAM "SGETAL"
#define UFOCDEVID     "SWHOIS"
#define UFOCREADPOS   "FPOSRO"
#define UFOCREADTEMP  "FTMPRO"
#define UFOCMOVEOUT   "O"
#define UFOCMOVEIN    "I"
#define UFOCABORT     "FQUITx"
#define UFOCSETMAX    "M"
#define UFOCSETSPEED  "SMO"
#define UFOCSETTCTHR  "SMA"
#define UFOCSETSDIR   "SMROTH"
#define UFOCSETRDIR   "SMROTT"
#define UFOCSETFSTEPS "SMSTPF"
#define UFOCSETHSTEPS "SMSTPD"
#define UFOCSETSTDEG  "FLA"
#define UFOCGETSIGN   "FTAXXA"
#define UFOCSETSIGN   "FZAXX"
#define UFOCSETAUTO   "FAMODE"
#define UFOCSETMANU   "FMMODE"
#define UFOCRESET     "SEERAZ"

/**************************** USB_Dewpoint Constants **************************/

#define UFOID "UFO"

#define UFORSACK   "*"
#define UFORSEQU   "="
#define UFORSAUTO  "AP"
#define UFORSDONE  "DONE"
#define UFORSERR   "ER="
#define UFORSRESET "EEPROM RESET"

#define UFOPSDIR   0 // standard direction
#define UFOPRDIR   1 // reverse direction
#define UFOPFSTEPS 0 // full steps
#define UFOPHSTEPS 1 // half steps
#define UFOPPSIGN  0 // positive temp. comp. sign
#define UFOPNSIGN  1 // negative temp. comp. sign

#define UFOPSPDERR 0 // invalid speed
#define UFOPSPDAV  2 // average speed
#define UFOPSPDSL  3 // slow speed
#define UFOPSPDUS  4 // ultra slow speed

#define UFORTEMPLEN 8  // maximum length of returned temperature string
#define UFORSIGNLEN 3  // maximum length of temp. comp. sign string
#define UFORPOSLEN  7  // maximum length of returned position string
#define UFORSTLEN   26 // maximum length of returned status string
#define UFORIDLEN   3  // maximum length of returned temperature string
#define UFORDONELEN 4  // length of done response

#define UFOCTLEN  6 // length of temp parameter setting commands
#define UFOCMLEN  6 // length of move commands
#define UFOCMMLEN 6 // length of max. move commands
#define UFOCSLEN  6 // length of speed commands
#define UFOCDLEN  6 // length of direction commands
#define UFOCSMLEN 6 // length of step mode commands
#define UFOCTCLEN 6 // length of temp compensation commands

/******************************************************************************/

namespace Connection
{
class Serial;
};

class USBDewpoint : public INDI::DefaultDevice
{
  public:
    USBDewpoint();
    virtual ~USBDewpoint() = default;

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual void TimerHit() override;

  private:
    bool Handshake();
    bool getControllerStatus();
    bool oneMoreRead(char *response, unsigned int maxlen);

    bool reset();
    bool updateSettings();
    bool updateFWversion();

    bool Ack();

    unsigned int firmware { 0 };  // firmware version

    Connection::Serial *serialConnection { nullptr };
    int PortFD { -1 };

    INumber OutputsN[3];
    INumberVectorProperty OutputsNP;

    INumber TemperaturesN[3];
    INumberVectorProperty TemperaturesNP;

    INumber CalibrationsN[3];
    INumberVectorProperty CalibrationsNP;

    INumber ThresholdsN[2];
    INumberVectorProperty ThresholdsNP;

    INumber HumidityN[1];
    INumberVectorProperty HumidityNP;

    INumber AggressivityN[1];
    INumberVectorProperty AggressivityNP;

    ISwitch AutoModeS[1];
    ISwitchVectorProperty AutoModeSP;

    ISwitch LinkOut23S[1];
    ISwitchVectorProperty LinkOut23SP;

    ISwitch ResetS[1];
    ISwitchVectorProperty ResetSP;

    INumber FWversionN[1];
    INumberVectorProperty FWversionNP;
};
