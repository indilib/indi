/*******************************************************************************
 Copyright(c) 2016 Philippe Besson. All rights reserved.

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

#ifndef FILTERIFW_H
#define FILTERIFW_H

#include <indifilterwheel.h>
#include <indicom.h>
#include <regex>

#include <iostream>
#include <unistd.h>
#include <termios.h>
#include <memory>

#define VERSION                 0
#define SUBVERSION              2

#define OPTEC_MAX_RETRIES		2
#define OPTEC_TIMEOUT			5
#define OPTEC_TIMEOUT_MOVE		10
#define OPTEC_TIMEOUT_WHOME		40
#define OPTEC_TIMEOUT_FIRMWARE  1
#define OPTEC_MAXBUF			16

#define OPTEC_MAX_FILTER        9
#define OPTEC_LEN_FLTNAME       8
#define OPTEC_MAXLEN_CMD        ((OPTEC_MAX_FILTER) * OPTEC_LEN_FLTNAME) + 10
#define OPTEC_MAXLEN_RESP       OPTEC_MAX_FILTER * OPTEC_LEN_FLTNAME
#define OPTEC_MAXLEN_NAMES      OPTEC_MAX_FILTER * OPTEC_LEN_FLTNAME

#define OPTEC_WAIT_DATA_OK      5

#define filterSim5              "RED     GREEN   BLUE    H-ALPHA LIGHT   "
#define filterSim6              "RED     GREEN   BLUE    H-ALPHA LIGHT   OIII    "
#define filterSim8              "RED     GREEN   BLUE    H-ALPHA LIGHT   OIII    IR-CUT  SII     "
#define filterSim9              "RED     GREEN   BLUE    H-ALPHA LIGHT   OIII    IR-CUT  SII     ORANGE  "

/*******************************************************************************
Define text message error from IFW
*******************************************************************************/
#define MER1	"the number of steps to find position 1 is excessive"
#define MER2	"the SBIG pulse does not have the proper width for the IFW"
#define MER3	"the filter ID is not found/send successfully"
#define MER4	"the wheel is stuck in a position"
#define MER5	"the filter number is not in the set (1, 2, 3, 4, 5)"
#define MER6	"the wheel is slipping and takes too many steps to the next position"
#define MERO	"Unknown error received from IFW"


#define PRINT_ER(error)	 \
    if (!strcmp(error, "ER=1")) DEBUGF(INDI::Logger::DBG_ERROR, "%s -> %s", error, MER1); \
    else if (!strcmp(error, "ER=2")) DEBUGF(INDI::Logger::DBG_ERROR, "%s -> %s", error, MER2); \
        else if (!strcmp(error, "ER=3")) DEBUGF(INDI::Logger::DBG_ERROR, "%s -> %s", error, MER3); \
            else if (!strcmp(error, "ER=4")) DEBUGF(INDI::Logger::DBG_ERROR, "%s -> %s", error, MER4); \
                else if (!strcmp(error, "ER=5")) DEBUGF(INDI::Logger::DBG_ERROR, "%s -> %s", error, MER5); \
                    else if (!strcmp(error, "ER=6")) DEBUGF(INDI::Logger::DBG_ERROR, "%s -> %s", error, MER6); \
                        else if (!strcmp(error, "ER=0")) DEBUGF(INDI::Logger::DBG_ERROR, "%s -> %s", error, MERO);

#define DEBUGTAG() \
    DEBUGF(INDI::Logger::DBG_EXTRA_1, "DEBUG -> Function %s() is executing", __FUNCTION__);

/*******************************************************************************
* Class FilterIFW
*******************************************************************************/
class FilterIFW : public INDI::FilterWheel
{
    private:
    public:

        FilterIFW();
        virtual ~FilterIFW();

        virtual bool initProperties();
        virtual void ISGetProperties (const char *dev);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:
        virtual bool updateProperties();
        virtual bool Connect();
        virtual bool Disconnect();
        virtual bool WriteTTY(char* command);
        virtual bool ReadTTY(char* resp, char* simulation, int timeout);
        virtual const char *getDefaultName();
        virtual bool moveHome();
        virtual bool SelectFilter(int);
        virtual void TimerHit();
        virtual bool saveConfigItems(FILE *fp);
        virtual void simulationTriggered(bool enable);
        virtual bool loadConfig(bool silent = false, const char* property = NULL);
        virtual bool SetFilterNames();
        virtual bool GetFilterNames(const char* groupName);
        virtual bool GetWheelID();
        virtual int GetFilterPos();
        virtual bool GetFirmware();

        // Device physical port
        ITextVectorProperty PortTP;
        IText PortT[1];

        // Filter Wheel ID
        ITextVectorProperty WheelIDTP;
        IText WheelIDT[1];

        // Home function
        ISwitchVectorProperty HomeSP;
        ISwitch HomeS[1];

		//Simulation, number of filter function
        ISwitchVectorProperty FilterNbrSP;
        ISwitch FilterNbrS[4];

        // CharSet unrestricted for FilterNames
        ISwitchVectorProperty CharSetSP;
        ISwitch CharSetS[2];

        // Firmware of teh IFW
        ITextVectorProperty FirmwareTP;
        IText FirmwareT[1];

		//Communication file descriptor
        int PortFD = 0;
		
		//Filter position in simulation mode
        int actualSimFilter = 1;

		// Filter name list for simulation
        char filterSim[OPTEC_MAXLEN_NAMES + 1];

        // Filterwheel has been changed
        bool isWheelChanged = true;
};

#endif // FILTERIFW_H
