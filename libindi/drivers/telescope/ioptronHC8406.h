/*
    ioptronHC8406 INDI driver
    Copyright (C) 2017 Nacho Mas. Base on GotoNova driver by Jasem Mutlaq


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

#include "lx200generic.h"



class ioptronHC8406 : public LX200Generic
{
  public:
    ioptronHC8406();
    ~ioptronHC8406() {}

    virtual bool updateProperties() override;
    virtual bool initProperties() override;

    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

  protected:
    virtual const char *getDefaultName() override;

    virtual void getBasicData() override;
    virtual bool checkConnection() override;
    virtual bool isSlewComplete() override;

    virtual bool ReadScopeStatus() override;

    virtual bool SetSlewRate(int index) override;
    virtual bool SetTrackMode(uint8_t mode) override;
    virtual bool Goto(double, double) override;
    virtual bool Sync(double ra, double dec) override;
    virtual bool updateTime(ln_date *utc, double utc_offset) override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool Park() override;
    virtual bool UnPark() override;
    void sendScopeTime() ;
  private:
    int setioptronHC8406StandardProcedure(int fd, const char *data);
    void setGuidingEnabled(bool enable);
    int ioptronHC8406SyncCMR(char *matchedObject);

    // Settings
    int setioptronHC8406Latitude(double Lat);
    int setioptronHC8406Longitude(double Long);
    int setioptronHC8406UTCOffset(double hours);
    int setCalenderDate(int fd, int dd, int mm, int yy);

    // Motion
    int slewioptronHC8406();    

    // Track Mode
    int setioptronHC8406TrackMode(int mode);
    int getioptronHC8406TrackMode(int *mode);

    // Guide Rate
    int setioptronHC8406GuideRate(int rate);
    int getioptronHC8406GuideRate(int *rate);

    // Pier Side
    void syncSideOfPier();

    // Simulation
    void mountSim();

    // Sync type
    ISwitch SyncCMRS[2];
    ISwitchVectorProperty SyncCMRSP;
    enum { USE_REGULAR_SYNC, USE_CMR_SYNC };


    /* Guide Rate */
    ISwitch GuideRateS[4];
    ISwitchVectorProperty GuideRateSP;

    bool isGuiding=false;
};
