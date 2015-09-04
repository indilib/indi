/*
    Dome Interface
    Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef INDIDOMEINTERFACE_H
#define INDIDOMEINTERFACE_H

#include "indibase.h"
#include "indiapi.h"

/**
 * \class INDI::DomeInterface
   \brief Provides interface to implement Dome functionality.

   A Dome can be an independent device, or an embedded Dome within another device. Before using any of the dome functions, you must define the capabilities of the dome by calling
   SetDomeCapability() function. All positions are represented as degrees of azimuth.

   Relative motion is specified in degrees as either positive (clock wise direction), or negative (counter clock-wise direction).

   Slaving is used to synchronizes the dome's azimuth position with that of the mount. The mount's azimuth position is snooped from the ACTIVE_TELESCOPE property in ACTIVE_DEVICES vector.
   The AutoSync threshold is the difference in degrees between the dome's azimuth angle and the mount's azimuth angle that should trigger a dome motion. By default, it is set to 0.5 degrees which would
   trigger dome motion due to any difference between the dome and mount azimuth angles that exceeds 0.5 degrees. For example, if the threshold is set to 5 degrees, the dome will only start moving to sync with the mount's azimuth angle once
   the difference in azimuth angles is equal or exceeds 5 degrees. The dome will only commence movement once the mount completed slewing.

   \e IMPORTANT: After SetDomeCapability(), initDomeProperties() must be called before any other function to initilize the Dome properties.

   \e IMPORTANT: processDomeNumber() and processDomeSwitch() must be called in your driver's ISNewNumber() and ISNewSwitch functions recepectively.
\author Jasem Mutlaq
*/
class INDI::DomeInterface
{

public:
    enum DomeDirection { DOME_CW, DOME_CCW };
    enum DomeParam { DOME_HOME, DOME_AUTOSYNC };
    enum DomeMotionCommand { MOTION_START, MOTION_STOP };

    /*! Dome Parking data type enum */
    enum DomeParkData  { PARK_NONE,         /*!< Open loop Parking  */
                         PARK_AZ,           /*!< Parking via azimuth angle control */
                         PARK_AZ_ENCODER,   /*!< Parking via azimuth encoder control */
                       };

    /** \typedef ShutterOperation
        \brief Shutter operation command.
    */
    typedef enum
    {
        SHUTTER_OPEN,            /*!< Open Shutter */
        SHUTTER_CLOSE            /*!< Close Shutter */
    } ShutterOperation;

    /** \typedef DomeState
        \brief Dome status
    */
    typedef enum
    {
        DOME_IDLE,               /*!< Dome is idle */
        DOME_MOVING,             /*!< Dome is in motion */
        DOME_PARKING,            /*!< Dome is parking */
        DOME_PARKED,             /*!< Dome is parked */
    } DomeState;

    /** \typedef ShutterStatus
        \brief Shutter Status
    */
    typedef enum
    {
        SHUTTER_OPENED,           /*!< Shutter is open */
        SHUTTER_CLOSED,           /*!< Shutter is closed */
        SHUTTER_MOVING,           /*!< Shutter is in motion */
        SHUTTER_UNKNOWN           /*!< Shutter status is unknown */
    } ShutterStatus;


    /** \struct DomeCapability
        \brief Holds the capabilities of the dome.
    */
    typedef struct
    {
        /** Can the dome motion be aborted? */
        bool canAbort;
        /** Can the dome move to an absolute azimuth position? */
        bool canAbsMove;
        /** Can the dome move to a relative position a number of degrees away from current position? Positive degress is Clockwise direction. Negative Degrees is counter clock wise direction */
        bool canRelMove;
        /** Can the dome park and unpark itself? */
        bool canPark;
        /** Does the dome has a shutter than can be opened and closed electronically? */
        bool hasShutter;
        /** Can the dome move in different configurable speeds? */
        bool hasVariableSpeed;
    } DomeCapability;

    /**
     * @brief GetDomeCapability returns the capability of the dome
     */
    DomeCapability GetDomeCapability() const { return capability;}

    /**
     * @brief SetDomeCapability set the dome capabilities. All capabilities must be initialized.
     * @param cap pointer to dome capability
     */
    void SetDomeCapability(DomeCapability * cap);

protected:

    DomeInterface();
    virtual ~DomeInterface();

    /** \brief Initilize Dome properties. It is recommended to call this function within initProperties() of your primary device
        \param deviceName Name of the primary device
        \param groupName Group or tab name to be used to define Dome properties.
    */
    void initDomeProperties(const char *deviceName, const char* groupName);

    /** \brief Process dome number properties */
    bool processDomeNumber (const char *dev, const char *name, double values[], char *names[], int n);

    /** \brief Process dome switch properties */
    bool processDomeSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    /**
     * @brief SetSpeed Set Dome speed. This does not initiate motion, it sets the speed for the next motion command. If motion is in progress, then change speed accordingly.
     * @param rpm Dome speed (RPM)
     * @return true if successful, false otherwise
     */
    virtual bool SetSpeed(double rpm);

    /** \brief Move the Dome in a particular direction.
        \param dir Direction of Dome, either DOME_CW or DOME_CCW.
        \return Return true if dome started motion in the desired direction. False if there is an error.
    */
    virtual bool Move(DomeDirection dir, DomeMotionCommand operation);

    /** \brief Move the Dome to an absolute azimuth.
        \param az The new position of the Dome.
        \return Return IPS_OK if motion is completed and Dome reached requested position. Return IPS_BUSY if Dome started motion to requested position and is in progress.
                Return IPS_ALERT if there is an error.
    */
    virtual IPState MoveAbs(double az);

