/*
    ZEQ25 INDI driver

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

#ifndef LX200ZEQ25_H
#define LX200ZEQ25_H

#include "lx200generic.h"

class LX200ZEQ25 : public LX200Generic
{
public:

    LX200ZEQ25();
    ~LX200ZEQ25() {}

    virtual bool updateProperties();
    virtual bool initProperties();

    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

protected:

    virtual const char *getDefaultName();

    virtual void getBasicData();
    virtual bool checkConnection();
    virtual bool isSlewComplete();

    virtual bool ReadScopeStatus() override;

    virtual bool SetSlewRate(int index) override;
    virtual bool SetTrackMode(int mode) override;
    virtual bool Goto(double,double) override;
    virtual bool updateTime(ln_date * utc, double utc_offset) override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

    // Parking
    virtual bool SetCurrentPark() override;
    virtual bool SetDefaultPark() override;
    virtual bool Park() override;
    virtual bool UnPark() override;

private:
    int setZEQ25StandardProcedure(int fd, const char * data);
    int setZEQ25Latitude(double Lat);
    int setZEQ25Longitude(double Long);
    int setZEQ25UTCOffset(double hours);
    int slewZEQ25();
    int moveZEQ25To(int direction);
    int haltZEQ25Movement();
    int getZEQ25MoveRate();
    int setZEQ25Park();
    int setZEQ25UnPark();
    int setZEQ25TrackMode(int mode);

    bool isZEQ25Home();
    int gotoZEQ25Home();

    bool isZEQ25Parked();

    bool getMountInfo();
    void mountSim();

    ISwitch HomeS[1];
    ISwitchVectorProperty HomeSP;

};

#endif
