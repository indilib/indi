/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

  SVBONYPowerBox

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

#include "config.h"
#include "svbony_powerbox.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cerrno>
#include <cstring>
#include <chrono>
#include <thread>
#include <memory>
#include <cmath>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>

// Declaration for an auto pointer to SVBONYPowerBox
static std::unique_ptr<SVBONYPowerBox> svbonypowerbox(new SVBONYPowerBox());

#define CMD_MAX_LEN 6
#define CMD_OFFSET_FRAME_HEADER 0
#define CMD_OFFSET_DATA_LEN 1
#define CMD_OFFSET_CMD 2

#define TIMEOUT_SEC 0
#define TIMEOUT_MSEC 500

SVBONYPowerBox::SVBONYPowerBox() : INDI::PowerInterface(this)
{
    setVersion(1, 0);
}

bool SVBONYPowerBox::initProperties()
{
    INDI::DefaultDevice::initProperties();

    setDriverInterface(AUX_INTERFACE | POWER_INTERFACE);

    // Initialize Weather Sensor Properties
    WeatherSVBSensorsNP[SVB_SENSOR_DS18B20_TEMP].fill("DS18B20_TEMP", "Lens Temperature(C)", "%.1f", -100, 200, 0.1, 0);
    WeatherSVBSensorsNP[SVB_SENSOR_SHT40_TEMP].fill("SHT40_TEMP", "Temperature(C)", "%.1f", -100, 200, 0.1, 0);
    WeatherSVBSensorsNP[SVB_SENSOR_SHT40_HUMIDITY].fill("SHT40_HUMI", "Humidity %", "%.1f", 0, 100, 0.1, 0);
    WeatherSVBSensorsNP[SVB_SENSOR_DEW_POINT].fill("DEW_POINT", "Dew Point(C)", "%.1f", -100, 200, 0.1, 0);
    WeatherSVBSensorsNP.fill(getDeviceName(), "WEATHER_SV_SENSORS", "Weather Sensors", DEW_TAB, IP_RO, 0, IPS_IDLE);

    // Serial Connection
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    registerConnection(serialConnection);

    return true;
}

bool SVBONYPowerBox::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Define Weather sensor properties
        defineProperty(WeatherSVBSensorsNP);
        
        PI::updateProperties();
        setupComplete = true;
    }
    else
    {
        // Delete Weather sensor properties
        deleteProperty(WeatherSVBSensorsNP);

        PI::updateProperties();
        setupComplete = false;
    }

    return true;
}

bool SVBONYPowerBox::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    PI::saveConfigItems(fp);
    return true;
}

