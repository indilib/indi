#include "gemini_flatpanel.h"
#include "indicom.h"
#include <termios.h>

static std::unique_ptr<GeminiFlatpanel> mydriver(new GeminiFlatpanel());

GeminiFlatpanel::GeminiFlatpanel() : LightBoxInterface(this), DustCapInterface(this)
{
    setVersion(1, 0);
}

const char *GeminiFlatpanel::getDefaultName()
{
    return "Gemini Flatpanel";
}

bool GeminiFlatpanel::initProperties()
{
    INDI::DefaultDevice::initProperties();

    initStatusProperties();
    initLimitsProperties();

    LI::initProperties(MAIN_CONTROL_TAB, LI::CAN_DIM);
    DI::initProperties(MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE | DUSTCAP_INTERFACE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

bool GeminiFlatpanel::updateProperties()
{
    if (isConnected())
    {
        defineProperty(StatusTP);
        defineProperty(ConfigurationTP);

        // Only register beep and brightness mode controls for revision 2 devices
        if (deviceRevision == GEMINI_REVISION_2)
        {
            defineProperty(BeepSP);
            defineProperty(BrightnessModeSP);
        }

        defineProperty(ConfigureSP);
        defineProperty(ClosedPositionSP);
        defineProperty(SetClosedSP);
        defineProperty(OpenPositionSP);
        defineProperty(SetOpenSP);
    }
    else
    {
        deleteProperty(StatusTP);
        deleteProperty(ConfigurationTP);

        // Only delete beep and brightness mode controls if they were defined (revision 2)
        if (deviceRevision == GEMINI_REVISION_2)
        {
            deleteProperty(BeepSP);
            deleteProperty(BrightnessModeSP);
        }

        deleteProperty(ConfigureSP);
        deleteProperty(ClosedPositionSP);
        deleteProperty(SetClosedSP);
        deleteProperty(OpenPositionSP);
        deleteProperty(SetOpenSP);
    }
    return LI::updateProperties() && DI::updateProperties() && INDI::DefaultDevice::updateProperties();
}

void GeminiFlatpanel::ISGetProperties(const char *deviceName)
{
    LI::ISGetProperties(deviceName);
    INDI::DefaultDevice::ISGetProperties(deviceName);
}

bool GeminiFlatpanel::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr and strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processSwitch(dev, name, states, names, n))
        {
            return true;
        }
        if (DI::processSwitch(dev, name, states, names, n))
        {
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool GeminiFlatpanel::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr and strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processNumber(dev, name, values, names, n))
        {
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool GeminiFlatpanel::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr and strcmp(dev, getDeviceName()) == 0)
    {
        if (LI::processText(dev, name, texts, names, n))
        {
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool GeminiFlatpanel::ISSnoopDevice(XMLEle *root)
{
    LI::snoop(root);
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool GeminiFlatpanel::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    return LI::saveConfigItems(fp);
}

void GeminiFlatpanel::initStatusProperties()
{
    StatusTP.fill(
        getDeviceName(),
        "STATUS",
        "Status",
        MAIN_CONTROL_TAB,
        IP_RO,
        0,
        IPS_IDLE);
    StatusTP[STATUS_COVER].fill("COVER", "Cover", nullptr);
    StatusTP[STATUS_LIGHT].fill("LIGHT", "Light", nullptr);
    StatusTP[STATUS_MOTOR].fill("MOTOR", "Motor", nullptr);

    ConfigurationTP.fill(
        getDeviceName(),
        "CONFIGURATION",
        "Configuration",
        MAIN_CONTROL_TAB,
        IP_RO,
        0,
        IPS_IDLE);
    ConfigurationTP[0].fill("CONFIGURATION", "Configuration", nullptr);

    // Add beep control
    BeepSP.fill(
        getDeviceName(),
        "BEEP",
        "Beep",
        MAIN_CONTROL_TAB,
        IP_RW,
        ISR_ATMOST1,
        0,
        IPS_IDLE);
    BeepSP[0].fill("BEEP_OFF", "Off", ISS_ON);
    BeepSP[1].fill("BEEP_ON", "On", ISS_OFF);
    BeepSP.onUpdate([this]()
    {
        onBeepChange();
    });

    // Add brightness mode control
    BrightnessModeSP.fill(
        getDeviceName(),
        "BRIGHTNESS_MODE",
        "Brightness Mode",
        MAIN_CONTROL_TAB,
        IP_RW,
        ISR_ATMOST1,
        0,
        IPS_IDLE);
    BrightnessModeSP[0].fill("MODE_LOW", "Low", ISS_ON);
    BrightnessModeSP[1].fill("MODE_HIGH", "High", ISS_OFF);
    BrightnessModeSP.onUpdate([this]()
    {
        onBrightnessModeChange();
    });
}

void GeminiFlatpanel::onBeepChange()
{
    bool enable = BeepSP[1].getState() == ISS_ON;
    if (setBeep(enable))
    {
        BeepSP.setState(IPS_OK);
    }
    else
    {
        BeepSP.setState(IPS_ALERT);
    }
    BeepSP.apply();
}

void GeminiFlatpanel::onBrightnessModeChange()
{
    int mode = BrightnessModeSP[1].getState() == ISS_ON ? GEMINI_BRIGHTNESS_MODE_HIGH : GEMINI_BRIGHTNESS_MODE_LOW;
    if (setBrightnessMode(mode))
    {
        BrightnessModeSP.setState(IPS_OK);
    }
    else
    {
        BrightnessModeSP.setState(IPS_ALERT);
    }
    BrightnessModeSP.apply();
}

void GeminiFlatpanel::initLimitsProperties()
{
    ConfigureSP.fill(
        getDeviceName(),
        "CONFIGURE",
        "Configure",
        MOTION_TAB,
        IP_RW,
        ISR_ATMOST1,
        0,
        IPS_IDLE);
    ConfigureSP[0].fill("CONFIGURE", "Configure", ISS_OFF);
    ConfigureSP.onUpdate([this]
    { startConfiguration(); });

    ClosedPositionSP.fill(
        getDeviceName(),
        "CLOSE_LIMIT",
        "Close position",
        MOTION_TAB,
        IP_RW,
        ISR_ATMOST1,
        0,
        IPS_IDLE);
    ClosedPositionSP[MOVEMENT_LIMITS_45].fill("45", "-45", ISS_OFF);
    ClosedPositionSP[MOVEMENT_LIMITS_10].fill("10", "-10", ISS_OFF);
    ClosedPositionSP[MOVEMENT_LIMITS_01].fill("1", "-1", ISS_OFF);
    ClosedPositionSP.onUpdate([this]
    { onMove(GEMINI_DIRECTION_CLOSE); });

    SetClosedSP.fill(
        getDeviceName(),
        "SET_CLOSE_LIMIT",
        "Set closed",
        MOTION_TAB,
        IP_RW,
        ISR_ATMOST1,
        0,
        IPS_IDLE);
    SetClosedSP[0].fill("SET_CLOSED", "Set closed", ISS_OFF);
    SetClosedSP.onUpdate([this]
    { onSetPosition(GEMINI_DIRECTION_CLOSE); });

    OpenPositionSP.fill(
        getDeviceName(),
        "OPEN_LIMIT",
        "Open position",
        MOTION_TAB,
        IP_RW,
        ISR_ATMOST1,
        0,
        IPS_IDLE);
    OpenPositionSP[MOVEMENT_LIMITS_45].fill("45", "45", ISS_OFF);
    OpenPositionSP[MOVEMENT_LIMITS_10].fill("10", "10", ISS_OFF);
    OpenPositionSP[MOVEMENT_LIMITS_01].fill("1", "1", ISS_OFF);
    OpenPositionSP.onUpdate([this]
    { onMove(GEMINI_DIRECTION_OPEN); });

    SetOpenSP.fill(
        getDeviceName(),
        "SET_OPEN_LIMIT",
        "Set open",
        MOTION_TAB,
        IP_RW,
        ISR_ATMOST1,
        0,
        IPS_IDLE);
    SetOpenSP[0].fill("SET_OPEN", "Set open", ISS_OFF);
    SetOpenSP.onUpdate([this]
    { onSetPosition(GEMINI_DIRECTION_OPEN); });
}

void GeminiFlatpanel::TimerHit()
{
    if (!isConnected() && !isSimulation())
    {
        return;
    }

    int coverStatus = 0, lightStatus = 0, motorStatus = 0, brightness = 0;
    bool error = false;

    if (isSimulation())
    {
        coverStatus = simulationValues[SIMULATION_COVER];
        lightStatus = simulationValues[SIMULATION_LIGHT];
        motorStatus = simulationValues[SIMULATION_MOTOR];
        brightness = simulationValues[SIMULATION_BRIGHTNESS];
    }
    else
    {
        error |= !getStatus(&coverStatus, &lightStatus, &motorStatus);
        error |= !getBrightness(&brightness);
    }

    if (error)
    {
        return;
    }

    if (updateBrightness(brightness))
    {
        LightIntensityNP.apply();
    }

    bool statusUpdated = updateCoverStatus(coverStatus) ||
                         updateLightStatus(lightStatus) ||
                         updateMotorStatus(motorStatus);

    if (statusUpdated)
    {
        StatusTP.apply();
    }

    if (motorStatus == GEMINI_MOTOR_STATUS_RUNNING && (coverStatus == GEMINI_COVER_STATUS_TIMED_OUT
            || coverStatus == GEMINI_COVER_STATUS_MOVING))
    {
        LOG_WARN("Motor running with unknown cover status.");
        configStatus = GEMINI_CONFIG_NOTREADY;
        updateConfigStatus();
    }

    if (configStatus == GEMINI_CONFIG_READY)
    {
        SetTimer(getCurrentPollingPeriod());
    }
}

// LightBoxInterface methods
bool GeminiFlatpanel::SetLightBoxBrightness(uint16_t value)
{
    if (!validateOperation())
    {
        return false;
    }
    if (isSimulation())
    {
        LOGF_INFO("Setting brightness to %d.", value);
        simulationValues[SIMULATION_BRIGHTNESS] = (int)value;
        return true;
    }
    return setBrightness((int)value);
}

bool GeminiFlatpanel::EnableLightBox(bool enable)
{
    if (!validateOperation())
    {
        return false;
    }
    if (isSimulation())
    {
        LOGF_INFO("Turning light %s.", enable ? "on" : "off");
        simulationValues[SIMULATION_LIGHT] = enable ? GEMINI_LIGHT_STATUS_ON : GEMINI_LIGHT_STATUS_OFF;
        return true;
    }
    return enable ? lightOn() : lightOff();
}

// DustCapInterface methods
IPState GeminiFlatpanel::ParkCap()
{
    if (!validateOperation())
    {
        return IPS_ALERT;
    }
    if (isSimulation())
    {
        LOG_INFO("Parking dust cap.");
        simulationValues[SIMULATION_COVER] = GEMINI_COVER_STATUS_CLOSED;
        return IPS_OK;
    }
    ParkCapSP.setState(IPS_BUSY);
    ParkCapSP.apply();
    if (closeCover())
    {
        return IPS_OK;
    }
    return IPS_ALERT;
}

IPState GeminiFlatpanel::UnParkCap()
{
    if (!validateOperation())
    {
        return IPS_ALERT;
    }
    if (isSimulation())
    {
        LOG_INFO("Unparking dust cap.");
        simulationValues[SIMULATION_COVER] = GEMINI_COVER_STATUS_OPEN;
        return IPS_OK;
    }
    ParkCapSP.setState(IPS_BUSY);
    ParkCapSP.apply();
    if (openCover())
    {
        return IPS_OK;
    }
    return IPS_ALERT;
}

IPState GeminiFlatpanel::AbortCap()
{
    return IPS_ALERT;
}

// Handshake method
bool GeminiFlatpanel::Handshake()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        configStatus = GEMINI_CONFIG_READY;
        updateConfigStatus();
        return true;
    }

    PortFD = serialConnection->getPortFD();

    int firmwareVersion = 0;
    deviceRevision = GEMINI_REVISION_UNKNOWN;

    if (deviceRevision == GEMINI_REVISION_UNKNOWN)
    {
        commandTerminator = '\n';
        if (pingRevision1())
        {
            deviceRevision = GEMINI_REVISION_1;
            LOGF_INFO("Connected successfully to %s.", getDeviceName());
            LOGF_INFO("Device revision: %d", deviceRevision);
        }
    }

    if (deviceRevision == GEMINI_REVISION_UNKNOWN)
    {
        commandTerminator = '#';
        if (pingRevision2() && getFirmwareVersion(&firmwareVersion))
        {
            deviceRevision = GEMINI_REVISION_2;
            LOGF_INFO("Connected successfully to %s.", getDeviceName());
            LOGF_INFO("Device revision: %d (Firmware v%d)", deviceRevision, firmwareVersion);
        }
    }

    if (deviceRevision == GEMINI_REVISION_UNKNOWN)
    {
        LOG_ERROR("Handshake failed. Unable to communicate with the device.");
        return false;
    }

    if (getConfigStatus())
    {
        updateConfigStatus();
        return true;
    }

    return false;
}

// Commands
bool GeminiFlatpanel::formatCommand(char commandLetter, char *commandString, int value)
{
    if (commandString == nullptr)
    {
        LOG_ERROR("Command string buffer is null");
        return false;
    }

    if (deviceRevision == GEMINI_REVISION_1)
    {
        // Rev1 format: >X000# (no value) or >XNNN# (with value) where X is command letter and NNN is 3-digit value
        if (value == NO_VALUE)
        {
            // No value provided, pad with 000
            snprintf(commandString, MAXRBUF, ">%c000#", commandLetter);
        }
        else
        {
            // Value provided, pad to 3 digits
            snprintf(commandString, MAXRBUF, ">%c%03d#", commandLetter, value);
        }
    }
    else if (deviceRevision == GEMINI_REVISION_2)
    {
        // Rev2 format: >X# (no value) or >Xnnn# (with value) where X is command letter and nnn is value
        if (value == 0)
        {
            // No value provided, no padding
            snprintf(commandString, MAXRBUF, ">%c#", commandLetter);
        }
        else
        {
            // Value provided, no padding
            snprintf(commandString, MAXRBUF, ">%c%d#", commandLetter, value);
        }
    }
    else
    {
        LOG_ERROR("Unknown device revision");
        return false;
    }

    return true;
}

bool GeminiFlatpanel::extractIntValue(const char *response, int startPos, int *value)
{
    if (response == nullptr || value == nullptr)
    {
        LOG_ERROR("Invalid parameters for extractIntValue.");
        return false;
    }

    // Revision 1 responses have a 3-digit numeric part 0 padded
    int length = 3;
    if (deviceRevision == GEMINI_REVISION_2)
    {
        const char *terminator = strchr(response, '#');
        if (terminator == nullptr)
        {
            LOG_ERROR("Missing # terminator in response.");
            return false;
        }

        // Calculate the length of the numeric part
        length = terminator - (response + startPos);
    }

    if (length <= 0)
    {
        LOG_ERROR("Invalid numeric value length in response.");
        return false;
    }

    // Extract the numeric part
    char value_str[MAXRBUF] = {0};
    strncpy(value_str, response + startPos, length);
    *value = atoi(value_str);

    return true;
}

bool GeminiFlatpanel::sendCommand(const char *command, char *response, int timeout, bool log)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    char cmd_with_terminator[MAXRBUF];

    snprintf(cmd_with_terminator, MAXRBUF, "%s\r", command);

    LOGF_DEBUG("CMD <%s>", command);

    tcflush(PortFD, TCIOFLUSH);
    if ((rc = tty_write_string(PortFD, command, &nbytes_written)) != TTY_OK)
    {
        if (log)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Serial write error: %s.", errstr);
        }
        return false;
    }

    if (response == nullptr)
        return true;

    if ((rc = tty_nread_section(PortFD, response, MAXRBUF, commandTerminator, timeout, &nbytes_read)) != TTY_OK)
    {
        if (log)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Serial read error: %s.", errstr);
        }
        return false;
    }

    // Remove trailing newline character if present
    if (nbytes_read > 0 && response[nbytes_read - 1] == '\n')
    {
        response[nbytes_read - 1] = '\0';
    }
    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("RES <%s>", response);

    if (response[0] != '*' || response[1] != command[1])
    {
        LOG_ERROR("Invalid response.");
        return false;
    }

    return true;
}

