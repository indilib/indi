/*******************************************************************************
  Copyright(c) 2023 Christian Liska. All rights reserved.
  based on the 2018 MFOC implementation (c) Franck Le Rhun and Christian Liska

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

#include "lacerta_mfoc_fmc.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>

// We declare an auto pointer to lacerta_mfoc_fmc.
static std::unique_ptr<lacerta_mfoc_fmc> Lacerta_mfoc_fmc(new lacerta_mfoc_fmc());

// Delay for receiving messages
#define FOCUSMFOC_TIMEOUT  1000
// According to documentation for v2
#define MFOC_POSMAX_HARDWARE 250000
#define MFOC_POSMIN_HARDWARE 300

/************************************************************************************
 *
************************************************************************************/
lacerta_mfoc_fmc::lacerta_mfoc_fmc()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_HAS_BACKLASH |
                      FOCUSER_CAN_SYNC |
                      FOCUSER_CAN_ABORT);
}

/************************************************************************************
 *
************************************************************************************/
const char *lacerta_mfoc_fmc::getDefaultName()
{
    return "Lacerta MFOC FMC";
}

/************************************************************************************
 *
************************************************************************************/
void lacerta_mfoc_fmc::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    INDI::Focuser::ISGetProperties(dev);

    defineProperty(TempTrackDirSP);
    TempTrackDirSP.load();

    defineProperty(StartSavedPositionSP);
    StartSavedPositionSP.load();
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc_fmc::initProperties()
{
    INDI::Focuser::initProperties();

    FocusBacklashNP[0].setMin(0);
    FocusBacklashNP[0].setMax(255);
    FocusBacklashNP[0].setStep(1);
    FocusBacklashNP[0].setValue(12);

    CurrentHoldingNP[0].fill("CURRHOLD", "holding current mA", "%4d", 0, 1200, 1, 160);
    CurrentHoldingNP.fill(getDeviceName(), "CURRHOLD_SETTINGS", "Curr. Hold", FOCUS_TAB,
                       IP_WO, 60,
                       IPS_IDLE);

    CurrentMovingNP[0].fill("CURRMOVE", "moving current mA", "%4d", 0, 1200, 1, 400);
    CurrentMovingNP.fill(getDeviceName(), "CURRMOVE_SETTINGS", "Curr. Move", FOCUS_TAB,
                       IP_WO, 60,
                       IPS_IDLE);

    FocusMaxPosNP[0].setMin(MFOC_POSMIN_HARDWARE);
    FocusMaxPosNP[0].setMax(MFOC_POSMAX_HARDWARE);
    FocusMaxPosNP[0].setStep((FocusMaxPosNP[0].getMax() - FocusMaxPosNP[0].getMin()) / 20.0);
    FocusMaxPosNP[0].setValue(110000);

    FocusAbsPosNP[0].setMin(0);
    FocusAbsPosNP[0].setMax(FocusMaxPosNP[0].getValue());
    FocusAbsPosNP[0].setStep(FocusAbsPosNP[0].getMax() / 50.0);

    TempTrackDirSP[MODE_TDIR_BOTH].fill("Both", "Both", ISS_ON);
    TempTrackDirSP[MODE_TDIR_IN].fill("In",   "In",   ISS_ON);
    TempTrackDirSP[MODE_TDIR_OUT].fill("Out",  "Out",  ISS_ON);
    TempTrackDirSP.fill(getDeviceName(), "Temp. dir.", "Temp. dir.",
                       MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    StartSavedPositionSP[MODE_SAVED_ON].fill( "Yes", "Yes", ISS_ON);
    StartSavedPositionSP[MODE_SAVED_OFF].fill("No",  "No",  ISS_OFF);
    StartSavedPositionSP.fill(getDeviceName(), "Start saved pos.",
                       "Start saved pos.", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc_fmc::updateProperties()
{
    // Get Initial Position before we define it in the INDI::Focuser class
    FocusAbsPosNP[0].setValue(GetAbsFocuserPosition());

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&TempCompNP);
        defineProperty(CurrentHoldingNP);
        defineProperty(CurrentMovingNP);
        defineProperty(TempTrackDirSP);
        defineProperty(StartSavedPositionSP);

    }
    else
    {
        deleteProperty(TempCompNP.name);
        deleteProperty(CurrentHoldingNP);
        deleteProperty(CurrentMovingNP);
        deleteProperty(TempTrackDirSP);
        deleteProperty(StartSavedPositionSP);
    }

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc_fmc::Handshake()
{
    char MFOC_cmd[32] = ": q #";
    char MFOC_res[32] = {0};
    char MFOC_res_type[32] = "0";
    char device[5] = "0";
    int MFOC_pos_measd = 0;
    int nbytes_written = 0;
    int nbytes_read = 0;

    bool device_found = false;
    if (tty_write_string(PortFD, ": i #", &nbytes_written) == TTY_OK)
    {
        int count = 0;
        do
        {
            try
            {
                if (tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read) != TTY_OK)
                {
                    LOGF_ERROR("Unknown device or MFOC firmware not compatible with this driver version! Please update firmware! %s", device);
                    return false;
                }

                sscanf(MFOC_res, "%s %s", MFOC_res_type, device);
                LOGF_INFO("%s", device);
                device_found = strcmp(device, "MFOC") == 0 || strcmp(device, "FMC") == 0;
            }
            catch (...)
            {
                LOGF_ERROR("Cannot detect connected device %s", device);
                return false;
            }
            count ++;
        }
        while (!device_found && count != 10);
    }
    LOGF_INFO("Device detected: %s", device);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_INFO("CMD: %s", MFOC_cmd);
    tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
    MFOC_res[nbytes_read] = 0;
    LOGF_DEBUG("Handshake: RES [%s]", MFOC_res);

    sscanf(MFOC_res, "%s %d", MFOC_res_type, &MFOC_pos_measd);

    if (MFOC_res_type[0] == 'p')
    {
        FocusAbsPosNP[0].setValue(MFOC_pos_measd);
        FocusAbsPosNP.setState(IPS_OK);
        return true;
    }

    return false;
}


/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc_fmc::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Temp. Track Direction
        if (TempTrackDirSP.isNameMatch(name))
        {
            TempTrackDirSP.update(states, names, n);
            int tdir = 0;
            int index    = TempTrackDirSP.findOnSwitchIndex();
            char MFOC_cmd[32]  = ": W ";
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
                    TempTrackDirSP.setState(IPS_ALERT);
                    LOGF_ERROR("Unknown mode index %d", index);
                    return true;
            }


            tty_write_string(PortFD, MFOC_cmd, &nbytes_written);

            LOGF_DEBUG("ISNewSwitch: CMD [%s]", MFOC_cmd);
            tty_write_string(PortFD, ": W #", &nbytes_written);
            tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
            MFOC_res[nbytes_read] = 0;
            sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_tdir_measd);
            LOGF_DEBUG("ISNewSwitch: RES [%s]", MFOC_res);

            if  (MFOC_tdir_measd == tdir)
            {
                TempTrackDirSP.setState(IPS_OK);
            }
            else
            {
                TempTrackDirSP.setState(IPS_ALERT);
            }

            TempTrackDirSP.apply();
            return true;
        }

        // Start at saved position
        if (StartSavedPositionSP.isNameMatch(name))
        {
            StartSavedPositionSP.update(states, names, n);
            int svstart = 0;
            int index    = StartSavedPositionSP.findOnSwitchIndex();
            char MFOC_cmd[32]  = ": N ";
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
                    StartSavedPositionSP.setState(IPS_ALERT);
                    LOGF_ERROR("Unknown mode index %d", index);
                    return true;
            }

            tty_write_string(PortFD, MFOC_cmd, &nbytes_written);

            LOGF_DEBUG("CMD [%s]", MFOC_cmd);
            tty_write_string(PortFD, ": N #", &nbytes_written);
            tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
            MFOC_res[nbytes_read] = 0;
            sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_svstart_measd);

            LOGF_DEBUG("RES [%s]", MFOC_res);
            if  (MFOC_svstart_measd == svstart)
            {
                StartSavedPositionSP.setState(IPS_OK);
            }
            else
            {
                StartSavedPositionSP.setState(IPS_ALERT);
            }

            StartSavedPositionSP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool lacerta_mfoc_fmc::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "TEMPCOMP_SETTINGS") == 0)
        {
            return SetTempComp(values, names, n);
        }

        if (strcmp(name, "CURRHOLD_SETTINGS") == 0)
        {
            CurrentHoldingNP.update(values, names, n);
            if (!SetCurrHold(CurrentHoldingNP[0].getValue()))
            {
                CurrentHoldingNP.setState(IPS_ALERT);
                CurrentHoldingNP.apply();
                return false;
            }
            CurrentHoldingNP.setState(IPS_OK);
            CurrentHoldingNP.apply();
            return true;
        }

        if (strcmp(name, "CURRMOVE_SETTINGS") == 0)
        {
            CurrentMovingNP.update(values, names, n);
            if (!SetCurrMove(CurrentMovingNP[0].getValue()))
            {
                CurrentMovingNP.setState(IPS_ALERT);
                CurrentMovingNP.apply();
                return false;
            }
            CurrentMovingNP.setState(IPS_OK);
            CurrentMovingNP.apply();
            return true;
        }
    }

    // Let INDI::Focuser handle any other number properties
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc_fmc::SetFocuserBacklash(int32_t steps)
{
    char MFOC_cmd[32]  = ": B ";
    char MFOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int MFOC_tdir_measd = 0;
    char bl_char[32]  = {0};
    char MFOC_res_type[32]  = "0";

    sprintf(bl_char, "%d", steps);
    strcat(bl_char, " #");
    strcat(MFOC_cmd, bl_char);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD [%s]", MFOC_cmd);

    tty_write_string(PortFD, ": b #", &nbytes_written);
    do
    {
        tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
        MFOC_res[nbytes_read] = 0;
        sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_tdir_measd);
    }
    while(strcmp("b", MFOC_res_type) != 0);
    LOGF_DEBUG("RES [%s]", MFOC_res);

    return true;
}

