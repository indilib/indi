/*******************************************************************************
  Copyright(c) 2011 Gerry Rozema. All rights reserved.

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

#include "defaultdriver.h"

class INDI::Focuser : public INDI::DefaultDriver
{

    protected:
    private:

    public:
        Focuser();
        virtual ~Focuser();

        enum FocusDirection { FOCUS_INWARD, FOCUS_OUTWARD };

        INumberVectorProperty *FocusSpeedNP;
        INumber FocusSpeedN[1];
        ISwitchVectorProperty *FocusMotionSP; //  A Switch in the client interface to park the scope
        ISwitch FocusMotionS[2];
        INumberVectorProperty *FocusTimerNP;
        INumber FocusTimerN[1];


        virtual bool initProperties();
        virtual void ISGetProperties (const char *dev);
        bool updateProperties();


        //  Ok, we do need our virtual functions from the base class for processing
        //  client requests
        //  We process Numbers in a focusser
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual void ISSnoopDevice (XMLEle *root);


        //  And here are the virtual functions we will have for easy overrides
        virtual bool Move(FocusDirection dir, int speed, int duration);

};

#endif // INDIFOCUSSER_H
