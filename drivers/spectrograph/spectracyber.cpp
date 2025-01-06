/*
    Kuwait National Radio Observatory
    INDI Driver for SpectraCyber Hydrogen Line Spectrometer
    Communication: RS232 <---> USB

    Copyright (C) 2009 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    Change Log:

    Format of BLOB data is:

    ########### ####### ########## ## ###
    Julian_Date Voltage Freqnuency RA DEC

*/

#include "spectracyber.h"

#include "config.h"

#include <indicom.h>

#include <libnova/julian_day.h>

#include <memory>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define mydev         "SpectraCyber"
#define BASIC_GROUP   "Main Control"
#define OPTIONS_GROUP "Options"

#define CONT_CHANNEL 0
#define SPEC_CHANNEL 1

//const int SPECTROMETER_READ_BUFFER  = 16;
const int SPECTROMETER_ERROR_BUFFER = 128;
const int SPECTROMETER_CMD_LEN      = 5;
const int SPECTROMETER_CMD_REPLY    = 4;

//const double SPECTROMETER_MIN_FREQ  = 46.4;
const double SPECTROMETER_REST_FREQ = 48.6;
//const double SPECTROMETER_MAX_FREQ  = 51.2;
const double SPECTROMETER_RF_FREQ   = 1371.805;

const unsigned int SPECTROMETER_OFFSET = 0x050;

/* 90 Khz Rest Correction */
const double SPECTROMETER_REST_CORRECTION = 0.090;

static const char *contFMT = ".ascii_cont";
static const char *specFMT = ".ascii_spec";

// We declare an auto pointer to spectrometer.
std::unique_ptr<SpectraCyber> spectracyber(new SpectraCyber());

/****************************************************************
**
**
*****************************************************************/
SpectraCyber::SpectraCyber()
{
    // Command pre-limiter
    command[0] = '!';

    telescopeID = nullptr;

    srand(time(nullptr));

    buildSkeleton("indi_spectracyber_sk.xml");

    // Optional: Add aux controls for configuration, debug & simulation
    addAuxControls();

    setVersion(1, 3);
}

/****************************************************************
**
**
*****************************************************************/
void SpectraCyber::ISGetProperties(const char *dev)
{
    static int propInit = 0;

    INDI::DefaultDevice::ISGetProperties(dev);

    if (propInit == 0)
    {
        loadConfig();

        propInit = 1;

        auto tProp = getText("ACTIVE_DEVICES");

        if (tProp)
        {
            telescopeID = tProp.findWidgetByName("ACTIVE_TELESCOPE");

            if (telescopeID && strlen(telescopeID->text) > 0)
            {
                IDSnoopDevice(telescopeID->text, "EQUATORIAL_EOD_COORD");
            }
        }
    }
}

/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::ISSnoopDevice(XMLEle *root)
{
    if (IUSnoopNumber(root, &EquatorialCoordsRNP) != 0)
    {
        LOG_WARN("Error processing snooped EQUATORIAL_EOD_COORD_REQUEST value! No RA/DEC information available.");

        return true;
    }
    //else
    //    IDLog("Received RA: %g - DEC: %g\n", EquatorialCoordsRNP.np[0].value, EquatorialCoordsRNP.np[1].value);

    return false;
}

