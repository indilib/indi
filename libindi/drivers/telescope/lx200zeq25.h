/*
    ZEQ25 INDI driver

    Copyright (C) 2015 Jasem Mutlaq

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

#ifndef LX200ZEQ25_H
#define LX200ZEQ25_H

#include "lx200generic.h"

class LX200ZEQ25 : public LX200Generic
{
public:

    LX200ZEQ25();
    ~LX200ZEQ25() {}

    virtual bool updateProperties();

protected:

    virtual const char *getDefaultName();
    virtual bool checkConnection();
    virtual bool isSlewComplete();


private:
    bool getMountInfo();

};

#endif
