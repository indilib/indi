#ifndef AVALON_STARGO_H
#define AVALON_STARGO_H

#pragma once

#include "lx200telescope.h"
#include "lx200driver.h"
#include "indicom.h"
#include "indilogger.h"
#include "termios.h"

#include <cstring>
#include <string>
#include <unistd.h>

#define LX200_TIMEOUT 5 /* FD timeout in seconds */
#define RB_MAX_LEN    64
#define AVALON_TIMEOUT                                  5
#define AVALON_COMMAND_BUFFER_LENGTH                    32
#define AVALON_RESPONSE_BUFFER_LENGTH                   32


class LX200StarGo : public LX200Telescope
{
public:
    LX200StarGo();
    virtual ~LX200StarGo() = default;

    virtual const char *getDefaultName() override;
    virtual bool Handshake() override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool updateProperties() override;
    virtual bool initProperties() override;

protected:

    // Sync Home Position
    ISwitchVectorProperty SyncHomeSP;
    ISwitch SyncHomeS[1];

    // firmware info
    ITextVectorProperty MountInfoTP;
    IText MountFirmwareInfoT[1];

    // override LX200Generic
    virtual void getBasicData();
    virtual bool ReadScopeStatus() override;
    virtual bool Park() override;
    virtual bool UnPark() override;

    // StarGo stuff
    virtual bool syncHomePosition();

    // scope status
    virtual bool UpdateMotionStatus();
    TelescopeSlewRate CurrentSlewRate;

    // location
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;
    virtual int getSiteLatitude(double *siteLat);
    virtual int querySetSiteLatitude(double Lat);
    virtual int getSiteLongitude(double *siteLong);
    virtual int querySetSiteLongitude(double Long);

    bool sendScopeLocation();

    // queries to the scope interface
    virtual bool sendQuery(const char* cmd, char* response);
    virtual bool queryMountMotionState(int* motorsState, int* speedState, int* nrTrackingSpeed);
    virtual bool queryFirmwareInfo(char *version);
    virtual bool querySetTracking(bool enable);
    virtual bool queryParkSync(bool *isParked, bool *isSynched);

    // helper functions
    virtual bool receive(char* buffer, int* bytes);
    virtual void flush();
    virtual bool transmit(const char* buffer);
    virtual bool setStandardProcedureAvalon(char* command, int wait);

    /*
    virtual void getBasicData() override;
    virtual bool checkConnection() override;
    virtual bool isSlewComplete() override;


    virtual bool SetSlewRate(int index) override;
    virtual bool SetTrackMode(uint8_t mode) override;
    virtual bool Goto(double, double) override;
    virtual bool Sync(double ra, double dec) override;
    virtual bool updateTime(ln_date *utc, double utc_offset) override;

    virtual bool saveConfigItems(FILE *fp) override;

*/
};

#endif // AVALON_STARGO_H
