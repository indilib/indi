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
#include <indibase/defaultdriver.h>

using namespace std;

#define TCFS_MAX_CMD        16
#define TCFS_MAX_TRIES      3
#define TCFS_ERROR_BUFFER   1024

class TCFS : public INDI::DefaultDriver
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
   
    bool connect();
    void disconnect();

    // Standard INDI interface fucntions
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *name, double values[], char *names[], int n);
    virtual bool ISNewText (const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch (const char *name, ISState *states, char *names[], int n);
 	

    void ISPoll();


private: 

    ISwitchVectorProperty *ConnectSP;
    INumberVectorProperty *FocusStepNP;
    INumberVectorProperty *FocusInfoNP;

    // Functions
    void init_properties();

    bool read_tcfs();
    bool dispatch_command(TCFSCommand command);

    // Variables
    string default_port;
			
    int fd;
    char command[TCFS_MAX_CMD];
    char response[TCFS_MAX_CMD];

    unsigned int simulated_position;
    unsigned int target_position;
    float simulated_temperature;

};

#endif


