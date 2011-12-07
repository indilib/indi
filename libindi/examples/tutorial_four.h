/*
    Tutorial Four
    Copyright (C) 2010 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef TUTORIAL_FOUR_H
#define TUTORIAL_FOUR_H

#include "indidevapi.h"
#include "indicom.h"
#include "indibase/defaultdriver.h"

class MyScope : public INDI::DefaultDriver
{
 public:
 MyScope();
 ~MyScope();

 virtual void ISGetProperties (const char *dev);
 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

private:
 const char *getDefaultName();
 virtual bool initProperties();
 virtual bool Connect(char *msg);
 virtual bool Disconnect();

};

#endif
