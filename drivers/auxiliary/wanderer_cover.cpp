/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  Wanderer cover V3

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

#include "wanderer_cover.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>

// We declare an auto pointer to WandererCover.
static std::unique_ptr<WandererCover> wanderercover(new WandererCover());
#define PARK 1
#define DATA_BITS 8
#define FLAT_CMD 6
#define WANDERER_RESPONSE_SIZE 1
#define COMMAND_WAITING_TIME 120

#define FIRST_SUPPORTED_VERSION "20220920"

#define TAB_NAME_CONFIGURATION "Dust cover configuration"

#define CLOSE_COVER_COMMAND "1000\n"
#define HANDSHAKE_COMMAND "1500001\n"
#define OPEN_COVER_COMMAND "1001\n"
#define TURN_OFF_LIGHT_PANEL_COMMAND "9999\n"

# define SET_CURRENT_POSITION_TO_OPEN_POSITION "257\n"
# define SET_CURRENT_POSITION_TO_CLOSED_POSITION "256\n"


WandererCover::WandererCover() : LightBoxInterface(this, true)
{
    setVersion(1, 0);
}

bool WandererCover::initProperties()
{
    INDI::DefaultDevice::initProperties();
    initDustCapProperties(getDeviceName(), MAIN_CONTROL_TAB);
    initLightBoxProperties(getDeviceName(), MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);
    addAuxControls();

    // Status
    IUFillText(&StatusT[0], "Cover", "Cover", nullptr);
    IUFillText(&StatusT[1], "Light", "Light", nullptr);
    IUFillTextVector(&StatusTP, StatusT, 2, getDeviceName(), "Status", "Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Firmware version
    IUFillText(&FirmwareT[0], "Version", "Version", nullptr);
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // Configuration
    IUFillSwitch(&ControlPositionPositiveDegreesConfigurationV[PLUS_1_DEGREE], "PLUS_1_DEGREE", "+ 1°", ISS_OFF);
    IUFillSwitch(&ControlPositionPositiveDegreesConfigurationV[PLUS_10_DEGREE], "PLUS_10_DEGREE", "+ 10°", ISS_OFF);
    IUFillSwitch(&ControlPositionPositiveDegreesConfigurationV[PLUS_50_DEGREE], "PLUS_50_DEGREE", "+ 50°", ISS_OFF);
    IUFillSwitchVector(&ControlPositionPositiveDegreesConfigurationVP, ControlPositionPositiveDegreesConfigurationV, 3,
                       getDeviceName(), "Open dust cap", "Open dust cap", TAB_NAME_CONFIGURATION, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&ControlPositionNegativeDegreesConfigurationV[MINUS_1_DEGREE], "MINUS_1_DEGREE", "- 1°", ISS_OFF);
    IUFillSwitch(&ControlPositionNegativeDegreesConfigurationV[MINUS_10_DEGREE], "MINUS_10_DEGREE", "- 10°", ISS_OFF);
    IUFillSwitch(&ControlPositionNegativeDegreesConfigurationV[MINUS_50_DEGREE], "MINUS_50_DEGREE", "- 50°", ISS_OFF);
    IUFillSwitchVector(&ControlPositionNegativeDegreesConfigurationVP, ControlPositionNegativeDegreesConfigurationV, 3,
                       getDeviceName(), "Close dust cap", "Close dust cap", TAB_NAME_CONFIGURATION, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);

    IUFillSwitch(&DefinePositionConfigurationV[SET_CURRENT_POSITION_OPEN], "Set current position as open",
                 " 1 - Set current position as open", ISS_OFF);
    IUFillSwitch(&DefinePositionConfigurationV[SET_CURRENT_POSITION_CLOSE], "Set current position as close",
                 "2 - Set current position as close", ISS_OFF);
    IUFillSwitchVector(&DefinePositionConfigurationVP, DefinePositionConfigurationV, 2, getDeviceName(), "Define position",
                       "Action", TAB_NAME_CONFIGURATION, IP_RW,
                       ISR_ATMOST1, 0, IPS_IDLE);


    LightIntensityN[0].min = 1;
    LightIntensityN[0].max = 255;
    LightIntensityN[0].step = 10;



    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

bool WandererCover::Handshake()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s. Retrieving startup data...", getDeviceName());

        // SetTimer(getCurrentPollingPeriod());
        IUSaveText(&FirmwareT[0], "Simulation version");
        IDSetText(&FirmwareTP, nullptr);

        updateCoverStatus((char*) "0");

        setLightBoxStatusAsSwitchedOff();

        NumberOfStepsBeetweenOpenAndCloseState = 0;
        setNumberOfStepsStatusValue(NumberOfStepsBeetweenOpenAndCloseState);

        syncDriverInfo();
        return true;
    }
    PortFD = serialConnection->getPortFD();
    tcflush(PortFD, TCIOFLUSH);
    int nbytes_read_name = 0, nbytes_written = 0, rc = -1;
    char name[64] = {0};
    LOGF_DEBUG("CMD <%s>", HANDSHAKE_COMMAND);
    if ((rc = tty_write_string(PortFD, HANDSHAKE_COMMAND, &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    if ((rc = tty_read_section(PortFD, name, 'A', 5, &nbytes_read_name)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Device read error: %s", errorMessage);
        return false;
    }
    name[nbytes_read_name - 1] = '\0';
    LOGF_DEBUG("Name : <%s>", name);

    int nbytes_read_version = 0;
    char version[64] = {0};
    if ((rc = tty_read_section(PortFD, version, 'A', 5, &nbytes_read_version)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Device read error: %s", errorMessage);
        LOGF_ERROR("You have an old firmware. This version is not supported. You should update the device as described here : %s",
                   "https://www.wandererastro.com/en/col.jsp?id=106");
        return true;
    }

    displayConfigurationMessage();
    version[nbytes_read_version - 1] = '\0';
    LOGF_INFO("Version : %s", version);
    IUSaveText(&FirmwareT[0], version);
    IDSetText(&FirmwareTP, nullptr);


    // Cover status
    char cover_state[64] = {0};
    int nbytes_read_cover_state = 0;

    if ((rc = tty_read_section(PortFD, cover_state, 'A', 5, &nbytes_read_cover_state)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Device read error: %s", errorMessage);
        return false;
    }
    cover_state[nbytes_read_cover_state - 1] = '\0';
    LOGF_INFO("Cover state : %s", cover_state);
    updateCoverStatus(cover_state);

    // Number of steps
    char number_of_steps[64] = {0};
    int nbytes_read_number_of_steps = 0;

    if ((rc = tty_read_section(PortFD, number_of_steps, 'A', 5, &nbytes_read_number_of_steps)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Device read error: %s", errorMessage);
        return false;
    }
    number_of_steps[nbytes_read_number_of_steps - 1] = '\0';
    LOGF_INFO("Number of steps between open and close states : %s", cover_state);
    NumberOfStepsBeetweenOpenAndCloseState = sscanf(number_of_steps, "%d", &NumberOfStepsBeetweenOpenAndCloseState);
    if (NumberOfStepsBeetweenOpenAndCloseState == 0)
    {
        LOGF_ERROR("The number of steps is 0 meaning the falt panel may hit an obstacle. You should define opening and closing position first.",
                   "");
    }
    setNumberOfStepsStatusValue(NumberOfStepsBeetweenOpenAndCloseState);

    setLightBoxStatusAsSwitchedOff();

    LOGF_INFO("Handshake successful:%s", name);
    tcflush(PortFD, TCIOFLUSH);
    return true;
}

void WandererCover::updateCoverStatus(char* res)
{
    if (strcmp(res, "0") == 0)
    {
        setParkCapStatusAsClosed();
    }
    else if (strcmp(res, "1") == 0)
    {
        setParkCapStatusAsOpen();
    }
    else if (strcmp(res, "255") == 0)
    {
        LOGF_INFO("No cover status information available. You should first open/close the cover.", "");
    }
}

void WandererCover::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    // Get Light box properties
    isGetLightBoxProperties(dev);
}

bool WandererCover::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(&ParkCapSP);
        defineProperty(&LightSP);
        defineProperty(&LightIntensityNP);
        defineProperty(&StatusTP);
        defineProperty(&FirmwareTP);

        defineProperty(&ControlPositionPositiveDegreesConfigurationVP);
        defineProperty(&ControlPositionNegativeDegreesConfigurationVP);
        defineProperty(&DefinePositionConfigurationVP);

        updateLightBoxProperties();

        getStartupData();
    }
    else
    {
        deleteProperty(ParkCapSP.name);
        deleteProperty(LightSP.name);
        deleteProperty(LightIntensityNP.name);
        deleteProperty(StatusTP.name);
        deleteProperty(FirmwareTP.name);

        deleteProperty(ControlPositionPositiveDegreesConfigurationVP.name);
        deleteProperty(ControlPositionNegativeDegreesConfigurationVP.name);
        deleteProperty(DefinePositionConfigurationVP.name);

        updateLightBoxProperties();
    }
    return true;
}

const char *WandererCover::getDefaultName()
{
    return "Wanderer Cover v3";
}

bool WandererCover::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processLightBoxNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool WandererCover::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processLightBoxText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool WandererCover::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (processDustCapSwitch(dev, name, states, names, n))
            return true;

        if (processLightBoxSwitch(dev, name, states, names, n))
            return true;

        if (processConfigurationButtonSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool WandererCover::ISSnoopDevice(XMLEle *root)
{
    snoopLightBox(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool WandererCover::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    return saveLightBoxConfigItems(fp);
}

bool WandererCover::getStartupData()
{
    // // Closing cover
    // IUSaveText(&StatusT[0], "Closed");
    // IUResetSwitch(&ParkCapSP);
    // ParkCapS[0].s = ISS_ON;
    // ParkCapSP.s = IPS_OK;
    // LOG_INFO("Cover assumed as closed.");
    // IDSetSwitch(&ParkCapSP, nullptr);

    // // Switching off lamp
    // IUSaveText(&StatusT[1], "Off");
    // LightS[0].s = ISS_OFF;
    // LightS[1].s = ISS_ON;
    // IDSetSwitch(&LightSP, nullptr);
    // LightIntensityN[0].value = 0;
    // LOG_INFO("Light assumed as off.");
    // IDSetNumber(&LightIntensityNP, nullptr);

    return true;
}

IPState WandererCover::ParkCap()
{
    if (NumberOfStepsBeetweenOpenAndCloseState == 0)
    {
        LOGF_ERROR("The number of steps is 0 meaning the falt panel may hit an obstacle. You should define opening and closing position first.",
                   "");
        return IPS_ALERT;
    }

    if (isSimulation())
    {
        setParkCapStatusAsClosed();
        return IPS_OK;
    }

    // Not sure why the TTY sends crappy data there
    char response[20];
    if (!sendCommand(CLOSE_COVER_COMMAND, response, true))
        return IPS_ALERT;

    if (hasWandererSentAnError(response))
    {
        LOG_ERROR("You need to configure Open and closed position first in 'Dust cover configuration' tab.");
        return IPS_ALERT;
    }
    setParkCapStatusAsClosed();

    return IPS_OK;
}

void WandererCover::setParkCapStatusAsClosed()
{
    IUSaveText(&StatusT[0], "Closed");
    IUResetSwitch(&ParkCapSP);
    ParkCapS[0].s = ISS_ON;
    ParkCapSP.s = IPS_OK;
    LOG_INFO("Cover closed.");
    IDSetSwitch(&ParkCapSP, nullptr);
}

IPState WandererCover::UnParkCap()
{
    if (NumberOfStepsBeetweenOpenAndCloseState == 0)
    {
        LOG_ERROR("The number of steps is 0 meaning the falt panel may hit an obstacle. You should define opening and closing position first.");
        return IPS_ALERT;
    }

    if (isSimulation())
    {
        setParkCapStatusAsOpen();
        return IPS_OK;
    }

    // Not sure why the TTY sends crappy data there
    char response[20];
    if (!sendCommand(OPEN_COVER_COMMAND, response, true))
        return IPS_ALERT;

    if (hasWandererSentAnError(response))
    {
        displayConfigurationMessage();
        return IPS_ALERT;
    }

    setParkCapStatusAsOpen();
    return IPS_OK;
}

void WandererCover::setParkCapStatusAsOpen()
{
    IUSaveText(&StatusT[0], "Open");
    IUResetSwitch(&ParkCapSP);
    ParkCapS[1].s = ISS_ON;
    ParkCapSP.s = IPS_OK;
    LOG_INFO("Cover open.");
    IDSetSwitch(&ParkCapSP, nullptr);
}

bool WandererCover::EnableLightBox(bool enable)
{
    if (ParkCapS[1].s == ISS_ON)
    {
        LOG_ERROR("Cannot control light while cap is unparked.");
        return false;
    }

    if (enable)
    {
        return SetLightBoxBrightness(255);
    }
    else
    {
        return switchOffLightBox();
    }

    return false;
}

bool WandererCover::switchOffLightBox()
{
    if (isSimulation())
    {
        setLightBoxStatusAsSwitchedOff();
        return true;
    }
    char response[WANDERER_RESPONSE_SIZE];

    if (!sendCommand(TURN_OFF_LIGHT_PANEL_COMMAND, response, false))
        return false;

    setLightBoxStatusAsSwitchedOff();
    return true;
}

void WandererCover::setLightBoxStatusAsSwitchedOff()
{
    IUSaveText(&StatusT[1], "Off");
    LightS[0].s = ISS_OFF;
    LightS[1].s = ISS_ON;
    LightIntensityN[0].value = 0;
    IDSetNumber(&LightIntensityNP, nullptr);
    IDSetSwitch(&LightSP, nullptr);
    LOG_INFO("Light panel switched off");
}


bool WandererCover::SetLightBoxBrightness(uint16_t value)
{
    if (isSimulation())
    {
        setLightBoxBrightnesStatusToValue(value);
        return true;
    }
    char response[WANDERER_RESPONSE_SIZE];
    char command[3] = {0};
    snprintf(command, 3, "%03d\n", value);
    if (!sendCommand(command, response, false))
        return false;

    setLightBoxBrightnesStatusToValue(value);

    return true;
}

bool WandererCover::setCurrentPositionToOpenPosition()
{
    if (isSimulation())
    {
        LOG_INFO("Current position set to open position");
        NumberOfDegreesSinceLastOpenPositionSet = 0;
        return true;
    }
    char response[WANDERER_RESPONSE_SIZE];
    if (!sendCommand(SET_CURRENT_POSITION_TO_OPEN_POSITION, response, false))
        return false;

    NumberOfDegreesSinceLastOpenPositionSet = 0;

    return true;
}

bool WandererCover::setCurrentPositionToClosedPosition()
{
    int cumulative_angle_value = ((abs(NumberOfDegreesSinceLastOpenPositionSet) * 222.22) / 10) + 10000;

    if (isSimulation())
    {
        LOG_INFO("Current position set to closed position");
        LOGF_INFO("Sending cumulative angle of %d", cumulative_angle_value);
        setNumberOfStepsStatusValue(cumulative_angle_value);
        setParkCapStatusAsClosed();
        return true;
    }
    char response[WANDERER_RESPONSE_SIZE];
    if (!sendCommand(SET_CURRENT_POSITION_TO_CLOSED_POSITION, response, false))
        return false;

    if (!sendCommand(std::to_string(cumulative_angle_value).c_str(), response, false))
        return false;

    setNumberOfStepsStatusValue(cumulative_angle_value);

    setParkCapStatusAsClosed();

    return true;
}

void WandererCover::setLightBoxBrightnesStatusToValue(uint16_t value)
{
    LightIntensityN[0].value = value;
    IDSetNumber(&LightIntensityNP, nullptr);
    LOGF_INFO("Brightness set to %d.", value);
}

void WandererCover::setNumberOfStepsStatusValue(int value)
{
    LOGF_DEBUG("Current number of steps value configured between open and closed position : %d", value);
    NumberOfStepsBeetweenOpenAndCloseState = value;
    // NumberOfStepsBeetweenOpenAndCloseStateT[0].text = new char[5];
    // NumberOfStepsBeetweenOpenAndCloseStateT[0].text = (char*) std::to_string(value).c_str();
    // IDSetText(&NumberOfStepsBeetweenOpenAndCloseStateTP, nullptr);
}

bool WandererCover::sendCommand(std::string command, char *response, bool waitForAnswer)
{
    int nbytes_read = 0, nbytes_written = 0, rc = -1;
    std::string command_termination = "\n";
    LOGF_DEBUG("CMD: %s", command.c_str());
    if ((rc = tty_write_string(PortFD, (command + command_termination).c_str(), &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }

    if (waitForAnswer && (rc = tty_read_section(PortFD, response, 'A', COMMAND_WAITING_TIME, &nbytes_read)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Device read error: %s", errorMessage);
        return false;
    }
    LOGF_DEBUG("RESPONSE: %s", response);
    SetTimer(150);
    return true;
}

IPState WandererCover::moveDustCap(int degrees)
{
    if (degrees < -360 or degrees > 360)
    {
        LOGF_ERROR("Degrees must be between -360 and 360 :  %d", degrees);
        return IPS_ALERT;
    }
    if (isSimulation())
    {
        LOGF_INFO("Moving dust cap cover of %d degrees", degrees);
        NumberOfDegreesSinceLastOpenPositionSet += degrees;
        LOGF_INFO("Number of degrees since last open position set : %d", NumberOfDegreesSinceLastOpenPositionSet);
        return IPS_OK;
    }

    int stepping_offset = 100000;
    if (degrees < 0)
    {
        stepping_offset = -100000;
    }

    int command_value = (degrees * 222.22) + stepping_offset;

    char response[3];
    if (!sendCommand(std::to_string(command_value).c_str(), response, true))
        return IPS_ALERT;

    NumberOfDegreesSinceLastOpenPositionSet += degrees;
    LOGF_DEBUG("Number of degrees since last open position set : %d", NumberOfDegreesSinceLastOpenPositionSet);
    return IPS_OK;
}

bool WandererCover::processConfigurationButtonSwitch(const char *dev, const char *name, ISState *states, char *names[],
        int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // configuration of open state clicked
        if (!strcmp(ControlPositionPositiveDegreesConfigurationVP.name, name))
        {
            IUUpdateSwitch(&ControlPositionPositiveDegreesConfigurationVP, states, names, n);
            if (ControlPositionPositiveDegreesConfigurationV[PLUS_1_DEGREE].s )
            {
                moveDustCap(1);
                ControlPositionPositiveDegreesConfigurationV[PLUS_1_DEGREE].s = ISS_OFF;
            }
            else if (ControlPositionPositiveDegreesConfigurationV[PLUS_10_DEGREE].s == ISS_ON)
            {
                moveDustCap(10);
                ControlPositionPositiveDegreesConfigurationV[PLUS_10_DEGREE].s = ISS_OFF;
            }
            else if (ControlPositionPositiveDegreesConfigurationV[PLUS_50_DEGREE].s == ISS_ON)
            {
                moveDustCap(50);
                ControlPositionPositiveDegreesConfigurationV[PLUS_50_DEGREE].s = ISS_OFF;
            }
            IDSetSwitch(&ControlPositionPositiveDegreesConfigurationVP, nullptr);

            return true;
        }

        if (!strcmp(ControlPositionNegativeDegreesConfigurationVP.name, name))
        {
            IUUpdateSwitch(&ControlPositionNegativeDegreesConfigurationVP, states, names, n);
            if (ControlPositionNegativeDegreesConfigurationV[MINUS_1_DEGREE].s )
            {
                moveDustCap(-1);
                ControlPositionNegativeDegreesConfigurationV[MINUS_1_DEGREE].s = ISS_OFF;
            }
            else if (ControlPositionNegativeDegreesConfigurationV[MINUS_10_DEGREE].s == ISS_ON)
            {
                moveDustCap(-10);
                ControlPositionNegativeDegreesConfigurationV[MINUS_10_DEGREE].s = ISS_OFF;
            }
            else if (ControlPositionNegativeDegreesConfigurationV[MINUS_50_DEGREE].s == ISS_ON)
            {
                moveDustCap(-50);
                ControlPositionNegativeDegreesConfigurationV[MINUS_50_DEGREE].s = ISS_OFF;
            }
            IDSetSwitch(&ControlPositionNegativeDegreesConfigurationVP, nullptr);

            return true;
        }

        if (!strcmp(DefinePositionConfigurationVP.name, name))
        {
            IUUpdateSwitch(&DefinePositionConfigurationVP, states, names, n);
            if (DefinePositionConfigurationV[SET_CURRENT_POSITION_OPEN].s == ISS_ON)
            {
                setCurrentPositionToOpenPosition();
                DefinePositionConfigurationV[SET_CURRENT_POSITION_OPEN].s = ISS_OFF;
            }
            else if (DefinePositionConfigurationV[SET_CURRENT_POSITION_CLOSE].s == ISS_ON)
            {
                setCurrentPositionToClosedPosition();
                DefinePositionConfigurationV[SET_CURRENT_POSITION_CLOSE].s = ISS_OFF;
            }
            IDSetSwitch(&DefinePositionConfigurationVP, nullptr);
            return true;
        }
    }

    return false;
}

bool WandererCover::hasWandererSentAnError(char* response)
{
    const std::string response_string( response );
    size_t found = response_string.find("Error");
    if (found != std::string::npos)
        return true;

    return false;
}

void WandererCover::displayConfigurationMessage()
{
    LOG_WARN(" - Once thesest steps are done, the dust cover will remember the park and unpark positions.");
    LOG_WARN(" - Click on 'Set current position as close' to define the park position");
    LOG_WARN(" - Use again the select list to move your cover panel in close position on the scope");
    LOG_WARN(" - Click on 'Set current position as open' to define the unpark position");
    LOG_WARN(" - Use the select controler to move your panel to the open position");
    LOG_WARN("In order to do so, go to 'Dust cover configurtation' tab and do the following steps :");
    LOG_WARN("Before first use, or when you change your setup, you need to configure Open and closed position first in 'Dust cover configurtation' tab.");
}