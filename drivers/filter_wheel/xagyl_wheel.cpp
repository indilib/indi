/*******************************************************************************
  Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

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

void ISGetProperties(const char *dev)
{
    xagylWheel->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    xagylWheel->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    xagylWheel->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    xagylWheel->ISNewNumber(dev, name, values, names, n);
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
    xagylWheel->ISSnoopDevice(root);
}

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
    IUFillText(&FirmwareInfoT[FIRMWARE_PRODUCT], "FIRMWARE_PRODUCT", "Product", nullptr);
    IUFillText(&FirmwareInfoT[FIRMWARE_VERSION], "FIRMWARE_VERSION", "Version", nullptr);
    IUFillText(&FirmwareInfoT[FIRMWARE_SERIAL], "FIRMWARE_SERIAL", "Serial #", nullptr);
    IUFillTextVector(&FirmwareInfoTP, FirmwareInfoT, 3, getDeviceName(), "Info", "Info", MAIN_CONTROL_TAB, IP_RO, 60,
                     IPS_IDLE);

    // Settings
    IUFillNumber(&SettingsN[SETTING_SPEED], "SETTING_SPEED", "Speed", "%.f", 0, 100, 10., 0.);
    IUFillNumber(&SettingsN[SETTING_JITTER], "SETTING_JITTER", "Jitter", "%.f", 0, 10, 1., 0.);
    IUFillNumber(&SettingsN[SETTING_THRESHOLD], "SETTING_THRESHOLD", "Threshold", "%.f", 0, 100, 10., 0.);
    IUFillNumber(&SettingsN[SETTING_PW], "SETTING_PW", "Pulse", "%.f", 100, 10000, 100., 0.);
    IUFillNumberVector(&SettingsNP, SettingsN, 4, getDeviceName(), "Settings", "Settings", SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    // Reset
    IUFillSwitch(&ResetS[COMMAND_REBOOT], "COMMAND_REBOOT", "Reboot", ISS_OFF);
    IUFillSwitch(&ResetS[COMMAND_INIT], "COMMAND_INIT", "Initialize", ISS_OFF);
    IUFillSwitch(&ResetS[COMMAND_CLEAR_CALIBRATION], "COMMAND_CLEAR_CALIBRATION Calibration", "Clear Calibration", ISS_OFF);
    IUFillSwitch(&ResetS[COMMAND_PERFORM_CALIBRAITON], "COMMAND_PERFORM_CALIBRAITON", "Perform Calibration", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 4, getDeviceName(), "Commands", "Commands", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0,
                       IPS_IDLE);

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

        defineSwitch(&ResetSP);
        defineNumber(&OffsetNP);
        defineText(&FirmwareInfoTP);
        defineNumber(&SettingsNP);
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
    if (m_FirmwareVersion < 3)
        SettingsNP.nnp--;

    bool rc = getMaxFilterSlots();
    if (!rc) {
        LOG_ERROR("Unable to parse max filter slots.");
        return false;
    }

    initOffset();

    rc = getFilterPosition();
    if (!rc) {
        LOG_ERROR("Unable to initialize filter position.");
        return false;
    }

    LOG_INFO("XAGYL is online. Getting filter parameters...");

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(ResetSP.name, name) == 0)
        {
            IUUpdateSwitch(&ResetSP, states, names, n);
            int value = IUFindOnSwitchIndex(&ResetSP);
            IUResetSwitch(&ResetSP);

            if (value == COMMAND_PERFORM_CALIBRAITON)
                value = 6;

            bool rc = reset(value);

            if (rc)
            {
                switch (value)
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

                    case 6:
                        LOG_INFO("Calibrating...");
                        break;
                }
            }

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
bool XAGYLWheel::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(OffsetNP.name, name) == 0)
        {
            bool rc_offset = true;

            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], OffsetN[i].name))
                {
                    if (std::abs(values[i] - OffsetN[i].value) > 0)
                    {
                        if (values[i] > OffsetN[i].value)
                            rc_offset &= setOffset(1);
                        else
                            rc_offset &= setOffset(-1);
                    }
                }
            }

            OffsetNP.s = rc_offset ? IPS_OK : IPS_ALERT;
            IDSetNumber(&OffsetNP, nullptr);
            return true;
        }

        if (strcmp(SettingsNP.name, name) == 0)
        {
            double newSpeed = 0, newJitter = 0, newThreshold = 0, newPulseWidth = 0;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], SettingsN[SET_SPEED].name))
                    newSpeed = values[i];
                else if (!strcmp(names[i], SettingsN[SET_JITTER].name))
                    newJitter = values[i];
                if (!strcmp(names[i], SettingsN[SET_THRESHOLD].name))
                    newThreshold = values[i];
                if (!strcmp(names[i], SettingsN[SET_PULSE_WITDH].name))
                    newPulseWidth = values[i];
            }

            bool rc_speed = true, rc_jitter = true, rc_threshold = true, rc_pulsewidth = true;

            if (std::abs(newSpeed - SettingsN[SET_SPEED].value) > 0)
            {
                rc_speed = setCommand(SET_SPEED, newSpeed);
                getMaximumSpeed();
            }

            // Jitter
            if (std::abs(newJitter - SettingsN[SET_JITTER].value))
            {
                if (newJitter > SettingsN[SET_JITTER].value)
                {
                    rc_jitter &= setCommand(SET_JITTER, 1);
                    getJitter();
                }
                else
                {
                    rc_jitter &= setCommand(SET_JITTER, -1);
                    getJitter();
                }
            }

            // Threshold
            if (std::abs(newThreshold - SettingsN[SET_THRESHOLD].value) > 0)
            {
                if (newThreshold > SettingsN[SET_THRESHOLD].value)
                {
                    rc_threshold &= setCommand(SET_THRESHOLD, 1);
                    getThreshold();
                }
                else
                {
                    rc_threshold &= setCommand(SET_THRESHOLD, -1);
                    getThreshold();
                }
            }

            // Pulse width
            if (m_FirmwareVersion >= 3 && std::abs(newPulseWidth - SettingsN[SET_PULSE_WITDH].value))
            {
                if (newPulseWidth > SettingsN[SET_PULSE_WITDH].value)
                {
                    rc_pulsewidth &= setCommand(SET_PULSE_WITDH, 1);
                    getPulseWidth();
                }
                else
                {
                    rc_pulsewidth &= setCommand(SET_PULSE_WITDH, -1);
                    getPulseWidth();
                }
            }

            if (rc_speed && rc_jitter && rc_threshold && rc_pulsewidth)
                SettingsNP.s = IPS_OK;
            else
                SettingsNP.s = IPS_ALERT;

            IDSetNumber(&SettingsNP, nullptr);

            return true;
        }
    }

    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void XAGYLWheel::initOffset()
{
    delete [] OffsetN;
    OffsetN = new INumber[static_cast<uint8_t>(FilterSlotN[0].max)];
    char offsetName[MAXINDINAME], offsetLabel[MAXINDILABEL];
    for (int i = 0; i < FilterSlotN[0].max; i++)
    {
        snprintf(offsetName, MAXINDINAME, "OFFSET_%d", i + 1);
        snprintf(offsetLabel, MAXINDINAME, "#%d Offset", i + 1);
        IUFillNumber(OffsetN + i, offsetName, offsetLabel, "%.f", -99, 99, 10, 0);
    }

    IUFillNumberVector(&OffsetNP, OffsetN, FilterSlotN[0].max, getDeviceName(), "Offsets", "", FILTER_TAB, IP_RW, 0,
                       IPS_IDLE);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::setCommand(SET_COMMAND command, int value)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    switch (command)
    {
        case SET_SPEED:
            snprintf(cmd, DRIVER_LEN, "S%X", value / 10);
            break;

        case SET_JITTER:
            snprintf(cmd, DRIVER_LEN, "%s0", value > 0 ? "]" : "[");
            break;

        case SET_THRESHOLD:
            snprintf(cmd, DRIVER_LEN, "%s0", value > 0 ? "}" : "{");
            break;

        case SET_PULSE_WITDH:
            snprintf(cmd, DRIVER_LEN, "%s0", value > 0 ? "M" : "N");
            break;
    }

    return sendCommand(cmd, res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::SelectFilter(int f)
{
    // The wheel does not return a response when setting the value to the
    // current number.
    if (CurrentFilter == f) {
        SelectFilterDone(CurrentFilter);
        return true;
    }

    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "G%X", f);
    if (!sendCommand(cmd, res))
        return false;

    char opt[DRIVER_LEN] = {0};
    if (!optionalResponse(opt))
        return false;

    if (getFilterPosition())
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
        FilterSlotN[0].value = CurrentFilter;
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
    int rc         = sscanf(res, "Pulse Width %duS", &pulseWidth);

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
        FilterSlotN[0].max = maxFilterSlots;
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::reset(int value)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "R%d", value);
    if (!sendCommand(cmd, res))
        return false;

    getFilterPosition();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::setOffset(int value)
{
    char cmd[DRIVER_LEN] = {0}, res[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "%s", value > 0 ? "(" : ")");
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
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool XAGYLWheel::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        // Remove extra \r
        assert(nbytes_read > 0);

        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Read optional response in receive buffer to flush input
/////////////////////////////////////////////////////////////////////////////

bool XAGYLWheel::optionalResponse(char *res)
{
    int nbytes_read = 0, rc = -1;

    rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, FLUSH_TIMEOUT,
                           &nbytes_read);
    if (rc == TTY_TIME_OUT)
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
    assert(nbytes_read > 0);

    res[nbytes_read - 1] = 0;
    LOGF_DEBUG("RES <%s>", res);

    return true;
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

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> XAGYLWheel::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