/*
    Handshake with the device and initialize PowerInterface properties
*/
bool SVBONYPowerBox::Handshake()
{
    uint32_t SVB_capabilities =
        POWER_HAS_DC_OUT |
        POWER_HAS_DEW_OUT |
        POWER_HAS_VARIABLE_OUT |
        POWER_HAS_VOLTAGE_SENSOR |
        /* POWER_HAS_OVERALL_CURRENT | */ // Not supported
        /* POWER_HAS_PER_PORT_CURRENT | */ // Not supported
        /* POWER_HAS_LED_TOGGLE | */ // Not supported
        /* POWER_HAS_AUTO_DEW | */ // Not supported
        POWER_HAS_POWER_CYCLE |
        POWER_HAS_USB_TOGGLE;
        /* POWER_HAS_OVER_VOTALGE_PROTECTION | */ // Not supported
        /* POWER_OFF_ON_DISCONNECT; */ // Not supported

    if (isSimulation()) // If in simulation mode, skip actual handshake
    {
        // Set capabilities and initialize PI properties
        PI::SetCapability(SVB_capabilities);
        PI::initProperties(
            POWER_TAB,
            5, // DC Ports
            2, // Dew Ports
            1, // Variable Ports
            0, // Auto Dew Ports
            2  // USB Ports
        );
        return true;
    }
    /*
        Device Identification
    */
    PortFD = serialConnection->getPortFD();
    int flags;
    ioctl(PortFD, TIOCMGET, &flags);
    flags &= ~(TIOCM_RTS | TIOCM_DTR); // Clear RTS and DTR to reset device
    ioctl(PortFD, TIOCMSET, &flags);

    bool isResetting = true;
    int retryCount = 0;
    const int maxRetries = 10;

    while (isResetting && retryCount < maxRetries)
    {
        char buf[512] = {0};
        int nbytes_read = 0;

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // wait 50 ms

        int tty_ret = tty_nread_section_expanded(PortFD,
                      buf, (sizeof buf) - 1, '\n', TIMEOUT_SEC, TIMEOUT_MSEC, &nbytes_read);
        if (tty_ret != TTY_OK)
        {
            char errorMessage[MAXRBUF];
            tty_error_msg(tty_ret, errorMessage, MAXRBUF);

            LOGF_ERROR("Handshake error.(Serial read error: %s)", errorMessage);
            return false;
        }
        std::string response(buf);
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // wait 50 ms

        /*
            Verify that it matches the following string returned when the device is opened
            However, it does not need to be an exact match.
            ```
            ets Jun  8 2016 00:22:57\r\n
            \r\n
            rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\r\n
            configsip: 188777542, SPIWP:0xee\r\n
            clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00\r\n
            mode:DIO, clock div:1\r\n
            load:0x3fff0030,len:1344\r\n
            load:0x40078000,len:13964\r\n
            load:0x40080400,len:3600\r\n
            entry 0x400805f0\r\n
            ```
        */
        isResetting =
            response.find("ts") != std::string::npos ||
            response.find("\n") != std::string::npos ||
            response.find("POW") != std::string::npos ||
            response.find("0x00") != std::string::npos ||
            response.find("rst") != std::string::npos ||
            response.find("loa") != std::string::npos ||
            response.find("len") != std::string::npos ||
            response.find("ts")  != std::string::npos;

        ++retryCount;
    }
    tcflush(PortFD, TCIOFLUSH);  // Flush both input and output buffers
    sendCommand((const unsigned char*)"\x07", 1, nullptr, 4); // Send a dummy command to wake up the device

    LOG_INFO("Handshake successful.");

    // Set capabilities and initialize PI properties
    PI::SetCapability(SVB_capabilities);
    PI::initProperties(
        POWER_TAB,
        5, // DC Ports
        2, // Dew Ports
        1, // Variable Ports
        0, // Auto Dew Ports
        2  // USB Ports
    );

    /*
        Set the variable voltage channel to 0V to 15.3V
    */
    VariableChannelVoltsNP[0].setMinMax(0.0, 15.3); // Range for variable output
    VariableChannelVoltsNP[0].setStep(1); // Step size
    VariableChannelVoltsNP.apply();

    /*
        Change property labels to match the device
    */
    // DC Channels
    for (size_t i = 0; i < PowerChannelsSP.size(); i++)
    {
        char propLabel[MAXINDILABEL];
        snprintf(propLabel, MAXINDILABEL, "DC %d", static_cast<int>(i + 1));
        PowerChannelsSP[i].setLabel(propLabel);
        PowerChannelLabelsTP[i].setLabel(propLabel);
    }
    PowerChannelsSP.apply();
    PowerChannelLabelsTP.apply();
    //Dew Channels
    for (size_t i = 0; i < DewChannelsSP.size(); i++)
    {
        char propLabel[MAXINDILABEL];
        char propDutyLabel[MAXINDILABEL];
        snprintf(propLabel, MAXINDILABEL, "PWM %d", static_cast<int>(i + 1));
        snprintf(propDutyLabel, MAXINDILABEL, "PWM %d (%%)", static_cast<int>(i + 1));
        DewChannelsSP[i].setLabel(propLabel);
        DewChannelLabelsTP[i].setLabel(propLabel);
        DewChannelDutyCycleNP[i].setLabel(propDutyLabel);
    }
    DewChannelsSP.apply();
    DewChannelLabelsTP.apply();
    // USB Channels
    USBPortSP[0].setLabel("USB C,1,2");
    USBPortLabelsTP[0].setLabel("USB C,1,2");
    USBPortSP[1].setLabel("USB 3,4,5");
    USBPortLabelsTP[1].setLabel("USB 3,4,5");
    USBPortSP.apply();
    USBPortLabelsTP.apply();
    //Variable Channel
    VariableChannelsSP[0].setLabel("REGULATED");
    VariableChannelVoltsNP[0].setLabel("REGULATED (V)");
    VariableChannelLabelsTP[0].setLabel("REGULATED");
    VariableChannelsSP.apply();
    VariableChannelLabelsTP.apply();
    return true;
}

