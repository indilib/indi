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

#include "tcfs.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>

#define mydev           "Optec TCF-S"
#define currentPosition FocusAbsPosN[0].value

// We declare an auto pointer to TCFS.
static std::unique_ptr<TCFS> tcfs(new TCFS());

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
    tcfs->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    tcfs->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    tcfs->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    tcfs->ISNewNumber(dev, name, values, names, n);
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
    tcfs->ISSnoopDevice(root);
}

/****************************************************************
**
**
*****************************************************************/
TCFS::TCFS()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::initProperties()
{
    INDI::Focuser::initProperties();

    // Set upper limit for TCF-S3 focuser
    if (strcmp(me, "indi_tcfs3_focus") == 0)
    {
        isTCFS3 = true;

        FocusAbsPosN[0].max  = 9999;
        FocusRelPosN[0].max  = 2000;
        FocusRelPosN[0].step = FocusAbsPosN[0].step = 100;
        FocusRelPosN[0].value                       = 0;
        LOG_DEBUG("TCF-S3 detected. Updating maximum position value to 9999.");
    }
    else
    {
        isTCFS3 = false;

        FocusAbsPosN[0].max  = 7000;
        FocusRelPosN[0].max  = 2000;
        FocusRelPosN[0].step = FocusAbsPosN[0].step = 100;
        FocusRelPosN[0].value                       = 0;
        LOG_DEBUG("TCF-S detected. Updating maximum position value to 7000.");
    }

    // Mode parameters
    IUFillNumber(&FocusSlopeAN[0], "FOCUS_SLOPE_A", "Slope A", "%d", -999, 999, 10, 0);
    IUFillNumber(&FocusSlopeBN[0], "FOCUS_SLOPE_B", "Slope B", "%d", -999, 999, 10, 0);
    IUFillNumber(&FocusDelayAN[0], "FOCUS_DELAY_A", "Delay A", "%.2f", 0.0, 9.99, 1.0, 0.0);
    IUFillNumber(&FocusDelayBN[0], "FOCUS_DELAY_B", "Delay B", "%.2f", 0.0, 9.99, 1.0, 0.0);
    IUFillNumberVector(FocusSlopeANP, FocusSlopeAN, 1, getDeviceName(), "Slope A", "", "Slope A", IP_RW, 0, IPS_IDLE);
    IUFillNumberVector(FocusSlopeBNP, FocusSlopeBN, 1, getDeviceName(), "Slope A", "", "Slope A", IP_RW, 0, IPS_IDLE);
    IUFillNumberVector(FocusDelayANP, FocusDelayAN, 1, getDeviceName(), "Slope A", "", "Slope A", IP_RW, 0, IPS_IDLE);
    IUFillNumberVector(FocusDelayBNP, FocusDelayBN, 1, getDeviceName(), "Slope A", "", "Slope A", IP_RW, 0, IPS_IDLE);

    setDynamicPropertiesBehavior(false, false);

    buildSkeleton("indi_tcfs_sk.xml");

    FocusTemperatureNP = getNumber("FOCUS_TEMPERATURE");
    FocusPowerSP       = getSwitch("FOCUS_POWER");
    FocusModeSP        = getSwitch("FOCUS_MODE");
    FocusGotoSP        = getSwitch("FOCUS_GOTO");
    FocusQuietSP       = getSwitch("FOCUS_QUIET");
    FocusSlopeANP      = getNumber("FOCUS_SLOPE_A");
    FocusSlopeBNP      = getNumber("FOCUS_SLOPE_B");;
    FocusDelayANP      = getNumber("FOCUS_DELAY_A");
    FocusDelayBNP      = getNumber("FOCUS_DELAY_B");

    // Default to 19200
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineSwitch(FocusGotoSP);
        defineNumber(FocusTemperatureNP);
        defineSwitch(FocusPowerSP);
        defineSwitch(FocusModeSP);
        defineSwitch(FocusQuietSP);
        defineNumber(FocusSlopeANP);
        defineNumber(FocusSlopeBNP);;
        defineNumber(FocusDelayANP);
        defineNumber(FocusDelayBNP);
    }
    else
    {
        deleteProperty(FocusGotoSP->name);
        deleteProperty(FocusTemperatureNP->name);
        deleteProperty(FocusPowerSP->name);
        deleteProperty(FocusModeSP->name);
        deleteProperty(FocusQuietSP->name);
        deleteProperty(FocusSlopeANP->name);
        deleteProperty(FocusSlopeBNP->name);;
        deleteProperty(FocusDelayANP->name);
        deleteProperty(FocusDelayBNP->name);
    }

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::Handshake()
{
    if (isSimulation())
    {
        LOG_INFO("TCF-S: Simulating connection.");
        currentPosition = simulated_position;
        return true;
    }

    char response[TCFS_MAX_CMD] = { 0 };

    dispatch_command(FWAKUP);
    read_tcfs(response);

    for(int retry=0; retry<5; retry++)
    {
        dispatch_command(FMMODE);
        read_tcfs(response);
        if (strcmp(response, "!") == 0)
        {
            tcflush(PortFD, TCIOFLUSH);
            LOG_INFO("Successfully connected to TCF-S Focuser in Manual Mode.");

            // Enable temperature readout
            FocusTemperatureNP->s = IPS_OK;

            return true;
        }
    }
    tcflush(PortFD, TCIOFLUSH);

    LOG_ERROR("Failed connection to TCF-S Focuser.");

    return false;
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::Disconnect()
{
    FocusTemperatureNP->s = IPS_IDLE;
    IDSetNumber(FocusTemperatureNP, nullptr);

    dispatch_command(FFMODE);

    return INDI::Focuser::Disconnect();
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        char response[TCFS_MAX_CMD] = { 0 };

        if (!strcmp(name, FocusSlopeANP->name))
        {
            IUUpdateNumber(FocusSlopeANP, values, names, n);

            dispatch_command(FLSLOP, FocusSlopeAN[0].value, MODE_A);
            if (read_tcfs(response) == false)
            {
                FocusSlopeANP->s = IPS_ALERT;
                IDSetNumber(FocusSlopeANP, "Error reading TCF-S reply.");
                return true;
            }
            dispatch_command(FLSIGN, FocusSlopeAN[0].value, MODE_A);
            if (read_tcfs(response) == false)
            {
                FocusSlopeANP->s = IPS_ALERT;
                IDSetNumber(FocusSlopeANP, "Error reading TCF-S reply.");
                return true;
            }
            //saveConfig();
            FocusSlopeANP->s = IPS_OK;
            IDSetNumber(FocusSlopeANP, nullptr);

            return true;
        }
        if (!strcmp(name, FocusSlopeBNP->name))
        {
            IUUpdateNumber(FocusSlopeBNP, values, names, n);

            dispatch_command(FLSLOP, FocusSlopeBN[0].value, MODE_B);
            if (read_tcfs(response) == false)
            {
                FocusSlopeBNP->s = IPS_ALERT;
                IDSetNumber(FocusSlopeBNP, "Error reading TCF-S reply.");
                return true;
            }
            dispatch_command(FLSIGN, FocusSlopeBN[0].value, MODE_B);
            if (read_tcfs(response) == false)
            {
                FocusSlopeBNP->s = IPS_ALERT;
                IDSetNumber(FocusSlopeBNP, "Error reading TCF-S reply.");
                return true;
            }
            FocusSlopeBNP->s = IPS_OK;
            IDSetNumber(FocusSlopeBNP, nullptr);
            //saveConfig();

            return true;
        }
        if (!strcmp(name, FocusDelayANP->name))
        {
            IUUpdateNumber(FocusDelayANP, values, names, n);

            dispatch_command(FDELAY, FocusDelayAN[0].value*100, MODE_A);
            if (read_tcfs(response) == false)
            {
                FocusDelayANP->s = IPS_ALERT;
                IDSetNumber(FocusDelayANP, "Error reading TCF-S reply.");
                return true;
            }
            //saveConfig();
            FocusDelayANP->s = IPS_OK;
            IDSetNumber(FocusDelayANP, nullptr);

            return true;
        }
        if (!strcmp(name, FocusDelayBNP->name))
        {
            IUUpdateNumber(FocusDelayBNP, values, names, n);

            dispatch_command(FDELAY, FocusDelayBN[0].value*100, MODE_B);
            if (read_tcfs(response) == false)
            {
                FocusDelayBNP->s = IPS_ALERT;
                IDSetNumber(FocusDelayBNP, "Error reading TCF-S reply.");
                return true;
            }
            FocusDelayBNP->s = IPS_OK;
            IDSetNumber(FocusDelayBNP, nullptr);
            //saveConfig();

            return true;
        }
    }

    return Focuser::ISNewNumber(dev, name, values, names, n);
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        char response[TCFS_MAX_CMD] = { 0 };

        if (!strcmp(FocusPowerSP->name, name))
        {
            IUUpdateSwitch(FocusPowerSP, states, names, n);
            bool sleep = false;

            ISwitch *sp = IUFindOnSwitch(FocusPowerSP);

            // Sleep
            if (!strcmp(sp->name, "FOCUS_SLEEP"))
            {
                dispatch_command(FSLEEP);
                sleep = true;
            }
            // Wake Up
            else
                dispatch_command(FWAKUP);

            if (read_tcfs(response) == false)
            {
                IUResetSwitch(FocusPowerSP);
                FocusPowerSP->s = IPS_ALERT;
                IDSetSwitch(FocusPowerSP, "Error reading TCF-S reply.");
                return true;
            }

            if (sleep)
            {
                if (isSimulation())
                    strncpy(response, "ZZZ", TCFS_MAX_CMD);

                if (strcmp(response, "ZZZ") == 0)
                {
                    FocusPowerSP->s = IPS_OK;
                    IDSetSwitch(FocusPowerSP, "Focuser is set into sleep mode.");
                    FocusAbsPosNP.s = IPS_IDLE;
                    IDSetNumber(&FocusAbsPosNP, nullptr);
                    if (FocusTemperatureNP)
                    {
                        FocusTemperatureNP->s = IPS_IDLE;
                        IDSetNumber(FocusTemperatureNP, nullptr);
                    }

                    return true;
                }
                else
                {
                    FocusPowerSP->s = IPS_ALERT;
                    IDSetSwitch(FocusPowerSP, "Focuser sleep mode operation failed. Response: %s.", response);
                    return true;
                }
            }
            else
            {
                if (isSimulation())
                    strncpy(response, "WAKE", TCFS_MAX_CMD);

                if (strcmp(response, "WAKE") == 0)
                {
                    FocusPowerSP->s = IPS_OK;
                    IDSetSwitch(FocusPowerSP, "Focuser is awake.");
                    FocusAbsPosNP.s = IPS_OK;
                    IDSetNumber(&FocusAbsPosNP, nullptr);
                    if (FocusTemperatureNP)
                    {
                        FocusTemperatureNP->s = IPS_OK;
                        IDSetNumber(FocusTemperatureNP, nullptr);
                    }

                    return true;
                }
                else
                {
                    FocusPowerSP->s = IPS_ALERT;
                    IDSetSwitch(FocusPowerSP, "Focuser wake up operation failed. Response: %s", response);
                    return true;
                }
            }
        }

        // Do not process any command if focuser is asleep
        if (isConnected() && FocusPowerSP->sp[0].s == ISS_ON)
        {
            ISwitchVectorProperty *svp = getSwitch(name);
            if (svp)
            {
                svp->s = IPS_IDLE;
                LOG_WARN("Focuser is still in sleep mode. Wake up in order to issue commands.");
                IDSetSwitch(svp, nullptr);
            }
            return true;
        }

        if (!strcmp(FocusModeSP->name, name))
        {
            IUUpdateSwitch(FocusModeSP, states, names, n);
            FocusModeSP->s = IPS_OK;

            ISwitch *sp = IUFindOnSwitch(FocusModeSP);

            if (!strcmp(sp->name, "Manual"))
            {
                dispatch_command(FMMODE);
                read_tcfs(response);
                if (!isSimulation() && strcmp(response, "!") != 0)
                {
                    IUResetSwitch(FocusModeSP);
                    FocusModeSP->s = IPS_ALERT;
                    IDSetSwitch(FocusModeSP, "Error switching to manual mode. No reply from TCF-S. Try again.");
                    return true;
                }
            }
            else if (!strcmp(sp->name, "Auto A"))
            {
                dispatch_command(FAMODE);
                read_tcfs(response);
                if (!isSimulation() && strcmp(response, "A") != 0)
                {
                    IUResetSwitch(FocusModeSP);
                    FocusModeSP->s = IPS_ALERT;
                    IDSetSwitch(FocusModeSP, "Error switching to Auto Mode A. No reply from TCF-S. Try again.");
                    return true;
                }
            }
            else
            {
                dispatch_command(FBMODE);
                read_tcfs(response);
                if (!isSimulation() && strcmp(response, "B") != 0)
                {
                    IUResetSwitch(FocusModeSP);
                    FocusModeSP->s = IPS_ALERT;
                    IDSetSwitch(FocusModeSP, "Error switching to Auto Mode B. No reply from TCF-S. Try again.");
                    return true;
                }
            }

            IDSetSwitch(FocusModeSP, nullptr);
            return true;
        }

        if (!strcmp(FocusGotoSP->name, name))
        {
            if (FocusModeSP->sp[0].s != ISS_ON)
            {
                FocusGotoSP->s = IPS_IDLE;
                IDSetSwitch(FocusGotoSP, nullptr);
                LOG_WARN("The focuser can only be moved in Manual mode.");
                return false;
            }

            IUUpdateSwitch(FocusGotoSP, states, names, n);
            FocusGotoSP->s = IPS_BUSY;

            ISwitch *sp = IUFindOnSwitch(FocusGotoSP);

            // Min
            if (!strcmp(sp->name, "FOCUS_MIN"))
            {
                targetTicks = currentPosition;
                MoveRelFocuser(FOCUS_INWARD, currentPosition);
                IDSetSwitch(FocusGotoSP, "Moving focuser to minimum position...");
            }
            // Center
            else if (!strcmp(sp->name, "FOCUS_CENTER"))
            {
                dispatch_command(FCENTR);
                FocusAbsPosNP.s = FocusRelPosNP.s = IPS_BUSY;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                IDSetSwitch(FocusGotoSP, "Moving focuser to center position %d...", isTCFS3 ? 5000 : 3500);
                return true;
            }
            // Max
            else if (!strcmp(sp->name, "FOCUS_MAX"))
            {
                unsigned int delta = 0;
                delta              = FocusAbsPosN[0].max - currentPosition;
                MoveRelFocuser(FOCUS_OUTWARD, delta);
                IDSetSwitch(FocusGotoSP, "Moving focuser to maximum position %g...", FocusAbsPosN[0].max);
            }
            // Home
            else if (!strcmp(sp->name, "FOCUS_HOME"))
            {
                dispatch_command(FHOME);
                read_tcfs(response);

                if (isSimulation())
                    strncpy(response, "DONE", TCFS_MAX_CMD);

                if (strcmp(response, "DONE") == 0)
                {
                    IUResetSwitch(FocusGotoSP);
                    FocusGotoSP->s = IPS_OK;
                    IDSetSwitch(FocusGotoSP, "Moving focuser to new calculated position based on temperature...");
                    return true;
                }
                else
                {
                    IUResetSwitch(FocusGotoSP);
                    FocusGotoSP->s = IPS_ALERT;
                    IDSetSwitch(FocusGotoSP, "Failed to move focuser to home position!");
                    return true;
                }
            }

            IDSetSwitch(FocusGotoSP, nullptr);
            return true;
        }
        // handle quiet mode on/off
        if (!strcmp(FocusQuietSP->name, name))
        {
            IUUpdateSwitch(FocusQuietSP, states, names, n);


            bool quiet = false;

            ISwitch *sp = IUFindOnSwitch(FocusQuietSP);

            // Quiet
            if (!strcmp(sp->name, "FOCUS_QUIET"))
            {
                dispatch_command(FQUIET, 1);
                quiet = true;
            }
            // Chatty
            else
                dispatch_command(FQUIET, 0);

            if (read_tcfs(response) == false)
            {
                IUResetSwitch(FocusQuietSP);
                FocusQuietSP->s = IPS_ALERT;
                IDSetSwitch(FocusQuietSP, "Error reading TCF-S reply.");
                return true;
            }


            if (quiet)
            {
                if (isSimulation())
                    strncpy(response, "DONE", TCFS_MAX_CMD);

                if (strcmp(response, "DONE") == 0)
                {
                    FocusQuietSP->s = IPS_OK;
                    IDSetSwitch(FocusQuietSP, "Focuser is set into quiet mode.");
                    if (FocusTemperatureNP)
                    {
                        FocusTemperatureNP->s = IPS_IDLE;
                        IDSetNumber(FocusTemperatureNP, nullptr);
                    }
                    return true;
                }
                else
                {
                    FocusQuietSP->s = IPS_ALERT;
                    IDSetSwitch(FocusQuietSP, "Focuser quiet mode operation failed. Response: %s.", response);
                    return true;
                }
            }
            else
            {
                if (isSimulation())
                    strncpy(response, "DONE", TCFS_MAX_CMD);
                    
                if (strcmp(response, "DONE") == 0)
                {
                    FocusQuietSP->s = IPS_OK;
                    IDSetSwitch(FocusQuietSP, "Focuser is chatty.");
                    if (FocusTemperatureNP)
                    {
                        FocusTemperatureNP->s = IPS_OK;
                        IDSetNumber(FocusTemperatureNP, nullptr);
                    }
                    return true;
                }
                else
                {
                    FocusQuietSP->s = IPS_ALERT;
                    IDSetSwitch(FocusQuietSP, "Focuser chatty operation failed. Response: %s", response);
                    return true;
                }
            }
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

IPState TCFS::MoveAbsFocuser(uint32_t targetTicks)
{
    int delta = targetTicks - currentPosition;

    if (delta < 0)
        return MoveRelFocuser(FOCUS_INWARD, (uint32_t)std::abs(delta));

    return MoveRelFocuser(FOCUS_OUTWARD, (uint32_t)std::abs(delta));
}

IPState TCFS::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    if (FocusModeSP->sp[0].s != ISS_ON)
    {
        LOG_WARN("The focuser can only be moved in Manual mode.");
        return IPS_ALERT;
    }

    targetTicks    = ticks;
    targetPosition = currentPosition;

    // Inward
    if (dir == FOCUS_INWARD)
    {
        targetPosition -= targetTicks;
        dispatch_command(FIN);
    }
    // Outward
    else
    {
        targetPosition += targetTicks;
        dispatch_command(FOUT);
    }

    FocusAbsPosNP.s = IPS_BUSY;
    FocusRelPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, nullptr);
    IDSetNumber(&FocusRelPosNP, nullptr);

    simulated_position = targetPosition;

    return IPS_BUSY;
}

