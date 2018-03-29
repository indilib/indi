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

    // helper functions
    virtual bool receive(char* buffer, int* bytes);
    virtual void flush();
    virtual bool transmit(const char* buffer);
    virtual bool setStandardProcedureAvalon(const char *command, int wait);

protected:

    // Sync Home Position
    ISwitchVectorProperty SyncHomeSP;
    ISwitch SyncHomeS[1];

    // firmware info
    ITextVectorProperty MountInfoTP;
    IText MountFirmwareInfoT[1];

    // goto home
    ISwitchVectorProperty MountGotoHomeSP;
    ISwitch MountGotoHomeS[1];

    // set parking position
    ISwitchVectorProperty MountSetParkSP;
    ISwitch MountSetParkS[1];


    // override LX200Generic
    virtual void getBasicData();
    virtual bool ReadScopeStatus() override;
    virtual bool Park() override;
    virtual bool UnPark() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual bool isSlewComplete() override;

    // StarGo stuff
    virtual bool syncHomePosition();
    bool slewToHome(ISState *states, char *names[], int n);
    bool setParkPosition(ISState *states, char *names[], int n);

    // scope status
    virtual bool UpdateMotionStatus();
    TelescopeSlewRate CurrentSlewRate;

    // location
    virtual bool sendScopeLocation();
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;
    virtual int getSiteLatitude(double *siteLat);
    virtual int getSiteLongitude(double *siteLong);


    // queries to the scope interface
    virtual bool sendQuery(const char* cmd, char* response);
    virtual bool queryMountMotionState(int* motorsState, int* speedState, int* nrTrackingSpeed);
    virtual bool queryFirmwareInfo(char *version);
    virtual bool querySetSiteLatitude(double Lat);
    virtual bool querySetSiteLongitude(double Long);
    virtual bool querySetTracking(bool enable);
    virtual bool queryParkSync(bool *isParked, bool *isSynched);
    virtual bool queryIsSlewComplete();
    virtual bool querySendMountGotoHome();
    virtual bool querySendMountSetPark();

};

#endif // AVALON_STARGO_H