    /** \brief Move the Dome to an relative position.
        \param azDiff The relative azimuth angle to move. Positive degree is clock-wise direction. Negative degrees is counter clock-wise direction.
        \return Return IPS_OK if motion is completed and Dome reached requested position. Return IPS_BUSY if Dome started motion to requested position and is in progress.
                Return IPS_ALERT if there is an error.
    */
    virtual IPState MoveRel(double azDiff);

    /**
     * \brief Abort all dome motion
     * \return True if abort is successful, false otherwise.
     */
    virtual bool Abort();

    /**
     * \brief Goto Home Position. The home position is an absolute azimuth value.
     * \return Return IPS_OK if motion is completed and Dome reached home position. Return IPS_BUSY if Dome started motion to home position and is in progress.
                Return IPS_ALERT if there is an error.
     */
    virtual IPState Home();

    /**
     * \brief Goto Park Position. The park position is an absolute azimuth value.
     * \return Return IPS_OK if motion is completed and Dome reached park position. Return IPS_BUSY if Dome started motion to park requested position and is in progress.
                Return -IPS_ALERT if there is an error.
     */
    virtual IPState Park();

    /**
     * \brief UnPark dome. The action of the Unpark command is dome specific, but it may include opening the shutter and moving to home position. When UnPark() is successful
     * The observatory should be in a ready state to utilize the mount to perform observations.
     * \return Return IPS_OK if motion is completed and Dome is unparked. Return IPS_BUSY if Dome unparking is in progress.
                Return -IPS_ALERT if there is an error.
     */
    virtual IPState UnPark();

    /**
     * \brief Open or Close shutter
     * \param operation Either open or close the shutter.
     * \return Return IPS_OK if shutter operation is complete. Return IPS_BUSY if shutter operation is in progress.
                Return IPS_ALERT if there is an error.
     */
    virtual IPState ControlShutter(ShutterOperation operation);

    /**
     * @brief getShutterStatusString
     * @param status Status of shutter
     * @return Returns string representation of the shutter status
     */
    const char * GetShutterStatusString(ShutterStatus status);

    /**
     * \brief setParkDataType Sets the type of parking data stored in the park data file and presented to the user.
     * \param type parking data type. If PARK_NONE then no properties will be presented to the user for custom parking position.
     */
    void SetParkDataType(DomeParkData type);

    /**
     * @brief InitPark Loads parking data (stored in ~/.indi/ParkData.xml) that contains parking status
     * and parking position. InitPark() should be called after successful connection to the dome on startup.
     * @return True if loading is successful and data is read, false otherwise. On success, you must call
     * SetAzParkDefault() to set the default parking values. On failure, you must call
     * SetAzParkDefault() to set the default parking values in addition to SetAzPark()
     * to set the current parking position.
     */
    bool InitPark();

    /**
     * @brief isParked is dome currently parked?
     * @return True if parked, false otherwise.
     */
    bool isParked();

    /**
     * @brief SetParked Change the mount parking status. The data park file (stored in ~/.indi/ParkData.xml) is updated in the process.
     * @param isparked set to true if parked, false otherwise.
     */
    void SetParked(bool isparked);

    /**
     * @return Get current AZ parking position.
     */
    double GetAxis1Park();

    /**
     * @return Get default AZ parking position.
     */
    double GetAxis1ParkDefault();

    /**
     * @brief SetRAPark Set current AZ parking position. The data park file (stored in ~/.indi/ParkData.xml) is updated in the process.
     * @param value current Axis 1 value (AZ either in angles or encoder values as specificed by the DomeParkData type).
     */
    void SetAxis1Park(double value);

    /**
     * @brief SetAxis1Park Set default AZ parking position.
     * @param value Default Axis 1 value (AZ either in angles or encoder values as specificed by the DomeParkData type).
     */
    void SetAxis1ParkDefault(double steps);

    /**
     * @brief SetCurrentPark Set current coordinates/encoders value as the desired parking position
     * \note This function performs no action unless subclassed by the child class if required.
     */
    virtual void SetCurrentPark();

    /**
     * @brief SetDefaultPark Set default coordinates/encoders value as the desired parking position
     * \note This function performs no action unless subclassed by the child class if required.
     */
    virtual void SetDefaultPark();

    //Park
    char *LoadParkData();
    bool WriteParkData();

    INumberVectorProperty DomeSpeedNP;
    INumber DomeSpeedN[1];

    ISwitchVectorProperty DomeMotionSP;
    ISwitch DomeMotionS[2];

    INumberVectorProperty DomeAbsPosNP;
    INumber DomeAbsPosN[1];

    INumberVectorProperty DomeRelPosNP;
    INumber DomeRelPosN[1];

    ISwitchVectorProperty AbortSP;
    ISwitch AbortS[1];

    ISwitchVectorProperty DomeGotoSP;
    ISwitch DomeGotoS[1];

    INumberVectorProperty DomeParamNP;
    INumber DomeParamN[2];

    ISwitchVectorProperty DomeShutterSP;
    ISwitch DomeShutterS[2];

    ISwitchVectorProperty ParkSP;
    ISwitch ParkS[2];

    INumber ParkPositionN[1];
    INumberVectorProperty ParkPositionNP;

    ISwitch ParkOptionS[3];
    ISwitchVectorProperty ParkOptionSP;

    DomeCapability capability;
    DomeState domeState;
    ShutterStatus shutterState;
    DomeParkData parkDataType;
    int last_dome_motion;

private:

    char DomeName[MAXINDIDEVICE];

    bool IsParked;
    const char *ParkDeviceName;
    const char * Parkdatafile;
    XMLEle *ParkdataXmlRoot, *ParkdeviceXml, *ParkstatusXml, *ParkpositionXml, *ParkpositionAxis1Xml;

    double Axis1ParkPosition;
    double Axis1DefaultParkPosition;    
};

#endif // INDIDomeINTERFACE_H