const char *SVBONYPowerBox::getDefaultName()
{
    LOG_INFO("GET Name");
    return "SVBONY PowerBox";
}

bool SVBONYPowerBox::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Let PowerInterface handle its switches first
        if (PI::processSwitch(dev, name, states, names, n))
            return true;

        if (processButtonSwitch(dev, name, states, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SVBONYPowerBox::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Let PowerInterface handle its text properties
        if (PI::processText(dev, name, texts, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool SVBONYPowerBox::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // Let PowerInterface handle its number properties
        if (PI::processNumber(dev, name, values, names, n))
            return true;
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

/*
    Send command to the device and read response

    input:
        cmd: command bytes to send
        cmd_len: length of command bytes
        res_len: expected length of response bytes
    output:
        res: buffer to store response bytes. Should be at least res_len bytes long. Can be nullptr if response is not needed.
*/
bool SVBONYPowerBox::sendCommand(const unsigned char* cmd, unsigned int cmd_len, unsigned char* res, int res_len)
{
    int tty_ret = 0; // tty return code
    int nbytes_written = 0; // number of bytes written
    int nbytes_read = 0; // number of bytes read
    const char frame_header = 0x24; // '$'

    unsigned char command[CMD_MAX_LEN] = {0}; // frame_header + data_len + cmd + checksum
    int full_cmd_len = 2 + cmd_len + 1; // 2:(frame_header + data_len) + cmd + 1:checksum
    int full_res_len = 3 + res_len + 1; // 3:(frame_header + data_len + cmd) + res + 1:checksum
    unsigned checksum = 0; // checksum
    unsigned char *response = (unsigned char *)alloca(3 + res_len +
                              1); // Allocate response buffer. frame_header + data_len + cmd + res + checksum
    command[CMD_OFFSET_FRAME_HEADER] = frame_header; // Set frame header
    command[CMD_OFFSET_DATA_LEN] = full_cmd_len; // Set frame_header + data_len + cmd + checksum

    // set command bytes
    for (unsigned int i = 0; i < cmd_len; i++)
    {
        command[CMD_OFFSET_CMD + i] = cmd[i];
    }
    // calculate checksum
    for (unsigned int i = 0; i < 2 + cmd_len; i++)
    {
        checksum += command[i];
    }
    command[full_cmd_len - 1] = (unsigned char)(checksum % 0xFF);

    // Debug logging
    switch (full_cmd_len)
    {
        case 4:
            LOGF_DEBUG("CMD <%02X %02X %02X %02X>", command[0], command[1], command[2], command[3]);
            break;
        case 5:
            LOGF_DEBUG("CMD <%02X %02X %02X %02X %02X>", command[0], command[1], command[2], command[3], command[4]);
            break;
        case 6:
            LOGF_DEBUG("CMD <%02X %02X %02X %02X %02X %02X>", command[0], command[1], command[2], command[3], command[4], command[5]);
            break;
        default:
            break;
    }

    // Send command
    PortFD = serialConnection->getPortFD(); // Get port file descriptor
    tcflush(PortFD, TCIOFLUSH); // Flush both input and output buffers

    if ( (tty_ret = tty_write(PortFD, (const char*)command, full_cmd_len, &nbytes_written)) != TTY_OK)
    {
        // Write error
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_ret, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return false;
    }
    if (nbytes_written != full_cmd_len)
    {
        // The number of bytes requested for writing differs from the actual number of bytes written.
        LOGF_ERROR("Serial write error: expected %d bytes, wrote %d bytes", full_cmd_len, nbytes_written);
        return false;
    }
    tcdrain(PortFD); // Wait until all output written to the object referred to by fd has been transmitted
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait 100 ms

    // read response
    tty_ret = tty_read_expanded(PortFD, (char*)response, full_res_len, TIMEOUT_SEC, TIMEOUT_MSEC, &nbytes_read);
    tcflush(PortFD, TCIOFLUSH); // Flush both input and output buffers
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait 100 ms

    if (tty_ret != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(tty_ret, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial read error: %s", errorMessage);
        return false;
    }
    if (nbytes_read != full_res_len)
    {
        LOGF_ERROR("Serial read error: expected %d bytes, got %d bytes", res_len, nbytes_read);
        return false;
    }

    for (int i = 0; i < nbytes_read; i++)
    {
        LOGF_DEBUG("RES <%02X>", response[i]);
    }

    if (res)
    {
        for (int i = 0; i < res_len; i++)
        {
            res[i] = response[3 + i];
        }
    }

    return response[2] != 0xaa ? true : false; // return true if command was successful. 0xaa indicates failure.
}

void SVBONYPowerBox::TimerHit()
{
    if (!isConnected() || setupComplete == false)
    {
        SetTimer(100);
        return;
    }

    Get_State();
    SetTimer(100);
}

/*
    Retrieve each parameter of the SV241 Pro and reflect them in the INDI properties.
*/
void SVBONYPowerBox::Get_State()
{
    unsigned char cmd = 0;
    unsigned char res[10] = {0};
    double value = 0;
    double shtTemp = 0.0;
    double shtHumidity = 0.0;
    bool hasShtTemp = false;
    bool hasShtHumidity = false;
    // Read power
    cmd = 0x02;
    if (sendCommand(&cmd, 1, res, 4))
    {
        value = convert4BytesToDouble(res, 100);
        LOGF_DEBUG("INA219 Power Value: %lf mW", value);
        PI::PowerSensorsNP[PI::SENSOR_POWER].setValue(value/1000.0);
        PI::PowerSensorsNP.apply();
    }
    // Read load voltage
    cmd = 0x03;
    if (sendCommand(&cmd, 1, res, 4))
    {
        value = convert4BytesToDouble(res, 100);
        LOGF_DEBUG("INA219 Load Voltage Value: %lf V", value);
        PI::PowerSensorsNP[PI::SENSOR_VOLTAGE].setValue(value);
        PI::PowerSensorsNP.apply();
    }
    // Read DS18B20 Temperature
    cmd = 0x04;
    if (sendCommand(&cmd, 1, res, 4))
    {
        value = std::round((convert4BytesToDouble(res, 100) - 255.5) * 100.0) / 100.0; // Adjust for offset
        LOGF_DEBUG("DS18B20 Temperature Value: %lf C", value);
        WeatherSVBSensorsNP[SVB_SENSOR_DS18B20_TEMP].setValue(value);
    }

    // Read SHT40 Temperature
    cmd = 0x05;
    if (sendCommand(&cmd, 1, res, 4))
    {
        value = std::round((convert4BytesToDouble(res, 100) - 254) * 10.0) / 10.0; // Adjust for offset
        LOGF_DEBUG("SHT40 Temperature Value: %lf C", value);
        WeatherSVBSensorsNP[SVB_SENSOR_SHT40_TEMP].setValue(value);
        shtTemp = value;
        hasShtTemp = true;
    }
    // Read SHT40 Humidity
    cmd = 0x06;
    if (sendCommand(&cmd, 1, res, 4))
    {
        value = std::round((convert4BytesToDouble(res, 100) - 254) * 10.0) / 10.0; // Adjust for offset
        LOGF_DEBUG("SHT40 Humidity Value: %lf %", value);
        WeatherSVBSensorsNP[SVB_SENSOR_SHT40_HUMIDITY].setValue(value);
        shtHumidity = value;
        hasShtHumidity = true;
    }

    // Calculate dew point from SHT40 temperature and humidity
    if (hasShtTemp && hasShtHumidity)
    {
        double svp = CalculateSVP(shtTemp);
        double vp = CalculateVP(shtHumidity, svp);
        double dewPoint = CalculateDewPointFromVP(vp);
        LOGF_DEBUG("Dew Point Value: %lf C", dewPoint);
        WeatherSVBSensorsNP[SVB_SENSOR_DEW_POINT].setValue(dewPoint);
    }
    WeatherSVBSensorsNP.apply();  // Apply all parameter changes

    // Read load Current
    cmd = 0x07;
    if (sendCommand(&cmd, 1, res, 4))
    {
        value = convert4BytesToDouble(res, 100);
        LOGF_DEBUG("INA219 Current Value: %lf mA", value);
        PI::PowerSensorsNP[PI::SENSOR_CURRENT].setValue(value/1000.0);
        PI::PowerSensorsNP.apply();
    }
    /*
        USB and Power Channels State Update
    */
    cmd = 0x08;
    if (sendCommand(&cmd, 1, res, 10))
    {
        LOGF_DEBUG("Status: GPIO1:%02X GPIO2:%02X GPIO3:%02X GPIO4:%02X GPIO5:%02X GPIO6:%02X GPIO7:%02X pwmA:%02X pwmB:%02X pwmC:%02X",
                   res[0], res[1], res[2], res[3], res[4], res[5], res[6], res[7], res[8], res[9]);

        // Power Channels State Update
        for (int i = 0; i < 5; i++)
        {
            PI::PowerChannelsSP[i].setState(res[i] ? ISS_ON : ISS_OFF);
        }
        PI::PowerChannelsSP.apply();

        // USB Ports State Update
        for (int i = 0; i < 2; i++)
        {
            PI::USBPortSP[i].setState(res[i + 5] ? ISS_ON : ISS_OFF);
        }
        PI::USBPortSP.apply();

        // Regulated Output Voltage Update
        double voltage = std::round((res[7] * 15.3 / 253.0) * 10.0) / 10.0;

        if (voltage <= 0.0)
        {
            // When the voltage is 0V, set the channel to off.
            // Keep the voltage of the control panel channel at the set value.
            VariableChannelsSP[0].setState(ISS_OFF);
        }
        else
        {
            // When the voltage is greater than 0V, set the channel to on.
            // Reflect the acquired voltage on the control panel.
            VariableChannelsSP[0].setState(ISS_ON);
            VariableChannelVoltsNP[0].setValue(voltage);
        }        
        VariableChannelsSP.apply();
        VariableChannelVoltsNP.apply();

        // Dew State Update
        for (int i = 0; i < 2; i++)
        {
            unsigned char pwmValue = std::round(100.0*(((double)res[i+8])/255.0));

            if (pwmValue == 0)
            {
                // When the duty cycle is 0%, set the channel to off.
                // Keep the duty cycle of the control panel channel at the set value.
                DewChannelsSP[i].setState(ISS_OFF);
            }
            else
            {
                // When the duty cycle is greater than 0%, set the channel to on.
                // Reflect the acquired duty cycle on the control panel.
                DewChannelsSP[i].setState(ISS_ON);
                DewChannelDutyCycleNP[i].setValue(std::round(100.0*(res[i+8]/255.0)));
            }
        }
        DewChannelsSP.apply();
        DewChannelDutyCycleNP.apply();
    }
}

bool SVBONYPowerBox::processButtonSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(states);
    INDI_UNUSED(names);
    INDI_UNUSED(n);

    // Auto heater handling would go here if needed
    return false;
}

// Implement PowerInterface virtual methods
bool SVBONYPowerBox::SetPowerPort(size_t port, bool enabled)
{
    if (port >= 5)
        return false;

    unsigned char cmd[3];
    cmd[0] = 0x01;
    cmd[1] = port;
    cmd[2] = enabled ? 0xFF : 0x00;

    return sendCommand(cmd, sizeof cmd, nullptr, 2);
}

bool SVBONYPowerBox::SetDewPort(size_t port, bool enabled, double dutyCycle)
{
    if (port >= 2)
        return false;

    unsigned char cmd[3];
    cmd[0] = 0x01;
    cmd[1] = port + 8; // pwmA, pwmB are Dew ports
    cmd[2] = enabled ? (unsigned char)std::round(255.0 * ((double)dutyCycle / 100.0)) : 0x00;

    return sendCommand(cmd, sizeof cmd, nullptr, 2);
}

/*
    enabled: true to turn on the variable port, false to turn off
    voltage: voltage to set when enabled is true (0.0V to 15.3V)
*/
bool SVBONYPowerBox::SetVariablePort(size_t port, bool enabled, double voltage)
{
    if (port >= 1)
        return false;

    unsigned char cmd[3];
    cmd[0] = 0x01;
    cmd[1] = 7; // Regulated output port
    cmd[2] = enabled ? (unsigned char)(voltage * (253.0 / 15.3)) : 0x00;

    return sendCommand(cmd, sizeof cmd, nullptr, 2);
}

bool SVBONYPowerBox::SetLEDEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    // Not implemented on this device
    return false;
}

bool SVBONYPowerBox::SetAutoDewEnabled(size_t port, bool enabled)
{
    INDI_UNUSED(port);
    INDI_UNUSED(enabled);
    // No auto dew on this device
    return false;
}

bool SVBONYPowerBox::CyclePower()
{
    if (sendCommand((const unsigned char*)"\xFF\xFF", 2, nullptr, 2) == false)
        return false;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (sendCommand((const unsigned char*)"\xFE\xFE", 2, nullptr, 2) == false)
        return false;

    return true;
}

bool SVBONYPowerBox::SetUSBPort(size_t port, bool enabled)
{
    if (port > 1)
        return false;

    unsigned char cmd[3];
    cmd[0] = 0x01;
    cmd[1] = port + 5; // GPIO5 and GPIO6 are USB ports
    cmd[2] = enabled ? 0xFF : 0x00;

    return sendCommand(cmd, sizeof cmd, nullptr, 2);
}

double SVBONYPowerBox::convert4BytesToDouble(const unsigned char *data, double scale)
{
    if (data == nullptr || scale == 0.0)
        return 0.0;

    uint32_t raw = (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8)  |
                   static_cast<uint32_t>(data[3]);

    return static_cast<double>(raw) / scale;
}

double SVBONYPowerBox::CalculateSVP(double temperature)
{
    // 6.11 × 10^(7.5 × T / (237.7 + T))
    return 6.11 * std::pow(10.0, 7.5 * temperature / (237.7 + temperature));
}

double SVBONYPowerBox::CalculateVP(double humidity, double svp)
{
    // VP = RH × SVP / 100
    return humidity * svp / 100.0;
}

double SVBONYPowerBox::CalculateDewPointFromVP(double vp)
{
    double minDifference = 1e6;
    double minTemp = -100.0;

    for (double temp = -100.0; temp <= 100.0; temp += 0.01)
    {
        double currentSvp = CalculateSVP(temp);
        double difference = std::fabs(vp - currentSvp);
        if (difference < minDifference)
        {
            minDifference = difference;
            minTemp = temp;
        }
    }

    // Round to 2 decimal places
    return std::round(minTemp * 100.0) / 100.0;
}
