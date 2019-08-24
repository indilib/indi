/*******************************************************************************
  Copyright(c) 2018 Franck Le Rhun. All rights reserved.
  Copyright(c) 2018 Christian Liska. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "lacerta_mfoc.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>

// We declare an auto pointer to lacerta_mfoc.
static std::unique_ptr<lacerta_mfoc> Lacerta_mfoc(new lacerta_mfoc());

// Delay for receiving messages
#define FOCUSMFOC_TIMEOUT  1000
// According to documentation for v2
#define MFOC_POSMAX_HARDWARE 250000
#define MFOC_POSMIN_HARDWARE 300

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    Lacerta_mfoc->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    Lacerta_mfoc->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    Lacerta_mfoc->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    Lacerta_mfoc->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    Lacerta_mfoc->ISSnoopDevice(root);
}

/************************************************************************************
 *
************************************************************************************/
lacerta_mfoc::lacerta_mfoc()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_HAS_BACKLASH);
}

/************************************************************************************
 *
************************************************************************************/
const char *lacerta_mfoc::getDefaultName()
{
    return "Lacerta MFOC";
}

/************************************************************************************
 *
************************************************************************************/
void lacerta_mfoc::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Focuser::ISGetProperties(dev);

    defineSwitch(&TempTrackDirSP);
    loadConfig(true, TempTrackDirSP.name);

    defineSwitch(&StartSavedPositionSP);
    loadConfig(true, StartSavedPositionSP.name);
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc::initProperties()
{
    INDI::Focuser::initProperties();

    FocusBacklashN[0].min = 0;
    FocusBacklashN[0].max = 255;
    FocusBacklashN[0].step = 1;
    FocusBacklashN[0].value = 12;

    //    IUFillNumber(&BacklashN[0], "BACKLASH", "step", "%4.2f", 0, 255, 1, 12);
    //    IUFillNumberVector(&BacklashNP, BacklashN, 1, getDeviceName(), "BACKLASH_SETTINGS", "Backlash", MAIN_CONTROL_TAB, IP_RW, 60,
    //                       IPS_IDLE);

    IUFillNumber(&TempCompN[0], "TEMPCOMP", "step/10 degC", "%4.2f", -5000, 5000, 1, 65);
    IUFillNumberVector(&TempCompNP, TempCompN, 1, getDeviceName(), "TEMPCOMP_SETTINGS", "T Comp.", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    FocusMaxPosN[0].min = MFOC_POSMIN_HARDWARE;
    FocusMaxPosN[0].max = MFOC_POSMAX_HARDWARE;
    FocusMaxPosN[0].step = (FocusMaxPosN[0].max - FocusMaxPosN[0].min) / 20.0;
    FocusMaxPosN[0].value = 110000;

    FocusAbsPosN[0].min = 0;
    FocusAbsPosN[0].max = FocusMaxPosN[0].value;
    FocusAbsPosN[0].step = FocusAbsPosN[0].max / 50.0;

    IUFillSwitch(&TempTrackDirS[MODE_TDIR_BOTH], "Both", "Both", ISS_ON);
    IUFillSwitch(&TempTrackDirS[MODE_TDIR_IN],   "In",   "In",   ISS_ON);
    IUFillSwitch(&TempTrackDirS[MODE_TDIR_OUT],  "Out",  "Out",  ISS_ON);
    IUFillSwitchVector(&TempTrackDirSP, TempTrackDirS, MODE_COUNT_TEMP_DIR, getDeviceName(), "Temp. dir.", "Temp. dir.", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&StartSavedPositionS[MODE_SAVED_ON],  "Yes", "Yes", ISS_ON);
    IUFillSwitch(&StartSavedPositionS[MODE_SAVED_OFF], "No",  "No",  ISS_OFF);
    IUFillSwitchVector(&StartSavedPositionSP, StartSavedPositionS, MODE_COUNT_SAVED, getDeviceName(), "Start saved pos.", "Start saved pos.", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc::updateProperties()
{
    // Get Initial Position before we define it in the INDI::Focuser class
    FocusAbsPosN[0].value = GetAbsFocuserPosition();

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        //defineNumber(&BacklashNP);
        defineNumber(&TempCompNP);
        defineSwitch(&TempTrackDirSP);
        defineSwitch(&StartSavedPositionSP);

    }
    else
    {
        //deleteProperty(BacklashNP.name);
        deleteProperty(TempCompNP.name);
        deleteProperty(TempTrackDirSP.name);
        deleteProperty(StartSavedPositionSP.name);
    }

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc::Handshake()
{
    char MFOC_cmd[32] = ": Q #";
    char MFOC_res[32] = {0};
    char MFOC_res_type[32] = "0";
    int MFOC_pos_measd = 0;
    int nbytes_written = 0;
    int nbytes_read = 0;


    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_INFO("CMD <%s>", MFOC_cmd);
    tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
    LOGF_DEBUG("RES <%s>", MFOC_res_type);

    sscanf(MFOC_res, "%s %d", MFOC_res_type, &MFOC_pos_measd);

    if (MFOC_res_type[0] == 'P')
    {
        FocusAbsPosN[0].value = MFOC_pos_measd;
        FocusAbsPosNP.s = IPS_OK;
        return true;
    }

    return false;
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temp. Track Direction
        if (strcmp(TempTrackDirSP.name, name) == 0)
        {
            IUUpdateSwitch(&TempTrackDirSP, states, names, n);
            int tdir = 0;
            int index    = IUFindOnSwitchIndex(&TempTrackDirSP);
            char MFOC_cmd[32]  = ": I ";
            char MFOC_res[32]  = {0};
            int nbytes_read    = 0;
            int nbytes_written = 0;
            int MFOC_tdir_measd = 0;
            char MFOC_res_type[32]  = "0";

            switch (index)
            {
                case MODE_TDIR_BOTH:
                    tdir = 0;
                    strcat(MFOC_cmd, "0 #");
                    break;

                case MODE_TDIR_IN:
                    tdir = 1;
                    strcat(MFOC_cmd, "1 #");
                    break;

                case MODE_TDIR_OUT:
                    tdir = 2;
                    strcat(MFOC_cmd, "2 #");
                    break;

                default:
                    TempTrackDirSP.s = IPS_ALERT;
                    IDSetSwitch(&TempTrackDirSP, "Unknown mode index %d", index);
                    return true;
            }


            tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
            LOGF_DEBUG("CMD <%s>", MFOC_cmd);
            tty_write_string(PortFD, ": W #", &nbytes_written);
            tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
            sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_tdir_measd);
            LOGF_DEBUG("RES <%s>", MFOC_res);

            if  (MFOC_tdir_measd == tdir)
            {
                TempTrackDirSP.s = IPS_OK;
            }
            else
            {
                TempTrackDirSP.s = IPS_ALERT;
            }

            IDSetSwitch(&TempTrackDirSP, nullptr);
            return true;
        }


        // Start at saved position
        if (strcmp(StartSavedPositionSP.name, name) == 0)
        {
            IUUpdateSwitch(&StartSavedPositionSP, states, names, n);
            int svstart = 0;
            int index    = IUFindOnSwitchIndex(&StartSavedPositionSP);
            char MFOC_cmd[32]  = ": F ";
            char MFOC_res[32]  = {0};
            int nbytes_read    = 0;
            int nbytes_written = 0;
            int MFOC_svstart_measd = 0;
            char MFOC_res_type[32]  = "0";

            switch (index)
            {
                case MODE_SAVED_ON:
                    svstart = 1;
                    strcat(MFOC_cmd, "1 #");
                    break;

                case MODE_SAVED_OFF:
                    svstart = 0;
                    strcat(MFOC_cmd, "0 #");
                    break;

                default:
                    StartSavedPositionSP.s = IPS_ALERT;
                    IDSetSwitch(&StartSavedPositionSP, "Unknown mode index %d", index);
                    return true;
            }

            tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
            LOGF_DEBUG("CMD <%s>", MFOC_cmd);
            tty_write_string(PortFD, ": N #", &nbytes_written);
            tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
            sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_svstart_measd);

            LOGF_DEBUG("RES <%s>", MFOC_res);
            //            LOGF_DEBUG("Debug MFOC cmd sent %s", MFOC_cmd);
            if  (MFOC_svstart_measd == svstart)
            {
                StartSavedPositionSP.s = IPS_OK;
            }
            else
            {
                StartSavedPositionSP.s = IPS_ALERT;
            }

            IDSetSwitch(&StartSavedPositionSP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool lacerta_mfoc::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //        if (strcmp(name, "BACKLASH_SETTINGS") == 0)
        //        {
        //            return SetBacklash(values, names, n);
        //        }


        if (strcmp(name, "TEMPCOMP_SETTINGS") == 0)
        {
            return SetTempComp(values, names, n);
        }
    }

    // Let INDI::Focuser handle any other number properties
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
************************************************************************************/
//bool lacerta_mfoc::SetBacklash(double values[], char *names[], int n)
bool lacerta_mfoc::SetFocuserBacklash(int32_t steps)
{
    LOGF_DEBUG("-> BACKLASH_SETTINGS", 0);
    char MFOC_cmd[32]  = ": B ";
    char MFOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int MFOC_tdir_measd = 0;
    //int bl_int = 0;
    char bl_char[32]  = {0};
    char MFOC_res_type[32]  = "0";
    //    BacklashNP.s = IPS_OK;
    //    IUUpdateNumber(&BacklashNP, values, names, n);
    //    bl_int = BacklashN[0].value;
    sprintf(bl_char, "%d", steps);
    strcat(bl_char, " #");
    strcat(MFOC_cmd, bl_char);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD <%s>", MFOC_cmd);
    tty_write_string(PortFD, ": J #", &nbytes_written);

    tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);

    sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_tdir_measd);

    LOGF_DEBUG("RES <%s>", MFOC_res);

    //IDSetNumber(&BacklashNP, nullptr);

    return true;
}

bool lacerta_mfoc::SetTempComp(double values[], char *names[], int n)
{
    LOGF_INFO("-> TEMPCOMP_SETTINGS", 0);
    char MFOC_cmd[32]  = ": D ";
    char MFOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int MFOC_tc_measd = 0;
    int tc_int = 0;
    char tc_char[32]  = {0};
    char MFOC_res_type[32]  = "0";
    TempCompNP.s = IPS_OK;
    IUUpdateNumber(&TempCompNP, values, names, n);
    tc_int = TempCompN[0].value;
    sprintf(tc_char, "%d", tc_int);
    strcat(tc_char, " #");
    strcat(MFOC_cmd, tc_char);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD <%s>", MFOC_cmd);
    tty_write_string(PortFD, ": U #", &nbytes_written);

    tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);

    sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_tc_measd);

    LOGF_DEBUG("RES <%s>", MFOC_res);

    IDSetNumber(&TempCompNP, nullptr);

    return true;
}

