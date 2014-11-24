/*******************************************************************************
 INDI Dome Base Class
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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

#ifndef INDIDOME_H
#define INDIDOME_H

#include "defaultdevice.h"
#include "indicontroller.h"
#include "indidomeinterface.h"

/**
 * \class INDI::Dome
   \brief Class to provide general functionality of a Dome device.

   Both relative and absolute Dome supported. Furthermore, if no position feedback is available from the Dome,
   an open-loop control is possible using timers, speed presets, and direction of motion.
   Developers need to subclass INDI::Dome to implement any driver for Domes within INDI.

\author Jasem Mutlaq
\author Gerry Rozema
*/
class INDI::Dome : public INDI::DefaultDevice, public INDI::DomeInterface
{
    public:
        Dome();
        virtual ~Dome();

        virtual bool initProperties();
        virtual void ISGetProperties (const char *dev);
        virtual bool updateProperties();
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISSnoopDevice (XMLEle *root);

        static void buttonHelper(const char * button_n, ISState state, void *context);

    protected:

        /**
         * @brief saveConfigItems Saves the Device Port and Dome Presets in the configuration file
         * @param fp pointer to configuration file
         * @return true if successful, false otherwise.
         */
        virtual bool saveConfigItems(FILE *fp);

        ITextVectorProperty PortTP;
        IText PortT[1];

        INumber PresetN[3];
        INumberVectorProperty PresetNP;
        ISwitch PresetGotoS[3];
        ISwitchVectorProperty PresetGotoSP; 

        void processButton(const char * button_n, ISState state);

        INDI::Controller *controller;

};

#endif // INDIDOME_H