bool GeminiFlatpanel::pingRevision1()
{
    // This command is only supported by revision 1 devices
    // It is used to check if the device is a revision 1 device
    char response[MAXRBUF] = {0};

    if (!sendCommand(">P000#", response, SERIAL_TIMEOUT_SEC, false))
    {
        return false;
    }

    if (strcmp(response, "*P99OOO") != 0)
    {
        return false;
    }

    return true;
}

bool GeminiFlatpanel::pingRevision2()
{
    // This command is only supported by revision 2 devices
    // It is used to check if the device is a revision 2 device
    char response[MAXRBUF] = {0};

    if (!sendCommand(">H#", response, SERIAL_TIMEOUT_SEC, false))
    {
        return false;
    }

    if (strcmp(response, "*HGeminiFlatPanel#") != 0)
    {
        return false;
    }

    return true;
}

bool GeminiFlatpanel::getFirmwareVersion(int *version)
{
    // This command is only supported by revision 2 devices
    // It is used to get the firmware version
    char response[MAXRBUF] = {0};

    // Initialize version to 0 in case of errors
    *version = 0;

    if (!sendCommand(">V#", response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        LOG_ERROR("Invalid firmware version response.");
        return false;
    }

    if (response[0] != '*' || response[1] != 'V')
    {
        LOG_ERROR("Invalid firmware version response.");
        return false;
    }

    int firmwareVersion;
    if (!extractIntValue(response, 2, &firmwareVersion))
    {
        return false;
    }

    if (firmwareVersion < 402)
    {
        LOG_ERROR("Firmware version too old. Please update to version 402 or later.");
        return false;
    }

    *version = firmwareVersion;
    return true;
}

bool GeminiFlatpanel::getConfigStatus()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('A', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        LOG_ERROR("Invalid config status response.");
        return false;
    }

    if (response[0] != '*' || response[1] != 'A')
    {
        LOG_ERROR("Invalid config status response.");
        return false;
    }

    configStatus = response[2] - '0';
    return true;
}

