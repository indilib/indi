/*******************************************************************************
 Copyright(c) 2019 Christian Liska. All rights reserved.

 Implementation based on Lacerta MFOC driver
 (written 2018 by Franck Le Rhun and Christian Liska).

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

#include "astromech_focuser.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>

// We declare an auto pointer to astromechanics_foc.
static std::unique_ptr<astromechanics_foc> Astromechanics_foc(new astromechanics_foc());

// Delay for receiving messages
#define FOCUS_TIMEOUT  1000
#define FOC_POSMAX_HARDWARE 9999
#define FOC_POSMIN_HARDWARE 0

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    Astromechanics_foc->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    Astromechanics_foc->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    Astromechanics_foc->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    Astromechanics_foc->ISNewNumber(dev, name, values, names, n);
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
    Astromechanics_foc->ISSnoopDevice(root);
}

/************************************************************************************
 *
************************************************************************************/
astromechanics_foc::astromechanics_foc()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
}

/************************************************************************************
 *
************************************************************************************/
const char *astromechanics_foc::getDefaultName()
{
    return "Astromechanics FOC";
}

/************************************************************************************
 *
************************************************************************************/
void astromechanics_foc::ISGetProperties(const char *dev)
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
bool astromechanics_foc::initProperties()
{
    INDI::Focuser::initProperties();

    IUFillNumber(&BacklashN[0], "BACKLASH", "step", "%4.2f", 0, 255, 1, 12);
    IUFillNumberVector(&BacklashNP, BacklashN, 1, getDeviceName(), "BACKLASH_SETTINGS", "Backlash", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    IUFillNumber(&TempCompN[0], "TEMPCOMP", "step/10 degC", "%4.2f", -5000, 5000, 1, 65);
    IUFillNumberVector(&TempCompNP, TempCompN, 1, getDeviceName(), "TEMPCOMP_SETTINGS", "T Comp.", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    FocusMaxPosN[0].min = FOC_POSMIN_HARDWARE;
    FocusMaxPosN[0].max = FOC_POSMAX_HARDWARE;
    FocusMaxPosN[0].step = (FocusMaxPosN[0].max - FocusMaxPosN[0].min) / 20.0;
    FocusMaxPosN[0].value = 5000;

    FocusAbsPosN[0].max = FocusAbsPosN[0].value;

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
bool astromechanics_foc::updateProperties()
{
    // Get Initial Position before we define it in the INDI::Focuser class
    FocusAbsPosN[0].value = GetAbsFocuserPosition();

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineNumber(&BacklashNP);
        defineNumber(&TempCompNP);
        defineSwitch(&TempTrackDirSP);
        defineSwitch(&StartSavedPositionSP);

    }
    else
    {
        deleteProperty(BacklashNP.name);
        deleteProperty(TempCompNP.name);
        deleteProperty(TempTrackDirSP.name);
        deleteProperty(StartSavedPositionSP.name);
    }

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::Handshake()
{
    char FOC_cmd[32] = ": Q #";
    char FOC_res[32] = {0};
    char FOC_res_type[32] = "0";
    int FOC_pos_measd = 0;
    int nbytes_written = 0;
    int nbytes_read = 0;


    tty_write_string(PortFD, FOC_cmd, &nbytes_written);
    LOGF_INFO("CMD <%s>", FOC_cmd);
    tty_read_section(PortFD, FOC_res, 0xD, FOCUS_TIMEOUT, &nbytes_read);
    LOGF_DEBUG("RES <%s>", FOC_res_type);

    sscanf(FOC_res, "%s %d", FOC_res_type, &FOC_pos_measd);

    if (FOC_res_type[0] == 'P')
    {
        FocusAbsPosN[0].value = FOC_pos_measd;
        FocusAbsPosNP.s = IPS_OK;
        return true;
    }

    return false;
}

/************************************************************************************
 *
************************************************************************************/
bool astromechanics_foc::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temp. Track Direction
        if (strcmp(TempTrackDirSP.name, name) == 0)
        {
            IUUpdateSwitch(&TempTrackDirSP, states, names, n);
            int tdir = 0;
            int index    = IUFindOnSwitchIndex(&TempTrackDirSP);
            char FOC_cmd[32]  = ": I ";
            char FOC_res[32]  = {0};
            int nbytes_read    = 0;
            int nbytes_written = 0;
            int FOC_tdir_measd = 0;
            char FOC_res_type[32]  = "0";

            switch (index)
            {
                case MODE_TDIR_BOTH:
                    tdir = 0;
                    strcat(FOC_cmd, "0 #");
                    break;

                case MODE_TDIR_IN:
                    tdir = 1;
                    strcat(FOC_cmd, "1 #");
                    break;

                case MODE_TDIR_OUT:
                    tdir = 2;
                    strcat(FOC_cmd, "2 #");
                    break;

                default:
                    TempTrackDirSP.s = IPS_ALERT;
                    IDSetSwitch(&TempTrackDirSP, "Unknown mode index %d", index);
                    return true;
            }


            tty_write_string(PortFD, FOC_cmd, &nbytes_written);
            LOGF_DEBUG("CMD <%s>", FOC_cmd);
            tty_write_string(PortFD, ": W #", &nbytes_written);
            tty_read_section(PortFD, FOC_res, 0xD, FOCUS_TIMEOUT, &nbytes_read);
            sscanf (FOC_res, "%s %d", FOC_res_type, &FOC_tdir_measd);
            LOGF_DEBUG("RES <%s>", FOC_res);

            if  (FOC_tdir_measd == tdir)
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
            char FOC_cmd[32]  = ": F ";
            char FOC_res[32]  = {0};
            int nbytes_read    = 0;
            int nbytes_written = 0;
            int FOC_svstart_measd = 0;
            char FOC_res_type[32]  = "0";

            switch (index)
            {
                case MODE_SAVED_ON:
                    svstart = 1;
                    strcat(FOC_cmd, "1 #");
                    break;

                case MODE_SAVED_OFF:
                    svstart = 0;
                    strcat(FOC_cmd, "0 #");
                    break;

                default:
                    StartSavedPositionSP.s = IPS_ALERT;
                    IDSetSwitch(&StartSavedPositionSP, "Unknown mode index %d", index);
                    return true;
            }

            tty_write_string(PortFD, FOC_cmd, &nbytes_written);
            LOGF_DEBUG("CMD <%s>", FOC_cmd);
            tty_write_string(PortFD, ": N #", &nbytes_written);
            tty_read_section(PortFD, FOC_res, 0xD, FOCUS_TIMEOUT, &nbytes_read);
            sscanf (FOC_res, "%s %d", FOC_res_type, &FOC_svstart_measd);

            LOGF_DEBUG("RES <%s>", FOC_res);
            //            LOGF_DEBUG("Debug FOC cmd sent %s", FOC_cmd);
            if  (FOC_svstart_measd == svstart)
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

bool astromechanics_foc::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "BACKLASH_SETTINGS") == 0)
        {
            return SetBacklash(values, names, n);
        }


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
bool astromechanics_foc::SetBacklash(double values[], char *names[], int n)
{
    LOGF_DEBUG("-> BACKLASH_SETTINGS", 0);
    char FOC_cmd[32]  = ": B ";
    char FOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int FOC_tdir_measd = 0;
    int bl_int = 0;
    char bl_char[32]  = {0};
    char FOC_res_type[32]  = "0";
    BacklashNP.s = IPS_OK;
    IUUpdateNumber(&BacklashNP, values, names, n);
    bl_int = BacklashN[0].value;
    sprintf(bl_char, "%d", bl_int);
    strcat(bl_char, " #");
    strcat(FOC_cmd, bl_char);

    tty_write_string(PortFD, FOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD <%s>", FOC_cmd);
    tty_write_string(PortFD, ": J #", &nbytes_written);

    tty_read_section(PortFD, FOC_res, 0xD, FOCUS_TIMEOUT, &nbytes_read);

    sscanf (FOC_res, "%s %d", FOC_res_type, &FOC_tdir_measd);

    LOGF_DEBUG("RES <%s>", FOC_res);

    IDSetNumber(&BacklashNP, nullptr);

    return true;
}

bool astromechanics_foc::SetTempComp(double values[], char *names[], int n)
{
    LOGF_INFO("-> TEMPCOMP_SETTINGS", 0);
    char FOC_cmd[32]  = ": D ";
    char FOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int FOC_tc_measd = 0;
    int tc_int = 0;
    char tc_char[32]  = {0};
    char FOC_res_type[32]  = "0";
    TempCompNP.s = IPS_OK;
    IUUpdateNumber(&TempCompNP, values, names, n);
    tc_int = TempCompN[0].value;
    sprintf(tc_char, "%d", tc_int);
    strcat(tc_char, " #");
    strcat(FOC_cmd, tc_char);

    tty_write_string(PortFD, FOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD <%s>", FOC_cmd);
    tty_write_string(PortFD, ": U #", &nbytes_written);

    tty_read_section(PortFD, FOC_res, 0xD, FOCUS_TIMEOUT, &nbytes_read);

    sscanf (FOC_res, "%s %d", FOC_res_type, &FOC_tc_measd);

    LOGF_DEBUG("RES <%s>", FOC_res);

    IDSetNumber(&TempCompNP, nullptr);

    return true;
}

bool astromechanics_foc::SetFocuserMaxPosition(uint32_t ticks)
{
    char FOC_cmd[32]  = ": G ";
    char FOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int FOC_pm_measd = 0;
    char pm_char[32]  = {0};
    char FOC_res_type[32]  = "0";

    sprintf(pm_char, "%d", ticks);
    strcat(pm_char, " #");
    strcat(FOC_cmd, pm_char);

    tty_write_string(PortFD, FOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD <%s>", FOC_cmd);
    tty_write_string(PortFD, ": O #", &nbytes_written);

    tty_read_section(PortFD, FOC_res, 0xD, FOCUS_TIMEOUT, &nbytes_read);

    sscanf (FOC_res, "%s %d", FOC_res_type, &FOC_pm_measd);

    LOGF_DEBUG("RES <%s>", FOC_res);
    return true;
}

/************************************************************************************
 *
************************************************************************************/
IPState astromechanics_foc::MoveAbsFocuser(uint32_t targetTicks)
{
    char FOC_cmd[32]  = ": M ";
    char abs_pos_char[32]  = {0};
    int nbytes_written = 0;

    //int pos = GetAbsFocuserPosition();
    sprintf(abs_pos_char, "%d", targetTicks);
    strcat(abs_pos_char, " #");
    strcat(FOC_cmd, abs_pos_char);

    tty_write_string(PortFD, FOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD <%s>", FOC_cmd);

    //Waiting makes no sense - will be immediatly interrupted by the ekos system...
    //int ticks = std::abs((int)(targetTicks - pos) * FOCUS_MOTION_DELAY);
    //LOGF_INFO("sleep for %d ms", ticks);
    //usleep(ticks + 5000);

    FocusAbsPosN[0].value = targetTicks;

    //only for debugging! Maybe there is a bug in the FOC firmware command "Q #"!
    GetAbsFocuserPosition();

    return IPS_OK;
}

/************************************************************************************
 *
************************************************************************************/
IPState astromechanics_foc::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    // Calculation of the demand absolute position
    uint32_t targetTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));
    FocusAbsPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, nullptr);

    return MoveAbsFocuser(targetTicks);
}


bool astromechanics_foc::saveConfigItems(FILE *fp)
{
    // Save Focuser Config
    INDI::Focuser::saveConfigItems(fp);

    // Save additional MFPC Config
    IUSaveConfigNumber(fp, &BacklashNP);
    IUSaveConfigNumber(fp, &TempCompNP);

    return true;
}

uint32_t astromechanics_foc::GetAbsFocuserPosition()
{
    char FOC_cmd[32] = ": Q #";
    char FOC_res[32] = {0};
    char FOC_res_type[32] = "0";
    int FOC_pos_measd = 0;

    int nbytes_written = 0;
    int nbytes_read = 0;

    do
    {
        tty_write_string(PortFD, FOC_cmd, &nbytes_written);
        LOGF_INFO("CMD <%s>", FOC_cmd);
        tty_read_section(PortFD, FOC_res, 0xD, FOCUS_TIMEOUT, &nbytes_read);
        sscanf(FOC_res, "%s %d", FOC_res_type, &FOC_pos_measd);
    }
    while(strcmp(FOC_res_type, "P") != 0);

    LOGF_DEBUG("RES <%s>", FOC_res_type);
    LOGF_DEBUG("current position: %d", FOC_pos_measd);

    return static_cast<uint32_t>(FOC_pos_measd);
}
