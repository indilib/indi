/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

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

#ifndef SYNSCANMOUNT_H
#define SYNSCANMOUNT_H

#include "indibase/inditelescope.h"


class SynscanMount : public INDI::Telescope
{
    protected:
    private:
    public:
        SynscanMount();
        virtual ~SynscanMount();

        //  overrides of base class virtual functions
        bool initProperties();
        void ISGetProperties (const char *dev);
        const char *getDefaultName();

        //virtual bool Connect() {return true;}
        bool ReadScopeStatus();
        bool Goto(double,double);
        bool Park();


};

#endif // SYNSCANMOUNT_H
