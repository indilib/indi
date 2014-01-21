/*
    Filter Interface
    Copyright (C) 2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef INDIFOCUSERINTERFACE_H
#define INDIFOCUSERINTERFACE_H

#include "indibase.h"
#include "indiapi.h"

/**
 * \class INDI::FocuserInterface
   \brief Provides interface to implement focuser functionality.

   A focuser can be an independent device, or an embedded focuser within another device (e.g. Camera).

   \e IMPORTANT: initFocuserProperties() must be called before any other function to initilize the focuser properties.

   \e IMPORTANT: processFocuserNumber() and processFocuserSwitch() must be called in your driver's ISNewNumber() and ISNewSwitch functions recepectively.
\author Jasem Mutlaq
*/
class INDI::FocuserInterface
{

public:
    enum FocusDirection { FOCUS_INWARD, FOCUS_OUTWARD };

    /**
     * @brief setFocuserFeatures Sets Focuser features
     * @param CanAbsMove can the focuser move by absolute position?
     * @param CanRelMove can the focuser move by relative position?
     * @param CanAbort is it possible to abort focuser motion?
     * @param VariableSpeed Can you control the focuser motor speed?
     */
    void setFocuserFeatures(bool CanAbsMove, bool CanRelMove, bool CanAbort, bool VariableSpeed);

protected:

    FocuserInterface();
    virtual ~FocuserInterface();

    /** \brief Initilize focuser properties. It is recommended to call this function within initProperties() of your primary device
        \param deviceName Name of the primary device
        \param groupName Group or tab name to be used to define focuser properties.
    */
    void initFocuserProperties(const char *deviceName, const char* groupName);

    /** \brief Process focus number properties */
    bool processFocuserNumber (const char *dev, const char *name, double values[], char *names[], int n);

    /** \brief Process focus switch properties */
    bool processFocuserSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    /**
     * @brief SetSpeed Set Focuser speed
     * @param speed focuser speed
     * @return true if successful, false otherwise
     */
    virtual bool SetSpeed(int speed);

    /** \brief Move the focuser in a particular direction with a specific speed for a finite duration.
        \param dir Direction of focuser, either FOCUS_INWARD or FOCUS_OUTWARD.
        \param speed Speed of focuser if supported by the focuser.
        \param duration The timeout in milliseconds before the focus motion halts.
        \return Return 0 if motion is completed and focuser reached requested position. Return 1 if focuser started motion to requested position and is in progress.
                Return -1 if there is an error.
    */
    virtual int Move(FocusDirection dir, int speed, int duration);

    /** \brief Move the focuser to an absolute position.
        \param ticks The new position of the focuser.
        \return Return 0 if motion is completed and focuser reached requested position. Return 1 if focuser started motion to requested position and is in progress.
                Return -1 if there is an error.
    */
    virtual int MoveAbs(int ticks);

    /** \brief Move the focuser to an relative position.
        \param dir Direction of focuser, either FOCUS_INWARD or FOCUS_OUTWARD.
        \param ticks The relative ticks to move.
        \return Return 0 if motion is completed and focuser reached requested position. Return 1 if focuser started motion to requested position and is in progress.
                Return -1 if there is an error.
    */
    virtual int MoveRel(FocusDirection dir, unsigned int ticks);

    /**
     * @brief Abort all focus motion
     * @return True if abort is successful, false otherwise.
     */
    virtual bool Abort();

    INumberVectorProperty FocusSpeedNP;
    INumber FocusSpeedN[1];
    ISwitchVectorProperty FocusMotionSP; //  A Switch in the client interface to park the scope
    ISwitch FocusMotionS[2];
    INumberVectorProperty FocusTimerNP;
    INumber FocusTimerN[1];
    INumberVectorProperty FocusAbsPosNP;
    INumber FocusAbsPosN[1];
    INumberVectorProperty FocusRelPosNP;
    INumber FocusRelPosN[1];
    ISwitchVectorProperty AbortSP;
    ISwitch AbortS[1];

    bool canAbort, canAbsMove, canRelMove, variableSpeed;

    char focuserName[MAXINDIDEVICE];

};

#endif // INDIFOCUSERINTERFACE_H
