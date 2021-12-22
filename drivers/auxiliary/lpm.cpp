/*******************************************************************************
  Copyright(c) 2019 Christian Liska. All rights reserved.

  INDI Astromechanic Light Pollution Meter Driver
  https://www.astromechanics.org/lpm.html

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/
#include "lpm.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>

// We declare an auto pointer to LPM.
static std::unique_ptr<LPM> lpm(new LPM());

#define UNIT_TAB    "Unit"

LPM::LPM()
{
    setVersion(0, 1);
}

LPM::~LPM()
{
    if (fp) {
        fclose(fp);
    }
}

bool LPM::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Readings from device
    IUFillNumber(&AverageReadingN[0], "SKY_BRIGHTNESS", "Quality (mag/arcsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[1], "AVG_SKY_BRIGHTNESS", "Avg. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[2], "MIN_SKY_BRIGHTNESS", "Min. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[3], "MAX_SKY_BRIGHTNESS", "Max. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);
    IUFillNumber(&AverageReadingN[4], "NAKED_EYES_LIMIT", "NELM (V mags)", "%6.2f", -20, 30, 0, 0);

    IUFillNumberVector(&AverageReadingNP, AverageReadingN, 5, getDeviceName(), "SKY_QUALITY", "Readings",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // add reset button for SQ-Measurements
    IUFillSwitch(&ResetB[0], "RESET_BUTTON", "Reset", ISS_OFF);
    IUFillSwitchVector(&ResetBP, ResetB, 1, getDeviceName(), "RESET_READINGS", "",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillSwitch(&SaveB[SAVE_READINGS], "SAVE_BUTTON", "Save Readings", ISS_OFF);
    IUFillSwitch(&SaveB[DISCARD_READINGS], "DISCARD_BUTTON", "Discard Readings", ISS_OFF);
    IUFillSwitchVector(&SaveBP, SaveB, 2, getDeviceName(), "SAVE_READINGS", "",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // LPM-Readings-Log
    std::string defaultDirectory = std::string(getenv("HOME")) + std::string("/lpm");
    IUFillText(&RecordFileT[0], "RECORD_FILE_DIR", "Dir.", defaultDirectory.data());
    IUFillText(&RecordFileT[1], "RECORD_FILE_NAME", "Name", "lpmlog.txt");
    IUFillTextVector(&RecordFileTP, RecordFileT, NARRAY(RecordFileT), getDeviceName(), "RECORD_FILE", "Record File",
                     MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);
    snprintf(filename, 2048, "%s/%s", RecordFileT[0].text, RecordFileT[1].text);

    // Unit Info
    IUFillNumber(&UnitInfoN[0], "Calibdata", "", "%6.2f", -20, 30, 0, 0);
    IUFillNumberVector(&UnitInfoNP, UnitInfoN, 1, getDeviceName(), "Unit Info", "",
                       UNIT_TAB, IP_RO, 0, IPS_IDLE);

    if (lpmConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]() { return getDeviceInfo(); });
        serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
        registerConnection(serialConnection);
    }

    addDebugControl();
    addPollPeriodControl();

    return true;
}

bool LPM::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&AverageReadingNP);
        defineProperty(&UnitInfoNP);
        defineProperty(&ResetBP);
        defineProperty(&RecordFileTP);
        defineProperty(&SaveBP);
    }
    else
    {
        deleteProperty(AverageReadingNP.name);
        deleteProperty(UnitInfoNP.name);
        deleteProperty(RecordFileTP.name);
        deleteProperty(ResetBP.name);
        deleteProperty(SaveBP.name);
    }

    return true;
}

bool LPM::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, RecordFileTP.name) == 0)
        {
            IUUpdateText(&RecordFileTP, texts, names, n);
            RecordFileTP.s = IPS_OK;
            IDSetText(&RecordFileTP, nullptr);
            snprintf(filename, 2048, "%s/%s", RecordFileT[0].text, RecordFileT[1].text);
            LOGF_INFO("filename changed to %s", filename);
            if (fp != nullptr)
            {
                fclose(fp);
                openFilePtr();
            }
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool LPM::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Reset
        if (!strcmp(name, ResetBP.name))
        {
            IUUpdateSwitch(&ResetBP, states, names, n);
            ResetB[0].s = ISS_OFF;
            ResetBP.s   = IPS_OK;
            IDSetSwitch(&ResetBP, nullptr);

            AverageReadingN[0].value = 0;
            AverageReadingN[1].value = 0;
            AverageReadingN[2].value = 0;
            AverageReadingN[3].value = 0;

            sumSQ = 0;
            count = 0;
            return true;
        } else if (!strcmp(name, SaveBP.name))
        {
            IUUpdateSwitch(&SaveBP, states, names, n);

            if (SaveB[SAVE_READINGS].s == ISS_ON)
            {
                LOGF_INFO("Save readings to %s", filename);
                if (fp == nullptr) {
                    openFilePtr();
                }
                SaveBP.s = IPS_OK;
            } else {
                LOG_INFO("Discard readings");
                if (fp != nullptr) {
                    LOG_DEBUG("close file pointer");
                    fclose(fp);
                    fp = nullptr;
                } else {
                    LOG_WARN("no file open!");
                }
                SaveBP.s = IPS_IDLE;
            }

            IDSetSwitch(&SaveBP, nullptr);
            return true;
        }

    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

void LPM::openFilePtr()
{
    LOG_DEBUG("open file pointer");
    mkdir(RecordFileT[0].text,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    fp = fopen(filename, "a");
}

bool LPM::getReadings()
{
    const char *cmd = "V#";
    char res[32] = {0};
    int nbytes_written = 0;
    int nbytes_read = 0;

    tty_write_string(PortFD, cmd, &nbytes_written);
    if (tty_read_section(PortFD, res, '#', 60000, &nbytes_read) == TTY_OK)
    {
        LOGF_DEBUG("RES (%s)", res);
        float mpsas;
        int rc =
            sscanf(res, "%f#", &mpsas);
        if (rc < 1)
        {
            LOGF_ERROR("Failed to parse input %s", res);
            return false;
        } else 
        {
            if (fp != nullptr) {
                LOG_DEBUG("save reading...");
                fprintf(fp, "%f\t%s\n", mpsas, timestamp());
                fflush(fp);
            }
            count++;
        }

        AverageReadingN[0].value = mpsas;
        sumSQ += mpsas;

        if (count > 1)
        {
            AverageReadingN[1].value = sumSQ / count;
            if (mpsas < AverageReadingN[2].value) {
                AverageReadingN[2].value = mpsas;
            }
            if (mpsas > AverageReadingN[3].value) {
                AverageReadingN[3].value = mpsas;
            }
        } else {
            AverageReadingN[2].value = mpsas;
            AverageReadingN[3].value = mpsas;
        }
        //NELM (see http://unihedron.com/projects/darksky/NELM2BCalc.html)
        AverageReadingN[4].value = 7.93-5*log(pow(10,(4.316-(mpsas/5)))+1)/log(10);
    }
    return true;
}

const char *LPM::getDefaultName()
{
    return "Astromechanics LPM";
}

bool LPM::getDeviceInfo()
{
    const char *cmd = "C#";
    char buffer[5]={0};

    if (getActiveConnection() == serialConnection) {
        PortFD = serialConnection->getPortFD();
    }

    LOGF_DEBUG("CMD: %s", cmd);

    ssize_t written = write(PortFD, cmd, 2);

    if (written < 2)
    {
        LOGF_ERROR("Error getting device info while writing to device: %s", strerror(errno));
        return false;
    }

    ssize_t received = 0;

    while (received < 5)
    {
        ssize_t response = read(PortFD, buffer + received, 5 - received);
        if (response < 0)
        {
            LOGF_ERROR("Error getting device info while reading response: %s", strerror(errno));
            return false;
        }

        received += response;
    }

    if (received < 5)
    {
        LOG_ERROR("Error getting device info");
        return false;
    }

    LOGF_DEBUG("RES: %s", buffer);

    float calib;
    int rc = sscanf(buffer, "%f#", &calib);

    if (rc < 1)
    {
        LOGF_ERROR("Failed to parse input %s", buffer);
        return false;
    }

    UnitInfoN[0].value = calib;

    return true;
}

void LPM::TimerHit()
{
    if (!isConnected())
        return;

    bool rc = getReadings();

    AverageReadingNP.s = rc ? IPS_OK : IPS_ALERT;
    IDSetNumber(&AverageReadingNP, nullptr);

    SetTimer(getCurrentPollingPeriod());
}
