/*
    Baader Steeldrive Focuser
    Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "steeldrive.h"

#include "indicom.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>

#define STEELDRIVE_MAX_RETRIES       1
#define STEELDRIVE_TIMEOUT           1
#define STEELDRIVE_MAXBUF            16
#define STEELDRIVE_CMD               9
#define STEELDRIVE_CMD_LONG          11
#define STEELDRIVE_TEMPERATURE_FREQ  20 /* Update every 20 POLLMS cycles. For POLLMS 500ms = 10 seconds freq */
#define STEELDIVE_POSITION_THRESHOLD 5  /* Only send position updates to client if the diff exceeds 5 steps */

#define FOCUS_SETTINGS_TAB "Settings"

std::unique_ptr<SteelDrive> steelDrive(new SteelDrive());

SteelDrive::SteelDrive()
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and has variable speed.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_HAS_VARIABLE_SPEED);
}

bool SteelDrive::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedNP[0].setMin(350);
    FocusSpeedNP[0].setMax(1000);
    FocusSpeedNP[0].setValue(500);
    FocusSpeedNP[0].setStep(50);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Temperature Settings
    IUFillNumber(&TemperatureSettingN[FOCUS_T_COEFF], "Coefficient", "", "%.3f", 0, 0.999, 0.1, 0.1);
    IUFillNumber(&TemperatureSettingN[FOCUS_T_SAMPLES], "# of Samples", "", "%3.0f", 16, 128, 16, 16);
    IUFillNumberVector(&TemperatureSettingNP, TemperatureSettingN, 2, getDeviceName(), "Temperature Settings", "",
                       FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Compensate for temperature
    IUFillSwitch(&TemperatureCompensateS[0], "Enable", "", ISS_OFF);
    IUFillSwitch(&TemperatureCompensateS[1], "Disable", "", ISS_ON);
    IUFillSwitchVector(&TemperatureCompensateSP, TemperatureCompensateS, 2, getDeviceName(), "Temperature Compensate",
                       "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Focuser Models
    IUFillSwitch(&ModelS[0], "NT2", "", ISS_OFF);
    fSettings[0].maxTrip   = 30;
    fSettings[0].gearRatio = 0.25040;
    IUFillSwitch(&ModelS[1], "SC2", "", ISS_OFF);
    fSettings[1].maxTrip   = 30;
    fSettings[1].gearRatio = 0.25040;
    IUFillSwitch(&ModelS[2], "RT2", "", ISS_OFF);
    fSettings[2].maxTrip   = 80;
    fSettings[2].gearRatio = 0.25040;
    IUFillSwitch(&ModelS[3], "RT3", "", ISS_OFF);
    fSettings[3].maxTrip   = 115;
    fSettings[3].gearRatio = 0.25040;
    IUFillSwitch(&ModelS[4], "Custom", "", ISS_ON);
    fSettings[4].maxTrip   = 30;
    fSettings[4].gearRatio = 0.25040;
    IUFillSwitchVector(&ModelSP, ModelS, 5, getDeviceName(), "Model", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Custom Settings
    IUFillNumber(&CustomSettingN[FOCUS_MAX_TRIP], "Max Trip (mm)", "", "%6.2f", 20, 150, 0, 30);
    IUFillNumber(&CustomSettingN[FOCUS_GEAR_RATIO], "Gear Ratio", "", "%.5f", 0.1, 1, 0, .25040);
    IUFillNumberVector(&CustomSettingNP, CustomSettingN, 2, getDeviceName(), "Custom Settings", "", FOCUS_SETTINGS_TAB,
                       IP_RW, 0, IPS_IDLE);

    // Focuser Acceleration
    IUFillNumber(&AccelerationN[0], "Ramp", "", "%3.0f", 1500., 3000., 100., 2000.);
    IUFillNumberVector(&AccelerationNP, AccelerationN, 1, getDeviceName(), "FOCUS_ACCELERATION", "Acceleration",
                       FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Focus Sync
    IUFillNumber(&SyncN[0], "Position (steps)", "", "%3.0f", 0., 200000., 100., 0.);
    IUFillNumberVector(&SyncNP, SyncN, 1, getDeviceName(), "FOCUS_SYNC", "Sync", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    // Version
    IUFillText(&VersionT[0], "HW Rev.", "", nullptr);
    IUFillText(&VersionT[1], "FW Rev.", "", nullptr);
    IUFillTextVector(&VersionTP, VersionT, 2, getDeviceName(), "FOCUS_VERSION", "Version", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);

    FocusRelPosNP[0].setValue(0);
    FocusAbsPosNP[0].setValue(0);
    simPosition           = FocusAbsPosNP[0].getValue();

    updateFocusMaxRange(fSettings[4].maxTrip, fSettings[4].gearRatio);

    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

bool SteelDrive::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&TemperatureNP);
        defineProperty(&TemperatureSettingNP);
        defineProperty(&TemperatureCompensateSP);

        defineProperty(&ModelSP);
        defineProperty(&CustomSettingNP);
        defineProperty(&AccelerationNP);
        defineProperty(&SyncNP);

        defineProperty(&VersionTP);

        GetFocusParams();

        //loadConfig(true);

        LOG_INFO("SteelDrive parameters updated, focuser ready for use.");
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(TemperatureSettingNP.name);
        deleteProperty(TemperatureCompensateSP.name);

        deleteProperty(ModelSP.name);
        deleteProperty(CustomSettingNP.name);
        deleteProperty(AccelerationNP.name);
        deleteProperty(SyncNP.name);

        deleteProperty(VersionTP.name);
    }

    return true;
}

bool SteelDrive::Handshake()
{
    if (isSimulation())
        return true;

    if (Ack())
    {
        LOG_INFO("SteelDrive is online. Getting focus parameters...");
        temperatureUpdateCounter = 0;
        return true;
    }

    LOG_INFO("Error retrieving data from SteelDrive, please ensure SteelDrive controller is "
             "powered and the port is correct.");
    return false;
}

const char *SteelDrive::getDefaultName()
{
    return "Baader SteelDrive";
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char resp[STEELDRIVE_MAXBUF] = {0};
    char hwVer[STEELDRIVE_MAXBUF];

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, ":FVERSIO#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FVERSIO# getHWVersion error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:FVERSIO#)");

    if (sim)
    {
        strncpy(resp, ":FV2.00812#", STEELDRIVE_CMD_LONG + 1);
        nbytes_read = STEELDRIVE_CMD_LONG;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("getHWVersion error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":FV%s#", hwVer);

    return rc > 0;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::updateVersion()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char resp[STEELDRIVE_MAXBUF] = {0};
    char hardware_string[MAXRBUF];
    char firmware_string[MAXRBUF];

    char hwdate[STEELDRIVE_MAXBUF / 2];
    char hwrev[STEELDRIVE_MAXBUF / 2];
    char fwdate[STEELDRIVE_MAXBUF / 2];
    char fwrev[STEELDRIVE_MAXBUF / 2];

    memset(hwdate, 0, sizeof(hwdate));
    memset(hwrev, 0, sizeof(hwrev));
    memset(fwdate, 0, sizeof(fwdate));
    memset(fwrev, 0, sizeof(fwrev));

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, ":FVERSIO#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FVERSIO# getHWVersion write error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:FVERSIO#)");

    if (sim)
    {
        strncpy(resp, ":FV2.00812#", STEELDRIVE_CMD_LONG + 1);
        nbytes_read = STEELDRIVE_CMD_LONG;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("FVERSIO# getHWVersion read error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":FV%s#", hardware_string);

    if (rc > 0)
    {
        strncpy(hwrev, hardware_string, 3);
        strncpy(hwdate, hardware_string + 3, 4);
        char mon[3], year[3];
        memset(mon, 0, sizeof(mon));
        memset(year, 0, sizeof(year));
        strncpy(mon, hwdate, 2);
        strncpy(year, hwdate + 2, 2);
        snprintf(hardware_string, MAXRBUF, "Version: %s Date: %s.%s", hwrev, mon, year);
        IUSaveText(&VersionT[0], hardware_string);
    }
    else
    {
        LOGF_ERROR("Unknown error: getHWVersion value (%s)", resp);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, ":FNFIRMW#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FNFIRMW# getSWVersion write error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:FNFIRMW#)");

    if (sim)
    {
        strncpy(resp, ":FN2.21012#", STEELDRIVE_CMD_LONG + 1);
        nbytes_read = STEELDRIVE_CMD_LONG;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("FNFIRMW# getSWVersion read error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":FN%s#", firmware_string);

    if (rc > 0)
    {
        strncpy(fwrev, firmware_string, 3);
        strncpy(fwdate, firmware_string + 3, 4);
        char mon[3], year[3];
        memset(mon, 0, sizeof(mon));
        memset(year, 0, sizeof(year));
        strncpy(mon, fwdate, 2);
        strncpy(year, fwdate + 2, 2);
        snprintf(firmware_string, MAXRBUF, "Version: %s Date: %s.%s", fwrev, mon, year);
        IUSaveText(&VersionT[1], firmware_string);
    }
    else
    {
        LOGF_ERROR("Unknown error: getSWVersion value (%s)", resp);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::updateTemperature()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char resp[STEELDRIVE_MAXBUF] = {0};
    int temperature;

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, ":F5ASKT0#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":F5ASKT0# updateTemperature write error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:F5ASKT0#)");

    if (sim)
    {
        strncpy(resp, ":F5+1810#", STEELDRIVE_CMD + 1);
        nbytes_read = STEELDRIVE_CMD;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":F5ASKT0# updateTemperature read error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":F5%d#", &temperature);

    if (rc > 0)
    {
        TemperatureN[0].value = ((double)temperature) / 100.0;
        TemperatureNP.s       = IPS_OK;
    }
    else
    {
        char junk[STEELDRIVE_MAXBUF];
        rc = sscanf(resp, ":F5%s#", junk);
        if (rc > 0)
        {
            TemperatureN[0].value = 0;
            LOG_DEBUG("Temperature probe is not connected.");
        }
        else
            LOGF_ERROR("Unknown error: focuser temperature value (%s)", resp);

        TemperatureNP.s = IPS_ALERT;
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::updatePosition()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char resp[STEELDRIVE_MAXBUF] = {0};
    unsigned short pos = 0;
    int retries        = 0;

    for (retries = 0; retries < STEELDRIVE_MAX_RETRIES; retries++)
    {
        tcflush(PortFD, TCIOFLUSH);

        if (!sim && (rc = tty_write(PortFD, ":F8ASKS0#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR(":F8ASKS0# updatePosition write error: %s.", errstr);
            return false;
        }

        LOG_DEBUG("CMD (:F8ASKS0#)");

        if (sim)
        {
            snprintf(resp, STEELDRIVE_CMD_LONG + 1, ":F8%07u#", (int)simPosition);
            nbytes_read = STEELDRIVE_CMD_LONG;
            break;
        }
        else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT - retries, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            resp[nbytes_read] = '\0';
            LOGF_DEBUG(":F8ASKS0# updatePosition read error: %s. Retry: %d. Bytes: %d. Buffer (%s)", errstr, retries,
                       nbytes_read, resp);
        }
        else
            break;
    }

    if (retries == STEELDRIVE_MAX_RETRIES)
    {
        LOG_ERROR("UpdatePosition: failed to read.");
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":F8%hu#", &pos);

    if (rc > 0)
    {
        FocusAbsPosNP[0].setValue(pos);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser position value (%s)", resp);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::updateSpeed()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char resp[STEELDRIVE_MAXBUF] = {0};
    unsigned short speed;

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, ":FGSPMAX#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FGSPMAX# updateSpeed write error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:FGSPMAX#)");

    if (sim)
    {
        strncpy(resp, ":FG00350#", STEELDRIVE_CMD + 1);
        nbytes_read = STEELDRIVE_CMD;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FGSPMAX# updateSpeed read error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":FG%hu#", &speed);

    if (rc > 0)
    {
        FocusSpeedNP[0].setValue(speed);
    }
    else
    {
        LOGF_ERROR("Unknown error: focuser speed value (%s)", resp);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::updateAcceleration()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char resp[STEELDRIVE_MAXBUF] = {0};
    unsigned short accel;

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, ":FHSPMIN#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FHSPMIN# updateAcceleration write error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:FHSPMIN#)");

    if (sim)
    {
        strncpy(resp, ":FH01800#", STEELDRIVE_CMD + 1);
        nbytes_read = STEELDRIVE_CMD;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FHSPMIN# updateAcceleration read error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":FH%hu#", &accel);

    if (rc > 0)
    {
        AccelerationN[0].value = accel;
    }
    else
    {
        LOGF_ERROR("Unknown error: updateAcceleration value (%s)", resp);
        return false;
    }
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::updateTemperatureSettings()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char resp[STEELDRIVE_MAXBUF] = {0};

    char selectedFocuser[1], coeff[3], enabled[1], tResp[STEELDRIVE_MAXBUF];

    tcflush(PortFD, TCIOFLUSH);

    if (!sim && (rc = tty_write(PortFD, ":F7ASKC0#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":F7ASKC0# updateTemperatureSettings write error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:F7ASKC0#)");

    if (sim)
    {
        strncpy(resp, ":F710004#", STEELDRIVE_CMD + 1);
        nbytes_read = STEELDRIVE_CMD;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":F7ASKC0# updateTemperatureSettings read error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":F7%s#", tResp);

    if (rc > 0)
    {
        strncpy(coeff, tResp, 3);
        strncpy(enabled, tResp + 3, 1);
        strncpy(selectedFocuser, tResp + 4, 1);

        TemperatureSettingN[FOCUS_T_COEFF].value = atof(coeff) / 1000.0;

        IUResetSwitch(&TemperatureCompensateSP);
        if (enabled[0] == '0')
            TemperatureCompensateS[1].s = ISS_ON;
        else
            TemperatureCompensateS[0].s = ISS_ON;
    }
    else
    {
        LOGF_ERROR("Unknown error: updateTemperatureSettings value (%s)", resp);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::updateCustomSettings()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char resp[STEELDRIVE_MAXBUF] = {0};

    char selectedFocuser[2], maxTrip[8], tResp[STEELDRIVE_MAXBUF];
    int gearR;
    double gearRatio;

    tcflush(PortFD, TCIOFLUSH);

    // Get Gear Ratio
    if (!sim && (rc = tty_write(PortFD, ":FEASKGR#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FEASKGR# updateCustomSettings write error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:FEASKGR#)");

    if (sim)
    {
        strncpy(resp, ":FE25040#", STEELDRIVE_CMD + 1);
        nbytes_read = STEELDRIVE_CMD;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FEASKGR# updateCustomSettings read error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":FE%d#", &gearR);

    if (rc > 0)
    {
        gearRatio = ((double)gearR) / 100000.0;
    }
    else
    {
        LOGF_ERROR("Unknown error: updateCustomSettings value (%s)", resp);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    // Get Max Trip
    if (!sim && (rc = tty_write(PortFD, ":F8ASKS1#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":F8ASKS1# updateCustomSettings write error: %s.", errstr);
        return false;
    }

    LOG_DEBUG("CMD (:F8ASKS1#)");

    if (sim)
    {
        strncpy(resp, ":F40011577#", STEELDRIVE_CMD_LONG + 1);
        nbytes_read = STEELDRIVE_CMD_LONG;
    }
    else if ((rc = tty_read_section(PortFD, resp, '#', STEELDRIVE_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":F8ASKS1# updateCustomSettings read error: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    LOGF_DEBUG("RES (%s)", resp);

    rc = sscanf(resp, ":F%s#", tResp);

    if (rc > 0)
    {
        strncpy(selectedFocuser, tResp, 1);
        strncpy(maxTrip, tResp + 1, 7);

        int sFocuser = atoi(selectedFocuser);

        IUResetSwitch(&ModelSP);
        ModelS[sFocuser].s = ISS_ON;

        fSettings[sFocuser].maxTrip   = (atof(maxTrip) * gearRatio) / 100.0;
        fSettings[sFocuser].gearRatio = gearRatio;

        CustomSettingN[FOCUS_MAX_TRIP].value   = fSettings[sFocuser].maxTrip;
        CustomSettingN[FOCUS_GEAR_RATIO].value = fSettings[sFocuser].gearRatio;

        LOGF_DEBUG("Updated max trip: %g gear ratio: %g", fSettings[sFocuser].maxTrip,
                   fSettings[sFocuser].gearRatio);
    }
    else
    {
        LOGF_ERROR("Unknown error: updateCustomSettings value (%s)", resp);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::setTemperatureSamples(unsigned int targetSamples, unsigned int *finalSample)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char cmd[STEELDRIVE_MAXBUF] = {0};

    int maxSample = TemperatureSettingN[FOCUS_T_SAMPLES].max;
    int sample    = 0;

    for (int i = maxSample; i > 0;)
    {
        if (targetSamples & maxSample)
        {
            sample = maxSample;
            break;
        }

        maxSample >>= 1;
    }

    int value = 0;
    if (sample == 16)
        value = 5000;
    else if (sample == 32)
        value = 15000;
    else if (sample == 64)
        value = 25000;
    else
        value = 35000;

    snprintf(cmd, STEELDRIVE_CMD + 1, ":FI%05d#", value);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s setTemperatureSamples write error: %s.", cmd, errstr);
        return false;
    }

    TemperatureSettingN[FOCUS_T_SAMPLES].value = sample;
    *finalSample                               = sample;

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::setTemperatureCompensation()
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char cmd[STEELDRIVE_MAXBUF] = {0};

    double coeff      = TemperatureSettingN[FOCUS_T_COEFF].value;
    bool enable       = TemperatureCompensateS[0].s == ISS_ON;
    int selectedFocus = IUFindOnSwitchIndex(&ModelSP);

    snprintf(cmd, STEELDRIVE_CMD + 1, ":F%02d%03d%d#", selectedFocus, (int)(coeff * 1000), enable ? 2 : 0);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setTemperatureCoefficient error: %s.", errstr);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::setCustomSettings(double maxTrip, double gearRatio)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char cmd[STEELDRIVE_MAXBUF] = {0};

    unsigned short mmTrip = (unsigned short int)(maxTrip / gearRatio * 100.0);

    snprintf(cmd, STEELDRIVE_CMD_LONG + 1, ":FC%07d#", mmTrip);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD_LONG, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setCustomSettings error: %s.", errstr);
        return false;
    }

    snprintf(cmd, STEELDRIVE_CMD + 1, ":FD%05d#", (int)(gearRatio * 100000));

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setCustomSettings error: %s.", errstr);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::Sync(unsigned int position)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char cmd[STEELDRIVE_MAXBUF] = {0};

    snprintf(cmd, STEELDRIVE_CMD_LONG + 1, ":FB%07d#", position);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD_LONG, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Sync error: %s.", errstr);
        return false;
    }

    simPosition = position;

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::moveFocuser(unsigned int position)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char cmd[STEELDRIVE_MAXBUF] = {0};

    if (position < FocusAbsPosNP[0].getMin() || position > FocusAbsPosNP[0].getMax())
    {
        LOGF_ERROR("Requested position value out of bound: %d", position);
        return false;
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY)
        AbortFocuser();

    snprintf(cmd, STEELDRIVE_CMD_LONG + 1, ":F9%07d#", position);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    // Goto absolute step
    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD_LONG, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setPosition error: %s.", errstr);
        return false;
    }

    targetPos = position;

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::startMotion(FocusDirection dir)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char cmd[STEELDRIVE_MAXBUF] = {0};

    // inward  --> decreasing value --> DOWN
    // outward --> increasing value --> UP
    strncpy(cmd, (dir == FOCUS_INWARD) ? ":F2MDOW0#" : ":F1MUP00#", STEELDRIVE_CMD + 1);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("StartMotion error: %s.", errstr);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::setSpeed(unsigned short speed)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char cmd[STEELDRIVE_MAXBUF] = {0};

    snprintf(cmd, STEELDRIVE_CMD + 1, ":Fg%05d#", speed);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setSpeed error: %s.", errstr);
        return false;
    }

    currentSpeed = speed;

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::setAcceleration(unsigned short accel)
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};
    char cmd[STEELDRIVE_MAXBUF] = {0};

    snprintf(cmd, STEELDRIVE_CMD + 1, ":Fh%05d#", accel);

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%s)", cmd);

    if (!sim && (rc = tty_write(PortFD, cmd, STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setAcceleration error: %s.", errstr);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(TemperatureCompensateSP.name, name) == 0)
        {
            int last_index = IUFindOnSwitchIndex(&TemperatureCompensateSP);
            IUUpdateSwitch(&TemperatureCompensateSP, states, names, n);

            bool rc = setTemperatureCompensation();

            if (!rc)
            {
                TemperatureCompensateSP.s = IPS_ALERT;
                IUResetSwitch(&TemperatureCompensateSP);
                TemperatureCompensateS[last_index].s = ISS_ON;
                IDSetSwitch(&TemperatureCompensateSP, nullptr);
                return false;
            }

            TemperatureCompensateSP.s = IPS_OK;
            IDSetSwitch(&TemperatureCompensateSP, nullptr);
            return true;
        }

        if (strcmp(ModelSP.name, name) == 0)
        {
            IUUpdateSwitch(&ModelSP, states, names, n);

            int i = IUFindOnSwitchIndex(&ModelSP);

            double focusMaxPos  = floor(fSettings[i].maxTrip / fSettings[i].gearRatio) * 100;
            FocusAbsPosNP[0].setMax(focusMaxPos);
            FocusAbsPosNP.updateMinMax();

            CustomSettingN[FOCUS_MAX_TRIP].value   = fSettings[i].maxTrip;
            CustomSettingN[FOCUS_GEAR_RATIO].value = fSettings[i].gearRatio;

            IDSetNumber(&CustomSettingNP, nullptr);

            ModelSP.s = IPS_OK;
            IDSetSwitch(&ModelSP, nullptr);

            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Set Accelration
        if (strcmp(name, AccelerationNP.name) == 0)
        {
            if (setAcceleration((int)values[0]))
            {
                IUUpdateNumber(&AccelerationNP, values, names, n);
                AccelerationNP.s = IPS_OK;
                IDSetNumber(&AccelerationNP, nullptr);
                return true;
            }
            else
            {
                AccelerationNP.s = IPS_ALERT;
                IDSetNumber(&AccelerationNP, nullptr);
                return false;
            }
        }

        // Set Temperature Settings
        if (strcmp(name, TemperatureSettingNP.name) == 0)
        {
            // Coeff is only needed when we enable or disable the temperature compensation. Here we only set the # of samples
            unsigned int targetSamples;

            if (strcmp(names[0], TemperatureSettingN[FOCUS_T_SAMPLES].name) == 0)
                targetSamples = (int)values[0];
            else
                targetSamples = (int)values[1];

            unsigned int finalSample = targetSamples;

            if (setTemperatureSamples(targetSamples, &finalSample))
            {
                IUUpdateNumber(&TemperatureSettingNP, values, names, n);
                TemperatureSettingN[FOCUS_T_SAMPLES].value = finalSample;

                if (TemperatureSettingN[FOCUS_T_COEFF].value > TemperatureSettingN[FOCUS_T_COEFF].max)
                    TemperatureSettingN[FOCUS_T_COEFF].value = TemperatureSettingN[FOCUS_T_COEFF].max;

                TemperatureSettingNP.s = IPS_OK;
                IDSetNumber(&TemperatureSettingNP, nullptr);
                return true;
            }
            else
            {
                TemperatureSettingNP.s = IPS_ALERT;
                IDSetNumber(&TemperatureSettingNP, nullptr);
                return true;
            }
        }

        // Set Custom Settings
        if (strcmp(name, CustomSettingNP.name) == 0)
        {
            int i = IUFindOnSwitchIndex(&ModelSP);

            // If the model is NOT set to custom, then the values cannot be updated
            if (i != 4)
            {
                CustomSettingNP.s = IPS_IDLE;
                LOG_WARN("You can not set custom values for a non-custom focuser.");
                IDSetNumber(&CustomSettingNP, nullptr);
                return false;
            }

            double maxTrip, gearRatio;

            if (strcmp(names[0], CustomSettingN[FOCUS_MAX_TRIP].name) == 0)
            {
                maxTrip   = values[0];
                gearRatio = values[1];
            }
            else
            {
                maxTrip   = values[1];
                gearRatio = values[0];
            }

            if (setCustomSettings(maxTrip, gearRatio))
            {
                IUUpdateNumber(&CustomSettingNP, values, names, n);
                CustomSettingNP.s = IPS_OK;
                IDSetNumber(&CustomSettingNP, nullptr);

                updateFocusMaxRange(maxTrip, gearRatio);
                FocusAbsPosNP.updateMinMax();
                FocusRelPosNP.updateMinMax();
            }
            else
            {
                CustomSettingNP.s = IPS_ALERT;
                IDSetNumber(&CustomSettingNP, nullptr);
            }
        }

        // Set Sync Position
        if (strcmp(name, SyncNP.name) == 0)
        {
            if (Sync((unsigned int)values[0]))
            {
                IUUpdateNumber(&SyncNP, values, names, n);
                SyncNP.s = IPS_OK;
                IDSetNumber(&SyncNP, nullptr);

                if (updatePosition())
                    FocusAbsPosNP.apply();

                return true;
            }
            else
            {
                SyncNP.s = IPS_ALERT;
                IDSetNumber(&SyncNP, nullptr);

                return false;
            }
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/************************************************************************************
 *
* ***********************************************************************************/
void SteelDrive::GetFocusParams()
{
    if (updateVersion())
        IDSetText(&VersionTP, nullptr);

    if (updateTemperature())
        IDSetNumber(&TemperatureNP, nullptr);

    if (updateTemperatureSettings())
        IDSetNumber(&TemperatureSettingNP, nullptr);

    if (updatePosition())
        FocusAbsPosNP.apply();

    if (updateSpeed())
        FocusSpeedNP.apply();

    if (updateAcceleration())
        IDSetNumber(&AccelerationNP, nullptr);

    if (updateCustomSettings())
    {
        IDSetNumber(&CustomSettingNP, nullptr);
        IDSetSwitch(&ModelSP, nullptr);
    }
}

bool SteelDrive::SetFocuserSpeed(int speed)
{
    bool rc = false;

    rc = setSpeed(speed);

    if (!rc)
        return false;

    currentSpeed = speed;

    FocusSpeedNP.setState(IPS_OK);
    FocusSpeedNP.apply();

    return true;
}

IPState SteelDrive::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    if (speed != (int)currentSpeed)
    {
        bool rc = setSpeed(speed);

        if (!rc)
            return IPS_ALERT;
    }

    gettimeofday(&focusMoveStart, nullptr);
    focusMoveRequest = duration / 1000.0;

    startMotion(dir);

    if (duration <= getCurrentPollingPeriod())
    {
        usleep(getCurrentPollingPeriod() * 1000);
        AbortFocuser();
        return IPS_OK;
    }

    return IPS_BUSY;
}

IPState SteelDrive::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    bool rc = false;

    rc = moveFocuser(targetPos);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

IPState SteelDrive::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc            = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    rc = moveFocuser(newPosition);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);
    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

void SteelDrive::TimerHit()
{
    if (!isConnected())
        return;

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > STEELDIVE_POSITION_THRESHOLD)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }

    if (temperatureUpdateCounter++ > STEELDRIVE_TEMPERATURE_FREQ)
    {
        temperatureUpdateCounter = 0;
        rc                       = updateTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.5)
                lastTemperature = TemperatureN[0].value;
        }

        IDSetNumber(&TemperatureNP, nullptr);
    }

    if (FocusTimerNP.getState() == IPS_BUSY)
    {
        float remaining = CalcTimeLeft(focusMoveStart, focusMoveRequest);

        if (sim)
        {
            if (FocusMotionSP[FOCUS_INWARD].getState() == ISS_ON)
            {
                FocusAbsPosNP[0].setValue(FocusAbsPosNP[0].getValue() - FocusSpeedNP[0].getValue());

                if (FocusAbsPosNP[0].getValue() <= 0)
                    FocusAbsPosNP[0].setValue(0);
                simPosition = FocusAbsPosNP[0].getValue();
            }
            else
            {
                FocusAbsPosNP[0].setValue(FocusAbsPosNP[0].getValue() + FocusSpeedNP[0].getValue());
                if (FocusAbsPosNP[0].getValue() >= FocusAbsPosNP[0].getMax())
                    FocusAbsPosNP[0].setValue(FocusAbsPosNP[0].getMax());
                simPosition = FocusAbsPosNP[0].getValue();
            }
        }

        // If we exceed focus abs values, stop timer and motion
        if (FocusAbsPosNP[0].getValue() <= 0 || FocusAbsPosNP[0].getValue() >= FocusAbsPosNP[0].getMax())
        {
            AbortFocuser();

            if (FocusAbsPosNP[0].getValue() <= 0)
                FocusAbsPosNP[0].setValue(0);
            else
                FocusAbsPosNP[0].setValue(FocusAbsPosNP[0].getMax());

            FocusTimerNP[0].setValue(0);
            FocusTimerNP.setState(IPS_IDLE);
        }
        else if (remaining <= 0)
        {
            FocusTimerNP.setState(IPS_OK);
            FocusTimerNP[0].setValue(0);
            AbortFocuser();
        }
        else
            FocusTimerNP[0].setValue(remaining * 1000.0);

        FocusTimerNP.apply();
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY || FocusRelPosNP.getState() == IPS_BUSY)
    {
        if (sim)
        {
            if (FocusAbsPosNP[0].getValue() < targetPos)
                simPosition += 100;
            else
                simPosition -= 100;

            if (fabs(simPosition - targetPos) < 100)
            {
                FocusAbsPosNP[0].setValue(targetPos);
                simPosition           = FocusAbsPosNP[0].getValue();
            }
        }

        // Set it OK to within 5 steps
        if (fabs(targetPos - FocusAbsPosNP[0].getValue()) < 5)
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusRelPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            FocusRelPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::AbortFocuser()
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};

    tcflush(PortFD, TCIOFLUSH);

    LOG_DEBUG("CMD :F3STOP0#");

    if (!sim && (rc = tty_write(PortFD, ":F3STOP0#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":F3STOP0# Stop error: %s.", errstr);
        return false;
    }

    if (FocusRelPosNP.getState() == IPS_BUSY)
    {
        FocusRelPosNP.setState(IPS_IDLE);
        FocusRelPosNP.apply();
    }

    FocusTimerNP.setState(IPS_IDLE);
    FocusAbsPosNP.setState(IPS_IDLE);
    FocusTimerNP.apply();
    FocusAbsPosNP.apply();

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
float SteelDrive::CalcTimeLeft(timeval start, float req)
{
    double timesince;
    double timeleft;
    struct timeval now
    {
        0, 0
    };
    gettimeofday(&now, nullptr);

    timesince =
        (double)(now.tv_sec * 1000.0 + now.tv_usec / 1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec / 1000);
    timesince = timesince / 1000;
    timeleft  = req - timesince;
    return timeleft;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &TemperatureSettingNP);
    IUSaveConfigSwitch(fp, &TemperatureCompensateSP);
    FocusSpeedNP.save(fp);
    IUSaveConfigNumber(fp, &AccelerationNP);
    IUSaveConfigNumber(fp, &CustomSettingNP);
    IUSaveConfigSwitch(fp, &ModelSP);

    return saveFocuserConfig();
}

/************************************************************************************
 *
* ***********************************************************************************/
bool SteelDrive::saveFocuserConfig()
{
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF] = {0};

    tcflush(PortFD, TCIOFLUSH);

    LOG_DEBUG("CMD (:FFPOWER#)");

    if (!sim && (rc = tty_write(PortFD, ":FFPOWER#", STEELDRIVE_CMD, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR(":FFPOWER# saveFocuserConfig error: %s.", errstr);
        return false;
    }

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
void SteelDrive::debugTriggered(bool enable)
{
    tty_set_debug(enable ? 1 : 0);
}

/************************************************************************************
 *
* ***********************************************************************************/
void SteelDrive::updateFocusMaxRange(double maxTrip, double gearRatio)
{
    double maxSteps = floor(maxTrip / gearRatio * 100.0);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0);
    FocusRelPosNP[0].setMax(floor(maxSteps / 2.0));
    FocusRelPosNP[0].setStep(100);

    FocusAbsPosNP[0].setMin(0);
    FocusAbsPosNP[0].setMax(maxSteps);
    FocusAbsPosNP[0].setStep(1000);
}
