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
#include "indispectrograph.h"

#include "indicom.h"
#include "stream/streammanager.h"
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

Spectrograph::Spectrograph()
{
}

Spectrograph::~Spectrograph()
{
}

bool Spectrograph::initProperties()
{
    // PrimarySpectrograph Info
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_SAMPLERATE], "SPECTROGRAPH_SAMPLERATE", "Sample rate (SPS)", "%16.2f", 0.01, 1.0e+8, 0.01, 1.0e+6);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_FREQUENCY], "SPECTROGRAPH_FREQUENCY", "Center frequency (Hz)", "%16.2f", 0.01, 1.0e+15, 0.01, 1.42e+9);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_BITSPERSAMPLE], "SPECTROGRAPH_BITSPERSAMPLE", "Bits per sample", "%3.0f", -64, 64, 8, 8);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_BANDWIDTH], "SPECTROGRAPH_BANDWIDTH", "Bandwidth (Hz)", "%16.2f", 0.01, 1.0e+8, 0.01, 1.0e+3);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_GAIN], "SPECTROGRAPH_GAIN", "Gain", "%3.2f", 0.01, 255.0, 0.01, 1.0);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_ANTENNA], "SPECTROGRAPH_ANTENNA", "Antenna", "%16.2f", 1, 4, 1, 1);
    IUFillNumberVector(&SpectrographSettingsNP, SpectrographSettingsN, 6, getDeviceName(), "SPECTROGRAPH_SETTINGS", "Spectrograph Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    setDriverInterface(SPECTROGRAPH_INTERFACE);

    return SensorInterface::initProperties();
}

void Spectrograph::ISGetProperties(const char *dev)
{
    processProperties(dev);
}

bool Spectrograph::updateProperties()
{
    if (isConnected())
    {
        defineNumber(&SpectrographSettingsNP);

        if (HasCooler())
            defineNumber(&TemperatureNP);
    }
    else
    {
        deleteProperty(SpectrographSettingsNP.name);

        if (HasCooler())
            deleteProperty(TemperatureNP.name);
    }
    return SensorInterface::updateProperties();
}

bool Spectrograph::ISSnoopDevice(XMLEle *root)
{
    return processSnoopDevice(root);
}

bool Spectrograph::ISNewText(const char *dev, const char *name, char *values[], char *names[], int n)
{
    return processText(dev, name, values, names, n);
}

bool Spectrograph::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, SpectrographSettingsNP.name)) {
        IDSetNumber(&SpectrographSettingsNP, nullptr);
    }
    return processNumber(dev, name, values, names, n);
}

bool Spectrograph::ISNewSwitch(const char *dev, const char *name, ISState *values, char *names[], int n)
{
    return processSwitch(dev, name, values, names, n);
}

bool Spectrograph::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
           char *formats[], char *names[], int n)
{
    return processBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void Spectrograph::setSampleRate(double sr)
{
    Samplerate = sr;

    SpectrographSettingsN[Spectrograph::SPECTROGRAPH_SAMPLERATE].value = sr;

    IDSetNumber(&SpectrographSettingsNP, nullptr);
}

void Spectrograph::setBandwidth(double bw)
{
    Bandwidth = bw;

    SpectrographSettingsN[Spectrograph::SPECTROGRAPH_BANDWIDTH].value = bw;

    IDSetNumber(&SpectrographSettingsNP, nullptr);
}

void Spectrograph::setGain(double gain)
{
    Gain = gain;

    SpectrographSettingsN[Spectrograph::SPECTROGRAPH_GAIN].value = gain;

    IDSetNumber(&SpectrographSettingsNP, nullptr);
}

void Spectrograph::setFrequency(double freq)
{
    Frequency = freq;

    SpectrographSettingsN[Spectrograph::SPECTROGRAPH_FREQUENCY].value = freq;

    IDSetNumber(&SpectrographSettingsNP, nullptr);
}

void Spectrograph::SetSpectrographCapability(uint32_t cap)
{
    SetCapability(cap);
    setDriverInterface(getDriverInterface());
}

bool Spectrograph::StartIntegration(double duration)
{
    INDI_UNUSED(duration);
    DEBUGF(Logger::DBG_WARNING, "Spectrograph::StartIntegration %4.2f -  Should never get here", duration);
    return false;
}

void Spectrograph::setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                                   bool sendToClient)
{
    INumberVectorProperty *vp = nullptr;

    if (!strcmp(property, SpectrographSettingsNP.name)) {
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

void Spectrograph::addFITSKeywords(fitsfile *fptr, uint8_t* buf, int len)
{
    char fitsString[MAXINDILABEL];
    int status = 0;

    // SPECTROGRAPH
    sprintf(fitsString, "%d", getBPS());
    fits_update_key_s(fptr, TSTRING, "BPS", fitsString, "Bits per sample", &status);

    sprintf(fitsString, "%lf", getBandwidth());
    fits_update_key_s(fptr, TSTRING, "BANDWIDT", fitsString, "Bandwidth", &status);

    sprintf(fitsString, "%lf", getFrequency());
    fits_update_key_s(fptr, TSTRING, "FREQ", fitsString, "Center Frequency", &status);

    sprintf(fitsString, "%lf", getSampleRate());
    fits_update_key_s(fptr, TSTRING, "SRATE", fitsString, "Sampling Rate", &status);

    sprintf(fitsString, "%lf", getGain());
    fits_update_key_s(fptr, TSTRING, "GAIN", fitsString, "Gain", &status);

    SensorInterface::addFITSKeywords(fptr, buf, len);
}
}