bool lacerta_mfoc::SetFocuserMaxPosition(uint32_t ticks)
{
    char MFOC_cmd[32]  = ": G ";
    char MFOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int MFOC_pm_measd = 0;
    char pm_char[32]  = {0};
    char MFOC_res_type[32]  = "0";

    sprintf(pm_char, "%d", ticks);
    strcat(pm_char, " #");
    strcat(MFOC_cmd, pm_char);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD <%s>", MFOC_cmd);
    tty_write_string(PortFD, ": O #", &nbytes_written);

    tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);

    sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_pm_measd);

    LOGF_DEBUG("RES <%s>", MFOC_res);
    return true;
}

/************************************************************************************
 *
************************************************************************************/
IPState lacerta_mfoc::MoveAbsFocuser(uint32_t targetTicks)
{
    char MFOC_cmd[32]  = ": M ";
    char abs_pos_char[32]  = {0};
    int nbytes_written = 0;

    //int pos = GetAbsFocuserPosition();
    sprintf(abs_pos_char, "%d", targetTicks);
    strcat(abs_pos_char, " #");
    strcat(MFOC_cmd, abs_pos_char);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD <%s>", MFOC_cmd);

    //Waiting makes no sense - will be immediatly interrupted by the ekos system...
    //int ticks = std::abs((int)(targetTicks - pos) * FOCUS_MOTION_DELAY);
    //LOGF_INFO("sleep for %d ms", ticks);
    //usleep(ticks + 5000);

    FocusAbsPosN[0].value = targetTicks;

    //only for debugging! Maybe there is a bug in the MFOC firmware command "Q #"!
    GetAbsFocuserPosition();

    return IPS_OK;
}

