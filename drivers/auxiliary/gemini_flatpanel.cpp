#include "config.h"
#include "gemini_flatpanel.h"
#include "indicom.h"
#include <termios.h>
#include <functional>
#include <vector>
#include <cstdlib>


static std::unique_ptr<GeminiFlatpanel> mydriver(new GeminiFlatpanel());

GeminiFlatpanel::GeminiFlatpanel() : LightBoxInterface(this), DustCapInterface(this)
{
    setVersion(1, 2);
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


    // Driver interface will be set dynamically in Handshake() based on device capabilities
    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);

    // Initialize device selection property
    DeviceTypeSP.fill(
        getDeviceName(),
        "DEVICE_TYPE",
        "Device Type",
        CONNECTION_TAB,
        IP_RW,
        ISR_1OFMANY,
        60,
        IPS_IDLE
    );
    DeviceTypeSP[DEVICE_AUTO].fill("AUTO", "Auto-detect", ISS_ON);
    DeviceTypeSP[DEVICE_REV1].fill("REV1", "Revision 1", ISS_OFF);
    DeviceTypeSP[DEVICE_REV2].fill("REV2", "Revision 2", ISS_OFF);
    DeviceTypeSP[DEVICE_LITE].fill("LITE", "Lite", ISS_OFF);
    DeviceTypeSP.load();

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
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Hide device selection when connected
        deleteProperty(DeviceTypeSP);

        defineProperty(StatusTP);
        defineProperty(ConfigurationTP);

        // Only register advanced features if supported by the current adapter
        if (adapter && adapter->supportsBeep())
        {
            defineProperty(BeepSP);
        }
        if (adapter && adapter->supportsBrightnessMode())
        {
            defineProperty(BrightnessModeSP);
        }

        // Only register dust cap movement controls if supported
        if (adapter && adapter->supportsDustCap())
        {
            defineProperty(ConfigureSP);
            defineProperty(ClosedPositionSP);
            defineProperty(SetClosedSP);
            defineProperty(OpenPositionSP);
            defineProperty(SetOpenSP);
        }
    }
    else
    {
        // Show device selection when disconnected
        defineProperty(DeviceTypeSP);

        deleteProperty(StatusTP);
        deleteProperty(ConfigurationTP);

        // Only delete properties that were defined
        if (adapter && adapter->supportsBeep())
        {
            deleteProperty(BeepSP);
        }
        if (adapter && adapter->supportsBrightnessMode())
        {
            deleteProperty(BrightnessModeSP);
        }

        // Only delete dust cap properties that were defined
        if (adapter && adapter->supportsDustCap())
        {
            deleteProperty(ConfigureSP);
            deleteProperty(ClosedPositionSP);
            deleteProperty(SetClosedSP);
            deleteProperty(OpenPositionSP);
            deleteProperty(SetOpenSP);
        }
    }
    bool result = LI::updateProperties() && INDI::DefaultDevice::updateProperties();

    // Only update dust cap properties if the device supports dust cap functionality
    if (adapter && adapter->supportsDustCap())
    {
        result = result && DI::updateProperties();
    }

    return result;
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
        // Handle device type selection
        if (DeviceTypeSP.isNameMatch(name))
        {
            DeviceTypeSP.update(states, names, n);
            DeviceTypeSP.setState(IPS_OK);
            DeviceTypeSP.apply();
            saveConfig(DeviceTypeSP);
            return true;
        }

        if (LI::processSwitch(dev, name, states, names, n))
        {
            return true;
        }
        if (adapter && adapter->supportsDustCap() && DI::processSwitch(dev, name, states, names, n))
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

    // Save device type selection
    DeviceTypeSP.save(fp);

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

    if (!adapter || !adapter->supportsBeep())
    {
        LOG_WARN("Beep functionality not supported by this device.");
        BeepSP.setState(IPS_ALERT);
        BeepSP.apply();
        return;
    }

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

    if (!adapter || !adapter->supportsBrightnessMode())
    {
        LOG_WARN("Brightness mode selection not supported by this device.");
        BrightnessModeSP.setState(IPS_ALERT);
        BrightnessModeSP.apply();
        return;
    }

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

    // Use adapter for both real hardware and simulation
    error |= !getStatus(&coverStatus, &lightStatus, &motorStatus);
    error |= !getBrightness(&brightness);

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
    return setBrightness((int)value);
}

