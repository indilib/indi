/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
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

        INumberVectorProperty FocusspeedNV;
        INumber FocusspeedN[1];
        ISwitchVectorProperty FocusmotionSV; //  A Switch in the client interface to park the scope
        ISwitch FocusmotionS[2];
        INumberVectorProperty FocustimerNV;
        INumber FocustimerN[1];


        virtual bool initProperties();
        virtual void ISGetProperties (const char *dev);
        bool updateProperties();


        //  Ok, we do need our virtual functions from the base class for processing
        //  client requests
        //  We process Numbers in a focusser
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

        //  And here are the virtual functions we will have for easy overrides
        virtual int Move(int, int, int);

};

#endif // INDIFOCUSSER_H