/************************************************************************************
 *
************************************************************************************/
IPState lacerta_mfoc::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    // Calculation of the demand absolute position
    uint32_t targetTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));
    FocusAbsPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, nullptr);

    return MoveAbsFocuser(targetTicks);
}


bool lacerta_mfoc::saveConfigItems(FILE *fp)
{
    // Save Focuser Config
    INDI::Focuser::saveConfigItems(fp);

    // Save additional MFPC Config
    //IUSaveConfigNumber(fp, &BacklashNP);
    IUSaveConfigNumber(fp, &TempCompNP);

    return true;
}

uint32_t lacerta_mfoc::GetAbsFocuserPosition()
{
    char MFOC_cmd[32] = ": Q #";
    char MFOC_res[32] = {0};
    char MFOC_res_type[32] = "0";
    int MFOC_pos_measd = 0;

    int nbytes_written = 0;
    int nbytes_read = 0;

    do
    {
        tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
        LOGF_INFO("CMD <%s>", MFOC_cmd);
        tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
        sscanf(MFOC_res, "%s %d", MFOC_res_type, &MFOC_pos_measd);
    }
    while(strcmp(MFOC_res_type, "P") != 0);

    LOGF_DEBUG("RES <%s>", MFOC_res_type);
    LOGF_DEBUG("current position: %d", MFOC_pos_measd);

    return static_cast<uint32_t>(MFOC_pos_measd);
}