bool lacerta_mfoc_fmc::SetCurrHold(int currHoldValue)
{
    char MFOC_cmd[32]  = ": E ";
    char MFOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int MFOC_ch_measd = 0;
    char ch_char[32]  = {0};
    char MFOC_res_type[32]  = "0";

    sprintf(ch_char, "%d", currHoldValue);
    strcat(ch_char, " #");
    strcat(MFOC_cmd, ch_char);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD [%s]", MFOC_cmd);

    do
    {
        tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
        sscanf(MFOC_res, "%s %d", MFOC_res_type, &MFOC_ch_measd);
        MFOC_res[nbytes_read] = 0;
    }
    while(strcmp("e", MFOC_res_type) != 0);

    LOGF_DEBUG("RES [%s]", MFOC_res);
    LOGF_INFO("Holding Current set to %d mA", MFOC_ch_measd);

    return true;
}

bool lacerta_mfoc_fmc::SetCurrMove(int currMoveValue)
{
    char MFOC_cmd[32]  = ": F ";
    char MFOC_res[32]  = {0};
    int nbytes_read    =  0;
    int nbytes_written =  0;
    int MFOC_cm_measd = 0;
    char cm_char[32]  = {0};
    char MFOC_res_type[32]  = "0";

    sprintf(cm_char, "%d", currMoveValue);
    strcat(cm_char, " #");
    strcat(MFOC_cmd, cm_char);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD [%s]", MFOC_cmd);

    do
    {
        tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
        sscanf(MFOC_res, "%s %d", MFOC_res_type, &MFOC_cm_measd);
        MFOC_res[nbytes_read] = 0;
    }
    while(strcmp("f", MFOC_res_type) != 0);

    LOGF_DEBUG("RES [%s]", MFOC_res);
    LOGF_INFO("Moving Current set to %d mA", MFOC_cm_measd);

    return true;
}

