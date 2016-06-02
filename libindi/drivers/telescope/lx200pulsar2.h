/*
    Pulsar 2 INDI driver

    Copyright (C) 2015 Jasem Mutlaq

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

#ifndef LX200PULSAR2_H
#define LX200PULSAR2_H

#include "lx200generic.h"

class LX200Pulsar2 : public LX200Generic
{
public:

    LX200Pulsar2();
    ~LX200Pulsar2() {}

    bool updateProperties();
    bool initProperties();
    void ISGetProperties(const char *dev);
    bool Connect();
    
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);

protected:
    
    const char *getDefaultName();

    bool Goto(double,double);
    void getBasicData();
    bool checkConnection();
    bool isSlewComplete();
    bool Park();
    bool UnPark();
    bool updateTime(ln_date * utc, double utc_offset);
    
//    ITextVectorProperty AltHomeTP;
//    IText   AltHomeT[1];
    
//    ITextVectorProperty AzHomeTP;
//    IText   AzHomeT[1];

    ISwitchVectorProperty PierSideSP;
    ISwitch   PierSideS[2];

private:
    
    void cleanedOutput(char *);
    int currentPierSide;
    
    bool isHomeSet();
    bool isParked();
    bool isParking();
//    int setAltAzHome(int portfd, char * text, const char * name);
//    int setPierSide(int fd, char * pierSide);

};

#endif
