/*
  INDI Driver for DreamFocuser

  Copyright (C) 2016 Piotr Dlugosz

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

#ifndef DREAMFOCUSER_H
#define DREAMFOCUSER_H

#include <string>

#include <indidevapi.h>
#include <indicom.h>
#include <indifocuser.h>

using namespace std;

#define DREAMFOCUSER_STEP_SIZE      32
#define DREAMFOCUSER_ERROR_BUFFER   1024


class DreamFocuser : public INDI::Focuser
{

    public:

        struct DreamFocuserCommand
        {
            char M = 'M';
            char k;
            unsigned char a;
            unsigned char b;
            unsigned char c;
            unsigned char d;
            unsigned char addr = '\0';
            unsigned char z;
        };

        DreamFocuser();

        const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        //virtual bool saveConfigItems(FILE *fp) override;
        //virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool Handshake() override;
        virtual void TimerHit() override;
        virtual bool SyncFocuser(uint32_t ticks) override;

        virtual IPState MoveAbsFocuser(uint32_t ticks) override;
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
        virtual bool AbortFocuser() override;

    private:

        INDI::PropertyNumber TemperatureNP {1};

        INDI::PropertyNumber WeatherNP {2};
        enum
        {
            FOCUS_HUMIDITY,
            FOCUS_DEWPOINT
        };

        INDI::PropertySwitch ParkSP {2};

        INDI::PropertySwitch StatusSP {3};
        enum
        {
          ABSOLUTE,
          MOVING,
          PARKED
        };

        //INumber SetBacklashN[1];
        //INumberVectorProperty SetBacklashNP;

        unsigned char calculate_checksum(DreamFocuserCommand c);
        bool send_command(char k, uint32_t l = 0, unsigned char addr = 0);
        bool read_response();
        bool dispatch_command(char k, uint32_t l = 0, unsigned char addr = 0);

        bool getTemperature();
        bool getStatus();
        bool getPosition();
        bool getMaxPosition();
        bool setPosition(int32_t position);
        bool setSync(uint32_t position = 0);
        bool setPark();

       // Variables
        float currentTemperature;
        float currentHumidity;
        int32_t currentPosition;
        int32_t currentMaxPosition;
        bool isAbsolute;
        bool isMoving;
        unsigned char isParked;
        bool isVcc12V;
        DreamFocuserCommand currentResponse;
};

#endif
