/*******************************************************************************
  Copyright(c) 2010, 2011 Gerry Rozema. All rights reserved.

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

#ifndef INDI_FILTERWHEEL_H
#define INDI_FILTERWHEEL_H

#include "defaultdriver.h"
#include "indifilterinterface.h"

class INDI::FilterWheel: public INDI::DefaultDriver, public INDI::FilterInterface
{
protected:

    FilterWheel();
    virtual ~FilterWheel();

public:


        virtual bool initProperties();
        virtual bool updateProperties();
        virtual void ISGetProperties (const char *dev);

        //  Ok, we do need our virtual functions from the base class for processing
        //  client requests
        //  We process Numbers and text in a filter wheel
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

        virtual int QueryFilter();
        virtual bool SelectFilter(int);
        virtual bool SetFilterNames();
        virtual bool initFilterNames(const char *deviceName, const char* groupName);
        virtual void ISSnoopDevice (XMLEle *root);

};

#endif // INDI::FilterWheel_H