bool GeminiFlatpanel::getBrightness(int *const brightness)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('J', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        LOG_ERROR("Invalid brightness response.");
        return false;
    }

    if (response[0] != '*' || response[1] != 'J')
    {
        LOG_ERROR("Invalid brightness response.");
        return false;
    }

    int index = deviceRevision == GEMINI_REVISION_1 ? 4 : 2;

    return extractIntValue(response, index, brightness);
}

bool GeminiFlatpanel::setBrightness(int value)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (value > GEMINI_MAX_BRIGHTNESS)
    {
        LOG_WARN("Brightness level out of range, setting it to 255.");
        value = GEMINI_MAX_BRIGHTNESS;
    }

    if (value < GEMINI_MIN_BRIGHTNESS)
    {
        LOG_WARN("Brightness level out of range, setting it to 0.");
        value = GEMINI_MIN_BRIGHTNESS;
    }

    if (!formatCommand('B', command, value))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        LOG_ERROR("Invalid brightness response.");
        return false;
    }

    if (response[0] != '*' || response[1] != 'B')
    {
        LOG_ERROR("Invalid brightness response.");
        return false;
    }

    int response_value;
    int index = deviceRevision == GEMINI_REVISION_1 ? 4 : 2;
    if (!extractIntValue(response, index, &response_value))
    {
        return false;
    }

    return (response_value == value);
}

