/*******************************************************************************
  Copyright(c) 2021 Chrysikos Efstathios. All rights reserved.

  Pegasus ProdigyMF

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/
#pragma once

#include "indifocuser.h"

class PegasusProdigyMF : public INDI::Focuser
{
    public:
        PegasusProdigyMF();
        virtual ~PegasusProdigyMF() override = default;

        virtual bool Handshake() override;
        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

        virtual bool SyncFocuser(uint32_t ticks) override;
        virtual bool ReverseFocuser(bool enabled) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        bool updateFocusParams();
        bool move(uint32_t newPosition);
        bool setMaxSpeed(uint16_t speed);
        bool ack();
        void ignoreResponse();

        uint32_t currentPosition { 0 };
        uint32_t targetPosition { 0 };
        bool isMoving = false;

        // Temperature probe
        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;


        // Maximum Speed
        INumber MaxSpeedN[1];
        INumberVectorProperty MaxSpeedNP;

        // Firmware Version
        IText FirmwareVersionT[1] {};
        ITextVectorProperty FirmwareVersionTP;
};
