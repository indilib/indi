/*
    Pyrix Pyrix Focuser & Rotator
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include "indirotator.h"

class Pyrix : public INDI::Rotator
{
  public:

    Pyrix();
    virtual ~Pyrix() = default;

    virtual bool Handshake();
    const char * getDefaultName();
    virtual bool initProperties();

  protected:
    // Rotator Overrides
    virtual IPState HomeRotator();
    virtual IPState MoveRotator(double angle);
    virtual bool SyncRotator(double angle);
    virtual bool ReverseRotator(bool enabled);

    // Misc.
    virtual void TimerHit();

  private:    
    // Check if connection is OK
    bool Ack();    
    bool isHomingComplete();

    uint32_t lastRotatorPosition { 0 };
    uint32_t targetPosition { 0 };

};