bool TCFS::dispatch_command(TCFSCommand command_type, int val, TCFSMode m)
{
    int err_code = 0, nbytes_written = 0;
    char tcfs_error[TCFS_ERROR_BUFFER];
    char command[TCFS_MAX_CMD] = {0};

    switch (command_type)
    {
        // Focuser Manual Mode
        case FMMODE:
            strncpy(command, "FMMODE", TCFS_MAX_CMD);
            break;

        // Focuser Free Mode
        case FFMODE:
            strncpy(command, "FFMODE", TCFS_MAX_CMD);
            break;
        // Focuser Auto-A Mode
        case FAMODE:
            strncpy(command, "FAMODE", TCFS_MAX_CMD);
            break;

        // Focuser Auto-B Mode
        case FBMODE:
            strncpy(command, "FBMODE", TCFS_MAX_CMD);
            break;

        // Focus Center
        case FCENTR:
            strncpy(command, "FCENTR", TCFS_MAX_CMD);
            break;

        // Focuser In “nnnn”
        case FIN:
            simulated_position = currentPosition;

            snprintf(command, TCFS_MAX_CMD, "FI%04d", targetTicks);
            break;

        // Focuser Out “nnnn”
        case FOUT:
            simulated_position = currentPosition;

            snprintf(command, TCFS_MAX_CMD, "FO%04d", targetTicks);
            break;

        // Focuser Position Read Out
        case FPOSRO:
            strncpy(command, "FPOSRO", TCFS_MAX_CMD);
            break;

        // Focuser Position Read Out
        case FTMPRO:
            strncpy(command, "FTMPRO", TCFS_MAX_CMD);
            break;

        // Focuser Sleep
        case FSLEEP:
            strncpy(command, "FSLEEP", TCFS_MAX_CMD);
            break;
        // Focuser Wake Up
        case FWAKUP:
            strncpy(command, "FWAKUP", TCFS_MAX_CMD);
            break;
        // Focuser Home Command
        case FHOME:
            strncpy(command, "FHOME", TCFS_MAX_CMD);
            break;
        // Focuser Quiet Command
        case FQUIET:
            snprintf(command, TCFS_MAX_CMD, "FQUIT%01d", val);
            break;
        // Focuser Load Slope Command
        case FLSLOP:
            snprintf(command, TCFS_MAX_CMD, "FL%c%03d", m==MODE_A?'A':'B', abs(val));
            break;
        // Focuser Load Delay Command
        case FDELAY:
            snprintf(command, TCFS_MAX_CMD, "FD%c%03d", m==MODE_A?'A':'B', val);
            break;
        // Focuser Load Sign Command
        case FLSIGN:
            snprintf(command, TCFS_MAX_CMD, "FZ%cxx%01d", m==MODE_A?'A':'B', val>=0?0:1);
            break;
        // Focuser Read Slope Command
        case FRSLOP:
            snprintf(command, TCFS_MAX_CMD, "FREAD%c", m==MODE_A?'A':'B');
            break;
        // Focuser Read Sign Command
        case FRSIGN:
            snprintf(command, TCFS_MAX_CMD, "FTxxx%c", m==MODE_A?'A':'B');
            break;
    }

    LOGF_DEBUG("CMD <%s>", command);

    if (isSimulation())
        return true;

    tcflush(PortFD, TCIOFLUSH);

    if ((err_code = tty_write(PortFD, command, strlen(command), &nbytes_written) != TTY_OK))
    {
        tty_error_msg(err_code, tcfs_error, TCFS_ERROR_BUFFER);
        LOGF_ERROR("TTY error detected: %s", tcfs_error);
        return false;
    }

    return true;
}

