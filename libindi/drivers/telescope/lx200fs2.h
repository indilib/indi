/*    
    Astro-Electronic FS-2
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#ifndef LX200FS2_H
#define LX200FS2_H

#include "lx200generic.h"

class LX200FS2 : public LX200Generic
{
public:

    LX200FS2();
    ~LX200FS2() {}

    virtual bool initProperties();
    virtual bool updateProperties();
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

protected:

    virtual const char *getDefaultName();
    virtual bool isSlewComplete();
    virtual bool checkConnection();

    virtual bool saveConfigItems(FILE *fp);

    INumber SlewAccuracyN[2];
    INumberVectorProperty SlewAccuracyNP;

};


#endif
