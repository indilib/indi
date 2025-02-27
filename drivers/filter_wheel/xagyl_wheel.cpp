/*******************************************************************************
  Copyright(c) 2020 Jasem Mutlaq. All rights reserved.
  Copyright(c) 2020 Justin Husted.

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

#include "xagyl_wheel.h"
#include "indicom.h"

#include <cstring>
#include <cassert>
#include <memory>
#include <regex>
#include <termios.h>

static std::unique_ptr<XAGYLWheel> xagylWheel(new XAGYLWheel());

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
XAGYLWheel::XAGYLWheel()
{
    setVersion(0, 3);

    setFilterConnection(CONNECTION_SERIAL | CONNECTION_TCP);

    setDefaultPollingPeriod(500);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
XAGYLWheel::~XAGYLWheel()
{
    delete[] OffsetN;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *XAGYLWheel::getDefaultName()
{
    return "XAGYL Wheel";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Firmware info
    IUFillText(&FirmwareInfoT[FIRMWARE_PRODUCT], "FIRMWARE_PRODUCT", "Product",
               nullptr);
    IUFillText(&FirmwareInfoT[FIRMWARE_VERSION], "FIRMWARE_VERSION", "Version",
               nullptr);
    IUFillText(&FirmwareInfoT[FIRMWARE_SERIAL], "FIRMWARE_SERIAL", "Serial #",
               nullptr);
    IUFillTextVector(&FirmwareInfoTP, FirmwareInfoT, 3, getDeviceName(),
                     "Info", "Info", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Settings
    IUFillNumber(&SettingsN[SETTING_SPEED], "SETTING_SPEED", "Speed", "%.f",
                 0, 100, 10., 0.);
    IUFillNumber(&SettingsN[SETTING_JITTER], "SETTING_JITTER", "Jitter", "%.f",
                 1, 10, 1., 0.);
    IUFillNumber(&SettingsN[SETTING_THRESHOLD], "SETTING_THRESHOLD",
                 "Threshold", "%.f", 10, 30, 1., 0.);
    IUFillNumber(&SettingsN[SETTING_PW], "SETTING_PW", "Pulse", "%.f",
                 100, 10000, 100., 0.);
    IUFillNumberVector(&SettingsNP, SettingsN, 4, getDeviceName(), "Settings",
                       "Settings", SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Reset
    IUFillSwitch(&ResetS[COMMAND_REBOOT], "COMMAND_REBOOT", "Reboot", ISS_OFF);
    IUFillSwitch(&ResetS[COMMAND_INIT], "COMMAND_INIT", "Initialize", ISS_OFF);
    IUFillSwitch(&ResetS[COMMAND_CLEAR_CALIBRATION],
                 "COMMAND_CLEAR_CALIBRATION Calibration", "Clear Calibration",
                 ISS_OFF);
    IUFillSwitch(&ResetS[COMMAND_PERFORM_CALIBRAITON],
                 "COMMAND_PERFORM_CALIBRAITON", "Perform Calibration", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 4, getDeviceName(),
                       "Commands", "Commands", MAIN_CONTROL_TAB, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    addAuxControls();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        getStartupData();

        defineProperty(&ResetSP);
        defineProperty(&OffsetNP);
        defineProperty(&FirmwareInfoTP);
        defineProperty(&SettingsNP);
    }
    else
    {
        deleteProperty(ResetSP.name);
        deleteProperty(OffsetNP.name);
        deleteProperty(FirmwareInfoTP.name);
        deleteProperty(SettingsNP.name);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::Handshake()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    tcflush(PortFD, TCIOFLUSH);

    snprintf(cmd, DRIVER_LEN, "I%d", INFO_FIRMWARE_VERSION);
    if (!sendCommand(cmd, res))
        return false;

    int firmware_version = 0;
    int fw_rc = sscanf(res, "%d", &firmware_version);

    if (fw_rc != 1)
        fw_rc = sscanf(res, "FW %d", &firmware_version);

    if (fw_rc <= 0)
    {
        LOGF_ERROR("Unable to parse response <%s>", res);
        return false;
    }

    m_FirmwareVersion = firmware_version;

    // We don't have pulse width for version < 3
    SettingsNP.nnp = 4;
    if (m_FirmwareVersion < 3)
        SettingsNP.nnp = 3;

    bool rc = getMaxFilterSlots();
    if (!rc)
    {
        LOG_ERROR("Unable to parse max filter slots.");
        return false;
    }

    initOffset();

    rc = getFilterPosition();
    if (!rc)
    {
        LOG_ERROR("Unable to initialize filter position.");
        return false;
    }

    LOG_INFO("XAGYL is online. Getting filter parameters...");

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::ISNewSwitch(const char *dev, const char *name, ISState *states,
                             char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(ResetSP.name, name) == 0)
        {
            IUUpdateSwitch(&ResetSP, states, names, n);
            int command = IUFindOnSwitchIndex(&ResetSP);
            IUResetSwitch(&ResetSP);

            switch (command)
            {
                case COMMAND_REBOOT:
                    LOG_INFO("Executing hard reboot...");
                    break;

                case COMMAND_INIT:
                    LOG_INFO("Restarting and moving to filter position #1...");
                    break;

                case COMMAND_CLEAR_CALIBRATION:
                    LOG_INFO("Calibration removed.");
                    break;

                case COMMAND_PERFORM_CALIBRAITON:
                    LOG_INFO("Calibrating...");
                    break;
            }

            bool rc = reset(command);
            if (rc)
                LOG_INFO("Done.");
            else
                LOG_ERROR("Error. Please reset device.");

            ResetSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&ResetSP, nullptr);

            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::ISNewNumber(const char *dev, const char *name, double values[],
                             char *names[], int n)
{
    if (!(dev != nullptr && strcmp(dev, getDeviceName()) == 0))
        return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);

    // Handle Offsets
    if (strcmp(OffsetNP.name, name) == 0)
    {
        // Setting offsets changes the current filter.
        int origFiler = CurrentFilter;
        bool rc_offset = true;

        for (int i = 0; i < n; i++)
        {
            if (0 != strcmp(names[i], OffsetN[i].name))
                continue;

            int newOffset = values[i];
            int curOffset = OffsetN[i].value;

            if (newOffset != curOffset)
                rc_offset &= setOffset(i + 1, newOffset - curOffset);
        }

        OffsetNP.s = rc_offset ? IPS_OK : IPS_ALERT;
        IDSetNumber(&OffsetNP, nullptr);

        // Return filter to original position.
        if (!SelectFilter(origFiler))
            return false;

        return true;
    }

    // Handle Speed, Jitter, Threshold, Pulse width
    if (strcmp(SettingsNP.name, name) == 0)
    {
        int newSpeed = -1, newJitter = -1, newThreshold = -1,
            newPulseWidth = -1;

        for (int i = 0; i < n; i++)
        {
            if (!strcmp(names[i], SettingsN[SET_SPEED].name))
                newSpeed = values[i];
            if (!strcmp(names[i], SettingsN[SET_JITTER].name))
                newJitter = values[i];
            if (!strcmp(names[i], SettingsN[SET_THRESHOLD].name))
                newThreshold = values[i];
            if (!strcmp(names[i], SettingsN[SET_PULSE_WITDH].name))
                newPulseWidth = values[i];
        }

        bool rc_speed = true, rc_jitter = true, rc_threshold = true,
             rc_pulsewidth = true;

        // Speed
        if (newSpeed >= 0)
        {
            rc_speed = setMaximumSpeed(newSpeed);
            getMaximumSpeed();
        }

        // Jitter
        if (newJitter >= 0)
        {
            int curJitter = SettingsN[SET_JITTER].value;
            rc_jitter &= setRelativeCommand(SET_JITTER, newJitter - curJitter);
            getJitter();
        }

        // Threshold
        if (newThreshold >= 0)
        {
            int curThreshold = SettingsN[SET_THRESHOLD].value;
            rc_threshold &= setRelativeCommand(SET_THRESHOLD,
                                               newThreshold - curThreshold);
            getThreshold();
        }

        // Pulse width
        if (m_FirmwareVersion >= 3 && newPulseWidth >= 0)
        {
            int curPulseWidth = SettingsN[SET_PULSE_WITDH].value;
            rc_pulsewidth &= setRelativeCommand(SET_PULSE_WITDH, (newPulseWidth - curPulseWidth) / 100.0);
            getPulseWidth();
        }

        if (rc_speed && rc_jitter && rc_threshold && rc_pulsewidth)
            SettingsNP.s = IPS_OK;
        else
            SettingsNP.s = IPS_ALERT;

        IDSetNumber(&SettingsNP, nullptr);

        return true;
    }

    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void XAGYLWheel::initOffset()
{
    delete [] OffsetN;
    OffsetN = new INumber[static_cast<uint8_t>(FilterSlotNP[0].getMax())];
    char offsetName[MAXINDINAME], offsetLabel[MAXINDILABEL];
    for (int i = 0; i < FilterSlotNP[0].getMax(); i++)
    {
        snprintf(offsetName, MAXINDINAME, "OFFSET_%d", i + 1);
        snprintf(offsetLabel, MAXINDINAME, "#%d Offset", i + 1);
        IUFillNumber(OffsetN + i, offsetName, offsetLabel, "%.f", -9, 9, 1, 0);
    }

    IUFillNumberVector(&OffsetNP, OffsetN, FilterSlotNP[0].getMax(), getDeviceName(),
                       "Offsets", "", FILTER_TAB, IP_RW, 0, IPS_IDLE);
}

/////////////////////////////////////////////////////////////////////////////
/// Speed is encoded as S# where number is a hex value from 0 to 10, as a
/// percentage in 10% increments.
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::setMaximumSpeed(int value)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "S%X", value / 10);
    return sendCommand(cmd, res);
}

/////////////////////////////////////////////////////////////////////////////
/// These commands are implemented by sending the relevant set command multiple
/// times.  Each command shifts the value by 1 in the given direction.
///
/// TODO: Verify this is true for pulse_width on v3+ HW (and the range).
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::setRelativeCommand(SET_COMMAND command, int shift)
{
    if (shift == 0)
        return true;

    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    switch (command)
    {
        case SET_JITTER:
            snprintf(cmd, DRIVER_LEN, "%s0", shift > 0 ? "]" : "[");
            break;

        case SET_THRESHOLD:
            snprintf(cmd, DRIVER_LEN, "%s0", shift > 0 ? "}" : "{");
            break;

        case SET_PULSE_WITDH:
            snprintf(cmd, DRIVER_LEN, "%s0", shift > 0 ? "M" : "N");
            break;

        default:
            assert(false);
    }

    for (int i = 0; i < std::abs(shift); ++i)
    {
        if (!sendCommand(cmd, res))
            return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::SelectFilter(int f)
{
    // The wheel does not return a response when setting the value to the
    // current number.
    if (CurrentFilter == f)
    {
        SelectFilterDone(CurrentFilter);
        return true;
    }

    // The wheel moves to a new position, and responds with one line or two.
    // On success, the first line will be P#.  On failure, it is an ERROR.
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "G%X", f);
    if (!sendCommand(cmd, res))
        return false;

    // On success, the wheel may also return an ERROR on a second line.
    char opt[DRIVER_LEN] = {0};
    if (!receiveResponse(opt, true))
        return false;

    if (!getFilterPosition())
        return false;

    SelectFilterDone(CurrentFilter);

    return true;

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getStartupData()
{
    bool rc1 = getFirmwareInfo();

    bool rc2 = getSettingInfo();

    for (int i = 0; i < OffsetNP.nnp; i++)
        getOffset(i);

    return (rc1 && rc2);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getFirmwareInfo()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};

    // Product
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_PRODUCT_NAME);
    if (!sendCommand(cmd, res))
        return false;
    IUSaveText(&FirmwareInfoT[FIRMWARE_PRODUCT], res);

    // Version
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_FIRMWARE_VERSION);
    if (!sendCommand(cmd, res))
        return false;
    IUSaveText(&FirmwareInfoT[FIRMWARE_VERSION], res);

    // Serial Number
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_SERIAL_NUMBER);
    if (!sendCommand(cmd, res))
        return false;
    IUSaveText(&FirmwareInfoT[FIRMWARE_SERIAL], res);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getSettingInfo()
{
    bool rc1 = getMaximumSpeed();
    bool rc2 = getJitter();
    bool rc3 = getThreshold();
    bool rc4 = true;

    if (m_FirmwareVersion >= 3)
        rc4 = getPulseWidth();

    return (rc1 && rc2 && rc3 && rc4);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getFilterPosition()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_FILTER_POSITION);
    if (!sendCommand(cmd, res))
        return false;

    int rc = sscanf(res, "P%d", &CurrentFilter);

    if (rc > 0)
    {
        FilterSlotNP[0].setValue(CurrentFilter);
        return true;
    }
    else
        return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getMaximumSpeed()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_MAX_SPEED);
    if (!sendCommand(cmd, res))
        return false;

    int maxSpeed = 0;
    int rc       = sscanf(res, "MaxSpeed %d%%", &maxSpeed);
    if (rc > 0)
    {
        SettingsN[SET_SPEED].value = maxSpeed;
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getJitter()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_JITTER);
    if (!sendCommand(cmd, res))
        return false;

    int jitter = 0;
    int rc     = sscanf(res, "Jitter %d", &jitter);

    if (rc > 0)
    {
        SettingsN[SET_JITTER].value = jitter;
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getThreshold()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_THRESHOLD);
    if (!sendCommand(cmd, res))
        return false;

    int threshold = 0;
    int rc        = sscanf(res, "Threshold %d", &threshold);

    if (rc > 0)
    {
        SettingsN[SET_THRESHOLD].value = threshold;
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getPulseWidth()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_PULSE_WIDTH);
    if (!sendCommand(cmd, res))
        return false;

    int pulseWidth = 0;
    int rc         = sscanf(res, "PulseWidth %duS", &pulseWidth);

    if (rc > 0)
    {
        SettingsN[SET_PULSE_WITDH].value = pulseWidth;
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getMaxFilterSlots()
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "I%d", INFO_MAX_SLOTS);
    if (!sendCommand(cmd, res))
        return false;

    int maxFilterSlots = 0;
    int rc             = sscanf(res, "FilterSlots %d", &maxFilterSlots);

    if (rc > 0)
    {
        FilterSlotNP[0].setMax(maxFilterSlots);
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Reset commands perform various actions:
///
/// 0 & 1: Hard/soft reboot. Prints a message like:
///  Restart�
///  Xagyl FW5125V1
///  FW 1.9.9
///  Initializing
///  P1
///
/// (1 does not print "Restart")
///
/// 2 prints "Calibration Removed"
/// 6 prints nothing.
///
/// For safety, 0 & 1 need to wait until a line with "P1" appears.
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::reset(int command)
{
    char cmd[DRIVER_LEN] = {0}, resbuf[DRIVER_LEN] = {0};
    char *res = resbuf;

    int value = command;
    if (command == COMMAND_PERFORM_CALIBRAITON)
    {
        value = 6;
        res = nullptr;
    }

    snprintf(cmd, DRIVER_LEN, "R%d", value);
    if (!sendCommand(cmd, res))
        return false;

    // Wait for P1 on relevant commands.
    if (command == COMMAND_REBOOT || command == COMMAND_INIT)
    {
        do
        {
            if (!receiveResponse(res))
                return false;
        }
        while (0 != strcmp(res, "P1"));
    }

    getFilterPosition();
    getStartupData();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Offset values are relative encoded to the current filter selected.
///
/// Unfortunately, this means that to set them we have to move the wheel.
///
/// The valid range is -10 to 10, but the device prints -: for -10. Because of
/// this, we only support -9 to 9 for simplicity.
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::setOffset(int filter, int shift)
{
    if (!SelectFilter(filter))
        return false;

    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "%s0", shift > 0 ? "(" : ")");

    // Set filter offset.
    for (int i = 0; i < std::abs(shift); ++i)
    {
        res[0] = 0;
        if (!sendCommand(cmd, res))
            return false;
    }

    // Update filter offset based on result.
    int filter_num = 0, offset = 0;
    int rc = sscanf(res, "P%d Offset %d", &filter_num, &offset);

    if (rc > 0)
    {
        OffsetN[filter_num - 1].value = offset;
        return true;
    }
    else
        return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::getOffset(int filter)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "O%d", filter + 1);
    if (!sendCommand(cmd, res))
        return false;

    int filter_num = 0, offset = 0;
    int rc = sscanf(res, "P%d Offset %d", &filter_num, &offset);

    if (rc > 0)
    {
        OffsetN[filter_num - 1].value = offset;
        return true;
    }
    else
        return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &SettingsNP);
    if (OffsetN != nullptr)
        IUSaveConfigNumber(fp, &OffsetNP);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read a line from the device and handle it
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::receiveResponse(char * res, bool optional)
{
    int nbytes_read = 0, rc = -1;

    int timeout = optional ? OPTIONAL_TIMEOUT : DRIVER_TIMEOUT;
    rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR,
                           timeout, &nbytes_read);
    if (optional && rc == TTY_TIME_OUT)
    {
        res[0] = '\0';
        LOG_DEBUG("RES (optional): not found.");
        return true;
    }
    else if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    // Remove extra \r
    assert(nbytes_read > 1);
    res[nbytes_read - 2] = 0;

    // If the response starts with "ERROR" the command failed.
    if (0 == strncmp(res, "ERROR", 5))
    {
        LOGF_WARN("Device error: %s", res);
        if (!optional)
            return false;
    }
    else
        LOGF_DEBUG("RES <%s>", res);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    // Send
    LOGF_DEBUG("CMD <%s>", cmd);
    rc = tty_write_string(PortFD, cmd, &nbytes_written);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    // Receive
    if (!res)
        return true;

    return receiveResponse(res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void XAGYLWheel::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}
