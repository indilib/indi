#include "rainbowRSF.h"
#include "indicom.h"
//#include "connectionplugins/connectionserial.h"

//#include <cmath>
//#include <memory>
#include <cstring>
#include <termios.h>
//#include <unistd.h>
//#include <regex>


static std::unique_ptr<RainbowRSF> rainbowRSF(new RainbowRSF());

void ISGetProperties(const char *dev)
{
    rainbowRSF->ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    rainbowRSF->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    rainbowRSF->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    rainbowRSF->ISNewNumber(dev, name, values, names, n);
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
    rainbowRSF->ISSnoopDevice(root);
}

RainbowRSF::RainbowRSF()
{
    setVersion(1, 0);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *RainbowRSF::getDefaultName()
{
    return "Rainbow Astro RSF";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::initProperties()
{
    INDI::Focuser::initProperties();

    // Firmware Information
    //    IUFillText(&FirmwareT[0], "VERSION", "Version", "");
    //    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "FOCUS_FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0,
    //                     IPS_IDLE);

    // Temperature
    IUFillNumber(&CurrentTempN[0], "CURRENT_TEMPERATURE", "Temperature", "%.f", -20, 70, 0.1, 23);
    IUFillNumberVector(&CurrentTempNP, CurrentTempN, 1, getDeviceName(), "CURRENT_TEMP", "Current Temp", MAIN_CONTROL_TAB,
                       IP_RO, 0, IPS_IDLE);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::updateProperties()
{
    if (isConnected())
    {
        defineNumber(&CurrentTempNP);
    }
    else
    {
        deleteProperty(CurrentTempNP.name);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

    }
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // if new temp switch is clicked, get the new temp
        // set it to busy?
        // in the timed function check if it is complete
        // once it is, set the temp to the new value
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::Handshake()
{
    if (GetTemperature())
    {
        LOG_INFO("Rainbow Astro is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO(
        "Error retrieving data from Rainbow Astro, please ensure Rainbow Astro controller is powered and the port is correct.");
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::GetTemperature()
{
    char res[DRIVER_LEN] = {0};

    if (sendCommand(":Ft1#", res) == false)
        return false;

    //    CurrentTempN[0].value = static_cast<double>(res);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::findHome()
{
    return sendCommand(":Fh#");
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
//void RainbowRSF::TimerHit()
//{
//    if (!isConnected() || FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY || CalibrationSP.s == IPS_BUSY)
//    {
//        SetTimer(POLLMS);
//        return;
//    }

//    bool rc = updatePosition();
//    if (rc)
//    {
//        if (fabs(lastPos - FocusAbsPosN[0].value) > 0)
//        {
//            IDSetNumber(&FocusAbsPosNP, nullptr);
//            lastPos = FocusAbsPosN[0].value;
//        }
//    }

//    if (m_TemperatureCounter++ == SESTO_TEMPERATURE_FREQ)
//    {
//        rc = updateTemperature();
//        if (rc)
//        {
//            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.1)
//            {
//                IDSetNumber(&TemperatureNP, nullptr);
//                lastTemperature = TemperatureN[0].value;
//            }
//        }
//        m_TemperatureCounter = 0;   // Reset the counter
//    }

//    SetTimer(POLLMS);
//}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);

        char formatted_command[DRIVER_LEN] = {0};
        snprintf(formatted_command, DRIVER_LEN, "%s\r", cmd);
        rc = tty_write_string(PortFD, formatted_command, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        // Remove extra \r
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void RainbowRSF::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}
