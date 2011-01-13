#ifndef INDIFILTERWHEEL_H
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
#define INDIFILTERWHEEL_H

#include "IndiDevice.h"

class IndiFilterWheel: public IndiDevice
{
    protected:
    private:

    public:
        IndiFilterWheel();
        virtual ~IndiFilterWheel();

        INumberVectorProperty FilterSlotNV;   //  A number vector for filter slot
        INumber FilterSlotN[1];

        ITextVectorProperty FilterNameTV; //  A text vector that stores out physical port name
        IText FilterNameT[12];

        int MinFilter;
        int MaxFilter;
        int CurrentFilter;
        int TargetFilter;


        virtual int init_properties();
        virtual bool UpdateProperties();
        virtual void ISGetProperties (const char *dev);

        //  Ok, we do need our virtual functions from the base class for processing
        //  client requests
        //  We process Numbers and text in a filter wheel
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

        virtual int SelectFilter(int);
        virtual int SelectFilterDone(int);
        virtual int QueryFilter();
        //int SaveFilterNames();
        //int LoadFilterNames();
        virtual bool WritePersistentConfig(FILE *);


};

#endif // INDIFILTERWHEEL_H
