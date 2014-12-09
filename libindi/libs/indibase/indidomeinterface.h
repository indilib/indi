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

   A Dome can be an independent device, or an embedded Dome within another device (e.g. Telescope). Before using any of the dome functions, you must define the capabilities of the dome and calling
   SetDomeCapability() function. All positions are represented as degrees of azimuth.

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
    enum DomeParam { DOME_HOME, DOME_PARK, DOME_AUTOSYNC };

    /** \typedef ShutterOperation
        \brief Shutter operation command.
    */
    typedef enum
    {
        SHUTTER_OPEN,            /*!< Open Shutter */
        SHUTTER_CLOSE            /*!< Close Shutter */
    } ShutterOperation;

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
        /** Can the dome move to a relative position a number of degrees away from current position? */
        bool canRelMove;
        /** Does the dome has a shutter than can be opened and closed electronically? */
        bool hasShutter;
        /** Can the dome move in different configurable speeds? */
        bool variableSpeed;
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
     * @param speed Dome speed (RPM)
     * @return true if successful, false otherwise
     */
    virtual bool SetDomeSpeed(double rpm);

    /** \brief Move the Dome in a particular direction with a specific speed for a finite duration.
        \param dir Direction of Dome, either DOME_CW or DOME_CCW.
        \param speed Speed (RPM) of dome if supported by the dome.
        \param duration The timeout in milliseconds before the dome motion halts.
        \return Return 0 if motion is completed and Dome reached requested position. Return 1 if Dome started motion to requested position and is in progress.
                Return -1 if there is an error.
    */
    virtual int MoveDome(DomeDirection dir, double speed, int duration);

    /** \brief Move the Dome to an absolute azimuth.
        \param ticks The new position of the Dome.
        \return Return 0 if motion is completed and Dome reached requested position. Return 1 if Dome started motion to requested position and is in progress.
                Return -1 if there is an error.
    */
    virtual int MoveAbsDome(double az);

    /** \brief Move the Dome to an relative position.
        \param dir Direction of Dome, either DOME_CW or DOME_CCW.
        \param azDiff The relative azimuth angle to move.
        \return Return 0 if motion is completed and Dome reached requested position. Return 1 if Dome started motion to requested position and is in progress.
                Return -1 if there is an error.
    */
    virtual int MoveRelDome(DomeDirection dir, double azDiff);

    /**
     * \brief Abort all dome motion
     * \return True if abort is successful, false otherwise.
     */
    virtual bool AbortDome();

    /**
     * \brief Goto Home Position. The home position is an absolute azimuth value.
     * \return Return 0 if motion is completed and Dome reached home position. Return 1 if Dome started motion to home position and is in progress.
                Return -1 if there is an error.
     */
    virtual int HomeDome();

    /**
     * \brief Goto Park Position. The park position is an absolute azimuth value.
     * \return Return 0 if motion is completed and Dome reached park position. Return 1 if Dome started motion to park requested position and is in progress.
                Return -1 if there is an error.
     */
    virtual int ParkDome();

    /**
     * \brief Open or Close shutter
     * \param operation Either open or close the shutter.
     * \return Return 0 if shutter operation is complete. Return 1 if shutter operation is in progress.
                Return -1 if there is an error.
     */
    virtual int ControlDomeShutter(ShutterOperation operation);

    /**
     * @brief getShutterStatusString
     * @param status Status of shutter
     * @return Returns string representation of the shutter status
     */
    const char * GetShutterStatusString(ShutterStatus status);

    INumberVectorProperty DomeSpeedNP;
    INumber DomeSpeedN[1];
    ISwitchVectorProperty DomeMotionSP;
    ISwitch DomeMotionS[2];
    INumberVectorProperty DomeTimerNP;
    INumber DomeTimerN[1];
    INumberVectorProperty DomeAbsPosNP;
    INumber DomeAbsPosN[1];
    INumberVectorProperty DomeRelPosNP;
    INumber DomeRelPosN[1];
    ISwitchVectorProperty AbortSP;
    ISwitch AbortS[1];
    ISwitchVectorProperty DomeGotoSP;
    ISwitch DomeGotoS[2];
    INumberVectorProperty DomeParamNP;
    INumber DomeParamN[3];
    ISwitchVectorProperty DomeShutterSP;
    ISwitch DomeShutterS[2];

    DomeCapability capability;
    ShutterStatus shutterStatus;
    char DomeName[MAXINDIDEVICE];

};

#endif // INDIDomeINTERFACE_H
