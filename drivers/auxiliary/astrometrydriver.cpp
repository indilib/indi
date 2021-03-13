/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

  INDI Astrometry.net Driver

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

#include "astrometrydriver.h"

#include <memory>

#include <cerrno>
#include <cstring>

#include <zlib.h>

// We declare an auto pointer to AstrometryDriver.
std::unique_ptr<AstrometryDriver> astrometry(new AstrometryDriver());

void ISGetProperties(const char *dev)
{
    astrometry->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    astrometry->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    astrometry->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    astrometry->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    astrometry->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle *root)
{
    astrometry->ISSnoopDevice(root);
}

AstrometryDriver::AstrometryDriver()
{
    setVersion(1, 0);
}

bool AstrometryDriver::initProperties()
{
    INDI::DefaultDevice::initProperties();

    /**********************************************/
    /**************** Astrometry ******************/
    /**********************************************/

    // Solver Enable/Disable
    SolverSP[SOLVER_ENABLE].fill("ASTROMETRY_SOLVER_ENABLE", "Enable", ISS_OFF);
    SolverSP[SOLVER_DISABLE].fill("ASTROMETRY_SOLVER_DISABLE", "Disable", ISS_ON);
    SolverSP.fill(getDeviceName(), "ASTROMETRY_SOLVER", "Solver", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Solver Settings
    SolverSettingsTP[ASTROMETRY_SETTINGS_BINARY].fill("ASTROMETRY_SETTINGS_BINARY", "Solver",
               "/usr/bin/solve-field");
    SolverSettingsTP[ASTROMETRY_SETTINGS_OPTIONS].fill("ASTROMETRY_SETTINGS_OPTIONS", "Options",
               "--no-verify --no-plots --no-fits2fits --resort --downsample 2 -O");
    SolverSettingsTP.fill(getDeviceName(), "ASTROMETRY_SETTINGS", "Settings",
                     MAIN_CONTROL_TAB, IP_WO, 0, IPS_IDLE);

    // Solver Results
    SolverResultNP[ASTROMETRY_RESULTS_PIXSCALE].fill("ASTROMETRY_RESULTS_PIXSCALE", "Pixscale (arcsec/pixel)",
                 "%g", 0, 10000, 1, 0);
    SolverResultNP[ASTROMETRY_RESULTS_ORIENTATION].fill("ASTROMETRY_RESULTS_ORIENTATION",
                 "Orientation (E of N) °", "%g", -360, 360, 1, 0);
    SolverResultNP[ASTROMETRY_RESULTS_RA].fill("ASTROMETRY_RESULTS_RA", "RA (J2000)", "%g", 0, 24, 1, 0);
    SolverResultNP[ASTROMETRY_RESULTS_DE].fill("ASTROMETRY_RESULTS_DE", "DE (J2000)", "%g", -90, 90, 1, 0);
    SolverResultNP[ASTROMETRY_RESULTS_PARITY].fill("ASTROMETRY_RESULTS_PARITY", "Parity", "%g", -1, 1, 1, 0);
    SolverResultNP.fill(getDeviceName(), "ASTROMETRY_RESULTS", "Results",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Solver Data Blob
    SolverDataBP[0].fill("ASTROMETRY_DATA_BLOB", "Image", "");
    SolverDataBP.fill(getDeviceName(), "ASTROMETRY_DATA", "Upload", MAIN_CONTROL_TAB,
                     IP_WO, 60, IPS_IDLE);

    /**********************************************/
    /**************** Snooping ********************/
    /**********************************************/

    // Snooped Devices
    ActiveDeviceTP[0].fill("ACTIVE_CCD", "CCD", "CCD Simulator");
    ActiveDeviceTP.fill(getDeviceName(), "ACTIVE_DEVICES", "Snoop devices", OPTIONS_TAB,
                     IP_RW, 60, IPS_IDLE);

    // Primary CCD Chip Data Blob
    CCDDataBP[0].fill("CCD1", "Image", "");
    CCDDataBP.fill(ActiveDeviceTP[0].getText(), "CCD1", "Image Data", "Image Info", IP_RO, 60,
                     IPS_IDLE);

    IDSnoopDevice(ActiveDeviceTP[0].getText(), "CCD1");
    IDSnoopBLOBs(ActiveDeviceTP[0].getText(), "CCD1", B_ONLY);

    addDebugControl();

    setDriverInterface(AUX_INTERFACE);

    return true;
}

void AstrometryDriver::ISGetProperties(const char *dev)
{
    DefaultDevice::ISGetProperties(dev);

    defineProperty(ActiveDeviceTP);
    loadConfig(true, "ACTIVE_DEVICES");
}

bool AstrometryDriver::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(SolverSP);
        defineProperty(SolverSettingsTP);
        defineProperty(SolverDataBP);
    }
    else
    {
        if (SolverSP[0].getState() == ISS_ON)
        {
            deleteProperty(SolverResultNP.getName());
        }
        deleteProperty(SolverSP.getName());
        deleteProperty(SolverSettingsTP.getName());
        deleteProperty(SolverDataBP.getName());
    }

    return true;
}

