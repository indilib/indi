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

    /** \brief Connect or Disconnect a device.
      \param status If true, the driver will attempt to connect to the device (CONNECT=ON). If false, it will attempt
to disconnect the device.
      \param msg A message to be sent along with connect/disconnect command.
    */
    virtual void setConnected(bool status, IPState state=IPS_OK, const char *msg = NULL);

    int SetTimer(int);
    void RemoveTimer(int);
    virtual void TimerHit();

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

    //  These are the properties we define, that are generic to pretty much all devices
    //  They are public to make them available to all dervied classes and thier children
    ISwitchVectorProperty ConnectionSP;
    ISwitch ConnectionS[2];

    //  Helper functions that encapsulate the indi way of doing things
    //  and give us a clean c++ class method
    virtual int initProperties();

    //  This will be called after connecting
    //  to flesh out and update properties to the
    //  client when the device is connected
    virtual bool updateProperties();

    //  A helper for child classes
    virtual bool deleteProperty(const char *propertyName);

    //  some virtual functions that our underlying classes are meant to override
    virtual bool Connect();
    virtual bool Disconnect();
    virtual const char *getDefaultName()=0;



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
