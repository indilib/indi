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

#ifndef TCFS_H
#define TCFS_H

#include <string>

#include <indidevapi.h>
#include <indicom.h>
#include <indibase/indifocuser.h>

using namespace std;

#define TCFS_MAX_CMD        16
#define TCFS_MAX_TRIES      3
#define TCFS_ERROR_BUFFER   1024

class TCFS : public INDI::Focuser
{

public:

    enum TCFSCommand {         FMMODE ,         // Focuser Manual Mode
                               FFMODE,          // Focuser Free Mode
                               FAMODE,          // Focuser Auto-A Mode
                               FBMODE,          // Focuser Auto-B Mode
                               FCENTR,          // Focus Center
                               FIN,             // Focuser In “nnnn”
                               FOUT,            // Focuser Out “nnnn”
                               FPOSRO,          // Focuser Position Read Out
                               FTMPRO,          // Focuser Temperature Read Out
                               FSLEEP,          // Focuser Sleep
                               FWAKUP,          // Focuser Wake Up
                               FHOME,           // Focuser Home Command
			     };

    enum TCFSMode { TCFS_MANUAL_MODE, TCFS_A_MODE, TCFS_B_MODE };
    enum TCFSError { NO_ERROR, ER_1, ER_2, ER_3 };

    TCFS();
    ~TCFS();
   
    // Standard INDI interface fucntions
    virtual bool Connect();
    virtual bool Disconnect();
    const char *getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

protected:
    virtual int MoveAbs(int ticks);
    virtual int MoveRel(FocusDirection dir, unsigned int ticks);
    virtual void TimerHit();

private: 

    ISwitchVectorProperty *FocusPowerSP;
    ISwitchVectorProperty *FocusModeSP;
    ISwitchVectorProperty *FocusGotoSP;
    INumberVectorProperty *FocusTemperatureNP;

    bool read_tcfs(bool silent=false);
    bool dispatch_command(TCFSCommand command);

    // Variables
    string default_port;
			
    int fd;
    char command[TCFS_MAX_CMD];
    char response[TCFS_MAX_CMD];

    unsigned int simulated_position;
    float simulated_temperature;

    unsigned int targetTicks, targetPosition;
    TCFSCommand currentCommand;
    bool isTCFS3;

};

#endif