bool GeminiFlatpanel::lightOn()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('L', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    return (strlen(response) >= 3 && response[0] == '*' && response[1] == 'L');
}

bool GeminiFlatpanel::lightOff()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('D', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    return (strlen(response) >= 3 && response[0] == '*' && response[1] == 'D');
}

bool GeminiFlatpanel::openCover()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('O', command))
    {
        return false;
    }

    if (!sendCommand(command, response, SERIAL_TIMEOUT_SEC_LONG))
    {
        return false;
    }

    if (deviceRevision == GEMINI_REVISION_1)
    {
        return (strcmp(response, "*O99OOO") == 0);
    }
    else
    {
        return (strcmp(response, "*OOpened#") == 0);
    }
}

bool GeminiFlatpanel::closeCover()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('C', command))
    {
        return false;
    }

    if (!sendCommand(command, response, SERIAL_TIMEOUT_SEC_LONG))
    {
        return false;
    }

    if (deviceRevision == GEMINI_REVISION_1)
    {
        return (strcmp(response, "*C99OOO") == 0);
    }
    else
    {
        return (strcmp(response, "*CClosed#") == 0);
    }
}

bool GeminiFlatpanel::setBeep(bool enable)
{
    char command[MAXRBUF] = {0};

    if (!formatCommand('T', command, enable ? GEMINI_BEEP_ON : GEMINI_BEEP_OFF))
    {
        return false;
    }

    return sendCommand(command, nullptr);
}

