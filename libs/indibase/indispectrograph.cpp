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
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_SAMPLERATE], "SPECTROGRAPH_SAMPLERATE", "Sample rate (SPS)", "%16.2f", 0.01, 1.0e+15, 0.01, 1.0e+6);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_FREQUENCY], "SPECTROGRAPH_FREQUENCY", "Center frequency (Hz)", "%16.2f", 0.01, 1.0e+15, 0.01, 1.42e+9);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_BITSPERSAMPLE], "SPECTROGRAPH_BITSPERSAMPLE", "Bits per sample", "%3.0f", 1, 64, 1, 8);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_BANDWIDTH], "SPECTROGRAPH_BANDWIDTH", "Bandwidth (Hz)", "%16.2f", 0.01, 1.0e+15, 0.01, 1.42e+9);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_GAIN], "SPECTROGRAPH_GAIN", "Gain", "%16.2f", 0.01, 1.0e+15, 0.01, 1.42e+9);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_CHANNEL], "SPECTROGRAPH_CHANNEL", "Channel", "%16.2f", 0.01, 1.0e+15, 0.01, 1.42e+9);
    IUFillNumber(&SpectrographSettingsN[SPECTROGRAPH_ANTENNA], "SPECTROGRAPH_ANTENNA", "Antenna", "%16.2f", 0.01, 1.0e+15, 0.01, 1.42e+9);
    IUFillNumberVector(&SpectrographSettingsNP, SpectrographSettingsN, 7, getDeviceName(), "SPECTROGRAPH_SETTINGS", "Spectrograph Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

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
    // PrimarySensorInterface Info
    if (!strcmp(name, SpectrographSettingsNP.name))
    {
        SpectrographSettingsNP.s = IPS_OK;
        setParams(values[SPECTROGRAPH_SAMPLERATE], values[SPECTROGRAPH_FREQUENCY], values[SPECTROGRAPH_BITSPERSAMPLE], values[SPECTROGRAPH_BANDWIDTH], values[SPECTROGRAPH_GAIN]);
        return true;
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

bool Spectrograph::paramsUpdated(double sr, double freq, double bps, double bw, double gain)
{
    INDI_UNUSED(sr);
    INDI_UNUSED(freq);
    INDI_UNUSED(bw);
    INDI_UNUSED(bps);
    INDI_UNUSED(gain);
    DEBUGF(Logger::DBG_WARNING, "Spectrograph::paramsUpdated %15.0f %15.0f %15.0f -  Should never get here", sr, freq, bps);
    return false;
}

void Spectrograph::setParams(double samplerate, double freq, double bps, double bw, double gain)
{
    setSampleRate(samplerate);
    setFrequency(freq);
    setBandwidth(bw);
    setBPS(bps);
    setGain(gain);
    paramsUpdated(samplerate, freq, bps, bw, gain);
}

}
