#ifndef INDIDEFAULTDRIVER_H
#define INDIDEFAULTDRIVER_H

#include "basedriver.h"
#include "indidriver.h"

/**
 * \class INDI::DefaultDriver
   \brief Class to provide extended functionary for drivers in addition
to the functionality provided by INDI::BaseDriver. This class should \e only be subclassed by
drivers directly as it is linked with main(). Virtual drivers cannot employ INDI::DefaultDriver.

   INDI::DefaultDriver provides capability to add Debug, Simulation, and Configuration controls. These controls (switches) are
   defined to the client. Configuration options permit saving and loading of AS-IS property values.

\see <a href='tutorial__four_8h_source.html'>Tutorial Four</a>
\author Jasem Mutlaq
 */
class INDI::DefaultDriver : public INDI::BaseDriver
{
public:
    DefaultDriver();
    virtual ~DefaultDriver() {}

    /** \brief Add Debug, Simulation, and Configuration options to the driver */
    void addAuxControls();

    /** \brief Add Debug control to the driver */
    void addDebugControl();

    /** \brief Add Simulation control to the driver */
    void addSimulationControl();

    /** \brief Add Configuration control to the driver */
    void addConfigurationControl();

    /** \brief Set all properties to IDLE state */
    void resetProperties();

protected:

    /** \brief define the driver's properties to the client.
      \param dev name of the device
      \note This function is called by the INDI framework, do not call it directly.
    */
    virtual void ISGetProperties (const char *dev);

    /** \brief Process the client newSwitch command
      \note This function is called by the INDI framework, do not call it directly.
      \returns True if any property was successfully processed, false otherwise.
    */
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    /** \brief Process the client newNumber command
      \note This function is called by the INDI framework, do not call it directly.
      \returns True if any property was successfully processed, false otherwise.
    */
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {return false;}

    /** \brief Process the client newSwitch command
      \note This function is called by the INDI framework, do not call it directly.
      \returns True if any property was successfully processed, false otherwise.
    */
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) {return false;}

    // Configuration

    /** \brief Load the last saved configuration file
        \return True if successful, false otherwise.
    */
    bool loadConfig();

    /** \brief Save the current properties in a configuration file
        \return True if successful, false otherwise.
    */
    bool saveConfig();

    /** \brief Load the default configuration file
        \return True if successful, false otherwise.
    */
    bool loadDefaultConfig();

    // Simulatin & Debug

    /** \brief Toggle driver debug status

      A driver can be more verbose if Debug option is enabled by the client.

      \param enable If true, the Debug option is set to ON.
    */
    void setDebug(bool enable);

    /** \brief Toggle driver simulation status

      A driver can run in simulation mode if Simulation option is enabled by the client.

      \param enable If true, the Simulation option is set to ON.
    */
    void setSimulation(bool enable);

    /** \return True if Debug is on, False otherwise. */
    bool isDebug();

    /** \return True if Simulation is on, False otherwise. */
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