/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::initProperties()
{

    INDI::DefaultDevice::initProperties();

    FreqNP = getNumber("Freq (Mhz)");
    if (!FreqNP)
        LOG_ERROR("Error: Frequency property is missing. Spectrometer cannot be operated.");

    ScanNP = getNumber("Scan Parameters");
    if (!ScanNP)
        LOG_ERROR("Error: Scan parameters property is missing. Spectrometer cannot be operated.");

    ChannelSP = getSwitch("Channels");
    if (!ChannelSP)
        LOG_ERROR("Error: Channel property is missing. Spectrometer cannot be operated.");

    ScanSP = getSwitch("Scan");
    if (!ScanSP)
        LOG_ERROR("Error: Channel property is missing. Spectrometer cannot be operated.");

    DataStreamBP = getBLOB("Data");
    if (!DataStreamBP)
        LOG_ERROR("Error: BLOB data property is missing. Spectrometer cannot be operated.");

    if (DataStreamBP)
        DataStreamBP[0].setBlob((char *)malloc(MAXBLEN * sizeof(char)));

    /**************************************************************************/
    // Equatorial Coords - SET
    IUFillNumber(&EquatorialCoordsRN[0], "RA", "RA  H:M:S", "%10.6m", 0., 24., 0., 0.);
    IUFillNumber(&EquatorialCoordsRN[1], "DEC", "Dec D:M:S", "%10.6m", -90., 90., 0., 0.);
    IUFillNumberVector(&EquatorialCoordsRNP, EquatorialCoordsRN, NARRAY(EquatorialCoordsRN), "", "EQUATORIAL_EOD_COORD",
                       "Equatorial AutoSet", "", IP_RW, 0, IPS_IDLE);
    /**************************************************************************/

    setDriverInterface(SPECTROGRAPH_INTERFACE);

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::Connect()
{
    auto tProp = getText("DEVICE_PORT");

    if (isConnected())
        return true;

    if (!tProp)
        return false;

    if (isSimulation())
    {
        LOGF_INFO("%s Spectrometer: Simulating connection to port %s.", type_name.c_str(),
               tProp[0].getText());
        SetTimer(getCurrentPollingPeriod());
        return true;
    }

    if (tty_connect(tProp[0].getText(), 2400, 8, 0, 1, &fd) != TTY_OK)
    {
        LOGF_ERROR("Error connecting to port %s. Make sure you have BOTH read and write permission to the port.",
               tProp[0].getText());
        return false;
    }

    // We perform initial handshake check by resetting all parameter and watching for echo reply
    if (reset() == true)
    {
        LOG_INFO("Spectrometer is online. Retrieving preliminary data...");
        SetTimer(getCurrentPollingPeriod());
        return init_spectrometer();
    }
    else
    {
        DEBUG(INDI::Logger::DBG_ERROR,
              "Spectrometer echo test failed. Please recheck connection to spectrometer and try again.");
        return false;
    }
}

/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::init_spectrometer()
{
    // Enable speed mode
    if (isSimulation())
    {
        LOGF_INFO("%s Spectrometer: Simulating spectrometer init.", type_name.c_str());
        return true;
    }

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::Disconnect()
{
    tty_disconnect(fd);

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) != 0)
        return false;

    static double cont_offset = 0;
    static double spec_offset = 0;

    auto nProp = getNumber(name);

    if (!nProp)
        return false;

    if (isConnected() == false)
    {
        resetProperties();
        LOG_ERROR("Spectrometer is offline. Connect before issuing any commands.");
        return false;
    }

    // IF Gain
    if (nProp.isNameMatch("70 Mhz IF"))
    {
        double last_value = nProp[0].getValue();

        if (!nProp.update(values, names, n))
            return false;

        if (dispatch_command(IF_GAIN) == false)
        {
            nProp[0].setValue(last_value);
            nProp.setState(IPS_ALERT);
            nProp.apply("Error dispatching IF gain command to spectrometer. Check logs.");
            return false;
        }

        nProp.setState(IPS_OK);
        nProp.apply();

        return true;
    }

    // DC Offset
    if (nProp.isNameMatch("DC Offset"))
    {
        if (!nProp.update(values, names, n))
            return false;

        // Check which offset change, if none, return gracefully
        if (nProp[CONTINUUM_CHANNEL].getValue() != cont_offset)
        {
            if (dispatch_command(CONT_OFFSET) == false)
            {
                nProp[CONTINUUM_CHANNEL].setValue(cont_offset);
                nProp.setState(IPS_ALERT);
                nProp.apply("Error dispatching continuum DC offset command to spectrometer. Check logs.");
                return false;
            }

            cont_offset = nProp[CONTINUUM_CHANNEL].getValue();
        }

        if (nProp[SPECTRAL_CHANNEL].getValue() != spec_offset)
        {
            if (dispatch_command(SPEC_OFFSET) == false)
            {
                nProp[SPECTRAL_CHANNEL].setValue(spec_offset);
                nProp.setState(IPS_ALERT);
                nProp.apply("Error dispatching spectral DC offset command to spectrometer. Check logs.");
                return false;
            }

            spec_offset = nProp[SPECTRAL_CHANNEL].getValue();
        }

        // No Change, return
        nProp.setState(IPS_OK);
        nProp.apply();
        return true;
    }

    // Freq Change
    if (nProp.isNameMatch("Freq (Mhz)"))
        return update_freq(values[0]);

    // Scan Options
    if (nProp.isNameMatch("Scan Parameters"))
    {
        if (!nProp.update(values, names, n))
            return false;

        nProp.setState(IPS_OK);
        nProp.apply();
        return true;
    }
    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) != 0)
        return false;

    auto tProp = getText(name);

    if (!tProp)
        return false;

    // Device Port Text
    if (tProp.isNameMatch("DEVICE_PORT"))
    {
        if (!tProp.update(texts, names, n))
            return false;

        tProp.setState(IPS_OK);
        tProp.apply("Port updated.");

        return true;
    }

    // Telescope Source
    if (tProp.isNameMatch("ACTIVE_DEVICES"))
    {
        telescopeID = tProp.findWidgetByName("ACTIVE_TELESCOPE");

        if (telescopeID && strcmp(texts[0], telescopeID->text))
        {
            if (!tProp.update(texts, names, n))
                return false;

            strncpy(EquatorialCoordsRNP.device, tProp[0].getText(), MAXINDIDEVICE);

            LOGF_INFO("Active telescope updated to %s. Please save configuration.", telescopeID->text);

            IDSnoopDevice(telescopeID->text, "EQUATORIAL_EOD_COORD");
        }

        tProp.setState(IPS_OK);
        tProp.apply();
        return true;
    }

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool SpectraCyber::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) != 0)
        return false;

    // First process parent!
    if (INDI::DefaultDevice::ISNewSwitch(getDeviceName(), name, states, names, n) == true)
        return true;

    auto sProp = getSwitch(name);

    if (!sProp)
        return false;

    if (isConnected() == false)
    {
        resetProperties();
        LOG_ERROR("Spectrometer is offline. Connect before issuing any commands.");
        return false;
    }

    // Scan
    if (sProp.isNameMatch("Scan"))
    {
        if (!FreqNP || !DataStreamBP)
            return false;

        if (!sProp.update(states, names, n))
            return false;

        if (sProp[1].getState() == ISS_ON)
        {
            if (sProp.getState() == IPS_BUSY)
            {
                sProp.setState(IPS_IDLE);
                FreqNP.setState(IPS_IDLE);
                DataStreamBP.setState(IPS_IDLE);

                FreqNP.apply();
                DataStreamBP.apply();
                sProp.apply("Scan stopped.");
                return false;
            }

            sProp.setState(IPS_OK);
            sProp.apply();
            return true;
        }

        sProp.setState(IPS_BUSY);
        DataStreamBP.setState(IPS_BUSY);

        // Compute starting freq  = base_freq - low
        if (sProp[SPEC_CHANNEL].getState() == ISS_ON)
        {
            start_freq  = (SPECTROMETER_RF_FREQ + SPECTROMETER_REST_FREQ) - abs((int)ScanNP[0].getValue()) / 1000.;
            target_freq = (SPECTROMETER_RF_FREQ + SPECTROMETER_REST_FREQ) + abs((int)ScanNP[1].getValue()) / 1000.;
            sample_rate = ScanNP[2].getValue() * 5;
            FreqNP[0].setValue(start_freq);
            FreqNP.setState(IPS_BUSY);
            FreqNP.apply();
            sProp.apply("Starting spectral scan from %g MHz to %g MHz in steps of %g KHz...", start_freq,
                        target_freq, sample_rate);
        }
        else
            sProp.apply("Starting continuum scan @ %g MHz...", FreqNP[0].getValue());

        return true;
    }

    // Continuum Gain Control
    if (sProp.isNameMatch("Continuum Gain"))
    {
        int last_switch = sProp.findOnSwitchIndex();

        if (!sProp.update(states, names, n))
            return false;

        if (dispatch_command(CONT_GAIN) == false)
        {
            sProp.setState(IPS_ALERT);
            sProp.reset();
            sProp[last_switch].setState(ISS_ON);
            sProp.apply("Error dispatching continuum gain command to spectrometer. Check logs.");
            return false;
        }

        sProp.setState(IPS_OK);
        sProp.apply();

        return true;
    }

    // Spectral Gain Control
    if (sProp.isNameMatch("Spectral Gain"))
    {
        int last_switch = sProp.findOnSwitchIndex();

        if (!sProp.update(states, names, n))
            return false;

        if (dispatch_command(SPEC_GAIN) == false)
        {
            sProp.setState(IPS_ALERT);
            sProp.reset();
            sProp[last_switch].setState(ISS_ON);
            sProp.apply("Error dispatching spectral gain command to spectrometer. Check logs.");
            return false;
        }

        sProp.setState(IPS_OK);
        sProp.apply();

        return true;
    }

    // Continuum Integration Control
    if (sProp.isNameMatch("Continuum Integration (s)"))
    {
        int last_switch = sProp.findOnSwitchIndex();

        if (!sProp.update(states, names, n))
            return false;

        if (dispatch_command(CONT_TIME) == false)
        {
            sProp.setState(IPS_ALERT);
            sProp.reset();
            sProp[last_switch].setState(ISS_ON);
            sProp.apply("Error dispatching continuum integration command to spectrometer. Check logs.");
            return false;
        }

        sProp.setState(IPS_OK);
        sProp.apply();

        return true;
    }

    // Spectral Integration Control
    if (sProp.isNameMatch("Spectral Integration (s)"))
    {
        int last_switch = sProp.findOnSwitchIndex();

        if (!sProp.update(states, names, n))
            return false;

        if (dispatch_command(SPEC_TIME) == false)
        {
            sProp.setState(IPS_ALERT);
            sProp.reset();
            sProp[last_switch].setState(ISS_ON);
            sProp.apply("Error dispatching spectral integration command to spectrometer. Check logs.");
            return false;
        }

        sProp.setState(IPS_OK);
        sProp.apply();

        return true;
    }

    // Bandwidth Control
    if (sProp.isNameMatch("Bandwidth (Khz)"))
    {
        int last_switch = sProp.findOnSwitchIndex();

        if (!sProp.update(states, names, n))
            return false;

        if (dispatch_command(BANDWIDTH) == false)
        {
            sProp.setState(IPS_ALERT);
            sProp.reset();
            sProp[last_switch].setState(ISS_ON);
            sProp.apply("Error dispatching bandwidth change command to spectrometer. Check logs.");
            return false;
        }

        sProp.setState(IPS_OK);
        sProp.apply();

        return true;
    }

    // Channel selection
    if (sProp.isNameMatch("Channels"))
    {
        static int lastChannel;

        lastChannel = sProp.findOnSwitchIndex();

        if (!sProp.update(states, names, n))
            return false;

        sProp.setState(IPS_OK);
        if (ScanSP.getState() == IPS_BUSY && lastChannel != sProp.findOnSwitchIndex())
        {
            abort_scan();
            sProp.apply("Scan aborted due to change of channel selection.");
        }
        else
            sProp.apply();

        return true;
    }

    // Reset
    if (sProp.isNameMatch("Reset"))
    {
        if (reset() == true)
        {
            sProp.setState(IPS_OK);
            sProp.apply();
        }
        else
        {
            sProp.setState(IPS_ALERT);
            sProp.apply("Error dispatching reset parameter command to spectrometer. Check logs.");
            return false;
        }
        return true;
    }

    return true;
}

