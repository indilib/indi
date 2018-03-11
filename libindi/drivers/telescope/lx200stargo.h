#ifndef AVALON_STARGO_H
#define AVALON_STARGO_H

#include "lx200generic.h"
#include "lx200driver.h"
#include "indicom.h"

#include <cstring>

class LX200StarGo : public LX200Generic
{
public:
    LX200StarGo();
    ~LX200StarGo() = default;

    virtual bool Handshake() override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool updateProperties() override;
    virtual bool initProperties() override;

protected:

    // Sync Home Position
    ISwitchVectorProperty SyncHomeSP;
    ISwitch SyncHomeS[1];

    virtual const char *getDefaultName() override;

    /*
    virtual void getBasicData() override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;
    virtual bool checkConnection() override;
    virtual bool isSlewComplete() override;

    virtual bool ReadScopeStatus() override;

    virtual bool SetSlewRate(int index) override;
    virtual bool SetTrackMode(uint8_t mode) override;
    virtual bool Goto(double, double) override;
    virtual bool Sync(double ra, double dec) override;
    virtual bool updateTime(ln_date *utc, double utc_offset) override;

    virtual bool saveConfigItems(FILE *fp) override;

    virtual bool Park() override;
    virtual bool UnPark() override;
*/
};

#endif // AVALON_STARGO_H
