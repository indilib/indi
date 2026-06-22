/*******************************************************************************
  Copyright(c) 2026 Jérémie Klein. All rights reserved.
  Copyright(c) 2026 WandererAstro. All rights reserved. (modifications)

  Wanderer Snowflake Filter Wheel Driver

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

#include "wanderer_snowflake.h"

#include "indicom.h"

#include <connectionplugins/connectionserial.h>
#include <cstring>
#include <memory>
#include <strings.h>
#include <termios.h>

#define WANDERER_TIMEOUT 5

#define WANDERER_STATUS_TIMEOUT 12

#define WANDERER_CALIB_REPLY_TIMEOUT 10
#define WANDERER_MAX_FILTERS 8

#define WANDERER_CMD_ZERO_DETECT "1002"

static std::unique_ptr<WandererSnowflakeFW> wanderer_snowflake(new WandererSnowflakeFW());

WandererSnowflakeFW::WandererSnowflakeFW()
{
    setVersion(1, 0);
    setFilterConnection(CONNECTION_SERIAL);
}

const char *WandererSnowflakeFW::getDefaultName()
{
    return "Wanderer Snowflake Filter Wheel";
}

bool WandererSnowflakeFW::initProperties()
{
    INDI::FilterWheel::initProperties();

    if (serialConnection != nullptr)
        serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(WANDERER_MAX_FILTERS);

    IUFillSwitch(&CalibrationCmdS[0], "CALIBRATE", "Auto calibrate", ISS_OFF);
    IUFillSwitch(&CalibrationCmdS[1], "ZERO_DETECT", "Zero detection", ISS_OFF);
    IUFillSwitchVector(&CalibrationCmdSP, CalibrationCmdS, 2, getDeviceName(), "CALIBRATION_CMDS", "Calibration",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillNumber(&DeviceIDN[0], "DEVICE_ID", "ID", "%0.0f", 0, 10, 1, 0);
    IUFillNumberVector(&DeviceIDNP, DeviceIDN, 1, getDeviceName(),
                       "DEVICE_ID", "Device ID", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    IUFillText(&DeviceInfoT[0], "MODEL", "Model", "");
    IUFillText(&DeviceInfoT[1], "FIRMWARE", "Firmware", "");
    IUFillTextVector(&DeviceInfoTP, DeviceInfoT, 2, getDeviceName(),
                     "DEVICE_INFO", "Device Info", OPTIONS_TAB, IP_RO, 60, IPS_IDLE);

    addAuxControls();

    return true;
}

bool WandererSnowflakeFW::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineProperty(&CalibrationCmdSP);
        defineProperty(&DeviceIDNP);
        DeviceIDN[0].value = mDeviceID;
        DeviceIDNP.s = IPS_OK;
        IDSetNumber(&DeviceIDNP, nullptr);
        defineProperty(&DeviceInfoTP);
    }
    else
    {
        deleteProperty(CalibrationCmdSP.name);
        deleteProperty(DeviceIDNP.name);
        deleteProperty(DeviceInfoTP.name);
    }

    return true;
}

bool WandererSnowflakeFW::Handshake()
{
    if (isSimulation())
    {
        sendAutomaticCalibration();
        return true;
    }

    tcflush(PortFD, TCIOFLUSH);

    int current = 0;
    if (!readCurrentFilterFromStatus(current))
    {
        LOG_ERROR("Wanderer Snowflake model (WSFW508/WSFW368) not found in status. Wrong port or device.");
        return false;
    }

    if (current >= 1 && current <= WANDERER_MAX_FILTERS)
    {
        CurrentFilter = current;
        FilterSlotNP[0].setValue(CurrentFilter);
    }

    char field[64] = {0};
    int nbytes = 0;

    if (tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes) == TTY_OK)
    {
        while (nbytes > 0 && (field[nbytes - 1] == 'A' || field[nbytes - 1] == '\r' || field[nbytes - 1] == '\n'))
        {
            nbytes--;
            field[nbytes] = '\0';
        }
        for (int i = 0; i < WANDERER_MAX_FILTERS && i < nbytes; i++)
            mFilterLetters[i] = field[i];
    }

    for (int i = 0; i < WANDERER_MAX_FILTERS; i++)
        tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes);

    if (tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes) == TTY_OK)
    {
        while (nbytes > 0 && (field[nbytes - 1] == 'A' || field[nbytes - 1] == '\r' || field[nbytes - 1] == '\n'))
        {
            nbytes--;
            field[nbytes] = '\0';
        }
        mDeviceID = std::atoi(field);
    }

    IUSaveText(&DeviceInfoT[0], mModel);
    IUSaveText(&DeviceInfoT[1], mFirmware);
    DeviceInfoTP.s = IPS_OK;
    IDSetText(&DeviceInfoTP, nullptr);

    usleep(200000);

    if (!sendAutomaticCalibration())
        LOG_WARN("Automatic calibration (1500002) failed at connect; use Calibration → Auto calibrate.");
    else
        LOG_INFO("Automatic calibration (1500002) sent at connection.");

    return true;
}

bool WandererSnowflakeFW::sendAutomaticCalibration()
{
    if (isSimulation())
    {
        LOG_INFO("Automatic calibration (1500002, simulation).");
        return true;
    }

    if (!sendCommand("1500002", nullptr, 0, WANDERER_TIMEOUT))
    {
        LOG_ERROR("Failed to send automatic calibration command (1500002).");
        return false;
    }
    LOG_INFO("Automatic calibration command (1500002) sent.");
    return true;
}

bool WandererSnowflakeFW::sendZeroDetection()
{
    if (isSimulation())
    {
        LOG_INFO("Zero detection (" WANDERER_CMD_ZERO_DETECT ", simulation). CurrentFilter → 1.");
        CurrentFilter = 1;
        FilterSlotNP[0].setValue(1);
        FilterSlotNP.setState(IPS_OK);
        FilterSlotNP.apply("Zero detection complete (simulation).");
        return true;
    }

    if (!sendCommand(WANDERER_CMD_ZERO_DETECT, nullptr, 0, WANDERER_TIMEOUT))
    {
        LOGF_ERROR("Failed to send zero detection command (%s).", WANDERER_CMD_ZERO_DETECT);
        return false;
    }
    LOGF_INFO("Zero detection command (%s) sent; waiting for filter 1.", WANDERER_CMD_ZERO_DETECT);

    constexpr int maxAttempts = 80;
    for (int attempts = 0; attempts < maxAttempts; ++attempts)
    {
        int current = 0;
        if (readCurrentFilterFromStatus(current) && current == 1)
        {
            CurrentFilter = 1;
            FilterSlotNP[0].setValue(1);
            FilterSlotNP.setState(IPS_OK);
            FilterSlotNP.apply("Zero detection complete, at filter 1.");
            return true;
        }
        usleep(500 * 1000);
    }

    LOG_WARN("Zero detection: timed out waiting for filter 1; wheel may still be moving.");
    return false;
}

bool WandererSnowflakeFW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && std::strcmp(dev, getDeviceName()) == 0)
    {
        if (std::strcmp(CalibrationCmdSP.name, name) == 0)
        {
            IUUpdateSwitch(&CalibrationCmdSP, states, names, n);
            const bool wantCal  = (CalibrationCmdS[0].s == ISS_ON);
            const bool wantZero = (CalibrationCmdS[1].s == ISS_ON);
            IUResetSwitch(&CalibrationCmdSP);

            CalibrationCmdSP.s = IPS_BUSY;
            IDSetSwitch(&CalibrationCmdSP, nullptr);

            bool ok = true;
            if (wantCal)
                ok = sendAutomaticCalibration();
            else if (wantZero)
                ok = sendZeroDetection();

            CalibrationCmdSP.s = ok ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&CalibrationCmdSP, nullptr);
            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool WandererSnowflakeFW::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && std::strcmp(dev, getDeviceName()) == 0)
    {

        if (std::strcmp(DeviceIDNP.name, name) == 0)
        {
            IUUpdateNumber(&DeviceIDNP, values, names, n);
            int id = static_cast<int>(DeviceIDN[0].value);
            if (id < 0 || id > 10)
            {
                LOG_ERROR("Device ID must be 0-10.");
                DeviceIDNP.s = IPS_ALERT;
                IDSetNumber(&DeviceIDNP, nullptr);
                return true;
            }
            char cmd[16] = {0};
            snprintf(cmd, sizeof(cmd), "%d", 1900000 + id);
            if (!sendCommand(cmd, nullptr, 0, WANDERER_TIMEOUT))
            {
                LOG_ERROR("Failed to set device ID.");
                DeviceIDNP.s = IPS_ALERT;
                IDSetNumber(&DeviceIDNP, nullptr);
                return true;
            }
            mDeviceID = id;
            DeviceIDNP.s = IPS_OK;
            IDSetNumber(&DeviceIDNP, nullptr);
            LOGF_INFO("Device ID set to %d.", id);
            return true;
        }
    }
    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

int WandererSnowflakeFW::QueryFilter()
{
    return CurrentFilter;
}

bool WandererSnowflakeFW::SelectFilter(int position)
{
    if (position == CurrentFilter)
    {
        SelectFilterDone(CurrentFilter);
        return true;
    }

    if (position < 1 || position > WANDERER_MAX_FILTERS)
    {
        LOGF_ERROR("Requested filter position %d is out of range (1-%d).", position, WANDERER_MAX_FILTERS);
        return false;
    }

    TargetFilter = position;

    if (isSimulation())
    {
        CurrentFilter = TargetFilter;
        FilterSlotNP[0].setValue(CurrentFilter);
        FilterSlotNP.setState(IPS_OK);
        FilterSlotNP.apply("Selected filter position reached (simulation).");
        SelectFilterDone(CurrentFilter);
        return true;
    }

    char cmd[16] = {0};
    snprintf(cmd, sizeof(cmd), "%d", 2000 + TargetFilter);

    if (!sendCommand(cmd, nullptr, 0, WANDERER_TIMEOUT))
    {
        LOG_ERROR("Failed to send move command to Wanderer Snowflake filter wheel.");
        return false;
    }

    tcflush(PortFD, TCIFLUSH);

    int attempts = 0;
    constexpr int maxAttempts = 80;

    while (attempts++ < maxAttempts)
    {
        int current = 0;
        if (readCurrentFilterFromStatus(current) && current == TargetFilter)
        {
            CurrentFilter = current;
            FilterSlotNP[0].setValue(CurrentFilter);
            FilterSlotNP.setState(IPS_OK);
            FilterSlotNP.apply("Selected filter position reached");
            SelectFilterDone(CurrentFilter);
            return true;
        }
        usleep(500 * 1000);
    }

    LOG_ERROR("Timed out waiting for Wanderer Snowflake filter wheel to reach target position.");
    return false;
}

bool WandererSnowflakeFW::sendCommand(const char *command, char *response, int responseLen, int timeoutSeconds)
{
    int nbytes_written = 0;
    int nbytes_read = 0;
    int rc;

    if (PortFD < 0)
        return false;

    char line[48];
    const int linelen = std::snprintf(line, sizeof(line), "%s\r", command);
    if (linelen <= 0 || linelen >= static_cast<int>(sizeof(line)))
    {
        LOG_ERROR("Wanderer Snowflake command too long for transmit buffer.");
        return false;
    }

    if (response != nullptr && responseLen > 1)
        tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, line, linelen, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error sending command '%s' to Wanderer Snowflake filter wheel: %s", command, errstr);
        return false;
    }

    if (response == nullptr || responseLen <= 1)
        return true;

    rc = tty_read_section(PortFD, response, '\r', timeoutSeconds, &nbytes_read);
    if (rc != TTY_OK)
        rc = tty_read_section(PortFD, response, '\n', timeoutSeconds, &nbytes_read);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_DEBUG("No CR/LF line after command '%s' (status stream may omit it while moving): %s", command, errstr);
        response[0] = '\0';
        return true;
    }

    while (nbytes_read > 0 && (response[nbytes_read - 1] == '\r' || response[nbytes_read - 1] == '\n'))
    {
        nbytes_read--;
        response[nbytes_read] = '\0';
    }
    if (nbytes_read >= responseLen)
        nbytes_read = responseLen - 1;
    response[nbytes_read] = '\0';

    return true;
}

bool WandererSnowflakeFW::saveConfigItems(FILE *fp)
{

    INDI::DefaultDevice::saveConfigItems(fp);
    if (FilterNameTP.size() > 0)
        FilterNameTP.save(fp);
    return true;
}

bool WandererSnowflakeFW::GetFilterNames()
{
    FilterNameTP.resize(0);
    for (int i = 0; i < WANDERER_MAX_FILTERS; i++)
    {
        char filterName[32];
        char filterLabel[16];
        char filterText[4];
        snprintf(filterName, sizeof(filterName), "FILTER_SLOT_NAME_%d", i + 1);
        snprintf(filterLabel, sizeof(filterLabel), "Filter %d", i + 1);
        snprintf(filterText, sizeof(filterText), "%c", mFilterLetters[i]);
        INDI::WidgetText oneWidget;

        oneWidget.fill(filterName, filterLabel, filterText);
        FilterNameTP.push(std::move(oneWidget));
    }
    FilterNameTP.fill(getDeviceName(), "FILTER_NAME", "Filter",
                      FilterSlotNP.getGroupName(), IP_RW, 0, IPS_IDLE);
    FilterNameTP.shrink_to_fit();
    FilterSlotNP[0].setMax(FilterNameTP.count());
    return true;
}

bool WandererSnowflakeFW::SetFilterNames()
{
    if (isSimulation())
        return true;

    bool allOk = true;
    for (int i = 0; i < WANDERER_MAX_FILTERS; i++)
    {
        const char *txt = FilterNameTP[i].getText();
        if (txt == nullptr)
            continue;

        const char *p = txt;
        while (*p == ' ' || *p == '\t')
            p++;
        char c = *p;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        char after = p[1];
        bool singleLetter = (c >= 'B' && c <= 'Z') &&
                            (after == '\0' || after == ' ' || after == '\t' || after == '\r' || after == '\n');

        if (!singleLetter)
        {

            LOGF_ERROR("Filter %d name '%s' invalid — must be a single letter (B-Z).", i + 1, txt);

            char orig[2] = { mFilterLetters[i] ? mFilterLetters[i] : 'X', '\0' };
            FilterNameTP[i].setText(orig);
            allOk = false;
            continue;
        }

        char norm[2] = { c, '\0' };
        FilterNameTP[i].setText(norm);

        int code = (161 + i) * 10000 + (c - 'A' + 1);
        char cmd[16] = {0};
        snprintf(cmd, sizeof(cmd), "%d", code);

        if (!sendCommand(cmd, nullptr, 0, WANDERER_TIMEOUT))
        {
            LOGF_ERROR("Failed to set filter %d name to %c.", i + 1, c);
            allOk = false;
            continue;
        }

        mFilterLetters[i] = c;
        LOGF_INFO("Filter %d name set to %c.", i + 1, c);
    }

    if (!allOk)
    {
        FilterNameTP.setState(IPS_ALERT);
        FilterNameTP.apply("Invalid filter name(s): must be a single letter (B-Z). Invalid entries reverted.");
    }
    return true;
}

namespace
{

void wandererStripFieldDelimiter(char *field, int &nbytes_read, char delimiter)
{
    while (nbytes_read > 0)
    {
        char c = field[nbytes_read - 1];
        if (c == delimiter || c == '\r' || c == '\n')
        {
            nbytes_read--;
            field[nbytes_read] = '\0';
        }
        else
            break;
    }
    field[nbytes_read] = '\0';
}

char *wandererTrimLeft(char *s)
{
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if (p != s)
        std::memmove(s, p, std::strlen(p) + 1);
    return s;
}

bool wandererFieldLooksLikeModel(char *field, int nbytes_read)
{
    if (nbytes_read <= 0)
        return false;
    field[nbytes_read] = '\0';
    wandererTrimLeft(field);
    return (strncasecmp(field, "WSFW508", 7) == 0 || strncasecmp(field, "WSFW368", 7) == 0);
}
}

bool WandererSnowflakeFW::readCurrentFilterFromStatus(int &position)
{
    int rc = -1;
    int nbytes_read = 0;
    char field[64] = {0};

    if (PortFD < 0)
        return false;

    constexpr int maxResync = 32;
    bool haveModel = false;

    for (int skip = 0; skip < maxResync; ++skip)
    {
        if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_DEBUG("Failed to read Wanderer Snowflake status field: %s", errstr);
            return false;
        }
        wandererStripFieldDelimiter(field, nbytes_read, 'A');

        if (wandererFieldLooksLikeModel(field, static_cast<int>(std::strlen(field))))
        {
            snprintf(mModel, sizeof(mModel), "%s", field);
            haveModel = true;
            break;
        }

        LOGF_DEBUG("Skipping Wanderer Snowflake status field while resyncing: '%s'", field);
    }

    if (haveModel)
    {
        if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_DEBUG("Failed to read firmware field from Wanderer Snowflake status: %s", errstr);
            return false;
        }
        wandererStripFieldDelimiter(field, nbytes_read, 'A');
        snprintf(mFirmware, sizeof(mFirmware), "%s", field);

        double fwVersion = std::atof(field);
        if (fwVersion < 20260124.0)
        {
            LOGF_ERROR("Wanderer Snowflake firmware %.0f is too old (minimum 20260124). "
                       "Please update the firmware in WandererEmpire.", fwVersion);
            return false;
        }

        if ((rc = tty_read_section(PortFD, field, 'A', WANDERER_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_DEBUG("Failed to read current filter field from Wanderer Snowflake status: %s", errstr);
            return false;
        }
        wandererStripFieldDelimiter(field, nbytes_read, 'A');

        int current = std::atoi(field);
        if (current >= 1 && current <= WANDERER_MAX_FILTERS)
        {
            position = current;
            return true;
        }
        LOGF_DEBUG("Received invalid current filter value '%s' from Wanderer Snowflake status.", field);
        return false;
    }

    return false;
}