bool GeminiFlatpanel::EnableLightBox(bool enable)
{
    if (!validateOperation())
    {
        return false;
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
        // Use simulation adapter (can simulate both Rev1 and Rev2 features)
        adapter = std::make_unique<GeminiFlatpanelSimulationAdapter>(true); // true = Rev2 features
        deviceRevision = adapter->getRevision();
        commandTerminator = adapter->getCommandTerminator();

        // Set driver interface based on device capabilities
        if (adapter && adapter->supportsDustCap())
        {
            setDriverInterface(getDriverInterface() | DUSTCAP_INTERFACE);
            syncDriverInfo();
        }

        // Get config status from adapter
        int adapterConfigStatus;
        if (adapter->getConfigStatus(&adapterConfigStatus))
        {
            configStatus = adapterConfigStatus;
            updateConfigStatus();
        }

        LOGF_INFO("Connected successfully to simulated %s.", getDeviceName());
        return true;
    }

    PortFD = serialConnection->getPortFD();

    // Check if user has selected a specific device type
    int selectedDeviceType = DeviceTypeSP.findOnSwitchIndex();

    // List of adapter types to try in order
    struct AdapterInfo
    {
        std::function<std::unique_ptr<GeminiFlatpanelAdapter>()> factory;
        std::string name;
        int deviceType;
    };

    std::vector<AdapterInfo> adapterTypes;

    if (selectedDeviceType == DEVICE_AUTO)
    {
        // Auto-detect: try all adapters in order
        adapterTypes =
        {
            {[]() { return std::make_unique<GeminiFlatpanelRev1Adapter>(); }, "Rev1", DEVICE_REV1},
            {[]() { return std::make_unique<GeminiFlatpanelRev2Adapter>(); }, "Rev2", DEVICE_REV2},
            {[]() { return std::make_unique<GeminiFlatpanelLiteAdapter>(); }, "Lite", DEVICE_LITE}
        };
    }
    else
    {
        // Use only the selected adapter type
        switch (selectedDeviceType)
        {
            case DEVICE_REV1:
                adapterTypes = {{[]() { return std::make_unique<GeminiFlatpanelRev1Adapter>(); }, "Rev1", DEVICE_REV1}};
                break;
            case DEVICE_REV2:
                adapterTypes = {{[]() { return std::make_unique<GeminiFlatpanelRev2Adapter>(); }, "Rev2", DEVICE_REV2}};
                break;
            case DEVICE_LITE:
                adapterTypes = {{[]() { return std::make_unique<GeminiFlatpanelLiteAdapter>(); }, "Lite", DEVICE_LITE}};
                break;
        }
    }

    // Try each adapter type until one succeeds
    bool connected = false;
    for (const auto &adapterInfo : adapterTypes)
    {
        auto testAdapter = adapterInfo.factory();
        testAdapter->setupCommunication(PortFD);

        if (testAdapter->ping())
        {
            adapter = std::move(testAdapter);
            deviceRevision = adapter->getRevision();
            commandTerminator = adapter->getCommandTerminator();

            // Log connection success with firmware version details
            int firmwareVersion = 0;
            if (adapter->getFirmwareVersion(&firmwareVersion))
            {
                LOGF_INFO("Connected successfully to %s.", getDeviceName());
                LOGF_INFO("Device revision: %s (Firmware v%d)", adapterInfo.name.c_str(), firmwareVersion);
            }
            else
            {
                LOGF_INFO("Connected successfully to %s.", getDeviceName());
                LOGF_INFO("Device revision: %s", adapterInfo.name.c_str());
            }

            connected = true;
            break;
        }
    }

    if (!connected)
    {
        LOG_ERROR("Handshake failed. Unable to communicate with the device.");
        return false;
    }

    // Set driver interface based on device capabilities
    if (adapter && adapter->supportsDustCap())
    {
        setDriverInterface(getDriverInterface() | DUSTCAP_INTERFACE);
        syncDriverInfo();
    }

    // Check config status using adapter
    int adapterConfigStatus;
    if (adapter->getConfigStatus(&adapterConfigStatus))
    {
        configStatus = adapterConfigStatus;
        updateConfigStatus();
        return true;
    }

    return false;
}

// Note: Protocol-specific methods moved to adapter classes

// Device command methods - now using adapter pattern
bool GeminiFlatpanel::getBrightness(int *const brightness)
{
    return adapter ? adapter->getBrightness(brightness) : false;
}

bool GeminiFlatpanel::setBrightness(int value)
{
    return adapter ? adapter->setBrightness(value) : false;
}

bool GeminiFlatpanel::lightOn()
{
    return adapter ? adapter->lightOn() : false;
}

bool GeminiFlatpanel::lightOff()
{
    return adapter ? adapter->lightOff() : false;
}

bool GeminiFlatpanel::openCover()
{
    return adapter ? adapter->openCover() : false;
}

bool GeminiFlatpanel::closeCover()
{
    return adapter ? adapter->closeCover() : false;
}

bool GeminiFlatpanel::setBeep(bool enable)
{
    return adapter ? adapter->setBeep(enable) : false;
}

bool GeminiFlatpanel::setBrightnessMode(int mode)
{
    return adapter ? adapter->setBrightnessMode(mode) : false;
}

bool GeminiFlatpanel::getStatus(int *const coverStatus, int *const lightStatus, int *const motorStatus)
{
    return adapter ? adapter->getStatus(coverStatus, lightStatus, motorStatus) : false;
}

bool GeminiFlatpanel::move(uint16_t value, int direction)
{
    return adapter ? adapter->move(value, direction) : false;
}

bool GeminiFlatpanel::setClosePosition()
{
    return adapter ? adapter->setClosePosition() : false;
}

bool GeminiFlatpanel::setOpenPosition()
{
    return adapter ? adapter->setOpenPosition() : false;
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
    int adapterConfigStatus;
    if (adapter && adapter->getConfigStatus(&adapterConfigStatus))
    {
        configStatus = adapterConfigStatus;
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
    else
    {
        LOG_WARN("Failed to get configuration status.");
        configStatus = GEMINI_CONFIG_NOTREADY;
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
    // Check if dust cap functionality is supported
    if (!adapter || !adapter->supportsDustCap())
    {
        LOG_WARN("Dust cap movement not supported by this device.");
        return;
    }

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

    // Use adapter for both real hardware and simulation
    move(steps, direction);

    cleanupSwitch(currentSwitch, switchIndex);
}

void GeminiFlatpanel::onSetPosition(int direction)
{
    // Check if dust cap functionality is supported
    if (!adapter || !adapter->supportsDustCap())
    {
        LOG_WARN("Dust cap position setting not supported by this device.");
        return;
    }

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
            setClosePosition();
            configStatus = GEMINI_CONFIG_OPEN;
            break;
        case GEMINI_DIRECTION_OPEN:
            LOG_INFO("Setting open position.");
            setOpenPosition();
            endConfiguration();
            break;
    }

    cleanupSwitch(currentSwitch, switchIndex);
}