void TCFS::TimerHit()
{
    static double lastPosition = -1, lastTemperature = -1000;

    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    int f_position      = 0;
    float f_temperature = 0;
    char response[TCFS_MAX_CMD] = { 0 };

    if (FocusGotoSP->s == IPS_BUSY)
    {
        ISwitch *sp = IUFindOnSwitch(FocusGotoSP);

        if (sp != nullptr && strcmp(sp->name, "FOCUS_CENTER") == 0)
        {
            bool rc = read_tcfs(response, true);

            if (!rc)
            {
                SetTimer(POLLMS);
                return;
            }

            if (isSimulation())
                strncpy(response, "CENTER", TCFS_MAX_CMD);

            if (strcmp(response, "CENTER") == 0)
            {
                IUResetSwitch(FocusGotoSP);
                FocusGotoSP->s  = IPS_OK;
                FocusAbsPosNP.s = IPS_OK;

                IDSetSwitch(FocusGotoSP, nullptr);
                IDSetNumber(&FocusAbsPosNP, nullptr);

                LOG_INFO("Focuser moved to center position.");
            }
        }
    }

    switch (FocusAbsPosNP.s)
    {
        case IPS_OK:
            if (FocusModeSP->sp[0].s == ISS_ON)
                dispatch_command(FPOSRO);

            if (read_tcfs(response) == false)
            {
                SetTimer(POLLMS);
                return;
            }

            if (isSimulation())
                snprintf(response, TCFS_MAX_CMD, "P=%04d", (int)simulated_position);

            sscanf(response, "P=%d", &f_position);
            currentPosition = f_position;

            if (lastPosition != currentPosition)
            {
                lastPosition = currentPosition;
                IDSetNumber(&FocusAbsPosNP, nullptr);
            }
            break;

        case IPS_BUSY:
            if (read_tcfs(response, true) == false)
            {
                SetTimer(POLLMS);
                return;
            }

            // Ignore error
            if (strstr(response, "ER") != nullptr)
            {
                LOGF_DEBUG("Received error: %s", response);
                SetTimer(POLLMS);
                return;
            }

            if (isSimulation())
                strncpy(response, "*", 2);

            if (strcmp(response, "*") == 0)
            {
                LOGF_DEBUG("Moving focuser %d steps to position %d.", targetTicks, targetPosition);
                FocusAbsPosNP.s = IPS_OK;
                FocusRelPosNP.s = IPS_OK;
                FocusGotoSP->s  = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                IDSetSwitch(FocusGotoSP, nullptr);
            }
            else
            {
                FocusAbsPosNP.s = IPS_ALERT;
                LOGF_ERROR("Unable to read response from focuser #%s#.", response);
                IDSetNumber(&FocusAbsPosNP, nullptr);
            }
            break;

        default:
            break;
    }

    if (FocusTemperatureNP->s != IPS_IDLE)
    {
        // Read Temperature
        // Manual Mode
        if (FocusModeSP->sp[0].s == ISS_ON)
            dispatch_command(FTMPRO);

        if (read_tcfs(response) == false)
        {
            SetTimer(POLLMS);
            return;
        }

        if (isSimulation())
            snprintf(response, TCFS_MAX_CMD, "T=%0.1f", simulated_temperature);

        sscanf(response, "T=%f", &f_temperature);

        FocusTemperatureNP->np[0].value = f_temperature;

        if (lastTemperature != FocusTemperatureNP->np[0].value)
        {
            lastTemperature = FocusTemperatureNP->np[0].value;
            IDSetNumber(FocusTemperatureNP, nullptr);
        }
    }

    SetTimer(POLLMS);
}

bool TCFS::read_tcfs(char *response, bool silent)
{
    int err_code = 0, nbytes_read = 0;
    char err_msg[TCFS_ERROR_BUFFER];

    if (isSimulation())
    {
        strncpy(response, "SIMULATION", TCFS_MAX_CMD);
        return true;
    }

    // Read until encountring a CR
    if ((err_code = tty_read_section(PortFD, response, 0x0D, 5, &nbytes_read)) != TTY_OK)
    {
        if (!silent)
        {
            tty_error_msg(err_code, err_msg, 32);
            LOGF_ERROR("TTY error detected: %s", err_msg);
        }

        return false;
    }

    // Remove LF & CR
    response[nbytes_read - 2] = '\0';

    LOGF_DEBUG("RES <%s>", response);

    return true;
}

const char *TCFS::getDefaultName()
{
    return mydev;
}