bool GeminiFlatpanel::setBrightnessMode(int mode)
{
    if (mode != GEMINI_BRIGHTNESS_MODE_LOW && mode != GEMINI_BRIGHTNESS_MODE_HIGH)
    {
        LOG_ERROR("Invalid brightness mode.");
        return false;
    }

    char command[MAXRBUF] = {0};

    if (!formatCommand('Y', command, mode))
    {
        return false;
    }

    return sendCommand(command, nullptr);
}

bool GeminiFlatpanel::getStatus(int *const coverStatus, int *const lightStatus, int *const motorStatus)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('S', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 7)
    {
        LOG_ERROR("Invalid status response.");
        return false;
    }

    if (response[0] != '*' || response[1] != 'S')
    {
        LOG_ERROR("Invalid status response.");
        return false;
    }

    // Check if device ID is valid (19 or 99)
    char id_str[3] = {response[2], response[3], '\0'};
    int id = atoi(id_str);
    if (id != 19 && id != 99)
    {
        LOG_ERROR("Invalid device ID in status response.");
        return false;
    }

    *motorStatus = response[4] - '0';
    *lightStatus = response[5] - '0';
    *coverStatus = response[6] - '0';

    return true;
}

bool GeminiFlatpanel::move(uint16_t value, int direction)
{
    char command[MAXRBUF] = {0};

    if (direction == -1)
    {
        snprintf(command, sizeof(command), ">M-%02d#", value);
    }
    else
    {
        snprintf(command, sizeof(command), ">M%03d#", value);
    }

    char response[MAXRBUF] = {0};

    if (!sendCommand(command, response, 30))
    {
        return false;
    }

    LOGF_INFO("Move response: %s", response);
    return true;
}

