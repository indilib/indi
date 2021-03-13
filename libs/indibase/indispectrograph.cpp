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
    SpectrographSettingsNP[SPECTROGRAPH_SAMPLERATE].fill("SPECTROGRAPH_SAMPLERATE", "Sample rate (SPS)", "%16.2f", 0.01, 1.0e+8, 0.01, 1.0e+6);
    SpectrographSettingsNP[SPECTROGRAPH_FREQUENCY].fill("SPECTROGRAPH_FREQUENCY", "Center frequency (Hz)", "%16.2f", 0.01, 1.0e+15, 0.01, 1.42e+9);
    SpectrographSettingsNP[SPECTROGRAPH_BITSPERSAMPLE].fill("SPECTROGRAPH_BITSPERSAMPLE", "Bits per sample", "%3.0f", -64, 64, 8, 8);
    SpectrographSettingsNP[SPECTROGRAPH_BANDWIDTH].fill("SPECTROGRAPH_BANDWIDTH", "Bandwidth (Hz)", "%16.2f", 0.01, 1.0e+8, 0.01, 1.0e+3);
    SpectrographSettingsNP[SPECTROGRAPH_GAIN].fill("SPECTROGRAPH_GAIN", "Gain", "%3.2f", 0.01, 255.0, 0.01, 1.0);
    SpectrographSettingsNP[SPECTROGRAPH_ANTENNA].fill("SPECTROGRAPH_ANTENNA", "Antenna", "%16.2f", 1, 4, 1, 1);
    SpectrographSettingsNP.fill(getDeviceName(), "SPECTROGRAPH_SETTINGS", "Spectrograph Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

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
        defineProperty(SpectrographSettingsNP);

        if (HasCooler())
            defineProperty(TemperatureNP);
    }
    else
    {
        deleteProperty(SpectrographSettingsNP.getName());

        if (HasCooler())
            deleteProperty(TemperatureNP.getName());
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
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, SpectrographSettingsNP.getName())) {
        SpectrographSettingsNP.apply();
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

    SpectrographSettingsNP[Spectrograph::SPECTROGRAPH_SAMPLERATE].setValue(sr);

    SpectrographSettingsNP.apply();
}

void Spectrograph::setBandwidth(double bw)
{
    Bandwidth = bw;

    SpectrographSettingsNP[Spectrograph::SPECTROGRAPH_BANDWIDTH].setValue(bw);

    SpectrographSettingsNP.apply();
}

void Spectrograph::setGain(double gain)
{
    Gain = gain;

    SpectrographSettingsNP[Spectrograph::SPECTROGRAPH_GAIN].setValue(gain);

    SpectrographSettingsNP.apply();
}

void Spectrograph::setFrequency(double freq)
{
    Frequency = freq;

    SpectrographSettingsNP[Spectrograph::SPECTROGRAPH_FREQUENCY].setValue(freq);

    SpectrographSettingsNP.apply();
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

    if (SpectrographSettingsNP.isNameMatch(property)) {
        vp = FramedIntegrationNP.getNumber(); // #PS: refactor needed

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
