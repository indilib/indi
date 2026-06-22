/*******************************************************************************
  Copyright(c) 2026 WandererAstro. All rights reserved.

  RASA FilterCube Filter Wheel Driver

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

#include "rasa_filtercube.h"

#include "indicom.h"

#include <connectionplugins/connectionserial.h>
#include <cstring>
#include <memory>
#include <strings.h>
#include <termios.h>

#define FILTERCUBE_TIMEOUT          5
#define FILTERCUBE_STATUS_TIMEOUT   2
#define FILTERCUBE_MIN_FIRMWARE     20260312.0
#define FILTERCUBE_MODEL            "WFC50"

static std::unique_ptr<RasaFilterCube> rasa_filtercube(new RasaFilterCube());

RasaFilterCube::RasaFilterCube()
{
    setVersion(1, 0);
    setFilterConnection(CONNECTION_SERIAL);
}

const char *RasaFilterCube::getDefaultName()
{

    return "Wanderer FilterCube";
}

bool RasaFilterCube::initProperties()
{
    INDI::FilterWheel::initProperties();

    if (serialConnection != nullptr)
        serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(FILTERCUBE_MAX_FILTERS);

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

bool RasaFilterCube::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineProperty(&DeviceIDNP);
        DeviceIDN[0].value = mDeviceID;
        DeviceIDNP.s = IPS_OK;
        IDSetNumber(&DeviceIDNP, nullptr);
        defineProperty(&DeviceInfoTP);
    }
    else
    {
        deleteProperty(DeviceIDNP.name);
        deleteProperty(DeviceInfoTP.name);
    }

    return true;
}

bool RasaFilterCube::GetFilterNames()
{
    FilterNameTP.resize(0);
    for (int i = 0; i < FILTERCUBE_MAX_FILTERS; i++)
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

bool RasaFilterCube::SetFilterNames()
{
    if (isSimulation())
        return true;

    bool allOk = true;
    for (int i = 0; i < FILTERCUBE_MAX_FILTERS; i++)
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

        if (!sendCommand(cmd, nullptr, 0, FILTERCUBE_TIMEOUT))
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

bool RasaFilterCube::Handshake()
{
    if (isSimulation())
        return true;

    tcflush(PortFD, TCIOFLUSH);

    int current = 0;
    char field[64] = {0};
    int rc = 0, nbytes_read = 0;

    constexpr int maxResync = 32;
    bool foundModel = false;

    for (int skip = 0; skip < maxResync; ++skip)
    {
        if ((rc = tty_read_section(PortFD, field, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            LOG_ERROR("Failed to read from RASA FilterCube during handshake.");
            return false;
        }

        while (nbytes_read > 0 && (field[nbytes_read - 1] == 'A' || field[nbytes_read - 1] == '\r' || field[nbytes_read - 1] == '\n'))
        {
            nbytes_read--;
            field[nbytes_read] = '\0';
        }

        char *p = field;
        while (*p == ' ' || *p == '\t')
            p++;
        if (p != field)
            std::memmove(field, p, std::strlen(p) + 1);

        if (strncasecmp(field, FILTERCUBE_MODEL, std::strlen(FILTERCUBE_MODEL)) == 0)
        {
            snprintf(mModel, sizeof(mModel), "%s", field);
            foundModel = true;
            break;
        }
    }

    if (!foundModel)
    {
        LOG_ERROR("RASA FilterCube model identifier (WFC50) not found.");
        return false;
    }

    if ((rc = tty_read_section(PortFD, field, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOG_ERROR("Failed to read firmware version from RASA FilterCube.");
        return false;
    }
    while (nbytes_read > 0 && (field[nbytes_read - 1] == 'A' || field[nbytes_read - 1] == '\r' || field[nbytes_read - 1] == '\n'))
    {
        nbytes_read--;
        field[nbytes_read] = '\0';
    }
    snprintf(mFirmware, sizeof(mFirmware), "%s", field);

    double fwVersion = std::atof(mFirmware);
    if (fwVersion < FILTERCUBE_MIN_FIRMWARE)
    {
        LOGF_ERROR("RASA FilterCube firmware %.0f is too old (minimum: %d). "
                   "Please update the firmware in WandererEmpire.",
                   fwVersion, (int)FILTERCUBE_MIN_FIRMWARE);
        return false;
    }
    LOGF_INFO("RASA FilterCube firmware: %s", mFirmware);

    if ((rc = tty_read_section(PortFD, field, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOG_ERROR("Failed to read current filter position from RASA FilterCube.");
        return false;
    }
    while (nbytes_read > 0 && (field[nbytes_read - 1] == 'A' || field[nbytes_read - 1] == '\r' || field[nbytes_read - 1] == '\n'))
    {
        nbytes_read--;
        field[nbytes_read] = '\0';
    }
    current = std::atoi(field);
    if (current >= 1 && current <= FILTERCUBE_MAX_FILTERS)
    {
        CurrentFilter = current;
        FilterSlotNP[0].setValue(CurrentFilter);
    }

    char namesField[16] = {0};
    if ((rc = tty_read_section(PortFD, namesField, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOG_WARN("Failed to read filter names from RASA FilterCube; using defaults.");
    }
    else
    {
        while (nbytes_read > 0 && (namesField[nbytes_read - 1] == 'A' || namesField[nbytes_read - 1] == '\r' || namesField[nbytes_read - 1] == '\n'))
        {
            nbytes_read--;
            namesField[nbytes_read] = '\0';
        }
        if (nbytes_read >= FILTERCUBE_MAX_FILTERS)
        {
            for (int i = 0; i < FILTERCUBE_MAX_FILTERS; i++)
                mFilterLetters[i] = namesField[i];
            LOGF_DEBUG("Filter names: %c %c %c %c %c",
                       mFilterLetters[0], mFilterLetters[1], mFilterLetters[2],
                       mFilterLetters[3], mFilterLetters[4]);
        }
    }

    for (int i = 0; i < FILTERCUBE_MAX_FILTERS; i++)
    {
        if ((rc = tty_read_section(PortFD, field, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            LOGF_WARN("Failed to read offset %d from RASA FilterCube.", i + 1);
            break;
        }
    }

    if ((rc = tty_read_section(PortFD, field, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        LOG_WARN("Failed to read device ID from RASA FilterCube.");
    }
    else
    {
        while (nbytes_read > 0 && (field[nbytes_read - 1] == 'A' || field[nbytes_read - 1] == '\r' || field[nbytes_read - 1] == '\n'))
        {
            nbytes_read--;
            field[nbytes_read] = '\0';
        }
        mDeviceID = std::atoi(field);
    }

    LOGF_INFO("RASA FilterCube connected - Firmware: %s, ID: %d, Position: %d",
              mFirmware, mDeviceID, CurrentFilter);

    IUSaveText(&DeviceInfoT[0], mModel);
    IUSaveText(&DeviceInfoT[1], mFirmware);
    DeviceInfoTP.s = IPS_OK;
    IDSetText(&DeviceInfoTP, nullptr);

    return true;
}

int RasaFilterCube::QueryFilter()
{

    return CurrentFilter;
}

bool RasaFilterCube::SelectFilter(int position)
{
    if (position == CurrentFilter)
    {
        SelectFilterDone(CurrentFilter);
        return true;
    }

    if (position < 1 || position > FILTERCUBE_MAX_FILTERS)
    {
        LOGF_ERROR("Requested filter position %d is out of range (1-%d).", position, FILTERCUBE_MAX_FILTERS);
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

    if (!sendCommand(cmd, nullptr, 0, FILTERCUBE_TIMEOUT))
    {
        LOG_ERROR("Failed to send move command to RASA FilterCube.");
        return false;
    }

    tcflush(PortFD, TCIFLUSH);

    int attempts = 0;
    constexpr int maxAttempts = 150;

    while (attempts++ < maxAttempts)
    {
        int current = 0;
        if (readCurrentFilterFromStatus(current))
        {
            LOGF_DEBUG("SelectFilter: current=%d target=%d attempt %d", current, TargetFilter, attempts);
            if (current == TargetFilter)
            {
                CurrentFilter = current;
                FilterSlotNP[0].setValue(CurrentFilter);
                FilterSlotNP.setState(IPS_OK);
                FilterSlotNP.apply("Selected filter position reached");
                SelectFilterDone(CurrentFilter);
                return true;
            }
        }
        usleep(200 * 1000);
    }

    LOG_ERROR("Timed out waiting for RASA FilterCube to reach target position.");
    return false;
}

bool RasaFilterCube::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
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

            if (!sendCommand(cmd, nullptr, 0, FILTERCUBE_TIMEOUT))
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

bool RasaFilterCube::sendCommand(const char *command, char *response, int responseLen, int timeoutSeconds)
{
    int nbytes_written = 0;
    int nbytes_read = 0;
    int rc;

    if (PortFD < 0)
        return false;

    char line[48];

    const int linelen = std::snprintf(line, sizeof(line), "%s\n", command);
    if (linelen <= 0 || linelen >= static_cast<int>(sizeof(line)))
    {
        LOG_ERROR("RASA FilterCube command too long for transmit buffer.");
        return false;
    }

    if (response != nullptr && responseLen > 1)
        tcflush(PortFD, TCIFLUSH);

    if ((rc = tty_write(PortFD, line, linelen, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error sending command to RASA FilterCube: %s", errstr);
        return false;
    }

    if (response == nullptr || responseLen <= 1)
        return true;

    rc = tty_read_section(PortFD, response, '\r', timeoutSeconds, &nbytes_read);
    if (rc != TTY_OK)
        rc = tty_read_section(PortFD, response, '\n', timeoutSeconds, &nbytes_read);
    if (rc != TTY_OK)
    {
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

namespace
{
void filtercubeStripField(char *field, int &nbytes_read)
{
    while (nbytes_read > 0)
    {
        char c = field[nbytes_read - 1];
        if (c == 'A' || c == '\r' || c == '\n')
        {
            nbytes_read--;
            field[nbytes_read] = '\0';
        }
        else
            break;
    }
    field[nbytes_read] = '\0';
}

void filtercubeTrimLeft(char *s)
{
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if (p != s)
        std::memmove(s, p, std::strlen(p) + 1);
}
}

bool RasaFilterCube::readCurrentFilterFromStatus(int &position)
{
    int rc = -1;
    int nbytes_read = 0;
    char field[64] = {0};

    if (PortFD < 0)
        return false;

    constexpr int maxResync = 32;

    for (int skip = 0; skip < maxResync; ++skip)
    {
        if ((rc = tty_read_section(PortFD, field, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
            return false;
        filtercubeStripField(field, nbytes_read);

        filtercubeTrimLeft(field);
        if (strncasecmp(field, FILTERCUBE_MODEL, std::strlen(FILTERCUBE_MODEL)) == 0)
        {

            if ((rc = tty_read_section(PortFD, field, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
                return false;

            if ((rc = tty_read_section(PortFD, field, 'A', FILTERCUBE_STATUS_TIMEOUT, &nbytes_read)) != TTY_OK)
                return false;
            filtercubeStripField(field, nbytes_read);

            int current = std::atoi(field);
            LOGF_DEBUG("status: position field='%s' -> %d", field, current);
            if (current >= 1 && current <= FILTERCUBE_MAX_FILTERS)
            {
                position = current;
                return true;
            }
            return false;
        }
    }

    return false;
}

