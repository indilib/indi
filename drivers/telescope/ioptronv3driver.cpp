/*
    INDI IOptron v3 Driver for firmware version 20171001 or later.

    Copyright (C) 2018 Jasem Mutlaq

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

#include "ioptronv3driver.h"
#include "indicom.h"

#include <libnova/julian_day.h>
#include <cinttypes>

#include <cmath>
#include <cstring>
#include <termios.h>
#include <unistd.h>

namespace IOPv3
{

const std::map<std::string, std::string> Driver::models =
{
    {"0010", "Cube II EQ"},
    {"0011", "SmartEQ Pro+"},
    {"0025", "CEM25"},
    {"0026", "CEM25-EC"},
    {"0030", "iEQ30 Pro"},
    {"0040", "CEM40"},
    {"0041", "CEM40-EC"},
    {"0045", "iEQ45 Pro EQ"},
    {"0046", "iEQ45 Pro AA"},
    {"0060", "CEM60"},
    {"0061", "CEM60-EC"},
    {"0120", "CEM120"},
    {"0121", "CEM120-EC"},
    {"0122", "CEM120-EC2"},
    {"5010", "Cube II AA"},
    {"5035", "AZ Mount Pro"},
    {"5045", "iEQ45 Pro AA"}
};

const uint16_t Driver::IOP_SLEW_RATES[] = {1, 2, 8, 16, 64, 128, 256, 512, 1024};

Driver::Driver(const char *deviceName): m_DeviceName(deviceName) {}

bool Driver::sendCommand(const char *command, int count, char *response, uint8_t timeout, uint8_t debugLog)
{
    int errCode = 0;
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char errMsg[MAXRBUF];
    char res[IOP_BUFFER] = {0};

    DEBUGFDEVICE(m_DeviceName, debugLog, "CMD <%s>", command);

    if (m_Simulation)
        return true;

    tcflush(PortFD, TCIOFLUSH);

    if ((errCode = tty_write(PortFD, command, strlen(command), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errCode, errMsg, MAXRBUF);
        DEBUGFDEVICE(m_DeviceName, INDI::Logger::DBG_ERROR, "Write Command Error: %s", errMsg);
        return false;
    }

    if (count == 0)
        return true;

    if (count == -1)
        errCode = tty_read_section(PortFD, res, '#', timeout, &nbytes_read);
    else
        errCode = tty_read(PortFD, res, count, timeout, &nbytes_read);

    if (errCode != TTY_OK)
    {
        tty_error_msg(errCode, errMsg, MAXRBUF);
        DEBUGFDEVICE(m_DeviceName, INDI::Logger::DBG_ERROR, "Read Command Error: %s", errMsg);
        return false;
    }

    // Remove the extra #
    if (count == -1)
        res[nbytes_read - 1] = 0;

    DEBUGFDEVICE(m_DeviceName, debugLog, "RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    // Copy response to buffer
    if (response)
        strncpy(response, res, IOP_BUFFER);

    if (count == -1 || (count == 1 && res[0] == '1') || count == nbytes_read)
        return true;

    return false;
}

bool Driver::checkConnection(int fd)
{
    char res[IOP_BUFFER] = {0};

    DEBUGDEVICE(m_DeviceName, INDI::Logger::DBG_DEBUG, "Initializing IOptron using :MountInfo# CMD...");

    // Set FD for use
    PortFD = fd;

    if (m_Simulation)
        return true;

    for (int i = 0; i < 2; i++)
    {
        if (sendCommand(":MountInfo#", 4, res, 3) == false)
        {
            usleep(50000);
            continue;
        }

        return true;
    }

    return false;
}

void Driver::setDebug(bool enable)
{
    m_Debug = enable;
}

void Driver::setSimulation(bool enable)
{
    m_Simulation = enable;

    simData.ra_guide_rate = 0.5;
    simData.de_guide_rate = 0.5;
    simData.pier_state = IOP_PIER_WEST;
    simData.cw_state = IOP_CW_NORMAL;
    simData.JD = ln_get_julian_from_sys();
    simData.utc_offset_minutes = 3 * 60;
    simData.day_light_saving = false;

    simData.simInfo.gpsStatus = GPS_DATA_OK;
    simData.simInfo.hemisphere = HEMI_NORTH;
    simData.simInfo.slewRate = SR_6;
    simData.simInfo.timeSource = TS_GPS;
    simData.simInfo.trackRate = TR_SIDEREAL;
    simData.simInfo.longitude = 48.1;
    simData.simInfo.latitude  = 29.5;
}

void Driver::setSimGPSstatus(IOP_GPS_STATUS value)
{
    simData.simInfo.gpsStatus = value;
}

void Driver::setSimSytemStatus(IOP_SYSTEM_STATUS value)
{
    simData.simInfo.systemStatus = value;
}

void Driver::setSimTrackRate(IOP_TRACK_RATE value)
{
    simData.simInfo.trackRate = value;
}

void Driver::setSimSlewRate(IOP_SLEW_RATE value)
{
    simData.simInfo.slewRate = value;
}

void Driver::setSimTimeSource(IOP_TIME_SOURCE value)
{
    simData.simInfo.timeSource = value;
}

void Driver::setSimHemisphere(IOP_HEMISPHERE value)
{
    simData.simInfo.hemisphere = value;
}

void Driver::setSimRA(double ra)
{
    simData.ra = ra;
}

void Driver::setSimDE(double de)
{
    simData.de = de;
}

void Driver::setSimGuideRate(double raRate, double deRate)
{
    simData.ra_guide_rate = raRate;
    simData.de_guide_rate = deRate;
}

void Driver::setSimLongLat(double longitude, double latitude)
{
    simData.simInfo.longitude = longitude;
    simData.simInfo.latitude  = latitude;
}

bool Driver::getStatus(IOPInfo *info)
{
    char res[IOP_BUFFER] = {0};

    if (m_Simulation)
    {
        int iopLongitude = simData.simInfo.longitude * 360000;
        int iopLatitude  = (simData.simInfo.latitude + 90) * 360000;
        snprintf(res, IOP_BUFFER, "%c%08d%08d%d%d%d%d%d%d", simData.simInfo.longitude > 0 ? '+' : '-',
                 iopLongitude, iopLatitude, simData.simInfo.gpsStatus, simData.simInfo.systemStatus, simData.simInfo.trackRate,
                 simData.simInfo.slewRate, simData.simInfo.timeSource, simData.simInfo.hemisphere);
    }
    else if (sendCommand(":GLS#", -1, res) == false)
        return false;


    char longPart[16] = {0}, latPart[16] = {0};
    strncpy(longPart, res, 9);
    strncpy(latPart, res + 9, 8);

    int arcsecLongitude = atoi(longPart);
    int arcsecLatitude  = atoi(latPart);

    info->longitude    = arcsecLongitude / 360000.0;
    info->latitude     = arcsecLatitude / 360000.0 - 90.0;
    info->gpsStatus    = (IOP_GPS_STATUS)(res[17] - '0');
    info->systemStatus = (IOP_SYSTEM_STATUS)(res[18] - '0');
    info->trackRate    = (IOP_TRACK_RATE)(res[19] - '0');
    info->slewRate     = (IOP_SLEW_RATE)(res[20] - '0');
    info->timeSource   = (IOP_TIME_SOURCE)(res[21] - '0');
    info->hemisphere   = (IOP_HEMISPHERE)(res[22] - '0');

    return true;
}

bool Driver::getFirmwareInfo(FirmwareInfo *info)
{
    bool rc1 = getModel(info->Model);

    bool rc2 = getMainFirmware(info->MainBoardFirmware, info->ControllerFirmware);

    bool rc3 = getRADEFirmware(info->RAFirmware, info->DEFirmware);

    return (rc1 && rc2 && rc3);
}

bool Driver::getModel(std::string &model)
{
    char res[IOP_BUFFER] = {0};

    if (m_Simulation)
        strcpy(res, "0120");
    else if (sendCommand(":MountInfo#", 4, res) == false)
        return false;

    if (models.find(res) != models.end())
        model = models.at(res);
    else
        model = "Unknown";

    return true;
}

bool Driver::getMainFirmware(std::string &mainFirmware, std::string &controllerFirmware)
{
    char res[IOP_BUFFER] = {0};

    if (m_Simulation)
        strcpy(res, "180321171001");
    else if (sendCommand(":FW1#", -1, res) == false)
        return false;

    char mStr[16] = {0}, cStr[16] = {0};
    strncpy(mStr, res, 6);
    strncpy(cStr, res + 6, 6);

    mainFirmware = mStr;
    controllerFirmware = cStr;

    return true;
}

bool Driver::getRADEFirmware(std::string &RAFirmware, std::string &DEFirmware)
{
    char res[IOP_BUFFER] = {0};

    if (m_Simulation)
        strcpy(res, "140324140101");
    else if (sendCommand(":FW2#", -1, res) == false)
        return false;

    char mStr[16] = {0}, cStr[16] = {0};
    strncpy(mStr, res, 6);
    strncpy(cStr, res + 6, 6);

    RAFirmware = mStr;
    DEFirmware = cStr;

    return true;
}

bool Driver::startMotion(IOP_DIRECTION dir)
{
    switch (dir)
    {
        case IOP_N:
            return sendCommand(":mn#", 0);
            break;
        case IOP_S:
            return sendCommand(":ms#", 0);
            break;
        case IOP_W:
            return sendCommand(":mw#", 0);
            break;
        case IOP_E:
            return sendCommand(":me#", 0);
            break;
    }

    return false;
}

bool Driver::stopMotion(IOP_DIRECTION dir)
{
    switch (dir)
    {
        case IOP_N:
        case IOP_S:
            return sendCommand(":qD#");
            break;

        case IOP_W:
        case IOP_E:
            return sendCommand(":qR#");
            break;
    }

    return false;
}

bool Driver::findHome()
{
    return sendCommand(":MSH#");
}

bool Driver::gotoHome()
{
    return sendCommand(":MH#");
}

bool Driver::setCurrentHome()
{
    return sendCommand(":SZP#");
}

bool Driver::setSlewRate(IOP_SLEW_RATE rate)
{
    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":SR%d#", ((int)rate) + 1);

    simData.simInfo.slewRate = rate;

    return sendCommand(cmd);
}

bool Driver::setTrackMode(IOP_TRACK_RATE rate)
{
    simData.simInfo.trackRate = rate;

    switch (rate)
    {
        case TR_SIDEREAL:
            return sendCommand(":RT0#");
            break;
        case TR_LUNAR:
            return sendCommand(":RT1#");
            break;
        case TR_SOLAR:
            return sendCommand(":RT2#");
            break;
        case TR_KING:
            return sendCommand(":RT3#");
            break;
        case TR_CUSTOM:
            return sendCommand(":RT4#");
            break;
    }

    return false;
}

bool Driver::setCustomRATrackRate(double rate)
{
    if (rate < 0.1 || rate > 1.9)
        return false;

    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":RR%05d#", static_cast<uint32_t>(rate * 10000));

    return sendCommand(cmd);
}

bool Driver::setGuideRate(double RARate, double DERate)
{
    if (RARate < 0.01 || RARate > 0.9 || DERate < 0.01 || DERate > 0.9)
        return false;

    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":RG%02d%02d#", static_cast<uint32_t>(RARate * 100), static_cast<uint32_t>(DERate * 100));

    return sendCommand(cmd);
}

bool Driver::getGuideRate(double *RARate, double *DERate)
{
    char res[IOP_BUFFER] = {0};

    if (m_Simulation)
        snprintf(res, IOP_BUFFER, "%02d%02d", static_cast<uint32_t>(simData.ra_guide_rate * 100), static_cast<uint32_t>(simData.de_guide_rate * 100));
    else if (sendCommand(":AG#", -1, res) == false)
        return false;

    char raStr[8] = {0}, deStr[8] = {0};
    strncpy(raStr, res, 2);
    strncpy(deStr, res + 2, 2);

    *RARate = atoi(raStr) / 100.0;
    *DERate = atoi(deStr) / 100.0;

    return true;
}

bool Driver::startGuide(IOP_DIRECTION dir, uint32_t ms)
{
    char cmd[IOP_BUFFER] = {0};
    char dir_c = 0;

    switch (dir)
    {
        case IOP_N:
            dir_c = 'n';
            break;

        case IOP_S:
            dir_c = 's';
            break;

        case IOP_W:
            dir_c = 'w';
            break;

        case IOP_E:
            dir_c = 'e';
            break;
    }

    snprintf(cmd, IOP_BUFFER, ":M%c%05d#", dir_c, ms);

    return sendCommand(cmd, 0);
}

bool Driver::park()
{
    return sendCommand(":MP1#");
}

bool Driver::unpark()
{
    //NB: This command only available in CEM120 series, CEM60 series, iEQ45 Pro, iEQ45 Pro
    //AA and iEQ30 Pro.
    setSimSytemStatus(ST_STOPPED);
    return sendCommand(":MP0#");
}

bool Driver::abort()
{
    if (simData.simInfo.systemStatus == ST_SLEWING)
        simData.simInfo.systemStatus =  simData.simInfo.rememberSystemStatus;

    return sendCommand(":Q#");
}

bool Driver::slewNormal()
{
    simData.simInfo.rememberSystemStatus = simData.simInfo.systemStatus;
    simData.simInfo.systemStatus = ST_SLEWING;

    return sendCommand(":MS1#");
}

bool Driver::slewCWUp()
{
    simData.simInfo.rememberSystemStatus = simData.simInfo.systemStatus;
    simData.simInfo.systemStatus = ST_SLEWING;

    return sendCommand(":MS2#");
}

bool Driver::sync()
{
    return sendCommand(":CM#");
}

bool Driver::setTrackEnabled(bool enabled)
{
    simData.simInfo.systemStatus = enabled ? ST_TRACKING_PEC_ON : ST_STOPPED;
    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":ST%d#", enabled ? 1 : 0);

    return sendCommand(cmd);
}

bool Driver::setRA(double ra)
{
    // Send RA in centi-arcsecond (0.01) resolution.
    // ra is passed as hours. casRA is in centi-arcseconds in degrees.
    uint32_t casRA = ra * 15 * 60 * 60 * 100;

    simData.ra = ra;

    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":SRA%09d#", casRA);

    return sendCommand(cmd);
}

bool Driver::setDE(double de)
{
    // Send DE in centi-arcsecond (0.01) resolution.
    // de is passed as degrees. casDE is in centi-arcseconds in degrees.
    uint32_t casDE = fabs(de) * 60 * 60 * 100;

    simData.de = de;

    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":Sd%c%08d#", de >= 0 ? '+' : '-', casDE);

    return sendCommand(cmd);
}

bool Driver::setLongitude(double longitude)
{
    uint32_t casLongitude = fabs(longitude) * 60 * 60 * 100;

    simData.simInfo.longitude = longitude;

    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":SLO%c%08d#", longitude >= 0 ? '+' : '-', casLongitude);

    return sendCommand(cmd);
}

bool Driver::setLatitude(double latitude)
{
    uint32_t casLatitude = fabs(latitude) * 60 * 60 * 100;

    simData.simInfo.latitude = latitude;

    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":SLA%c%08d#", latitude >= 0 ? '+' : '-', casLatitude);

    return sendCommand(cmd);
}

bool Driver::setUTCDateTime(double JD)
{
    uint64_t msJD = (JD - J2000) * 8.64e+7;

    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":SUT%013" PRIu64 "#", msJD);

    simData.JD = JD;

    return sendCommand(cmd);
}

bool Driver::setUTCOffset(int offsetMinutes)
{
    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":SG%c%03d#", offsetMinutes >= 0 ? '+' : '-', abs(offsetMinutes));

    simData.utc_offset_minutes = offsetMinutes;

    return sendCommand(cmd);
}

bool Driver::setDaylightSaving(bool enabled)
{
    char cmd[IOP_BUFFER] = {0};
    snprintf(cmd, IOP_BUFFER, ":SDS%c#", enabled ? '1' : '0');

    simData.day_light_saving = enabled;

    return sendCommand(cmd);
}

bool Driver::getCoords(double *ra, double *de, IOP_PIER_STATE *pierState, IOP_CW_STATE *cwState)
{
    char res[IOP_BUFFER] = {0};
    if (m_Simulation)
    {
        snprintf(res, IOP_BUFFER, "%c%08d%09d%d%d", (simData.de >= 0 ? '+' : '-'), static_cast<uint32_t>(fabs(simData.de) * 60 * 60 * 100),
                 static_cast<uint32_t>(simData.ra * 15 * 60 * 60 * 100), simData.pier_state, simData.cw_state);
    }
    else if (sendCommand(":GEP#", -1, res, IOP_TIMEOUT, INDI::Logger::DBG_EXTRA_1) == false)
        return false;

    char deStr[16] = {0}, raStr[16] = {0};

    strncpy(deStr, res, 9);
    strncpy(raStr, res + 9, 9);

    *de = atoi(deStr) / (60.0 * 60.0 * 100.0);
    *ra = atoi(raStr) / (15.0 * 60.0 * 60.0 * 100.0);

    *pierState = static_cast<IOP_PIER_STATE>(res[18] - '0');
    *cwState   = static_cast<IOP_CW_STATE>(res[19] - '0');

    return true;
}

bool Driver::getUTCDateTime(double *JD, int *utcOffsetMinutes, bool *dayLightSaving)
{
    char res[IOP_BUFFER] = {0};
    if (m_Simulation)
    {
        snprintf(res, IOP_BUFFER, "%c%03d%c%013" PRIu64, (simData.utc_offset_minutes >= 0 ? '+' : '-'), abs(simData.utc_offset_minutes),
                 (simData.day_light_saving ? '1' : '0'), static_cast<uint64_t>((simData.JD - J2000) * 8.64e+7));
    }
    else if (sendCommand(":GUT#", -1, res) == false)
        return false;

    char offsetStr[16] = {0}, JDStr[16] = {0};

    strncpy(offsetStr, res, 4);
    strncpy(JDStr, res + 5, 13);

    *utcOffsetMinutes = atoi(offsetStr);
    *dayLightSaving   = (res[4] == '1');

    uint64_t iopJD = std::stoull(JDStr);
    *JD = (iopJD / 8.64e+7) + J2000;

    return true;
}

}