bool SpectraCyber::dispatch_command(SpectrometerCommand command_type)
{
    char spectrometer_error[SPECTROMETER_ERROR_BUFFER];
    int err_code = 0, nbytes_written = 0, final_value = 0;
    // Maximum of 3 hex digits in addition to null terminator
    char hex[5];

    tcflush(fd, TCIOFLUSH);

    switch (command_type)
    {
        // Intermediate Frequency Gain
        case IF_GAIN: {
            auto prop = getNumber("70 Mhz IF");
            if (!prop)
                return false;
            command[1] = 'A';
            command[2] = '0';
            // Equation is
            // Value = ((X - 10) * 63) / 15.75, where X is the user selection (10dB to 25.75dB)
            final_value = (int)((prop[0].getValue() - 10) * 63) / 15.75;
            sprintf(hex, "%02X", (uint16_t)final_value);
            command[3] = hex[0];
            command[4] = hex[1];
            break;
        }

        // Continuum Gain
        case CONT_GAIN: {
            auto prop = getSwitch("Continuum Gain");
            if (!prop)
                return false;
            command[1]  = 'G';
            command[2]  = '0';
            command[3]  = '0';
            final_value = prop.findOnSwitchIndex();
            sprintf(hex, "%d", (uint8_t)final_value);
            command[4] = hex[0];
            break;
        }

        // Continuum Integration
        case CONT_TIME: {
            auto prop = getSwitch("Continuum Integration (s)");
            if (!prop)
                return false;
            command[1]  = 'I';
            command[2]  = '0';
            command[3]  = '0';
            final_value = prop.findOnSwitchIndex();
            sprintf(hex, "%d", (uint8_t)final_value);
            command[4] = hex[0];
            break;
        }

        // Spectral Gain
        case SPEC_GAIN: {
            auto prop = getSwitch("Spectral Gain");
            if (!prop)
                return false;
            command[1]  = 'K';
            command[2]  = '0';
            command[3]  = '0';
            final_value = prop.findOnSwitchIndex();
            sprintf(hex, "%d", (uint8_t)final_value);
            command[4] = hex[0];
            break;
        }

        // Spectral Integration
        case SPEC_TIME: {
            auto prop = getSwitch("Spectral Integration (s)");
            if (!prop)
                return false;
            command[1]  = 'L';
            command[2]  = '0';
            command[3]  = '0';
            final_value = prop.findOnSwitchIndex();
            sprintf(hex, "%d", (uint8_t)final_value);
            command[4] = hex[0];
            break;
        }

        // Continuum DC Offset
        case CONT_OFFSET: {
            auto prop = getNumber("DC Offset");
            if (!prop)
                return false;
            command[1]  = 'O';
            final_value = (int)prop[CONTINUUM_CHANNEL].getValue() / 0.001;
            sprintf(hex, "%03X", (uint32_t)final_value);
            command[2] = hex[0];
            command[3] = hex[1];
            command[4] = hex[2];
            break;
        }

        // Spectral DC Offset
        case SPEC_OFFSET: {
            auto prop = getNumber("DC Offset");
            if (!prop)
                return false;
            command[1]  = 'J';
            final_value = (int)prop[SPECTRAL_CHANNEL].getValue() / 0.001;
            sprintf(hex, "%03X", (uint32_t)final_value);
            command[2] = hex[0];
            command[3] = hex[1];
            command[4] = hex[2];
            break;
        }

        // FREQ
        case RECV_FREQ: {
            command[1] = 'F';
            // Each value increment is 5 Khz. Range is 050h to 3e8h.
            // 050h corresponds to 46.4 Mhz (min), 3e8h to 51.2 Mhz (max)
            // To compute the desired received freq, we take the diff (target - min) / 0.005
            // 0.005 Mhz = 5 Khz
            // Then we add the diff to 050h (80 in decimal) to get the final freq.
            // e.g. To set 50.00 Mhz, diff = 50 - 46.4 = 3.6 / 0.005 = 800 = 320h
            //      Freq = 320h + 050h (or 800 + 80) = 370h = 880 decimal

            final_value = (int)((FreqNP[0].getValue() + SPECTROMETER_REST_CORRECTION - FreqNP[0].getMin()) / 0.005 +
                                SPECTROMETER_OFFSET);
            sprintf(hex, "%03X", (uint32_t)final_value);
            if (isDebug())
                IDLog("Required Freq is: %.3f --- Min Freq is: %.3f --- Spec Offset is: %d -- Final Value (Dec): %d "
                      "--- Final Value (Hex): %s\n",
                      FreqNP[0].getValue(), FreqNP[0].getMin(), SPECTROMETER_OFFSET, final_value, hex);
            command[2] = hex[0];
            command[3] = hex[1];
            command[4] = hex[2];
            break;
        }

        // Read Channel
        case READ_CHANNEL: {
            command[1]  = 'D';
            command[2]  = '0';
            command[3]  = '0';
            final_value = ChannelSP.findOnSwitchIndex();
            command[4]  = (final_value == 0) ? '0' : '1';
            break;
        }

        // Bandwidth
        case BANDWIDTH: {
            auto prop = getSwitch("Bandwidth (Khz)");
            if (!prop)
                return false;
            command[1]  = 'B';
            command[2]  = '0';
            command[3]  = '0';
            final_value = prop.findOnSwitchIndex();
            //sprintf(hex, "%x", final_value);
            command[4] = (final_value == 0) ? '0' : '1';
            break;
        }

        // Reset
        case RESET:
            command[1] = 'R';
            command[2] = '0';
            command[3] = '0';
            command[4] = '0';
            break;

        // Noise source
        case NOISE_SOURCE:
            // TODO: Do something here?
            break;
    }

    if (isDebug())
        IDLog("Dispatching command #%s#\n", command);

    if (isSimulation())
        return true;

    if ((err_code = tty_write(fd, command, SPECTROMETER_CMD_LEN, &nbytes_written) != TTY_OK))
    {
        tty_error_msg(err_code, spectrometer_error, SPECTROMETER_ERROR_BUFFER);
        if (isDebug())
            IDLog("TTY error detected: %s\n", spectrometer_error);
        return false;
    }

    return true;
}