bool GeminiFlatpanel::setClosePosition()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('F', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    LOGF_INFO("Close position: %s", response);

    return true;
}

bool GeminiFlatpanel::setOpenPosition()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('E', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    LOGF_INFO("Open position: %s", response);

    return true;
}

// Status update and transitions
bool GeminiFlatpanel::updateCoverStatus(char coverStatus)
{
    bool statusUpdated = false;

    if (coverStatus != prevCoverStatus)
    {
        prevCoverStatus = coverStatus;
        statusUpdated = true;

        switch (coverStatus)
        {
            case GEMINI_COVER_STATUS_MOVING:
                StatusTP[STATUS_COVER].setText("Moving");
                ParkCapSP.reset();
                ParkCapSP.setState(IPS_BUSY);
                ParkCapSP.apply();
                break;
            case GEMINI_COVER_STATUS_CLOSED:
                StatusTP[STATUS_COVER].setText("Closed");
                if (ParkCapSP.getState() == IPS_BUSY || ParkCapSP.getState() == IPS_IDLE)
                {
                    ParkCapSP.reset();
                    ParkCapSP[CAP_PARK].setState(ISS_ON);
                    ParkCapSP.setState(IPS_OK);
                    LOG_INFO("Cover closed.");
                    ParkCapSP.apply();
                }
                break;
            case GEMINI_COVER_STATUS_OPEN:
                StatusTP[STATUS_COVER].setText("Open");
                if (ParkCapSP.getState() == IPS_BUSY || ParkCapSP.getState() == IPS_IDLE)
                {
                    ParkCapSP.reset();
                    ParkCapSP[CAP_UNPARK].setState(ISS_ON);
                    ParkCapSP.setState(IPS_OK);
                    LOG_INFO("Cover open.");
                    ParkCapSP.apply();
                }
                break;
            case GEMINI_COVER_STATUS_TIMED_OUT:
                StatusTP[STATUS_COVER].setText("Timed Out");
                ParkCapSP.reset();
                ParkCapSP.setState(IPS_ALERT);
                LOG_ERROR("Cover operation timed out.");
                ParkCapSP.apply();
                break;
        }
    }

    return statusUpdated;
}

