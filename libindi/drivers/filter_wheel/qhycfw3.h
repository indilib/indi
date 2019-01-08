/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

  Tru Technology Filter Wheel

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

#pragma once

#include "indifilterwheel.h"

class QHYCFW3 : public INDI::FilterWheel
{
  public:
    QHYCFW3();

  protected:
    const char *getDefaultName() override;
    bool initProperties() override;

    bool Handshake() override;
    bool SelectFilter(int) override;
};
