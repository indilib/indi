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
#include "indireceiver.h"

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

Receiver::Receiver()
{
}

Receiver::~Receiver()
{
}

bool Receiver::initProperties()
{
    // Receiver Info
    IUFillNumber(&ReceiverSettingsN[RECEIVER_GAIN], "RECEIVER_GAIN", "Gain", "%16.2f", 1, 4, 1, 1);
    IUFillNumber(&ReceiverSettingsN[RECEIVER_FREQUENCY], "RECEIVER_FREQUENCY", "Frequency", "%16.2f", 1, 4, 1, 1);
    IUFillNumber(&ReceiverSettingsN[RECEIVER_BANDWIDTH], "RECEIVER_BANDWIDTH", "Bandwidth", "%16.2f", 1, 4, 1, 1);
    IUFillNumber(&ReceiverSettingsN[RECEIVER_BITSPERSAMPLE], "RECEIVER_BITSPERSAMPLE", "Bits per sample", "%16.2f", 1, 4, 1, 1);
    IUFillNumber(&ReceiverSettingsN[RECEIVER_SAMPLERATE], "RECEIVER_SAMPLERATE", "Sampling rate", "%16.2f", 1, 4, 1, 1);
    IUFillNumber(&ReceiverSettingsN[RECEIVER_ANTENNA], "RECEIVER_ANTENNA", "Antenna", "%16.2f", 1, 4, 1, 1);
    IUFillNumberVector(&ReceiverSettingsNP, ReceiverSettingsN, 6, getDeviceName(), "RECEIVER_SETTINGS", "Receiver Settings", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    setDriverInterface(SPECTROGRAPH_INTERFACE);

    return SensorInterface::initProperties();
}

void Receiver::ISGetProperties(const char *dev)
{
    processProperties(dev);
}

bool Receiver::updateProperties()
{
    if (isConnected())
    {
        defineProperty(&ReceiverSettingsNP);

        if (HasCooler())
            defineProperty(&TemperatureNP);
    }
    else
    {
        deleteProperty(ReceiverSettingsNP.name);

        if (HasCooler())
            deleteProperty(TemperatureNP.name);
    }
    return SensorInterface::updateProperties();
}

bool Receiver::ISSnoopDevice(XMLEle *root)
{
    return processSnoopDevice(root);
}

bool Receiver::ISNewText(const char *dev, const char *name, char *values[], char *names[], int n)
{
    return processText(dev, name, values, names, n);
}

bool Receiver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, ReceiverSettingsNP.name)) {
        IDSetNumber(&ReceiverSettingsNP, nullptr);
    }
    if (dev && !strcmp(dev, getDeviceName()) && !strcmp(name, ReceiverSettingsNP.name)) {
        IDSetNumber(&ReceiverSettingsNP, nullptr);
    }
    return processNumber(dev, name, values, names, n);
}

bool Receiver::ISNewSwitch(const char *dev, const char *name, ISState *values, char *names[], int n)
{
    return processSwitch(dev, name, values, names, n);
}

bool Receiver::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
           char *formats[], char *names[], int n)
{
    return processBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void Receiver::setBandwidth(double bandwidth)
{
    Bandwidth = bandwidth;

    ReceiverSettingsN[Receiver::RECEIVER_BANDWIDTH].value = bandwidth;

    IDSetNumber(&ReceiverSettingsNP, nullptr);
}

void Receiver::setSampleRate(double sr)
{
    SampleRate = sr;

    ReceiverSettingsN[Receiver::RECEIVER_SAMPLERATE].value = sr;

    IDSetNumber(&ReceiverSettingsNP, nullptr);
}

void Receiver::setGain(double gain)
{
    Gain = gain;

    ReceiverSettingsN[Receiver::RECEIVER_GAIN].value = gain;

    IDSetNumber(&ReceiverSettingsNP, nullptr);
}

void Receiver::setFrequency(double freq)
{
    Frequency = freq;

    ReceiverSettingsN[Receiver::RECEIVER_FREQUENCY].value = freq;

    IDSetNumber(&ReceiverSettingsNP, nullptr);
}

void Receiver::SetReceiverCapability(uint32_t cap)
{
    SetCapability(cap);
    setDriverInterface(getDriverInterface());
}

bool Receiver::StartIntegration(double duration)
{
    INDI_UNUSED(duration);
    DEBUGF(Logger::DBG_WARNING, "Receiver::StartIntegration %4.2f -  Should never get here", duration);
    return false;
}

void Receiver::setMinMaxStep(const char *property, const char *element, double min, double max, double step,
                                   bool sendToClient)
{
    INumberVectorProperty *vp = nullptr;

    if (!strcmp(property, ReceiverSettingsNP.name)) {
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

void Receiver::addFITSKeywords(fitsfile *fptr, uint8_t* buf, int len)
{
    char fitsString[MAXINDILABEL];
    int status = 0;

    // RECEIVER
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