const char *AstrometryDriver::getDefaultName()
{
    return (const char *)"Astrometry";
}

bool AstrometryDriver::Connect()
{
    return true;
}

bool AstrometryDriver::Disconnect()
{
    return true;
}

bool AstrometryDriver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool AstrometryDriver::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                 char *formats[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (SolverDataBP.isNameMatch(name))
        {
            SolverDataBP.setState(IPS_OK);
            SolverDataBP.apply();

            // If the client explicitly uploaded the data then we solve it.
            if (SolverSP[SOLVER_ENABLE].getState() == ISS_OFF)
            {
                SolverSP[SOLVER_ENABLE].setState(ISS_ON);
                SolverSP[SOLVER_DISABLE].setState(ISS_OFF);
                SolverSP.setState(IPS_BUSY);
                LOG_INFO("Astrometry solver is enabled.");
                defineProperty(SolverResultNP);
            }

            processBLOB(reinterpret_cast<uint8_t *>(blobs[0]), static_cast<uint32_t>(sizes[0]),
                        static_cast<uint32_t>(blobsizes[0]));

            return true;
        }
    }

    return INDI::DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool AstrometryDriver::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if (ActiveDeviceTP.isNameMatch(name))
        {
            ActiveDeviceTP.setState(IPS_OK);
            ActiveDeviceTP.update(texts, names, n);
            ActiveDeviceTP.apply();

            // Update the property name!
            CCDDataBP.setDeviceName(ActiveDeviceTP[0].getText());
            IDSnoopDevice(ActiveDeviceTP[0].getText(), "CCD1");
            IDSnoopBLOBs(ActiveDeviceTP[0].getText(), "CCD1", B_ONLY);

            //  We processed this one, so, tell the world we did it
            return true;
        }

        if (SolverSettingsTP.isNameMatch(name))
        {
            SolverSettingsTP.update(texts, names, n);
            SolverSettingsTP.setState(IPS_OK);
            SolverSettingsTP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool AstrometryDriver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Astrometry Enable/Disable
        if (SolverSP.isNameMatch(name))
        {
            pthread_mutex_lock(&lock);

            SolverSP.update(states, names, n);
            SolverSP.setState(IPS_OK);

            if (SolverSP[SOLVER_ENABLE].getState() == ISS_ON)
            {
                LOG_INFO("Astrometry solver is enabled.");
                defineProperty(SolverResultNP);
            }
            else
            {
                LOG_INFO("Astrometry solver is disabled.");
                deleteProperty(SolverResultNP.getName());
            }

            SolverSP.apply();

            pthread_mutex_unlock(&lock);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool AstrometryDriver::ISSnoopDevice(XMLEle *root)
{
    if (SolverSP[SOLVER_ENABLE].getState() == ISS_ON && IUSnoopBLOB(root, CCDDataBP.getBLOB()) == 0) // #PS: refactor needed
    {
        processBLOB(reinterpret_cast<uint8_t *>(CCDDataBP[0].blob), static_cast<uint32_t>(CCDDataBP[0].size),
                    static_cast<uint32_t>(CCDDataBP[0].bloblen));
        return true;
    }

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool AstrometryDriver::saveConfigItems(FILE *fp)
{
    ActiveDeviceTP.save(fp);
    SolverSettingsTP.save(fp);
    return true;
}

bool AstrometryDriver::processBLOB(uint8_t *data, uint32_t size, uint32_t len)
{
    FILE *fp = nullptr;
    char imageFileName[MAXRBUF];

    uint8_t *processedData = data;

    // If size != len then we have compressed buffer
    if (size != len)
    {
        uint8_t *dataBuffer = new uint8_t[size];
        uLongf destLen      = size;

        if (dataBuffer == nullptr)
        {
            LOG_DEBUG("Unable to allocate memory for data buffer");
            return false;
        }

        int r = uncompress(dataBuffer, &destLen, data, len);
        if (r != Z_OK)
        {
            LOGF_ERROR("Astrometry compression error: %d", r);
            delete[] dataBuffer;
            return false;
        }

        if (destLen != size)
        {
            LOGF_WARN("Discrepency between uncompressed data size %ld and expected size %ld",
                   size, destLen);
        }

        processedData = dataBuffer;
    }

    strncpy(imageFileName, "/tmp/ccdsolver.fits", MAXRBUF);

    fp = fopen(imageFileName, "w");
    if (fp == nullptr)
    {
        LOGF_ERROR("Unable to save image file (%s). %s", imageFileName, strerror(errno));
        if (size != len)
            delete[] processedData;

        return false;
    }

    int n = 0;
    for (uint32_t nr = 0; nr < size; nr += n)
        n = fwrite(processedData + nr, 1, size - nr, fp);

    fclose(fp);

    // Do not forget to release uncompressed buffer
    if (size != len)
        delete[] processedData;

    pthread_mutex_lock(&lock);
    SolverSP.setState(IPS_BUSY);
    LOG_INFO("Solving image...");
    SolverSP.apply();
    pthread_mutex_unlock(&lock);

    int result = pthread_create(&solverThread, nullptr, &AstrometryDriver::runSolverHelper, this);

    if (result != 0)
    {
        SolverSP.setState(IPS_ALERT);
        LOGF_INFO("Failed to create solver thread: %s", strerror(errno));
        SolverSP.apply();
    }

    return true;
}

void *AstrometryDriver::runSolverHelper(void *context)
{
    (static_cast<AstrometryDriver *>(context))->runSolver();
    return nullptr;
}

void AstrometryDriver::runSolver()
{
    char cmd[MAXRBUF]={0}, line[256]={0}, parity_str[8]={0};
    float ra = -1000, dec = -1000, angle = -1000, pixscale = -1000, parity = 0;
    snprintf(cmd, MAXRBUF, "%s %s -W /tmp/solution.wcs /tmp/ccdsolver.fits",
             SolverSettingsTP[ASTROMETRY_SETTINGS_BINARY].getText(), SolverSettingsTP[ASTROMETRY_SETTINGS_OPTIONS].getText());

    LOGF_DEBUG("%s", cmd);
    FILE *handle = popen(cmd, "r");
    if (handle == nullptr)
    {
        LOGF_DEBUG("Failed to run solver: %s", strerror(errno));
        pthread_mutex_lock(&lock);
        SolverSP.setState(IPS_ALERT);
        SolverSP.apply();
        pthread_mutex_unlock(&lock);
        return;
    }

    while (fgets(line, sizeof(line), handle) != nullptr)
    {
        LOGF_DEBUG("%s", line);

        sscanf(line, "Field rotation angle: up is %f", &angle);
        sscanf(line, "Field center: (RA,Dec) = (%f,%f)", &ra, &dec);
        sscanf(line, "Field parity: %s", parity_str);
        sscanf(line, "%*[^p]pixel scale %f", &pixscale);

        if (strcmp(parity_str, "pos") == 0)
            parity = 1;
        else if (strcmp(parity_str, "neg") == 0)
            parity = -1;

        if (ra != -1000 && dec != -1000 && angle != -1000 && pixscale != -1000)
        {
            // Pixscale is arcsec/pixel. Astrometry result is in arcmin
            SolverResultNP[ASTROMETRY_RESULTS_PIXSCALE].setValue(pixscale);
            // Astrometry.net angle, E of N
            SolverResultNP[ASTROMETRY_RESULTS_ORIENTATION].setValue(angle);
            // Astrometry.net J2000 RA in degrees
            SolverResultNP[ASTROMETRY_RESULTS_RA].setValue(ra);
            // Astrometry.net J2000 DEC in degrees
            SolverResultNP[ASTROMETRY_RESULTS_DE].setValue(dec);
            // Astrometry.net parity
            SolverResultNP[ASTROMETRY_RESULTS_PARITY].setValue(parity);

            SolverResultNP.setState(IPS_OK);
            SolverResultNP.apply();

            pthread_mutex_lock(&lock);
            SolverSP.setState(IPS_OK);
            SolverSP.apply();
            pthread_mutex_unlock(&lock);

            fclose(handle);
            LOG_INFO("Solver complete.");
            return;
        }

        pthread_mutex_lock(&lock);
        if (SolverSP[SOLVER_DISABLE].getState() == ISS_ON)
        {
            SolverSP.setState(IPS_IDLE);
            SolverSP.apply();
            pthread_mutex_unlock(&lock);
            fclose(handle);
            LOG_INFO("Solver canceled.");
            return;
        }
        pthread_mutex_unlock(&lock);
    }

    fclose(handle);

    pthread_mutex_lock(&lock);
    SolverSP.setState(IPS_ALERT);
    SolverSP.apply();
    LOG_INFO("Solver failed.");
    pthread_mutex_unlock(&lock);

    pthread_exit(nullptr);
}
