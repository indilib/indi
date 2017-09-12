/*
    Rotator Interface
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include "indibase.h"
#include <stdint.h>

/**
 * \class INDI::RotatorInterface
   \brief Provides interface to implement Rotator functionality.

   A Rotator can be an independent device, or an embedded Rotator within another device (usually a rotating focuser). Child class must implement all the
   pure virtual functions. Only absolute position Rotators are supported.

   In order support Positoin Angle (-180 to +180, E of N), a multiplier and an offset are defined to the client to alter the raw angle values.

   Final_Angle = Raw_Angle * Multiplier + Offset

   By default, multipler = 1 and offset = 0. All internal calls are made using Final_Angle (Position Angle). The final angle is calculated and sent to clients.

   \e IMPORTANT: initRotatorProperties() must be called before any other function to initilize the Rotator properties.

   \e IMPORTANT: processRotatorNumber() must be called in your driver's ISNewNumber() function. Similary, processRotatorSwitch() must be called in ISNewSwitch()

\author Jasem Mutlaq
*/
class INDI::RotatorInterface
{
  public:    

    /**
     * \struct RotatorCapability
     * \brief Holds the capabilities of a Rotator.
     */
    enum
    {
        ROTATOR_CAN_ABORT          = 1 << 0, /** Can the Rotator abort motion once started? */
        ROTATOR_CAN_HOME           = 1 << 1, /** Can the Rotator go to home position? */
        ROTATOR_CAN_SYNC           = 1 << 2, /** Can the Rotator sync to specific tick? */
    } RotatorCapability;

    /**
     * @brief GetRotatorCapability returns the capability of the Rotator
     */
    uint32_t GetRotatorCapability() const { return rotatorCapability; }

    /**
     * @brief SetRotatorCapability sets the Rotator capabilities. All capabilities must be initialized.
     * @param cap pointer to Rotator capability struct.
     */
    void SetRotatorCapability(uint32_t cap) { rotatorCapability = cap; }

    /**
     * @return Whether Rotator can abort.
     */
    bool CanAbort() { return RotatorCapability & ROTATOR_CAN_ABORT;}

    /**
     * @return Whether Rotator can go to home position.
     */
    bool CanHome()  { return RotatorCapability & ROTATOR_CAN_HOME;}

    /**
     * @return Whether Rotator can sync ticks position to a new one.
     */
    bool CanSync()  { return RotatorCapability & ROTATOR_CAN_SYNC;}

protected:

    RotatorInterface();

    /**
     * \brief Initilize Rotator properties. It is recommended to call this function within
     * initProperties() of your primary device
     * \param groupName Group or tab name to be used to define Rotator properties.
     */
    void initProperties(INDI::DefaultDevice *defaultDevice, const char *groupName);

    /**
     * @brief updateRotatorProperties Define or Delete Rotator properties based on the connection status of the base device
     * @return True if successful, false otherwise.
     */
    bool updateProperties(INDI::DefaultDevice *defaultDevice);


    /**
     * @brief GotoRotatorTicks Go to an absolute position.
     * @param ticks Target ticks.
     * @return State of operation: IPS_OK is motion is completed, IPS_BUSY if motion in progress, IPS_ALERT on error.
     */
    virtual IPState MoveAbsRotator(uint32_t ticks) = 0;

    /**
     * @brief GotoRotatorAngle Go to specific position angle. Child class can decode raw angle from the current multiplier and offset settings.
     * @param angle Target position angle in degrees.
     * @return State of operation: IPS_OK is motion is completed, IPS_BUSY if motion in progress, IPS_ALERT on error.
     */
    virtual IPState MoveAngleRotator(double angle) = 0;

    /**
     * @brief SyncRotator Set current absolute position as the supplied ticks.
     * @param ticks Desired new position.
     * @return True if successful, false otherwise.
     */
    virtual bool SyncRotator(uint32_t ticks);

    /**
     * @brief HomeRotator Go to home position.
     * @return State of operation: IPS_OK is motion is completed, IPS_BUSY if motion in progress, IPS_ALERT on error.
     */
    virtual IPState HomeRotator();

    /**
     * @brief AbortRotator Abort all motion
     * @return True if successful, false otherwise.
     */
    virtual bool AbortRotator();

    /**
     * @brief saveRotatorConfig Save Rotator properties in configuration file
     * @param fp Pointer to configuration file
     * @return True if successful, false otherwise.
     */
    bool saveRotatorConfig(FILE *fp);

    /** \brief Process Rotator number properties */
    bool processRotatorNumber(const char *dev, const char *name, double values[], char *names[], int n);

    /** \brief Process Rotator switch properties */
    bool processRotatorSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

    INumber RotatorAbsPosN[1];
    INumberVectorProperty RotatorAbsPosNP;

    INumber RotatorPositionAngleN[1];
    INumberVectorProperty RotatorPositionAngleNP;

    INumber SyncRotatorN[1];
    INumberVectorProperty SyncRotatorNP;

    ISwitch AbortRotatorS[1];
    ISwitchVectorProperty AbortRotatorSP;

    ISwitch HomeRotatorS[1];
    ISwitchVectorProperty HomeRotatorSP;

    INumber RotatorAngleSettingN[2];
    INumberVectorProperty RotatorAngleSettingNP;

    uint32_t rotatorCapability = 0;
    char rotatorName[MAXINDIDEVICE];
};
