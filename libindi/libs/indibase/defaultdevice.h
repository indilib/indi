#ifndef INDIDEFAULTDEVICE_H
#define INDIDEFAULTDEVICE_H

#include "basedevice.h"
#include "indidriver.h"

class INDI::DefaultDevice : public INDI::BaseDevice
{
public:
    DefaultDevice();
    virtual ~DefaultDevice() {}

    virtual void setConnected(bool status);
    void addAuxControls();

protected:

    virtual void ISGetProperties (const char *dev);
    virtual void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {}
    virtual void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) {}

    // Configuration
    bool loadConfig(bool ignoreConnection = false);
    bool saveConfig();
    bool loadDefaultConfig();

    // Simulatin & Debug
    void setDebug(bool enable);
    void setSimulation(bool enable);
    bool isDebug();
    bool isSimulation();


private:

    bool pDebug;
    bool pSimulation;

    ISwitch DebugS[2];
    ISwitch SimulationS[2];
    ISwitch ConfigProcessS[3];

    ISwitchVectorProperty *DebugSP;
    ISwitchVectorProperty *SimulationSP;
    ISwitchVectorProperty *ConfigProcessSP;


};

#endif // INDIDEFAULTDEVICE_H