int SpectraCyber::get_on_switch(ISwitchVectorProperty *sp)
{
    for (int i = 0; i < sp->nsp; i++)
        if (sp->sp[i].s == ISS_ON)
            return i;

    return -1;
}

bool SpectraCyber::update_freq(double nFreq)
{
    double last_value = FreqNP[0].getValue();

    if (nFreq < FreqNP[0].getMin() || nFreq > FreqNP[0].getMax())
        return false;

    FreqNP[0].setValue(nFreq);

    if (dispatch_command(RECV_FREQ) == false)
    {
        FreqNP[0].setValue(last_value);
        FreqNP.setState(IPS_ALERT);
        FreqNP.apply("Error dispatching RECV FREQ command to spectrometer. Check logs.");
        return false;
    }

    if (ScanSP.getState() != IPS_BUSY)
        FreqNP.setState(IPS_OK);

    FreqNP.apply();

    // Delay 0.5s for INT
    usleep(500000);
    return true;
}

bool SpectraCyber::reset()
{
    int err_code = 0, nbytes_read = 0;
    char response[4];
    char err_msg[SPECTROMETER_ERROR_BUFFER];

    if (isDebug())
        IDLog("Attempting to write to spectrometer....\n");

    dispatch_command(RESET);

    if (isDebug())
        IDLog("Attempting to read from spectrometer....\n");

    // Read echo from spectrometer, we're expecting R000
    if ((err_code = tty_read(fd, response, SPECTROMETER_CMD_REPLY, 5, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(err_code, err_msg, 32);
        if (isDebug())
            IDLog("TTY error detected: %s\n", err_msg);
        return false;
    }

    if (isDebug())
        IDLog("Response from Spectrometer: #%c# #%c# #%c# #%c#\n", response[0], response[1], response[2], response[3]);

    if (strstr(response, "R000"))
    {
        if (isDebug())
            IDLog("Echo test passed.\n");

        loadDefaultConfig();

        return true;
    }

    if (isDebug())
        IDLog("Echo test failed.\n");
    return false;
}

void SpectraCyber::TimerHit()
{
    if (!isConnected())
        return;

    char RAStr[16], DecStr[16];

    switch (ScanSP.getState())
    {
        case IPS_BUSY:
            if (ChannelSP[CONT_CHANNEL].getState() == ISS_ON)
                break;

            if (FreqNP[0].getValue() >= target_freq)
            {
                ScanSP.setState(IPS_OK);
                FreqNP.setState(IPS_OK);

                FreqNP.apply();
                ScanSP.apply("Scan complete.");
                SetTimer(getCurrentPollingPeriod());
                return;
            }

            if (update_freq(FreqNP[0].getValue()) == false)
            {
                abort_scan();
                SetTimer(getCurrentPollingPeriod());
                return;
            }

            FreqNP[0].setValue(FreqNP[0].getValue() +  sample_rate / 1000.);
            break;

        default:
            break;
    }

    switch (DataStreamBP.getState())
    {
        case IPS_BUSY: {
            if (ScanSP.getState() != IPS_BUSY)
            {
                DataStreamBP.setState(IPS_IDLE);
                DataStreamBP.apply();
                break;
            }

            if (read_channel() == false)
            {
                DataStreamBP.setState(IPS_ALERT);

                if (ScanSP.getState() == IPS_BUSY)
                    abort_scan();

                DataStreamBP.apply();
            }

            JD = ln_get_julian_from_sys();

            // Continuum
            if (ChannelSP[0].getState() == ISS_ON)
                DataStreamBP[0].setFormat(contFMT);
            else
                DataStreamBP[0].setFormat(specFMT);

            fs_sexa(RAStr, EquatorialCoordsRN[0].value, 2, 3600);
            fs_sexa(DecStr, EquatorialCoordsRN[1].value, 2, 3600);

            if (telescopeID && strlen(telescopeID->text) > 0)
                snprintf(bLine, MAXBLEN, "%.8f %.3f %.3f %s %s", JD, chanValue, FreqNP[0].getValue(), RAStr, DecStr);
            else
                snprintf(bLine, MAXBLEN, "%.8f %.3f %.3f", JD, chanValue, FreqNP[0].getValue());

            auto bLineSize = strlen(bLine);
            DataStreamBP[0].setBlobLen(bLineSize);
            DataStreamBP[0].setSize(bLineSize);
            memcpy(DataStreamBP[0].getBlob(), bLine, bLineSize);

            //IDLog("\nSTRLEN: %d -- BLOB:'%s'\n", strlen(bLine), (char *) DataStreamBP->bp[0].blob);

            DataStreamBP.apply();

            break;
        }
        default:
            break;
    }

    SetTimer(getCurrentPollingPeriod());
}

void SpectraCyber::abort_scan()
{
    FreqNP.setState(IPS_IDLE);
    ScanSP.setState(IPS_ALERT);

    ScanSP.reset();
    ScanSP[1].setState(ISS_ON);

    FreqNP.apply();
    ScanSP.apply("Scan aborted due to errors.");
}

bool SpectraCyber::read_channel()
{
    int err_code = 0, nbytes_read = 0;
    char response[SPECTROMETER_CMD_REPLY];
    char err_msg[SPECTROMETER_ERROR_BUFFER];

    if (isSimulation())
    {
        chanValue = ((double)rand()) / ((double)RAND_MAX) * 10.0;
        return true;
    }

    dispatch_command(READ_CHANNEL);
    if ((err_code = tty_read(fd, response, SPECTROMETER_CMD_REPLY, 5, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(err_code, err_msg, 32);
        if (isDebug())
            IDLog("TTY error detected: %s\n", err_msg);
        return false;
    }

    if (isDebug())
        IDLog("Response from Spectrometer: #%s#\n", response);

    int result = 0;
    sscanf(response, "D%x", &result);
    // We divide by 409.5 to scale the value to 0 - 10 VDC range
    chanValue = result / 409.5;

    return true;
}

const char *SpectraCyber::getDefaultName()
{
    return mydev;
}
