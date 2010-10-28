#ifndef INDIDEFAULTDRIVER_H
#define INDIDEFAULTDRIVER_H

#include "basedriver.h"
#include "indidriver.h"

/**
 * \class INDI::DefaultDriver
   \brief Class to provide extended functionary for drivers in addition
to the functionality provided by INDI::BaseDriver. This class should \e only be subclassed by
drivers directly as it is linked with main(). Virtual drivers cannot employ INDI::DefaultDriver.

\see <a href='tutorial__four_8h_source.html'>Tutorial Four</a>
 */
class INDI::DefaultDriver : public INDI::BaseDriver
{
public:
    DefaultDriver();
    virtual ~DefaultDriver() {}

    virtual void setConnected(bool status);
    void addAuxControls();

    void resetProperties();

protected:

    virtual void ISGetProperties (const char *dev);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {return false;}
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) {return false;}

    // Configuration
    bool loadConfig();
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

#endif // INDIDEFAULTDRIVER_H
