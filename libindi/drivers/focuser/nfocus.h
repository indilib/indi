/*
    NFocus
    Copyright (C) 2006 Markus Wildi (markus.wildi@datacomm.ch)
                  2011 Jasem Mutlaq (mutlaqja@ikarustech.com)
		  2013 Felix Krämer (rigelsys@felix-kraemer.de)

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

#ifndef NFOCUS_H
#define NFOCUS_H

#include "indibase/indifocuser.h"
#include "nfocus.h"

class NFocus : public INDI::Focuser
{
public:
    NFocus();
    ~NFocus();

    virtual bool Connect();
    virtual bool Disconnect();
    const char * getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual int Move(FocusDirection dir, int speed, int duration);
    virtual int MoveAbs(int ticks);
    virtual int MoveRel(FocusDirection dir, unsigned int ticks);

protected:
    bool saveConfigItems(FILE *fp);

private:

    int PortFD;

    unsigned char CalculateSum(char *rf_cmd);
    int SendCommand(char *rf_cmd);
    int SendRequest(char *rf_cmd);
    int ReadResponse(char *buf, int nbytes, int timeout);
    void GetFocusParams();

    int updateNFPosition(double *value);
    int updateNFTemperature(double *value) ;
    int updateNFBacklash(double *value);
    int updateNFInOutScalar(double *value);
    int updateNFFirmware(char *rf_cmd) ;
    int updateNFMotorSettings(double *duty, double *delay, double *ticks);
    int updateNFPositionRelativeInward(double *value);
    int updateNFPositionRelativeOutward(double *value) ;
    int updateNFPositionAbsolute(double *value);
    int updateNFMaxPosition(double *value);
    int updateNFSetPosition(double *value);



    ITextVectorProperty PortTP;
    IText PortT[1];

    INumber TemperatureN[1];
    INumberVectorProperty TemperatureNP;

    INumber SettingsN[3];
    INumberVectorProperty SettingsNP;

    INumber MinMaxPositionN[2];
    INumberVectorProperty MinMaxPositionNP;

    INumber MaxTravelN[1];
    INumberVectorProperty MaxTravelNP;

    INumber SetRegisterPositionN[1];
    INumberVectorProperty SetRegisterPositionNP;

    INumber InOutScalarN[1];
    INumberVectorProperty InOutScalarNP;

    INumber RelMovementN[1];
    INumberVectorProperty RelMovementNP;

    INumber AbsMovementN[1];
    INumberVectorProperty AbsMovementNP;

    INumber SetBacklashN[1];
    INumberVectorProperty SetBacklashNP;

};

#endif // NFOCUS_H