bool lacerta_mfoc_fmc::SetTempComp(double values[], char *names[], int n)
{
    char MFOC_cmd[32]  = ": U ";
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
    LOGF_DEBUG("CMD [%s]", MFOC_cmd);

    tty_write_string(PortFD, ": u #", &nbytes_written);
    do
    {
        tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
        MFOC_res[nbytes_read] = 0;
        sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_tc_measd);
    }
    while(strcmp("u", MFOC_res_type) != 0);
    LOGF_DEBUG("RES [%s]", MFOC_res);

    IDSetNumber(&TempCompNP, nullptr);

    return true;
}

bool lacerta_mfoc_fmc::SetFocuserMaxPosition(uint32_t ticks)
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
    LOGF_DEBUG("CMD [%s]", MFOC_cmd);
    IgnoreButLogResponse();

    tty_write_string(PortFD, ": g #", &nbytes_written);
    do
    {
        tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
        MFOC_res[nbytes_read] = 0;
        sscanf (MFOC_res, "%s %d", MFOC_res_type, &MFOC_pm_measd);
    }
    while (strcmp("g", MFOC_res_type) != 0);
    LOGF_DEBUG("RES [%s]", MFOC_res);

    return true;
}

/************************************************************************************
 *
************************************************************************************/
IPState lacerta_mfoc_fmc::MoveAbsFocuser(uint32_t targetTicks)
{
    char MFOC_cmd[32]  = ": M ";
    char abs_pos_char[32]  = {0};
    int nbytes_written = 0;

    //int pos = GetAbsFocuserPosition();
    sprintf(abs_pos_char, "%d", targetTicks);
    strcat(abs_pos_char, " #");
    strcat(MFOC_cmd, abs_pos_char);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD [%s]", MFOC_cmd);
    IgnoreButLogResponse();

    FocusAbsPosNP[0].setValue(targetTicks);

    GetAbsFocuserPosition();

    return IPS_OK;
}

