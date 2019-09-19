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
    currentMode = MANUAL;
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);

    setVersion(0, 4);
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::initProperties()
{
    //LOGF_DEBUG("%s %s",__FUNCTION__, me);
    INDI::Focuser::initProperties();

    // Set upper limit for TCF-S3 focuser
    if (strcmp(me, "indi_tcfs3_focus") == 0)
    {
        isTCFS3 = true;

        FocusMaxPosN[0].max = 9999;

        FocusAbsPosN[0].max  = 9999;
        FocusRelPosN[0].max  = 2000;
        FocusRelPosN[0].step = FocusAbsPosN[0].step = 100;
        FocusRelPosN[0].value                       = 0;
        LOG_DEBUG("TCF-S3 detected. Updating maximum position value to 9999.");
    }
    else
    {
        isTCFS3 = false;

        FocusMaxPosN[0].max = 7000;

        FocusAbsPosN[0].max  = 7000;
        FocusRelPosN[0].max  = 2000;
        FocusRelPosN[0].step = FocusAbsPosN[0].step = 100;
        FocusRelPosN[0].value                       = 0;
        LOG_DEBUG("TCF-S detected. Updating maximum position value to 7000.");
    }

    IUFillSwitch(&FocusModeS[0], "Manual", "", ISS_ON);
    IUFillSwitch(&FocusModeS[1], "Auto A", "", ISS_OFF);
    IUFillSwitch(&FocusModeS[2], "Auto B", "", ISS_OFF);
    IUFillSwitchVector(&FocusModeSP, FocusModeS, 3, getDeviceName(), "FOCUS_MODE", "Mode", "Main Control", IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&FocusPowerS[0], "FOCUS_SLEEP", "Sleep", ISS_OFF);
    IUFillSwitch(&FocusPowerS[1], "FOCUS_WAKEUP", "Wake up", ISS_OFF);
    IUFillSwitchVector(&FocusPowerSP, FocusPowerS, 2, getDeviceName(), "FOCUS_POWER", "Power", "Operation", IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&FocusGotoS[0], "FOCUS_MIN", "Min", ISS_OFF);
    IUFillSwitch(&FocusGotoS[1], "FOCUS_CENTER", "Center", ISS_OFF);
    IUFillSwitch(&FocusGotoS[2], "FOCUS_MAX", "Max", ISS_OFF);
    IUFillSwitch(&FocusGotoS[3], "FOCUS_HOME", "Home", ISS_OFF);
    IUFillSwitchVector(&FocusGotoSP, FocusGotoS, 4, getDeviceName(), "FOCUS_GOTO", "Go to", "Main Control", IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillNumber(&FocusTemperatureN[0], "FOCUS_TEMPERATURE_VALUE", "Temperature (c)", "%.3f", -50.0, 80, 0, 0);
    IUFillNumberVector(&FocusTemperatureNP, FocusTemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Probe", "Operation", IP_RO, 0, IPS_IDLE);

    IUFillSwitch(&FocusTelemetryS[0], "FOCUS_TELEMETRY_ON", "Enable", ISS_ON);
    IUFillSwitch(&FocusTelemetryS[1], "FOCUS_TELEMETRY_OFF", "Disable", ISS_OFF);
    IUFillSwitchVector(&FocusTelemetrySP, FocusTelemetryS, 2, getDeviceName(), "FOCUS_TELEMETRY", "Telemetry", "Operation", IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Mode parameters
    IUFillNumber(&FocusModeAN[0], "FOCUS_SLOPE_A", "Slope A", "%.0f", -999, 999, 10, 0);
    IUFillNumber(&FocusModeAN[1], "FOCUS_INTERCEPT_A", "Intercept A", "%.0f", 0, FocusAbsPosN[0].max, 10, 0);
    IUFillNumber(&FocusModeAN[2], "FOCUS_DELAY_A", "Delay A", "%.2f", 0.00, 9.99, 1.0, 0);
    IUFillNumberVector(&FocusModeANP, FocusModeAN, 3, getDeviceName(), "FOCUS_MODE_A", "Mode A", "Presets", IP_RW, 0, IPS_IDLE);
    IUFillNumber(&FocusModeBN[0], "FOCUS_SLOPE_B", "Slope B", "%.0f", -999, 999, 10, 0);
    IUFillNumber(&FocusModeBN[1], "FOCUS_INTERCEPT_B", "Intercept B", "%.0f", 0, FocusAbsPosN[0].max, 10, 0);
    IUFillNumber(&FocusModeBN[2], "FOCUS_DELAY_B", "Delay B", "%.2f", 0.00, 9.99, 1.0, 0);
    IUFillNumberVector(&FocusModeBNP, FocusModeBN, 3, getDeviceName(), "FOCUS_MODE_B", "Mode B", "Presets", IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&FocusStartModeS[0], "FOCUS_START_ON", "Enable", ISS_OFF);
    IUFillSwitch(&FocusStartModeS[1], "FOCUS_START_OFF", "Disable", ISS_ON);
    IUFillSwitchVector(&FocusStartModeSP, FocusStartModeS, 2, getDeviceName(), "FOCUS_START_MODE", "Startup Mode", "Presets", IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

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
    //LOGF_DEBUG("%s %s",__FUNCTION__, me);
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineSwitch(&FocusGotoSP);
        defineNumber(&FocusTemperatureNP);
        defineSwitch(&FocusPowerSP);
        defineSwitch(&FocusModeSP);
        defineSwitch(&FocusTelemetrySP);
        defineSwitch(&FocusStartModeSP);
        defineNumber(&FocusModeANP);
        defineNumber(&FocusModeBNP);;
        GetFocusParams();
    }
    else
    {
        deleteProperty(FocusGotoSP.name);
        deleteProperty(FocusTemperatureNP.name);
        deleteProperty(FocusPowerSP.name);
        deleteProperty(FocusModeSP.name);
        deleteProperty(FocusTelemetrySP.name);
        deleteProperty(FocusStartModeSP.name);
        deleteProperty(FocusModeANP.name);
        deleteProperty(FocusModeBNP.name);;
    }

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &FocusModeANP);
    IUSaveConfigNumber(fp, &FocusModeBNP);
    IUSaveConfigSwitch(fp, &FocusStartModeSP);

    // Add more properties to config file here
    //        IUSaveConfigSwitch(fp, &SwitchSP);
    //        IUSaveConfigNumber(fp, &NumberNP);
    //        IUSaveConfigText(fp, &TextTP);

    return true;
}

/****************************************************************
**
**
*****************************************************************/
void TCFS::GetFocusParams()
{
    LOGF_DEBUG("A slope=%.0f", FocusModeANP.np[0].value);//, FocusModeAN[0].value);
    /*
        char response[TCFS_MAX_CMD] = { 0 };
        int slope;
        int is_negative;
        dispatch_command(FRSLOP, 0, MODE_A);
        read_tcfs(response);
        if(sscanf(response, "A=%04d", &slope)<=0)
        {
            LOGF_WARN("Failed to read slope A from response: %s", response);
            return;
        }
        response[0] = '\0';
        dispatch_command(FRSIGN, 0, MODE_A);
        read_tcfs(response);
        if(sscanf(response, "A=%01d", &is_negative)<=0)
        {
            LOGF_WARN("Failed to read slope sign A from response: %s", response);
            return;
        }

        FocusModeANP.np[0].value = slope * (is_negative==1?-1:1);
        IDSetNumber(&FocusModeANP, nullptr);

        response[0] = '\0';
        dispatch_command(FRSLOP, 0, MODE_B);
        read_tcfs(response);
        if(sscanf(response, "B=%04d", &slope)<=0)
        {
            LOGF_WARN("Failed to read slope B from response: %s", response);
            return;
        }

        response[0] = '\0';
        dispatch_command(FRSIGN, 0, MODE_B);
        read_tcfs(response);
        if(sscanf(response, "B=%01d", &is_negative)<=0)
        {
            LOGF_WARN("Failed to read slope sign B from response: %s", response);
            return;
        }

        FocusModeBNP.np[0].value = slope * (is_negative==1?-1:1);
        IDSetNumber(&FocusModeBNP, nullptr);
    */
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::Handshake()
{
    LOGF_DEBUG("%s %s", __FUNCTION__, me);
    if (isSimulation())
    {
        LOG_INFO("TCF-S: Simulating connection.");
        currentPosition = simulated_position;
        return true;
    }
    char response[TCFS_MAX_CMD] = { 0 };
    dispatch_command(FWAKUP);
    read_tcfs(response);
    if (strcmp(response, "WAKE") == 0)
    {
        LOG_INFO("TCF-S Focuser is awake");
        tcflush(PortFD, TCIOFLUSH);
    }

    if(SetManualMode())
    {
        LOG_INFO("Successfully connected to TCF-S Focuser in Manual Mode.");

        // Enable temperature readout
        FocusTemperatureNP.s = IPS_OK;

        return true;
    }
    tcflush(PortFD, TCIOFLUSH);
    LOG_ERROR("Failed connection to TCF-S Focuser.");
    return false;
}
/****************************************************************
**
**
*****************************************************************/
bool TCFS::SetManualMode()
{
    char response[TCFS_MAX_CMD] = { 0 };
    for(int retry = 0; retry < 5; retry++)
    {
        dispatch_command(FMMODE);
        read_tcfs(response);
        if (strcmp(response, "!") == 0)
        {
            tcflush(PortFD, TCIOFLUSH);
            currentMode = MANUAL;
            return true;
        }
    }
    tcflush(PortFD, TCIOFLUSH);
    return false;
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::Disconnect()
{
    FocusTemperatureNP.s = IPS_IDLE;
    IDSetNumber(&FocusTemperatureNP, nullptr);

    dispatch_command(FFMODE);

    return INDI::Focuser::Disconnect();
}

/****************************************************************
**
**
*****************************************************************/
bool TCFS::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //LOGF_DEBUG("%s %s %s %s",__FUNCTION__, me, dev, name);
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        char response[TCFS_MAX_CMD] = { 0 };
        // In Auto mode only FMMODE can be accepted
        // In Sleep mode only FWAKUP can be accepted
        // While focuser is moving don't allow actions other than FMMODE

        if (!strcmp(name, FocusModeANP.name))
        {
            IUUpdateNumber(&FocusModeANP, values, names, n);

            dispatch_command(FLSLOP, FocusModeAN[0].value, MODE_A);
            if (read_tcfs(response) == false)
            {
                FocusModeANP.s = IPS_ALERT;
                IDSetNumber(&FocusModeANP, "Error reading TCF-S reply.");
                return true;
            }
            dispatch_command(FLSIGN, FocusModeAN[0].value, MODE_A);
            if (read_tcfs(response) == false)
            {
                FocusModeANP.s = IPS_ALERT;
                IDSetNumber(&FocusModeANP, "Error reading TCF-S reply.");
                return true;
            }
            //saveConfig();
            dispatch_command(FDELAY, FocusModeAN[2].value * 100, MODE_A);
            if (read_tcfs(response) == false)
            {
                FocusModeANP.s = IPS_ALERT;
                IDSetNumber(&FocusModeANP, "Error reading TCF-S reply.");
                return true;
            }
            //saveConfig();
            FocusModeANP.s = IPS_OK;
            IDSetNumber(&FocusModeANP, nullptr);

            return true;
        }
        if (!strcmp(name, FocusModeBNP.name))
        {
            IUUpdateNumber(&FocusModeBNP, values, names, n);

            dispatch_command(FLSLOP, FocusModeBN[0].value, MODE_B);
            if (read_tcfs(response) == false)
            {
                FocusModeBNP.s = IPS_ALERT;
                IDSetNumber(&FocusModeBNP, "Error reading TCF-S reply.");
                return true;
            }
            dispatch_command(FLSIGN, FocusModeBN[0].value, MODE_B);
            if (read_tcfs(response) == false)
            {
                FocusModeBNP.s = IPS_ALERT;
                IDSetNumber(&FocusModeBNP, "Error reading TCF-S reply.");
                return true;
            }
            dispatch_command(FDELAY, FocusModeBN[2].value * 100, MODE_B);
            if (read_tcfs(response) == false)
            {
                FocusModeBNP.s = IPS_ALERT;
                IDSetNumber(&FocusModeBNP, "Error reading TCF-S reply.");
                return true;
            }
            //saveConfig();
            FocusModeBNP.s = IPS_OK;
            IDSetNumber(&FocusModeBNP, nullptr);

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
    //LOGF_DEBUG("%s %s %s %s",__FUNCTION__, me, dev, name);
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        char response[TCFS_MAX_CMD] = { 0 };
        // In Auto mode only FMMODE can be accepted
        // In Sleep mode only FWAKUP can be accepted
        // While focuser is moving don't allow actions other than FMMODE
        if (!strcmp(FocusMotionSP.name, name))
        {
            if (FocusModeSP.sp[0].s != ISS_ON)
            {
                LOG_WARN("The focuser can only be moved in Manual mode.");
                return true;
            }
        }
        if (FocusRelPosNP.s == IPS_BUSY)
        {
            LOG_WARN("The focuser is in motion. Wait until it has stopped");
            return true;
        }
        if (!strcmp(FocusPowerSP.name, name))
        {
            IUUpdateSwitch(&FocusPowerSP, states, names, n);
            bool sleep = false;

            ISwitch *sp = IUFindOnSwitch(&FocusPowerSP);

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
                IUResetSwitch(&FocusPowerSP);
                FocusPowerSP.s = IPS_ALERT;
                IDSetSwitch(&FocusPowerSP, "Error reading TCF-S reply.");
                return true;
            }

            if (sleep)
            {
                if (isSimulation())
                    strncpy(response, "ZZZ", TCFS_MAX_CMD);

                if (strcmp(response, "ZZZ") == 0)
                {
                    FocusPowerSP.s = IPS_OK;
                    IDSetSwitch(&FocusPowerSP, "Focuser is set into sleep mode.");
                    FocusAbsPosNP.s = IPS_IDLE;
                    IDSetNumber(&FocusAbsPosNP, nullptr);
                    //                    if (FocusTemperatureNP)
                    {
                        FocusTemperatureNP.s = IPS_IDLE;
                        IDSetNumber(&FocusTemperatureNP, nullptr);
                    }

                    return true;
                }
                else
                {
                    FocusPowerSP.s = IPS_ALERT;
                    IDSetSwitch(&FocusPowerSP, "Focuser sleep mode operation failed. Response: %s.", response);
                    return true;
                }
            }
            else
            {
                if (isSimulation())
                    strncpy(response, "WAKE", TCFS_MAX_CMD);

                if (strcmp(response, "WAKE") == 0)
                {
                    FocusPowerSP.s = IPS_OK;
                    IDSetSwitch(&FocusPowerSP, "Focuser is awake.");
                    FocusAbsPosNP.s = IPS_OK;
                    IDSetNumber(&FocusAbsPosNP, nullptr);
                    //                    if (FocusTemperatureNP)
                    {
                        FocusTemperatureNP.s = IPS_OK;
                        IDSetNumber(&FocusTemperatureNP, nullptr);
                    }

                    return true;
                }
                else
                {
                    FocusPowerSP.s = IPS_ALERT;
                    IDSetSwitch(&FocusPowerSP, "Focuser wake up operation failed. Response: %s", response);
                    return true;
                }
            }
        }

        // Do not process any command if focuser is asleep
        if (isConnected() && FocusPowerSP.sp[0].s == ISS_ON)
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

        if (!strcmp(FocusModeSP.name, name))
        {
            IUUpdateSwitch(&FocusModeSP, states, names, n);
            FocusModeSP.s = IPS_OK;

            ISwitch *sp = IUFindOnSwitch(&FocusModeSP);

            if (!strcmp(sp->name, "Manual"))
            {
                if (!isSimulation() && !SetManualMode())
                {
                    IUResetSwitch(&FocusModeSP);
                    FocusModeSP.s = IPS_ALERT;
                    IDSetSwitch(&FocusModeSP, "Error switching to manual mode. No reply from TCF-S. Try again.");
                    return true;
                }
                LOG_INFO("Entered Manual Mode");
                currentMode = MANUAL;
            }
            else if (!strcmp(sp->name, "Auto A"))
            {
                if(FocusStartModeSP.sp[0].s == ISS_ON)
                {
                    FocusModeSP.s = IPS_BUSY;
                    uint32_t startPos = -FocusTemperatureN[0].value * FocusModeAN[0].value
                                        + FocusModeAN[1].value;
                    LOGF_DEBUG("Autocomp A T=%.1f; m=%f; i=%f; p0=%d;",
                               FocusTemperatureN[0].value,
                               FocusModeAN[0].value,
                               FocusModeAN[1].value,
                               startPos);
                    MoveAbsFocuser(startPos);
                }
                else
                {
                    dispatch_command(FAMODE);
                    read_tcfs(response);
                    if (!isSimulation() && strcmp(response, "A") != 0)
                    {
                        IUResetSwitch(&FocusModeSP);
                        FocusModeSP.s = IPS_ALERT;
                        IDSetSwitch(&FocusModeSP, "Error switching to Auto Mode A, No reply from TCF-S. Try again.");
                    }
                    LOG_INFO("Entered Auto Mode A");
                    currentMode = MODE_A;
                }
            }
            else
            {
                if(FocusStartModeSP.sp[0].s == ISS_ON)
                {
                    FocusModeSP.s = IPS_BUSY;
                    uint32_t startPos = -FocusTemperatureN[0].value * FocusModeBN[0].value
                                        + FocusModeBN[1].value;
                    LOGF_DEBUG("Autocomp B T=%.1f; m=%f; i=%f; p0=%d;",
                               FocusTemperatureN[0].value,
                               FocusModeBN[0].value,
                               FocusModeBN[1].value,
                               startPos);
                    MoveAbsFocuser(startPos);
                }
                else
                {
                    dispatch_command(FBMODE);
                    read_tcfs(response);
                    if (!isSimulation() && strcmp(response, "B") != 0)
                    {
                        IUResetSwitch(&FocusModeSP);
                        FocusModeSP.s = IPS_ALERT;
                        IDSetSwitch(&FocusModeSP, "Error switching to Auto Mode B, No reply from TCF-S. Try again.");
                    }
                    LOG_INFO("Entered Auto Mode B");
                    currentMode = MODE_B;
                }
            }

            IDSetSwitch(&FocusModeSP, nullptr);
            return true;
        }
        // Do not process any other command if focuser is in auto mode
        if (isConnected() && FocusModeSP.sp[0].s != ISS_ON)
        {
            ISwitchVectorProperty *svp = getSwitch(name);
            if (svp)
            {
                svp->s = IPS_IDLE;
                LOG_WARN("Focuser is in auto mode. Change to manual in order to issue commands.");
                IDSetSwitch(svp, nullptr);
            }
            return true;
        }

        if (!strcmp(FocusStartModeSP.name, name))
        {
            IUUpdateSwitch(&FocusStartModeSP, states, names, n);
            FocusStartModeSP.s = IPS_OK;
            //            ISwitch *sp = IUFindOnSwitch(&FocusStartModeSP);
            IDSetSwitch(&FocusStartModeSP, nullptr);
            LOGF_DEBUG("Start Mode %d", FocusStartModeSP.sp[0].s );
            return true;
        }

        if (!strcmp(FocusGotoSP.name, name))
        {
            if (FocusModeSP.sp[0].s != ISS_ON)
            {
                FocusGotoSP.s = IPS_IDLE;
                IDSetSwitch(&FocusGotoSP, nullptr);
                LOG_WARN("The focuser can only be moved in Manual mode.");
                return false;
            }

            IUUpdateSwitch(&FocusGotoSP, states, names, n);
            FocusGotoSP.s = IPS_BUSY;

            ISwitch *sp = IUFindOnSwitch(&FocusGotoSP);

            // Min
            if (!strcmp(sp->name, "FOCUS_MIN"))
            {
                targetTicks = currentPosition;
                MoveRelFocuser(FOCUS_INWARD, currentPosition);
                IDSetSwitch(&FocusGotoSP, "Moving focuser to minimum position...");
            }
            // Center
            else if (!strcmp(sp->name, "FOCUS_CENTER"))
            {
                dispatch_command(FCENTR);
                FocusAbsPosNP.s = FocusRelPosNP.s = IPS_BUSY;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                IDSetSwitch(&FocusGotoSP, "Moving focuser to center position %d...", isTCFS3 ? 5000 : 3500);
                return true;
            }
            // Max
            else if (!strcmp(sp->name, "FOCUS_MAX"))
            {
                unsigned int delta = 0;
                delta              = FocusAbsPosN[0].max - currentPosition;
                MoveRelFocuser(FOCUS_OUTWARD, delta);
                IDSetSwitch(&FocusGotoSP, "Moving focuser to maximum position %g...", FocusAbsPosN[0].max);
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
                    IUResetSwitch(&FocusGotoSP);
                    FocusGotoSP.s = IPS_OK;
                    IDSetSwitch(&FocusGotoSP, "Moving focuser to new calculated position based on temperature...");
                    return true;
                }
                else
                {
                    IUResetSwitch(&FocusGotoSP);
                    FocusGotoSP.s = IPS_ALERT;
                    IDSetSwitch(&FocusGotoSP, "Failed to move focuser to home position!");
                    return true;
                }
            }

            IDSetSwitch(&FocusGotoSP, nullptr);
            return true;
        }
        // handle quiet mode on/off
        if (!strcmp(FocusTelemetrySP.name, name))
        {
            IUUpdateSwitch(&FocusTelemetrySP, states, names, n);


            bool quiet = false;

            ISwitch *sp = IUFindOnSwitch(&FocusTelemetrySP);

            // Telemetry off
            if (!strcmp(sp->name, "FOCUS_TELEMETRY_OFF"))
            {
                dispatch_command(FQUIET, 1);
                quiet = true;
            }
            // Telemetry On
            else
                dispatch_command(FQUIET, 0);

            if (read_tcfs(response) == false)
            {
                IUResetSwitch(&FocusTelemetrySP);
                FocusTelemetrySP.s = IPS_ALERT;
                IDSetSwitch(&FocusTelemetrySP, "Error reading TCF-S reply.");
                return true;
            }

            if (isSimulation())
                strncpy(response, "DONE", TCFS_MAX_CMD);

            if (strcmp(response, "DONE") == 0)
            {
                FocusTelemetrySP.s = IPS_OK;
                IDSetSwitch(&FocusTelemetrySP,
                            quiet ? "Focuser Telemetry is off." : "Focuser Telemetry is on.");
                //                if (FocusTemperatureNP)
                {
                    FocusTemperatureNP.s = quiet ? IPS_IDLE : IPS_OK;
                    IDSetNumber(&FocusTemperatureNP, nullptr);
                }
                return true;
            }
            else
            {
                FocusTelemetrySP.s = IPS_ALERT;
                IDSetSwitch(&FocusTelemetrySP, "Focuser telemetry mode failed. Response: %s.", response);
                return true;
            }
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

IPState TCFS::MoveAbsFocuser(uint32_t targetTicks)
{
    int delta = targetTicks - currentPosition;
    LOGF_DEBUG("Moving to absolute position %d using offset %d", targetTicks, delta);
    return MoveRelFocuser(delta < 0 ? FOCUS_INWARD : FOCUS_OUTWARD, std::abs(delta));
}

IPState TCFS::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    //    if (FocusModeSP.sp[0].s != ISS_ON)
    //    {
    //        LOG_WARN("The focuser can only be moved in Manual mode.");
    //        return IPS_ALERT;
    //    }
    // The TCFS does not allow any commands othern than FMMODE whilst it is
    // in auto mode. But this then prevents auto setting of filter offsets during
    // an imaging sequence. So switch to manual mode, apply the offset then return
    // to auto mode.
    targetTicks    = ticks;
    targetPosition = currentPosition;

    TCFSMode prevMode = currentMode;
    if(currentMode != MANUAL)
    {
        SetManualMode();
    }
    // Inward
    if (dir == FOCUS_INWARD)
    {
        targetPosition -= targetTicks;
        dispatch_command(FIN);

        LOGF_DEBUG("Moving inward by %d steps to position %d", targetTicks, targetPosition);
    }
    // Outward
    else
    {
        targetPosition += targetTicks;
        dispatch_command(FOUT);

        LOGF_DEBUG("Moving outward by %d steps to position %d", targetTicks, targetPosition);
    }

    FocusAbsPosNP.s = IPS_BUSY;
    FocusRelPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, nullptr);
    IDSetNumber(&FocusRelPosNP, nullptr);

    simulated_position = targetPosition;
    if(prevMode != MANUAL)
    {
        IUResetSwitch(&FocusModeSP);
        if (prevMode == MODE_A)
        {
            FocusModeSP.sp[1].s = ISS_ON;
        }
        else
        {
            FocusModeSP.sp[2].s = ISS_ON;
        }
        FocusModeSP.s = IPS_BUSY;
        IDSetSwitch(&FocusModeSP, nullptr);
    }

    return IPS_BUSY;
}

bool TCFS::dispatch_command(TCFSCommand command_type, int val, TCFSMode m)
{
    int err_code = 0, nbytes_written = 0;
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

            snprintf(command, TCFS_MAX_CMD, "FI%04u", targetTicks);
            break;

        // Focuser Out “nnnn”
        case FOUT:
            simulated_position = currentPosition;

            snprintf(command, TCFS_MAX_CMD, "FO%04u", targetTicks);
            break;

        // Focuser Position Read Out
        case FPOSRO:
            strncpy(command, "FPOSRO", TCFS_MAX_CMD);
            break;

        // Focuser Temperature
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
            snprintf(command, TCFS_MAX_CMD, "FL%c%03d", m == MODE_A ? 'A' : 'B', abs(val));
            break;
        // Focuser Load Delay Command
        case FDELAY:
            snprintf(command, TCFS_MAX_CMD, "FD%c%03d", m == MODE_A ? 'A' : 'B', val);
            break;
        // Focuser Load Sign Command
        case FLSIGN:
            snprintf(command, TCFS_MAX_CMD, "FZ%cxx%01d", m == MODE_A ? 'A' : 'B', val >= 0 ? 0 : 1);
            break;
        // Focuser Read Slope Command
        case FRSLOP:
            snprintf(command, TCFS_MAX_CMD, "FREAD%c", m == MODE_A ? 'A' : 'B');
            break;
        // Focuser Read Sign Command
        case FRSIGN:
            snprintf(command, TCFS_MAX_CMD, "Ftxxx%c", m == MODE_A ? 'A' : 'B');
            break;
        case FFWVER:
            snprintf(command, TCFS_MAX_CMD, "FVxxxx");
            break;
    }

    LOGF_DEBUG("CMD <%s>", command);

    if (isSimulation())
        return true;

    tcflush(PortFD, TCIOFLUSH);

    if ( (err_code = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
    {
        char tcfs_error[TCFS_ERROR_BUFFER];
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

    char response[TCFS_MAX_CMD] = { 0 };
    // If focuser is moving wait until "*" is received
    // Then set moving indicator to OK
    if (FocusRelPosNP.s == IPS_BUSY)
    {
        LOGF_DEBUG("%s Motion in Progress...", __FUNCTION__);
        if (read_tcfs(response, true) == false)
        {
            SetTimer(POLLMS);
            return;
        }
        LOGF_DEBUG("%s READY %s", __FUNCTION__, response );
        if(strcmp(response, "*") == 0)
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);

            // If the focuser has stopped moving and auto mode is requested then
            // it is ok to set it now
            if(FocusModeSP.s == IPS_BUSY &&  // Had to move before going Auto
                    (FocusModeSP.sp[1].s == ISS_ON || FocusModeSP.sp[2].s == ISS_ON))
            {
                const char* mode = (FocusModeSP.sp[1].s == ISS_ON) ? "A" : "B";
                dispatch_command(
                    (FocusModeSP.sp[1].s == ISS_ON) ? FAMODE : FBMODE);
                read_tcfs(response);
                if (!isSimulation() && strcmp(response, mode) != 0)
                {
                    IUResetSwitch(&FocusModeSP);
                    FocusModeSP.s = IPS_ALERT;
                    IDSetSwitch(&FocusModeSP, "Error switching to Auto Mode %s. No reply from TCF-S. Try again.", mode);
                    SetTimer(POLLMS);
                    return;
                }
                FocusModeSP.s = IPS_OK;
                LOGF_INFO("Entered Auto Mode %s", mode);
                currentMode = (FocusModeSP.sp[1].s == ISS_ON) ? MODE_A : MODE_B;
                IDSetSwitch(&FocusModeSP, nullptr);
            }
            SetTimer(POLLMS);
            return;
        }
    }

    int f_position      = 0;
    float f_temperature = 0;

    if(!isSimulation() && currentMode != MANUAL)
    {
        if (FocusTelemetrySP.sp[1].s == ISS_ON)
        {
            LOGF_DEBUG("%s %s", __FUNCTION__, "Telemetry is off");
            SetTimer(POLLMS);
            return;
        }
        for(int i = 0; i < 2; i++)
        {
            if (read_tcfs(response, true) == false)
            {
                SetTimer(POLLMS);
                return;
            }
            LOGF_DEBUG("%s Received %s", __FUNCTION__, response);
            if(sscanf(response, "P=%d", &f_position) == 1)
            {
                currentPosition = f_position;
                if (lastPosition != currentPosition)
                {
                    lastPosition = currentPosition;
                    IDSetNumber(&FocusAbsPosNP, nullptr);
                }
            }
            else if(sscanf(response, "T=%f", &f_temperature) == 1)
            {
                FocusTemperatureNP.np[0].value = f_temperature;

                if (lastTemperature != FocusTemperatureNP.np[0].value)
                {
                    lastTemperature = FocusTemperatureNP.np[0].value;
                    IDSetNumber(&FocusTemperatureNP, nullptr);
                }
            }
        }
        SetTimer(POLLMS);
        return;
    }

    if (FocusGotoSP.s == IPS_BUSY)
    {
        ISwitch *sp = IUFindOnSwitch(&FocusGotoSP);

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
                IUResetSwitch(&FocusGotoSP);
                FocusGotoSP.s  = IPS_OK;
                FocusAbsPosNP.s = IPS_OK;

                IDSetSwitch(&FocusGotoSP, nullptr);
                IDSetNumber(&FocusAbsPosNP, nullptr);

                LOG_INFO("Focuser moved to center position.");
            }
        }
    }

    switch (FocusAbsPosNP.s)
    {
        case IPS_OK:
            if (FocusModeSP.sp[0].s == ISS_ON)
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
                FocusGotoSP.s  = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                IDSetSwitch(&FocusGotoSP, nullptr);
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

    if (FocusTemperatureNP.s == IPS_OK || FocusTemperatureNP.s == IPS_BUSY)
    {
        // Read Temperature
        // Manual Mode
        if (FocusModeSP.sp[0].s == ISS_ON)
            dispatch_command(FTMPRO);

        if (read_tcfs(response) == false)
        {
            FocusTemperatureNP.s = IPS_ALERT;
            IDSetNumber(&FocusTemperatureNP, nullptr);
            LOG_ERROR("Failed to read temperature. Is sensor connected?");

            SetTimer(POLLMS);
            return;
        }

        if (isSimulation())
            snprintf(response, TCFS_MAX_CMD, "T=%0.1f", simulated_temperature);

        int rc = sscanf(response, "T=%f", &f_temperature);

        if (rc == 1)
        {
            FocusTemperatureNP.np[0].value = f_temperature;

            if (fabs(lastTemperature - FocusTemperatureNP.np[0].value) > 0.01)
            {
                lastTemperature = FocusTemperatureNP.np[0].value;
                IDSetNumber(&FocusTemperatureNP, nullptr);
            }
        }
        else
        {
            FocusTemperatureNP.s = IPS_ALERT;
            LOGF_ERROR("Failed to read temperature: %s", response);
            IDSetNumber(&FocusTemperatureNP, nullptr);
        }
    }

    SetTimer(POLLMS);
}

bool TCFS::read_tcfs(char *response, bool silent)
{
    int err_code = 0, nbytes_read = 0;

    if (isSimulation())
    {
        strncpy(response, "SIMULATION", TCFS_MAX_CMD);
        return true;
    }

    // Read until encountring a CR
    if ((err_code = tty_read_section(PortFD, response, 0x0D, 2, &nbytes_read)) != TTY_OK)
    {
        if (!silent)
        {
            char err_msg[TCFS_ERROR_BUFFER];
            tty_error_msg(err_code, err_msg, 32);
            LOGF_ERROR("TTY error detected: %s", err_msg);
        }

        return false;
    }

    // Remove LF & CR
    response[nbytes_read - 2] = '\0';

    tcflush(PortFD, TCIOFLUSH);

    if (strstr(response, "ER="))
    {
        int errorCode = 0;
        sscanf(response, "ER=%d", &errorCode);
        LOGF_ERROR("Error Code <%d>", errorCode);
        return false;
    }

    LOGF_DEBUG("RES <%s>", response);
    return true;
}

const char *TCFS::getDefaultName()
{
    return mydev;
}
