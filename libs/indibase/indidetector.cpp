/*******************************************************************************
 Copyright(c) 2010, 2017 Ilia Platone, Jasem Mutlaq. All rights reserved.


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

#include "defaultdevice.h"
#include "indidetector.h"

#include "indicom.h"
#include "locale_compat.h"

#include <fitsio.h>

#include <libnova/julian_day.h>
#include <libnova/ln_types.h>
#include <libnova/precession.h>

#include <regex>

#include <dirent.h>
#include <cerrno>
#include <locale.h>
#include <cstdlib>
#include <zlib.h>
#include <sys/stat.h>

namespace INDI
{

Detector::Detector()
{
    setBPS(sizeof(pulse_t) * sizeof(uint8_t));
    setIntegrationFileExtension("raw");
}

Detector::~Detector()
{
}

bool Detector::initProperties()
{
    // PrimaryDetector Info
    IUFillNumber(&DetectorSettingsN[DETECTOR_RESOLUTION], "DETECTOR_RESOLUTION", "Resolution (ns)", "%16.3f", 0.01, 1.0e+8, 0.01, 1.0e+6);
    IUFillNumber(&DetectorSettingsN[DETECTOR_TRIGGER], "DETECTOR_TRIGGER", "Trigger pulse (%)", "%3.2f", 0.01, 1.0e+15, 0.01, 1.42e+9);
    IUFillNumberVector(&DetectorSettingsNP, DetectorSettingsN, 2, getDeviceName(), "DETECTOR_SETTINGS", "Detector Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    setDriverInterface(DETECTOR_INTERFACE);

    return SensorInterface::initProperties();
}

void Detector::ISGetProperties(const char *dev)
{
    return processProperties(dev);
}

bool Detector::updateProperties()
{
    if (isConnected())
    {
        defineNumber(&DetectorSettingsNP);

        if (HasCooler())
            defineNumber(&TemperatureNP);
    }
    else
    {
        deleteProperty(DetectorSettingsNP.name);

        if (HasCooler())
            deleteProperty(TemperatureNP.name);
    }
    return SensorInterface::updateProperties();
}

bool Detector::ISSnoopDevice(XMLEle *root)
{
    return processSnoopDevice(root);
}

bool Detector::ISNewText(const char *dev, const char *name, char *values[], char *names[], int n)
{
    return processText(dev, name, values, names, n);
}

bool Detector::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, DetectorSettingsNP.name)) {
        IDSetNumber(&DetectorSettingsNP, nullptr);
    }
    return processNumber(dev, name, values, names, n);
}

bool Detector::ISNewSwitch(const char *dev, const char *name, ISState *values, char *names[], int n)
{
    return processSwitch(dev, name, values, names, n);
}

bool Detector::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
           char *formats[], char *names[], int n)
{
    return processBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void Detector::setTriggerLevel(double level)
{
    TriggerLevel = level;

    DetectorSettingsN[Detector::DETECTOR_TRIGGER].value = level;

    IDSetNumber(&DetectorSettingsNP, nullptr);
}

void Detector::setResolution(double res)
{
    Resolution = res;

    DetectorSettingsN[Detector::DETECTOR_RESOLUTION].value = res;

    IDSetNumber(&DetectorSettingsNP, nullptr);
}

void Detector::SetDetectorCapability(uint32_t cap)
{
    SetCapability(cap);
    setDriverInterface(getDriverInterface());
}

bool Detector::StartIntegration(double duration)
{
    INDI_UNUSED(duration);
    DEBUGF(Logger::DBG_WARNING, "Detector::StartIntegration %4.2f -  Should never get here", duration);
    return false;
}

void Detector::setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                                   bool sendToClient)
{
    INumberVectorProperty *vp = nullptr;

    if (!strcmp(property, DetectorSettingsNP.name)) {
        vp = &FramedIntegrationNP;

        INumber *np = IUFindNumber(vp, element);
        if (np)
        {
            np->min  = min;
            np->max  = max;
            np->step = step;

            if (sendToClient)
                IUUpdateMinMax(vp);
        }
    }
    INDI::SensorInterface::setMinMaxStep(property, element, min, max, step, sendToClient);
}

void Detector::addFITSKeywords(fitsfile *fptr, uint8_t* buf, int len)
{
    char fitsString[MAXINDILABEL];
    int status = 0;

    // SPECTROGRAPH
    sprintf(fitsString, "%lf", getResolution());
    fits_update_key_s(fptr, TSTRING, "RESOLUTI", fitsString, "Timing resolution", &status);

    sprintf(fitsString, "%lf", getTriggerLevel());
    fits_update_key_s(fptr, TSTRING, "TRIGGER", fitsString, "Trigger level", &status);

    SensorInterface::addFITSKeywords(fptr, buf, len);
}
}

