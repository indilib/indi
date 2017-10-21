/*
    10micron INDI driver

    Copyright (C) 2017 Hans Lambermont

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

typedef enum {
    GSTAT_UNSET                       = -999,
    GSTAT_TRACKING                    = 0,
    GSTAT_STOPPED                     = 1,
    GSTAT_PARKING                     = 2,
    GSTAT_UNPARKING                   = 3,
    GSTAT_SLEWING_TO_HOME             = 4,
    GSTAT_PARKED                      = 5,
    GSTAT_SLEWING_OR_STOPPING         = 6,
    GSTAT_NOT_TRACKING_AND_NOT_MOVING = 7,
    GSTAT_MOTORS_TOO_COLD             = 8,
    GSTAT_TRACKING_OUTSIDE_LIMITS     = 9,
    GSTAT_FOLLOWING_SATELLITE         = 10,
    GSTAT_NEED_USEROK                 = 11,
    GSTAT_UNKNOWN_STATUS              = 98,
    GSTAT_ERROR                       = 99
} _10MICRON_GSTAT;

class LX200_10MICRON : public LX200Generic
{
  public:
    LX200_10MICRON();
    ~LX200_10MICRON() {}

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

    virtual const char *getDefaultName() override;
    virtual bool Handshake() override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ReadScopeStatus() override;
    virtual bool Park() override;
    virtual bool UnPark() override;
    virtual bool SyncConfigBehaviour(bool cmcfg);

    // TODO move these things elsewhere
    int monthToNumber(const char *monthName);
    int setStandardProcedureWithoutRead(int fd, const char *data);
    int setStandardProcedureAndExpect(int fd, const char *data, const char *expect);

  protected:
    virtual void getBasicData() override;

    IText ProductT[4];
    ITextVectorProperty ProductTP;

    virtual int SetRefractionModelTemperature(double temperature);
    INumber RefractionModelTemperatureN[1];
    INumberVectorProperty RefractionModelTemperatureNP;

    virtual int SetRefractionModelPressure(double pressure);
    INumber RefractionModelPressureN[1];
    INumberVectorProperty RefractionModelPressureNP;

    INumber ModelCountN[1];
    INumberVectorProperty ModelCountNP;

    INumber AlignmentPointsN[1];
    INumberVectorProperty AlignmentPointsNP;

    enum
    {
        ALIGN_IDLE,
        ALIGN_START,
        ALIGN_END,
        ALIGN_COUNT
    };
    ISwitch AlignmentS[ALIGN_COUNT];
    ISwitchVectorProperty AlignmentSP;

    INumber MiniNewAlpN[2];
    INumberVectorProperty MiniNewAlpNP;

    INumber NewAlpN[6];
    INumberVectorProperty NewAlpNP;

    INumber NewAlignmentPointsN[1];
    INumberVectorProperty NewAlignmentPointsNP;

    IText NewModelNameT[1];
    ITextVectorProperty NewModelNameTP;

  private:
    int fd = -1; // short notation for PortFD/sockfd
    bool getMountInfo();

    int OldGstat = GSTAT_UNSET;
    struct _Ginfo {
        float RA_JNOW   = 0.0;
        float DEC_JNOW  = 0.0;
        char SideOfPier = 'x';
        float AZ        = 0.0;
        float ALT       = 0.0;
        float Jdate     = 0.0;
        int Gstat       = -1;
        int SlewStatus  = -1;
    } Ginfo;

};