/************************************************************************************
 *
************************************************************************************/
IPState lacerta_mfoc_fmc::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    // Calculation of the demand absolute position
    auto targetTicks = FocusAbsPosNP[0].getValue();
    if (dir == FOCUS_INWARD) targetTicks -= ticks;
    else targetTicks += ticks;
    targetTicks = std::clamp(targetTicks, FocusAbsPosNP[0].getMin(), FocusAbsPosNP[0].getMax());

    FocusAbsPosNP.setState(IPS_BUSY);
    FocusAbsPosNP.apply();

    return MoveAbsFocuser(targetTicks);
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc_fmc::SyncFocuser(uint32_t ticks)
{
    char MFOC_cmd[32] = ": P ";
    char sync_pos[8] = {0};

    int nbytes_written = 0;

    sprintf(sync_pos, "%d", ticks);
    strcat(sync_pos, " #");
    strcat(MFOC_cmd, sync_pos);

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD [%s]", MFOC_cmd);
    IgnoreButLogResponse();

    return true;
}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc_fmc::AbortFocuser()
{
    char MFOC_cmd[32] = ": H #";
    char MFOC_res[32]  = {0};
    char MFOC_res_type[32]  = "0";
    int nbytes_read    =  0;
    int nbytes_written = 0;
    int halt_flag = 0;

    LOGF_DEBUG("CMD [%s]", MFOC_cmd);

    if (tty_write_string(PortFD, MFOC_cmd, &nbytes_written) == TTY_OK)
    {

        int count = 0;
        do
        {
            count ++;
            tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
            MFOC_res[nbytes_read] = 0;
            sscanf (MFOC_res, "%s %d", MFOC_res_type, &halt_flag);
        }
        while(strcmp(MFOC_res_type, "H") != 0 && count <= 100);

        FocusAbsPosNP[0].setValue(GetAbsFocuserPosition());
        FocusAbsPosNP.setState(IPS_OK);
        return true;
    }
    else
        return false;

}

/************************************************************************************
 *
************************************************************************************/
bool lacerta_mfoc_fmc::saveConfigItems(FILE *fp)
{
    LOGF_DEBUG("saveConfigItems()", 0);
    // Save Focuser Config
    INDI::Focuser::saveConfigItems(fp);

    // Save additional MFPC Config
    IUSaveConfigNumber(fp, &TempCompNP);
    CurrentHoldingNP.save(fp);
    CurrentMovingNP.save(fp);

    return true;
}

uint32_t lacerta_mfoc_fmc::GetAbsFocuserPosition()
{
    char MFOC_cmd[32] = ": q #";
    char MFOC_res[32] = {0};
    char MFOC_res_type[32] = "0";
    int MFOC_pos_measd = 0;

    int nbytes_written = 0;
    int nbytes_read = 0;
    int count = 0;

    tty_write_string(PortFD, MFOC_cmd, &nbytes_written);
    LOGF_DEBUG("CMD [%s]", MFOC_cmd);

    do
    {
        tty_read_section(PortFD, MFOC_res, 0xD, FOCUSMFOC_TIMEOUT, &nbytes_read);
        sscanf(MFOC_res, "%s %d", MFOC_res_type, &MFOC_pos_measd);
        count++;
    }
    while(strcmp(MFOC_res_type, "p") != 0 && count < 100);

    return static_cast<uint32_t>(MFOC_pos_measd);
}

void lacerta_mfoc_fmc::IgnoreResponse()
{
    int nbytes_read = 0;
    char MFOC_res[64];
    tty_read_section(PortFD, MFOC_res, 0xA, FOCUSMFOC_TIMEOUT, &nbytes_read);
}

void lacerta_mfoc_fmc::IgnoreButLogResponse()
{
    int nbytes_read = 0;
    char MFOC_res[64];
    tty_read_section(PortFD, MFOC_res, 0xA, FOCUSMFOC_TIMEOUT, &nbytes_read);
    MFOC_res[nbytes_read] = 0;
    LOGF_DEBUG("*RES [%s]", MFOC_res);
}