bool GeminiFlatpanel::updateLightStatus(char lightStatus)
{
    bool statusUpdated = false;

    if (lightStatus != prevLightStatus)
    {
        prevLightStatus = lightStatus;
        statusUpdated = true;

        switch (lightStatus)
        {
            case GEMINI_LIGHT_STATUS_OFF:
                StatusTP[STATUS_LIGHT].setText("Off");
                if (LightSP[0].getState() == ISS_ON)
                {
                    LightSP.reset();
                    LightSP[FLAT_LIGHT_OFF].setState(ISS_ON);
                    LightSP.apply();
                }
                break;
            case GEMINI_LIGHT_STATUS_ON:
                StatusTP[STATUS_LIGHT].setText("On");
                if (LightSP[1].getState() == ISS_ON)
                {
                    LightSP.reset();
                    LightSP[FLAT_LIGHT_ON].setState(ISS_ON);
                    LightSP.apply();
                }
                break;
        }
    }

    return statusUpdated;
}

bool GeminiFlatpanel::updateMotorStatus(char motorStatus)
{
    bool statusUpdated = false;

    if (motorStatus != prevMotorStatus)
    {
        prevMotorStatus = motorStatus;
        statusUpdated = true;

        switch (motorStatus)
        {
            case GEMINI_MOTOR_STATUS_STOPPED:
                StatusTP[STATUS_MOTOR].setText("Stopped");
                break;
            case GEMINI_MOTOR_STATUS_RUNNING:
                StatusTP[STATUS_MOTOR].setText("Running");
                break;
        }
    }

    return statusUpdated;
}

bool GeminiFlatpanel::updateBrightness(int brightness)
{
    bool statusUpdated = false;
    if (brightness != prevBrightness)
    {
        prevBrightness = brightness;
        statusUpdated = true;

        LightIntensityNP[0].setValue(brightness);
    }

    return statusUpdated;
}

void GeminiFlatpanel::updateConfigStatus()
{
    const char *configStatusText = nullptr;
    switch (configStatus)
    {
        case GEMINI_CONFIG_NOTREADY:
            configStatusText = "Not ready";
            break;
        case GEMINI_CONFIG_READY:
            configStatusText = "Ready";
            break;
        case GEMINI_CONFIG_OPEN:
            configStatusText = "Open";
            break;
        case GEMINI_CONFIG_CLOSED:
            configStatusText = "Closed";
            break;
    }
    ConfigurationTP[0].setText(configStatusText);
    ConfigurationTP.apply();
}

void GeminiFlatpanel::startConfiguration()
{
    configStatus = GEMINI_CONFIG_CLOSED;
}

