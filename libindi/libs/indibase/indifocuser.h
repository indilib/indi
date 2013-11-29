/*******************************************************************************
  Copyright(c) 2013 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef INDIFOCUSSER_H
#define INDIFOCUSSER_H

#include "defaultdevice.h"

/**
 * \class INDI::Focuser
   \brief Class to provide general functionality of a focuser device.

   Both relative and absolute focuser supported. Furthermore, if no position feedback is available from the focuser,
   an open-loop control is possible using timers, speed presets, and direction of motion.
   Developers need to subclass INDI::Focuser to implement any driver for focusers within INDI.

\author Jasem Mutlaq
\author Gerry Rozema
*/
class INDI::Focuser : public INDI::DefaultDevice
{
    public:
        Focuser();
        virtual ~Focuser();

        enum FocusDirection { FOCUS_INWARD, FOCUS_OUTWARD };

        virtual bool initProperties();
        virtual void ISGetProperties (const char *dev);
        virtual bool updateProperties();
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISSnoopDevice (XMLEle *root);

    protected:

        /** \brief Move the focuser in a particular direction with a specific speed for a finite duration.
            \param dir Direction of focuser, either FOCUS_INWARD or FOCUS_OUTWARD.
            \param speed Speed of focuser if supported by the focuser.
            \param duration The timeout in milliseconds before the focus motion halts.
            \return True if succssfull, false otherwise.
        */
        virtual bool Move(FocusDirection dir, int speed, int duration);

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

};

#endif // INDIFOCUSSER_H
