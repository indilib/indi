#include "celestron_dewpower.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>
#include <chrono>

static std::unique_ptr<CelestronDewPower> celestronDewPower(new CelestronDewPower());

CelestronDewPower::CelestronDewPower() : INDI::DefaultDevice(), INDI::PowerInterface(this), INDI::WeatherInterface(this)
{
    setVersion(1, 0);
}

const char *CelestronDewPower::getDefaultName()
{
    return "Celestron Dew Power";
}

bool CelestronDewPower::initProperties()
{
    INDI::DefaultDevice::initProperties();
    WI::initProperties("Weather", "Weather Parameters");

    setDriverInterface(POWER_INTERFACE | WEATHER_INTERFACE);

    // Reboot
    RebootSP[0].fill("REBOOT", "Reboot Device", ISS_OFF);
    RebootSP.fill(getDefaultName(), "REBOOT_DEVICE", "Device", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                  60, IPS_IDLE);

    // Overall Power Consumption (remains as is, not part of INDI::Power)
    PowerConsumptionNP[0].fill("CONSUMPTION_AVG_AMPS", "Avg. Amps", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP[1].fill("CONSUMPTION_AMP_HOURS", "Amp Hours", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP[2].fill("CONSUMPTION_WATT_HOURS", "Watt Hours", "%4.2f", 0, 999, 100, 0);
    PowerConsumptionNP.fill(getDefaultName(), "POWER_CONSUMPTION", "Consumption",
                            MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    PowerStatusLP[POWER_STATUS_OVERCURRENT].fill("OVERCURRENT", "Overcurrent", IPS_IDLE);
    PowerStatusLP[POWER_STATUS_UNDERVOLTAGE].fill("UNDERVOLTAGE", "Under Voltage", IPS_IDLE);
    PowerStatusLP[POWER_STATUS_OVERVOLTAGE].fill("OVERVOLTAGE", "Over Voltage", IPS_IDLE);
    PowerStatusLP.fill(getDefaultName(), "POWER_STATUS", "Power Status", MAIN_CONTROL_TAB, IPS_IDLE);

    // Define weather parameters
    addParameter("WEATHER_AMBIENT_TEMPERATURE", "Ambient Temperature (C)", -50, 50, 15);
    addParameter("WEATHER_DEW_POINT", "Dew Point (C)", -50, 50, 15);
    addParameter("WEATHER_HUMIDITY", "Humidity (%)", 0, 100, 15);

    // Set critical weather parameters
    setCriticalParameter("WEATHER_AMBIENT_TEMPERATURE");

    AutoDewModeSP[0].fill("DEW_MODE_AMBIENT", "Above Ambient", ISS_OFF);
    AutoDewModeSP[1].fill("DEW_MODE_DEWPOINT", "Above Dew Point", ISS_ON);
    AutoDewModeSP.fill(getDefaultName(), "AUTO_DEW_MODE", "Auto Dew Mode", DEW_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    AutoDewTempNP[0].fill("AUTO_DEW_TEMP", "Temp. Offset", "%2.0f", 0, 20, 1, 5);
    AutoDewTempNP.fill(getDefaultName(), "AUTO_DEW_TEMP", "Auto Dew Temp. Offset", DEW_TAB, IP_RW, 60, IPS_IDLE);


    ////////////////////////////////////////////////////////////////////////////
    /// Serial Connection
    ////////////////////////////////////////////////////////////////////////////
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    AUXCommand::setDebugInfo(getDeviceName(), DBG_CAUX);

    return true;
}

IPState CelestronDewPower::updateWeather()
{
    AUXCommand cmd(PORTCTRL_GET_ENVIRONMENT, HC, DEW_POWER_CTRL);
    AUXCommand response;
    if (sendAUXCommand(cmd) && readAUXResponse(response))
    {
        if (response.command() == PORTCTRL_GET_ENVIRONMENT)
        {
            // RESP: <0:3 the ambient temperature in mC><4:7 the dew point in mC><8 relative humidity 0-100%>
            AUXBuffer data = response.getDataBuffer();
            if (data.size() >= 9)
            {
                int32_t ambientTemp_mC = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
                int32_t dewPoint_mC = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
                uint8_t humidity = data[8];

                setParameterValue("WEATHER_AMBIENT_TEMPERATURE", ambientTemp_mC / 1000.0);
                setParameterValue("WEATHER_DEW_POINT", dewPoint_mC / 1000.0);
                setParameterValue("WEATHER_HUMIDITY", humidity);

                lastEnvironmentData = data; // Update lastEnvironmentData
                return IPS_OK;
            }
        }
    }
    return IPS_ALERT; // Return ALERT if data fetching or parsing fails
}

bool CelestronDewPower::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(RebootSP);
        defineProperty(PowerConsumptionNP);
        defineProperty(PowerStatusLP);
        defineProperty(AutoDewModeSP);
        defineProperty(AutoDewTempNP);
        PI::updateProperties();
        WI::updateProperties();
    }
    else
    {
        deleteProperty(RebootSP);
        deleteProperty(PowerConsumptionNP);
        deleteProperty(PowerStatusLP);
        deleteProperty(AutoDewModeSP);
        deleteProperty(AutoDewTempNP);
        PI::updateProperties();
        WI::updateProperties();
    }

    return true;
}

bool CelestronDewPower::Handshake()
{
    PortFD = serialConnection->getPortFD();

    if (PortFD > 0)
    {
        serialConnection->setDefaultBaudRate(Connection::Serial::B_9600); // Assuming 9600 baud for Dew/Power
        if (!tty_set_speed(B9600))
        {
            LOG_ERROR("Cannot set serial speed to 9600 baud.");
            return false;
        }

        // wait for speed to settle
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        LOG_INFO("Setting serial speed to 9600 baud.");

        m_IsRTSCTS = detectRTSCTS(); // Detect RTS/CTS if applicable

        // Get version
        if (!getDewPowerControllerVersion())
        {
            LOG_ERROR("Failed to get device version.");
            return false;
        }

        // Get number of ports
        if (!getNumberOfPorts())
        {
            LOG_ERROR("Failed to get number of ports.");
            return false;
        }

        // Initialize lastPortInfoData and lastDewHeaterPortInfoData vectors
        lastPortInfoData.resize(m_NumPorts);
        lastDewHeaterPortInfoData.resize(m_NumPorts); // Max possible dew ports is total ports
        m_PortTypes.resize(m_NumPorts);

        // Query each port to determine its type and capabilities
        uint32_t capabilities = 0;
        for (uint8_t i = 0; i < m_NumPorts; ++i)
        {
            AUXCommand cmd(PORTCTRL_GET_PORT_INFO, APP, DEW_POWER_CTRL);
            cmd.setData(i, 1);
            AUXCommand response;
            if (sendAUXCommand(cmd) && readAUXResponse(response))
            {
                if (response.command() == PORTCTRL_GET_PORT_INFO)
                {
                    AUXBuffer data = response.getDataBuffer();
                    if (data.size() >= 7)
                    {
                        uint8_t portType = data[0];
                        m_PortTypes[i] = portType;
                        // Assuming portType 0 for DC, 1 for Dew, 2 for Variable, etc.
                        // This needs to be clarified from the protocol if different.
                        if (portType == 0) // DC Output
                        {
                            m_NumDCPorts++;
                            capabilities |= POWER_HAS_DC_OUT;
                            capabilities |= POWER_HAS_PER_PORT_CURRENT; // Assuming per-port current for DC
                        }
                        else if (portType == 1) // Dew Heater
                        {
                            m_NumDewPorts++;
                            capabilities |= POWER_HAS_DEW_OUT;
                            capabilities |= POWER_HAS_AUTO_DEW; // Assuming auto dew for dew ports
                            capabilities |= POWER_HAS_PER_PORT_CURRENT; // Assuming per-port current for Dew
                        }
                        else if (portType == 2) // Variable Voltage
                        {
                            m_NumVariablePorts++;
                            capabilities |= POWER_HAS_VARIABLE_OUT;
                        }
                        // Add other port types as needed (e.g., USB)
                    }
                }
            }
        }

        // Set overall capabilities
        capabilities |= POWER_HAS_VOLTAGE_SENSOR;
        capabilities |= POWER_HAS_OVERALL_CURRENT;
        capabilities |= POWER_HAS_LED_TOGGLE; // Assuming LED toggle is always present

        PI::SetCapability(capabilities);
        PI::initProperties(POWER_TAB, m_NumDCPorts, m_NumDewPorts, m_NumVariablePorts, m_NumDewPorts, m_NumUSBPorts);

        setupComplete = true;
        return true;
    }
    else
    {
        return false;
    }
}

void CelestronDewPower::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    // Poll for input power
    getInputPower();

    // Poll for port info for all detected ports
    for (uint8_t i = 0; i < m_NumPorts; ++i)
    {
        if (m_PortTypes[i] == 1) // Dew Heater
        {
            getDewHeaterPortInfo(i);
        }
        else // DC or Variable
        {
            getPortInfo(i);
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool CelestronDewPower::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDefaultName()))
    {
        if (RebootSP.isNameMatch(name))
        {
            RebootSP.setState(IPS_OK); // Placeholder, actual reboot command to be implemented
            RebootSP.apply();
            LOG_INFO("Rebooting device (not implemented yet)...");
            return true;
        }

        if (PI::processSwitch(dev, name, states, names, n))
            return true;

        if (AutoDewModeSP.isNameMatch(name))
        {
            AutoDewModeSP.update(states, names, n);
            AutoDewModeSP.setState(IPS_OK);
            AutoDewModeSP.apply();
            return true;
        }
    }
    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool CelestronDewPower::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{

    if (dev && !strcmp(dev, getDefaultName()))
    {
        if (PI::processNumber(dev, name, values, names, n))
            return true;

        if (WI::processNumber(dev, name, values, names, n))
            return true;

        if (AutoDewTempNP.isNameMatch(name))
        {
            AutoDewTempNP.update(values, names, n);
            AutoDewTempNP.setState(IPS_OK);
            AutoDewTempNP.apply();
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool CelestronDewPower::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDefaultName()))
    {
        if (PI::processText(dev, name, texts, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool CelestronDewPower::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    PI::saveConfigItems(fp);
    WI::saveConfigItems(fp);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronDewPower::processResponse(AUXCommand &m)
{
    m.logResponse();

    if (m.destination() == APP)
    {
        switch (m.command())
        {
            case PORTCTRL_GET_VERSION:
                // Version is handled in getDewPowerControllerVersion
                break;
            case PORTCTRL_GET_NUMBER_OF_PORTS:
                m_NumPorts = m.getData();
                break;
            case PORTCTRL_GET_INPUT_POWER:
                // Handled in getInputPower
                break;
            case PORTCTRL_GET_PORT_INFO:
                // Handled in getPortInfo
                break;
            case PORTCTRL_GET_DH_PORT_INFO:
                // Handled in getDewHeaterPortInfo
                break;
            case PORTCTRL_GET_ENVIRONMENT:
                // Handled by updateWeather()
                break;
            case PORTCTRL_SET_PORT_ENABLED:
            case PORTCTRL_SET_PORT_VOLTAGE:
            case PORTCTRL_DH_ENABLE_AUTO:
            case PORTCTRL_DH_ENABLE_MANUAL:
            case PORTCTRL_SET_LED_BRIGHTNESS:
                // These are "set" commands, no specific data to process in response beyond success/failure
                break;
            default:
                break;
        }
    }
    else
    {
        DEBUGF(DBG_CAUX, "Got msg not for me (%s). Ignoring.", m.moduleName(m.destination()));
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronDewPower::serialReadResponse(AUXCommand c)
{
    int n;
    unsigned char buf[32];
    char hexbuf[24];
    AUXCommand cmd;

    INDI_UNUSED(c);

    // We are not connected. Nothing to do.
    if ( PortFD <= 0 )
        return false;

    // if connected to AUX or PC ports, receive AUX command response.
    // search for packet preamble (0x3b)
    do
    {
        if (aux_tty_read((char * )buf, 1, READ_TIMEOUT, &n) != TTY_OK)
            return false;
    }
    while (buf[0] != 0x3b);

    // packet preamble is found, now read packet length.
    if (aux_tty_read((char * )(buf + 1), 1, READ_TIMEOUT, &n) != TTY_OK)
        return false;

    // now packet length is known, read the rest of the packet.
    if (aux_tty_read((char * )(buf + 2), buf[1] + 1, READ_TIMEOUT, &n)
            != TTY_OK || n != buf[1] + 1)
    {
        LOG_DEBUG("Did not got whole packet. Dropping out.");
        return false;
    }

    AUXBuffer b(buf, buf + (n + 2));
    hex_dump(hexbuf, b, b.size());
    DEBUGF(DBG_SERIAL, "RES <%s>", hexbuf);
    cmd.parseBuf(b);

    // Got the packet, process it
    // n:length field >=3
    // The buffer of n+2>=5 bytes contains:
    // 0x3b <n>=3> <from> <to> <type> <n-3 bytes> <xsum>

    DEBUGF(DBG_SERIAL, "Got %d bytes:  ; payload length field: %d ; MSG:", n, buf[1]);
    logBytes(buf, n + 2, getDeviceName(), DBG_SERIAL);
    processResponse(cmd);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronDewPower::readAUXResponse(AUXCommand c)
{
    // Assuming only serial connection for Dew/Power controller
    return serialReadResponse(c);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
int CelestronDewPower::sendBuffer(AUXBuffer buf)
{
    if ( PortFD > 0 )
    {
        int n;

        if (aux_tty_write((char * )buf.data(), buf.size(), CTS_TIMEOUT, &n) != TTY_OK)
            return 0;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (n == -1)
            LOG_ERROR("CelestronDewPower::sendBuffer");
        if ((unsigned)n != buf.size())
            LOGF_WARN("sendBuffer: incomplete send n=%d size=%d", n, (int)buf.size());

        char hexbuf[32 * 3] = {0};
        hex_dump(hexbuf, buf, buf.size());
        DEBUGF(DBG_SERIAL, "CMD <%s>", hexbuf);

        return n;
    }
    else
        return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronDewPower::sendAUXCommand(AUXCommand &command)
{
    AUXBuffer buf;
    command.logCommand();

    // For Dew/Power controller, assume direct connection (AUX/PC/USB port)
    command.fillBuf(buf);

    tcflush(PortFD, TCIOFLUSH);
    return (sendBuffer(buf) == static_cast<int>(buf.size()));
}


////////////////////////////////////////////////////////////////////////////////
// Wrap functions around the standard driver communication functions tty_read
// and tty_write.
// When the communication is serial, these wrap functions implement the
// Celestron hardware handshake used by telescope serial ports AUX and PC.
// When the communication is by network, these wrap functions are trasparent.
// Read and write calls are passed, as is, to the standard functions tty_read
// and tty_write.
// 16-Feb-2020 Fabrizio Pollastri <mxgbot@gmail.com>
////////////////////////////////////////////////////////////////////////////////
void CelestronDewPower::setRTS(bool rts)
{
    if (ioctl(PortFD, TIOCMGET, &m_ModemControl) == -1)
        LOGF_ERROR("Error getting handshake lines %s(%d).", strerror(errno), errno);
    if (rts)
        m_ModemControl |= TIOCM_RTS;
    else
        m_ModemControl &= ~TIOCM_RTS;
    if (ioctl(PortFD, TIOCMSET, &m_ModemControl) == -1)
        LOGF_ERROR("Error setting handshake lines %s(%d).", strerror(errno), errno);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronDewPower::waitCTS(float timeout)
{
    float step = timeout / 20.;
    for (; timeout >= 0; timeout -= step)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(step)));
        if (ioctl(PortFD, TIOCMGET, &m_ModemControl) == -1)
        {
            LOGF_ERROR("Error getting handshake lines %s(%d).", strerror(errno), errno);
            return 0;
        }
        if (m_ModemControl & TIOCM_CTS)
            return 1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronDewPower::detectRTSCTS()
{
    setRTS(1);
    bool retval = waitCTS(300.);
    setRTS(0);
    return retval;
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool CelestronDewPower::tty_set_speed(speed_t speed)
{
    struct termios tty_setting;

    if (tcgetattr(PortFD, &tty_setting))
    {
        LOGF_ERROR("Error getting tty attributes %s(%d).", strerror(errno), errno);
        return false;
    }

    if (cfsetspeed(&tty_setting, speed))
    {
        LOGF_ERROR("Error setting serial speed %s(%d).", strerror(errno), errno);
        return false;
    }

    if (tcsetattr(PortFD, TCSANOW, &tty_setting))
    {
        LOGF_ERROR("Error setting tty attributes %s(%d).", strerror(errno), errno);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void CelestronDewPower::hex_dump(char *buf, AUXBuffer data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", data[i]);

    if (size > 0)
        buf[3 * size - 1] = '\0';
}


/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
int CelestronDewPower::aux_tty_read(char *buf, int bufsiz, int timeout, int *n)
{
    int errcode;
    DEBUGF(DBG_SERIAL, "aux_tty_read: %d", PortFD);

    // if hardware flow control is required, set RTS to off to receive: PC port
    // bahaves as half duplex.
    if (m_IsRTSCTS)
        setRTS(0);

    if((errcode = tty_read(PortFD, buf, bufsiz, timeout, n)) != TTY_OK)
    {
        char errmsg[MAXRBUF] = {0};
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
    }

    return errcode;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
int CelestronDewPower::aux_tty_write(char *buf, int bufsiz, float timeout, int *n)
{
    int errcode, ne;
    char errmsg[MAXRBUF];

    //DEBUGF(DBG_CAUX, "aux_tty_write: %d", PortFD);

    // if hardware flow control is required, set RTS to on then wait for CTS
    // on to write: PC port bahaves as half duplex. RTS may be already on.
    if (m_IsRTSCTS)
    {
        DEBUG(DBG_SERIAL, "aux_tty_write: set RTS");
        setRTS(1);
        DEBUG(DBG_SERIAL, "aux_tty_write: wait CTS");
        if (!waitCTS(timeout))
        {
            LOGF_ERROR("Error getting handshake lines %s(%d).\n", strerror(errno), errno);
            return TTY_TIME_OUT;
        }
    }

    errcode = tty_write(PortFD, buf, bufsiz, n);

    if (errcode != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return errcode;
    }

    // if hardware flow control is required, Wait for tx complete, set RTS to
    // off, to receive (half duplex).
    if (m_IsRTSCTS)
    {
        DEBUG(DBG_SERIAL, "aux_tty_write: clear RTS");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        setRTS(0);

        // ports requiring hardware flow control echo all sent characters,
        // verify them.
        DEBUG(DBG_SERIAL, "aux_tty_write: verify echo");
        if ((errcode = tty_read(PortFD, errmsg, *n, READ_TIMEOUT, &ne)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return errcode;
        }

        if (*n != ne)
            return TTY_WRITE_ERROR;

        for (int i = 0; i < ne; i++)
            if (buf[i] != errmsg[i])
                return TTY_WRITE_ERROR;
    }

    return TTY_OK;
}

bool CelestronDewPower::getDewPowerControllerVersion()
{
    AUXCommand cmd(PORTCTRL_GET_VERSION, HC, DEW_POWER_CTRL);
    AUXCommand response;
    if (sendAUXCommand(cmd) && readAUXResponse(response))
    {
        if (response.command() == PORTCTRL_GET_VERSION)
        {
            uint32_t version = response.getData();
            LOGF_INFO("Celestron Dew/Power Controller Version: %d", version);
            // Update firmware property if available
            return true;
        }
    }
    return false;
}

bool CelestronDewPower::getNumberOfPorts()
{
    AUXCommand cmd(PORTCTRL_GET_NUMBER_OF_PORTS, HC, DEW_POWER_CTRL);
    AUXCommand response;
    if (sendAUXCommand(cmd) && readAUXResponse(response))
    {
        if (response.command() == PORTCTRL_GET_NUMBER_OF_PORTS)
        {
            m_NumPorts = response.getData();
            LOGF_INFO("Number of ports: %d", m_NumPorts);
            return true;
        }
    }
    return false;
}

bool CelestronDewPower::getInputPower()
{
    AUXCommand cmd(PORTCTRL_GET_INPUT_POWER, HC, DEW_POWER_CTRL);
    AUXCommand response;
    if (sendAUXCommand(cmd) && readAUXResponse(response))
    {
        if (response.command() == PORTCTRL_GET_INPUT_POWER)
        {
            if (response.getDataBuffer() == lastInputPowerData)
                return true;

            // RESP: data[0:1] is voltage (mV), data[2:3] is current (mA), data[4] is 0, -1 or 1 if ok/under/over voltage, data[5] is 0/1 for overcurrent
            AUXBuffer data = response.getDataBuffer();
            if (data.size() >= 6)
            {
                uint16_t voltage_mV = (data[0] << 8) | data[1];
                uint16_t current_mA = (data[2] << 8) | data[3];
                int8_t voltageStatus = (int8_t)data[4];
                uint8_t overcurrentStatus = data[5];

                PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].setValue(voltage_mV / 1000.0); // Convert mV to V
                PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(current_mA / 1000.0); // Convert mA to A
                PI::PowerSensorsNP[PI::SENSOR_POWER].setValue((voltage_mV * current_mA) / 1000000.0); // P = V * I in Watts

                PI::PowerSensorsNP.setState(IPS_OK);
                PI::PowerSensorsNP.apply();

                // Update PowerStatusLP based on overcurrentStatus and voltageStatus
                PowerStatusLP[POWER_STATUS_OVERCURRENT].setState((overcurrentStatus == 1) ? IPS_ALERT : IPS_OK);
                PowerStatusLP[POWER_STATUS_UNDERVOLTAGE].setState((voltageStatus == -1) ? IPS_ALERT : IPS_OK);
                PowerStatusLP[POWER_STATUS_OVERVOLTAGE].setState((voltageStatus == 1) ? IPS_ALERT : IPS_OK);
                PowerStatusLP.apply();

                lastInputPowerData = data;
                return true;
            }
        }
    }
    return false;
}

bool CelestronDewPower::getPortInfo(uint8_t portNumber)
{
    AUXCommand cmd(PORTCTRL_GET_PORT_INFO, HC, DEW_POWER_CTRL);
    cmd.setData(portNumber, 1); // Port Number is 1 byte
    AUXCommand response;
    if (sendAUXCommand(cmd) && readAUXResponse(response))
    {
        if (response.command() == PORTCTRL_GET_PORT_INFO)
        {
            // RESP: <0 type><1 enabled><2 isShorted><3:4 power(mW)><5:6 VoltageLevel (mV)>
            AUXBuffer data = response.getDataBuffer();
            if (data.size() >= 7)
            {
                // Update PI::PowerChannelsSP, PI::PowerChannelCurrentNP, OverCurrentLP
                // This requires mapping portNumber to the correct INDI property index
                // For now, just log
                LOGF_DEBUG("Port %d Info: Type=%d, Enabled=%d, Shorted=%d, Power=%d mW, Voltage=%d mV",
                           portNumber, data[0], data[1], data[2], (data[3] << 8) | data[4], (data[5] << 8) | data[6]);
                return true;
            }
        }
    }
    return false;
}

bool CelestronDewPower::getDewHeaterPortInfo(uint8_t portNumber)
{
    AUXCommand cmd(PORTCTRL_GET_DH_PORT_INFO, HC, DEW_POWER_CTRL);
    cmd.setData(portNumber, 1); // Port Number is 1 byte
    AUXCommand response;
    if (sendAUXCommand(cmd) && readAUXResponse(response))
    {
        if (response.command() == PORTCTRL_GET_DH_PORT_INFO)
        {
            // RESP: <0 type><nmode (-1 == short detected, 0 == manual, 1 == auto_above_dp, 2 == auto_above_ambient)><2 power level><3:4 power(mW)><5 aggression level (C)><6:9 heaterTemp (if present)>
            AUXBuffer data = response.getDataBuffer();
            if (data.size() >= 6) // Minimum size without heaterTemp
            {
                // Update PI::DewChannelsSP, PI::DewChannelDutyCycleNP, PI::DewChannelCurrentNP, PI::AutoDewSP
                // This requires mapping portNumber to the correct INDI property index
                // For now, just log
                LOGF_DEBUG("Dew Port %d Info: Type=%d, Mode=%d, PowerLevel=%d, Power=%d mW, Aggression=%d C",
                           portNumber, data[0], (int8_t)data[1], data[2], (data[3] << 8) | data[4], data[5]);
                return true;
            }
        }
    }
    return false;
}


bool CelestronDewPower::setPortEnabled(uint8_t portNumber, bool enabled)
{
    AUXCommand cmd(PORTCTRL_SET_PORT_ENABLED, HC, DEW_POWER_CTRL);
    AUXBuffer data;
    data.push_back(portNumber);
    data.push_back(enabled ? 1 : 0);
    cmd.setDataBuffer(data);
    return sendAUXCommand(cmd);
}

bool CelestronDewPower::setPortVoltage(uint8_t portNumber, uint16_t voltage_mV)
{
    AUXCommand cmd(PORTCTRL_SET_PORT_VOLTAGE, HC, DEW_POWER_CTRL);
    AUXBuffer data;
    data.push_back(portNumber);
    data.push_back((voltage_mV >> 8) & 0xFF);
    data.push_back(voltage_mV & 0xFF);
    cmd.setDataBuffer(data);
    return sendAUXCommand(cmd);
}

bool CelestronDewPower::setDewHeaterAuto(uint8_t portNumber, uint8_t mode, uint8_t temp_C)
{
    AUXCommand cmd(PORTCTRL_DH_ENABLE_AUTO, HC, DEW_POWER_CTRL);
    AUXBuffer data;
    data.push_back(portNumber);
    data.push_back(mode);
    data.push_back(temp_C);
    cmd.setDataBuffer(data);
    return sendAUXCommand(cmd);
}

bool CelestronDewPower::setDewHeaterManual(uint8_t portNumber, uint8_t powerLevel)
{
    AUXCommand cmd(PORTCTRL_DH_ENABLE_MANUAL, HC, DEW_POWER_CTRL);
    AUXBuffer data;
    data.push_back(portNumber);
    data.push_back(powerLevel);
    cmd.setDataBuffer(data);
    return sendAUXCommand(cmd);
}

bool CelestronDewPower::setLEDBrightness(uint8_t brightness)
{
    AUXCommand cmd(PORTCTRL_SET_LED_BRIGHTNESS, HC, DEW_POWER_CTRL);
    cmd.setData(brightness, 1);
    return sendAUXCommand(cmd);
}

// INDI::PowerInterface overrides
bool CelestronDewPower::SetPowerPort(size_t port, bool enabled)
{
    return setPortEnabled(static_cast<uint8_t>(port), enabled);
}

bool CelestronDewPower::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    // Convert duty cycle percentage (0-100) to 0-255 range
    auto powerLevel = static_cast<uint8_t>(dutyCycle / 100.0 * 255.0);
    return setDewHeaterManual(static_cast<uint8_t>(port), enabled ? powerLevel : 0);
}

bool CelestronDewPower::SetVariablePort(size_t port, bool enabled, double voltage)
{
    INDI_UNUSED(port); // Assuming only one variable port for now
    uint16_t voltage_mV = static_cast<uint16_t>(voltage * 1000.0);
    return setPortVoltage(static_cast<uint8_t>(port), enabled ? voltage_mV : 0);
}

bool CelestronDewPower::SetLEDEnabled(bool enabled)
{
    return setLEDBrightness(enabled ? 255 : 0); // Full brightness or off
}

bool CelestronDewPower::SetAutoDewEnabled(size_t port, bool enabled)
{
    if (enabled)
    {
        uint8_t mode = AutoDewModeSP.findOnSwitchIndex();
        uint8_t temp_C = static_cast<uint8_t>(AutoDewTempNP[0].getValue());
        return setDewHeaterAuto(static_cast<uint8_t>(port), mode, temp_C);
    }
    else
    {
        // To disable auto dew, we set it to manual mode with 0 power
        return setDewHeaterManual(static_cast<uint8_t>(port), 0);
    }
}