void GeminiFlatpanel::endConfiguration()
{
    if (isSimulation())
    {
        LOG_INFO("Configuration completed successfully.");
        configStatus = GEMINI_CONFIG_READY;
    }
    else
    {
        getConfigStatus();
        if (configStatus == GEMINI_CONFIG_READY)
        {
            LOG_INFO("Configuration completed successfully.");
            SetTimer(getCurrentPollingPeriod());
        }
        else
        {
            LOGF_WARN("Invalid configuration status. Please restart configuration in %s tab.", MOTION_TAB);
            configStatus = GEMINI_CONFIG_NOTREADY;
        }
    }
    ConfigureSP.reset();
    ConfigureSP[0].setState(ISS_OFF);
    ConfigureSP.setState(IPS_IDLE);
    ConfigureSP.apply();
}

bool GeminiFlatpanel::validateOperation()
{
    if (configStatus != GEMINI_CONFIG_READY)
    {
        LOG_WARN("Flatpanel not ready. Click the configure button to start configuration.");
        return false;
    }
    return true;
}

bool GeminiFlatpanel::validateCalibrationOperation(int direction)
{
    if (configStatus == GEMINI_CONFIG_NOTREADY || configStatus == GEMINI_CONFIG_READY)
    {
        LOG_WARN("Click the configure button to start configuration.");
        return false;
    }
    if (configStatus == GEMINI_CONFIG_CLOSED && direction != GEMINI_DIRECTION_CLOSE)
    {
        LOG_WARN("Please set the closed configuration using the close position controls.");
        return false;
    }
    if (configStatus == GEMINI_CONFIG_OPEN && direction != GEMINI_DIRECTION_OPEN)
    {
        LOG_WARN("Please set the open configuration using the open position controls.");
        return false;
    }
    return true;
}

void GeminiFlatpanel::cleanupSwitch(INDI::PropertySwitch &currentSwitch, int switchIndex)
{
    currentSwitch[switchIndex].setState(ISS_OFF);
    currentSwitch.setState(IPS_IDLE);
    currentSwitch.apply();
}

void GeminiFlatpanel::onMove(int direction)
{
    // Determine the switch based solely on the direction.
    auto &currentSwitch = (direction == GEMINI_DIRECTION_CLOSE) ? ClosedPositionSP : OpenPositionSP;
    int switchIndex = currentSwitch.findOnSwitchIndex();

    if (!validateCalibrationOperation(direction))
    {
        cleanupSwitch(currentSwitch, switchIndex);
        return;
    }

    // Perform movement based on the current switch state.
    int steps = 0;
    switch (currentSwitch.findOnSwitchIndex())
    {
        case MOVEMENT_LIMITS_45:
            steps = 45;
            break;
        case MOVEMENT_LIMITS_10:
            steps = 10;
            break;
        case MOVEMENT_LIMITS_01:
            steps = 1;
            break;
    }

    if (isSimulation())
    {
        LOGF_INFO("Moving %d steps in %s direction.", steps, direction == GEMINI_DIRECTION_CLOSE ? "close" : "open");
    }
    else
    {
        move(steps, direction);
    }

    cleanupSwitch(currentSwitch, switchIndex);
}

void GeminiFlatpanel::onSetPosition(int direction)
{
    auto currentSwitch = (direction == GEMINI_DIRECTION_CLOSE) ? SetClosedSP : SetOpenSP;
    auto switchIndex = currentSwitch.findOnSwitchIndex();

    if (!validateCalibrationOperation(direction))
    {
        cleanupSwitch(currentSwitch, switchIndex);
        return;
    }

    // Set the position based on the given direction.
    switch (direction)
    {
        case GEMINI_DIRECTION_CLOSE:
            LOG_INFO("Close position set.");
            if (!isSimulation())
            {
                setClosePosition();
            }
            configStatus = GEMINI_CONFIG_OPEN;
            break;
        case GEMINI_DIRECTION_OPEN:
            LOG_INFO("Setting open position.");
            if (!isSimulation())
            {
                setOpenPosition();
            }
            endConfiguration();
            break;
    }

    cleanupSwitch(currentSwitch, switchIndex);
}
