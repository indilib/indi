/*
    INDI Driver for Optec TCF-S Focuser

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

#pragma once

#include "indifocuser.h"

#include <string>

/* Smart Widget-Property */
#include "indipropertynumber.h"
#include "indipropertyswitch.h"

#define TCFS_MAX_CMD      16
#define TCFS_ERROR_BUFFER 1024

class TCFS : public INDI::Focuser
{
  public:
    enum TCFSCommand
    {
        FMMODE, // Focuser Manual Mode
        FFMODE, // Focuser Free Mode
        FAMODE, // Focuser Auto-A Mode
        FBMODE, // Focuser Auto-B Mode
        FCENTR, // Focus Center
        FIN,    // Focuser In “nnnn”
        FOUT,   // Focuser Out “nnnn”
        FPOSRO, // Focuser Position Read Out
        FTMPRO, // Focuser Temperature Read Out
        FSLEEP, // Focuser Sleep
        FWAKUP, // Focuser Wake Up
        FHOME,  // Focuser Home Command        
        FRSLOP, // Focuser Read Slope Command
        FLSLOP, // Focuser Load Slope Command
        FQUIET, // Focuser Quiet Command
        FDELAY, // Focuser Load Delay Command
        FRSIGN, // Focuser Read Slope Sign Command
        FLSIGN, // Focuser Load Slope Sign Command
        FFWVER, // Focuser Firmware Version
    };

    enum TCFSMode
    {
    	MANUAL,
        MODE_A,
        MODE_B
    };
    
    enum TCFSError
    {
        NO_ERROR,
        ER_1,
        ER_2,
        ER_3
    };

    TCFS();
    virtual ~TCFS() = default;

    // Standard INDI interface fucntions
    virtual bool Handshake();
    virtual bool Disconnect();
    const char *getDefaultName();
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool saveConfigItems(FILE *fp);

  protected:
    virtual IPState MoveAbsFocuser(uint32_t targetTicks);
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
    virtual void TimerHit();
    void GetFocusParams();
    bool SetManualMode();

  private:
    bool read_tcfs(char *response, bool silent = false);
    bool dispatch_command(TCFSCommand command_type, int val=0, TCFSMode m=MANUAL);

    INDI::PropertyNumber FocusModeANP {3};
    INDI::PropertyNumber FocusModeBNP {3};
    INDI::PropertySwitch FocusTelemetrySP {2};
    INDI::PropertySwitch FocusModeSP {3};
    INDI::PropertySwitch FocusPowerSP {2};
    INDI::PropertySwitch FocusGotoSP {4};
    INDI::PropertyNumber FocusTemperatureNP {1};
    INDI::PropertySwitch FocusStartModeSP {3};

    unsigned int simulated_position { 3000 };
    float simulated_temperature { 25.4 };
    TCFSMode currentMode;

    unsigned int targetTicks { 0 };
    unsigned int targetPosition { 0 };
    bool isTCFS3 { false };
};
