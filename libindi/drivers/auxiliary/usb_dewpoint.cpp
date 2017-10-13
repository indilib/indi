/*
    USB_Dewpoint
    Copyright (C) 2017 Jarno Paananen

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "usb_dewpoint.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define USBFOCUSV3_TIMEOUT 3

#define POLLMS 250

#define SRTUS 25000

/***************************** Class USBDewpoint *******************************/

std::unique_ptr<USBDewpoint> usbDewpoint(new USBDewpoint());

void ISGetProperties(const char *dev)
{
    usbDewpoint->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    usbDewpoint->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    usbDewpoint->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    usbDewpoint->ISNewNumber(dev, name, values, names, n);
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
    usbDewpoint->ISSnoopDevice(root);
}

USBDewpoint::USBDewpoint()
{
    setVersion(1, 0);
};


bool USBDewpoint::initProperties()
{
    DefaultDevice::initProperties();

    /* Channel duty cycles */
    IUFillNumber(&OutputsN[0], "CHANNEL1", "%%", "%3.0f", 0., 100., 10., 0.);
    IUFillNumber(&OutputsN[1], "CHANNEL2", "%%", "%3.0f", 0., 100., 10., 0.);
    IUFillNumber(&OutputsN[2], "CHANNEL3", "%%", "%3.0f", 0., 100., 10., 0.);
    IUFillNumberVector(&OutputsNP, OutputsN, 3, getDeviceName(), "OUTPUT", "Outputs",
                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /* Humidity */
    IUFillNumber(&HumidityN[0], "HUMIDITY", "%%", "%3.0f", 0., 100., 0., 0.);
    IUFillNumberVector(&HumidityNP, HumidityN, 1, getDeviceName(), "HUMIDITY", "Humidity",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Temperatures */
    IUFillNumber(&TemperaturesN[0], "AMBIENT", "Celsius", "%6.2f", -50., 70., 0., 0.);
    IUFillNumber(&TemperaturesN[1], "CHANNEL1", "Celsius", "%6.2f", -50., 70., 0., 0.);
    IUFillNumber(&TemperaturesN[2], "CHANNEL2", "Celsius", "%6.2f", -50., 70., 0., 0.);
    IUFillNumberVector(&TemperaturesNP, TemperaturesN, 3, getDeviceName(), "TEMPERATURES", "Temperatures",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&CalibrationsN[0], "AMBIENT", "Celsius", "%6.2f", -50., 70., 1., 0.);
    IUFillNumber(&CalibrationsN[1], "CHANNEL1", "Celsius", "%6.2f", -50., 70., 1., 0.);
    IUFillNumber(&CalibrationsN[2], "CHANNEL2", "Celsius", "%6.2f", -50., 70., 1., 0.);
    IUFillNumberVector(&CalibrationsNP, CalibrationsN, 3, getDeviceName(), "CALIBRATIONS", "Calibrations",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&ThresholdsN[0], "CHANNEL1", "Celsius", "%6.2f", -50., 70., 1., 0.);
    IUFillNumber(&ThresholdsN[1], "CHANNEL2", "Celsius", "%6.2f", -50., 70., 1., 0.);
    IUFillNumberVector(&ThresholdsNP, ThresholdsN, 2, getDeviceName(), "THRESHOLDS", "Thresholds",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&AggressivityN[0], "AGGRESSIVITY", "", "%2.0f", 0., 10., 1., 0.);
    IUFillNumberVector(&AggressivityNP, AggressivityN, 1, getDeviceName(), "AGGRESSIVITY", "Aggressivity",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // AutoMode
    IUFillSwitch(&AutoModeS[0], "AUTO", "Automatic", ISS_ON);
    IUFillSwitchVector(&AutoModeSP, AutoModeS, 1, getDeviceName(), "MODE", "Operating mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // LinkOut23
    IUFillSwitch(&LinkOut23S[0], "LINK", "Link", ISS_OFF);
    IUFillSwitchVector(&LinkOut23SP, LinkOut23S, 1, getDeviceName(), "LINK23", "Link channels 2 and 3", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);
    // Reset
    IUFillSwitch(&ResetS[0], "Reset", "", ISS_OFF);
    IUFillSwitchVector(&ResetSP, ResetS, 1, getDeviceName(), "Reset", "", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Firmware version
    IUFillNumber(&FWversionN[0], "FIRMWARE", "Firmware Version", "%5.0f", 0., 65535., 1., 0.);
    IUFillNumberVector(&FWversionNP, FWversionN, 1, getDeviceName(), "FW_VERSION", "Firmware", OPTIONS_TAB, IP_RO, 0,
                       IPS_IDLE);

    setDriverInterface(AUX_INTERFACE);

    addAuxControls();
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(serialConnection);

    return true;
}

bool USBDewpoint::updateProperties()
{
    DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineNumber(&OutputsNP);
        defineNumber(&HumidityNP);
        defineNumber(&TemperaturesNP);
        defineNumber(&CalibrationsNP);
        defineNumber(&ThresholdsNP);
        defineNumber(&AggressivityNP);
        defineSwitch(&AutoModeSP);
        defineSwitch(&LinkOut23SP);
        defineSwitch(&ResetSP);
        defineNumber(&FWversionNP);

        loadConfig(true);

        DEBUG(INDI::Logger::DBG_SESSION, "USB_Dewpoint paramaters updated, device ready for use.");
    }
    else
    {
        deleteProperty(OutputsNP.name);
        deleteProperty(HumidityNP.name);
        deleteProperty(TemperaturesNP.name);
        deleteProperty(CalibrationsNP.name);
        deleteProperty(ThresholdsNP.name);
        deleteProperty(AggressivityNP.name);
        deleteProperty(AutoModeSP.name);
        deleteProperty(LinkOut23SP.name);
        deleteProperty(ResetSP.name);
        deleteProperty(FWversionNP.name);
    }

    return true;
}

bool USBDewpoint::Handshake()
{
    if (isSimulation())
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Connected successfuly to simulated %s. Retrieving startup data...",
               getDeviceName());

        SetTimer(POLLMS);
        return true;
    }

    PortFD = serialConnection->getPortFD();
    DEBUG(INDI::Logger::DBG_SESSION, "USB_Dewpoint is online. Getting device parameters...");
    return true;
}

const char *USBDewpoint::getDefaultName()
{
    return "USB_Dewpoint";
}

bool USBDewpoint::getControllerStatus()
{
    return true;
}

bool USBDewpoint::updateFWversion()
{
    FWversionN[0].value = firmware;
    return true;
}


bool USBDewpoint::reset()
{
    return true;
}


bool USBDewpoint::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(ResetSP.name, name) == 0)
        {
            IUResetSwitch(&ResetSP);

            if (reset())
                ResetSP.s = IPS_OK;
            else
                ResetSP.s = IPS_ALERT;

            IDSetSwitch(&ResetSP, nullptr);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool USBDewpoint::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, FWversionNP.name) == 0)
        {
            IUUpdateNumber(&FWversionNP, values, names, n);
            FWversionNP.s = IPS_OK;
            IDSetNumber(&FWversionNP, nullptr);
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

void USBDewpoint::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    // Get temperatures etc.
    SetTimer(POLLMS);
}
