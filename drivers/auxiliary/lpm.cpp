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

void ISGetProperties(const char *dev)
{
    lpm->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    lpm->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    lpm->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    lpm->ISNewNumber(dev, name, values, names, n);
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
    lpm->ISSnoopDevice(root);
}

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
    AverageReadingNP[0].fill("SKY_BRIGHTNESS", "Quality (mag/arcsec^2)", "%6.2f", -20, 30, 0, 0);
    AverageReadingNP[1].fill("AVG_SKY_BRIGHTNESS", "Avg. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);
    AverageReadingNP[2].fill("MIN_SKY_BRIGHTNESS", "Min. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);
    AverageReadingNP[3].fill("MAX_SKY_BRIGHTNESS", "Max. Quality (mag/argsec^2)", "%6.2f", -20, 30, 0, 0);
    AverageReadingNP[4].fill("NAKED_EYES_LIMIT", "NELM (V mags)", "%6.2f", -20, 30, 0, 0);

    AverageReadingNP.fill(getDeviceName(), "SKY_QUALITY", "Readings",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // add reset button for SQ-Measurements
    ResetBP[0].fill("RESET_BUTTON", "Reset", ISS_OFF);
    ResetBP.fill(getDeviceName(), "RESET_READINGS", "",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    SaveBP[SAVE_READINGS].fill("SAVE_BUTTON", "Save Readings", ISS_OFF);
    SaveBP[DISCARD_READINGS].fill("DISCARD_BUTTON", "Discard Readings", ISS_OFF);
    SaveBP.fill(getDeviceName(), "SAVE_READINGS", "",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // LPM-Readings-Log
    std::string defaultDirectory = std::string(getenv("HOME")) + std::string("/lpm");
    RecordFileTP[0].fill("RECORD_FILE_DIR", "Dir.", defaultDirectory.data());
    RecordFileTP[1].fill("RECORD_FILE_NAME", "Name", "lpmlog.txt");
    RecordFileTP.fill(getDeviceName(), "RECORD_FILE", "Record File",
                     MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);
    snprintf(filename, 2048, "%s/%s", RecordFileTP[0].getText(), RecordFileTP[1].getText());

    // Unit Info
    UnitInfoNP[0].fill("Calibdata", "", "%6.2f", -20, 30, 0, 0);
    UnitInfoNP.fill(getDeviceName(), "Unit Info", "",
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
        defineProperty(AverageReadingNP);
        defineProperty(UnitInfoNP);
        defineProperty(ResetBP);
        defineProperty(RecordFileTP);
        defineProperty(SaveBP);
    }
    else
    {
        deleteProperty(AverageReadingNP.getName());
        deleteProperty(UnitInfoNP.getName());
        deleteProperty(RecordFileTP.getName());
        deleteProperty(ResetBP.getName());
        deleteProperty(SaveBP.getName());
    }

    return true;
}

bool LPM::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (RecordFileTP.isNameMatch(name))
        {
            RecordFileTP.update(texts, names, n);
            RecordFileTP.setState(IPS_OK);
            RecordFileTP.apply();
            snprintf(filename, 2048, "%s/%s", RecordFileTP[0].getText(), RecordFileTP[1].getText());
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
        if (ResetBP.isNameMatch(name))
        {
            ResetBP.update(states, names, n);
            ResetBP[0].setState(ISS_OFF);
            ResetBP.setState(IPS_OK);
            ResetBP.apply();

            AverageReadingNP[0].setValue(0);
            AverageReadingNP[1].setValue(0);
            AverageReadingNP[2].setValue(0);
            AverageReadingNP[3].setValue(0);

            sumSQ = 0;
            count = 0;
            return true;
        } else if (SaveBP.isNameMatch(name))
        {
            SaveBP.update(states, names, n);

            if (SaveBP[SAVE_READINGS].getState() == ISS_ON)
            {
                LOGF_INFO("Save readings to %s", filename);
                if (fp == nullptr) {
                    openFilePtr();
                }
                SaveBP.setState(IPS_OK);
            } else {
                LOG_INFO("Discard readings");
                if (fp != nullptr) {
                    LOG_DEBUG("close file pointer");
                    fclose(fp);
                    fp = nullptr;
                } else {
                    LOG_WARN("no file open!");
                }
                SaveBP.setState(IPS_IDLE);
            }

            SaveBP.apply();
            return true;
        }

    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

void LPM::openFilePtr()
{
    LOG_DEBUG("open file pointer");
    mkdir(RecordFileTP[0].getText(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
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

        AverageReadingNP[0].setValue(mpsas);
        sumSQ += mpsas;

        if (count > 1)
        {
            AverageReadingNP[1].setValue(sumSQ / count);
            if (mpsas < AverageReadingNP[2].getValue()) {
                AverageReadingNP[2].setValue(mpsas);
            }
            if (mpsas > AverageReadingNP[3].getValue()) {
                AverageReadingNP[3].setValue(mpsas);
            }
        } else {
            AverageReadingNP[2].setValue(mpsas);
            AverageReadingNP[3].setValue(mpsas);
        }
        //NELM (see http://unihedron.com/projects/darksky/NELM2BCalc.html)
        AverageReadingNP[4].setValue(7.93-5*log(pow(10,(4.316-(mpsas/5)))+1)/log(10));
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

    UnitInfoNP[0].setValue(calib);

    return true;
}

void LPM::TimerHit()
{
    if (!isConnected())
        return;

    bool rc = getReadings();

    AverageReadingNP.setState(rc ? IPS_OK : IPS_ALERT);
    AverageReadingNP.apply();

    SetTimer(getCurrentPollingPeriod());
}
