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

// StarGo specific tabs
extern const char *RA_DEC_TAB;

class LX200StarGo : public LX200Telescope
{
public:
    LX200StarGo();
    virtual ~LX200StarGo() = default;

    virtual const char *getDefaultName() override;
    virtual bool Handshake() override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool updateProperties() override;
    virtual bool initProperties() override;

    // helper functions
    virtual bool receive(char* buffer, int* bytes, bool wait=true);
    virtual void flush();
    virtual bool transmit(const char* buffer);
//    virtual bool setStandardProcedureAvalon(const char *command, int wait);
    virtual bool SetTrackMode(uint8_t mode) override;

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

    // parking position
    ISwitchVectorProperty MountSetParkSP;
    ISwitch MountSetParkS[1];
    ILightVectorProperty MountParkingStatusLP;
    ILight MountParkingStatusL[2];

    // guiding
    INumberVectorProperty GuidingSpeedNP;
    INumber GuidingSpeedP[2];

    ISwitchVectorProperty ST4StatusSP;
    ISwitch ST4StatusS[2];

    // meridian flip
    ISwitchVectorProperty MeridianFlipEnabledSP;
    ISwitch MeridianFlipEnabledS[2];
    ISwitchVectorProperty MeridianFlipForcedSP;
    ISwitch MeridianFlipForcedS[2];
    
    int controller_format;

    // override LX200Generic
    virtual void getBasicData() override;
    virtual bool ReadScopeStatus() override;
    virtual bool Park() override;
    virtual void SetParked(bool isparked);
    virtual bool UnPark() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual bool isSlewComplete() override;

    // StarGo stuff
    virtual bool syncHomePosition();
    bool slewToHome(ISState *states, char *names[], int n);
    bool setParkPosition(ISState *states, char *names[], int n);

    // autoguiding
    virtual bool setGuidingSpeeds(int raSpeed, int decSpeed);

    // scope status
    virtual bool UpdateMotionStatus();
    virtual void UpdateMotionStatus(int motorsState, int speedState, int nrTrackingSpeed);
//    bool parseMotionState(char *response, int *motorsState, int *speedState, int *nrTrackingSpeed);
    bool isIdle();
    TelescopeSlewRate CurrentSlewRate;

    // location
    virtual bool sendScopeLocation();
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;
    virtual bool getSiteLatitude(double *siteLat);
    virtual bool getSiteLongitude(double *siteLong);
    virtual bool getLST_String(char* input);


    // queries to the scope interface
    virtual bool sendQuery(const char* cmd, char* response, bool wait=true);
//    virtual bool queryMountMotionState(int* motorsState, int* speedState, int* nrTrackingSpeed);
    virtual bool queryMountMotionState();
    virtual bool queryFirmwareInfo(char *version);
    virtual bool querySetSiteLatitude(double Lat);
    virtual bool querySetSiteLongitude(double Long);
    virtual bool querySetTracking(bool enable);
    virtual bool queryParkSync(bool *isParked, bool *isSynched);
    virtual bool queryIsSlewComplete();
    virtual bool querySendMountGotoHome();
    virtual bool querySendMountSetPark();

    // guiding
    virtual bool queryGetST4Status(bool *isEnabled);
    virtual bool queryGetGuidingSpeeds(int *raSpeed, int *decSpeed);
    virtual bool setST4Enabled(bool enabled);

    // meridian flip
    virtual bool queryGetMeridianFlipEnabledStatus(bool *isEnabled);
    virtual bool queryGetMeridianFlipForcedStatus(bool *isEnabled);
    virtual bool setMeridianFlipEnabled(bool enabled);
    virtual bool setMeridianFlipForced(bool enabled);

    virtual bool syncSideOfPier();
    bool checkLX200Format();
    // Guide Commands
    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;
    virtual bool SetSlewRate(int index) override;
    bool setSlewMode(int slewMode);
    int motorsState;
    int speedState;
    int nrTrackingSpeed;

};

#endif // AVALON_STARGO_H
