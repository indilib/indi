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

#ifndef FILTERSIM_H
#define FILTERSIM_H

#include "indibase/indifilterwheel.h"

class FilterSim : public INDI::FilterWheel
{
    protected:
    private:
    public:
        FilterSim();
        virtual ~FilterSim();

        const char *getDefaultName();

        bool Connect();
        bool Disconnect();
        bool SelectFilter(int);
        void TimerHit();

        virtual bool SetFilterNames() { return false; }
        virtual bool GetFilterNames(const char *deviceName, const char* groupName) { return false; }


};

#endif // FILTERSIM_H
