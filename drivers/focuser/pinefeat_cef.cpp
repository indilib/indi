/*
    Pinefeat EF / EF-S Lens Controller – Canon® Lens Compatible
    Copyright (C) 2025 Pinefeat LLP (support@pinefeat.co.uk)

    Based on Moonlite focuser
    Copyright (C) 2013-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "pinefeat_cef.h"

#include <connectionplugins/connectionserial.h>

#include <cstring>
#include <termios.h>
#include <thread>

#define ERR_NC(res) (strcmp(res, "nc") > 0 ? "lens is not attached" : res)

static std::unique_ptr<PinefeatCEF> pinefeatCEF(new PinefeatCEF());

PinefeatCEF::PinefeatCEF()
{
    setVersion(1, 0);

    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_HAS_VARIABLE_SPEED);

    lastUpdate = std::chrono::steady_clock::now();
}

const char * PinefeatCEF::getDefaultName()
{
    return "Pinefeat EF Lens Controller";
}

bool PinefeatCEF::initProperties()
{
    INDI::Focuser::initProperties();

    FocusSpeedNP[0].setMinMax(1, 4);
    FocusSpeedNP[0].setStep(1);
    FocusSpeedNP[0].setValue(1);

    FocusMaxPosNP[0].setMinMax(0, 32767);
    FocusMaxPosNP[0].setStep(1);
    FocusMaxPosNP[0].setValue(0);

    FocusRelPosNP[0].setMinMax(0, 32767);
    FocusRelPosNP[0].setStep(1);
    FocusRelPosNP[0].setValue(0);

    FocusAbsPosNP[0].setMinMax(0, 32767);
    FocusAbsPosNP[0].setStep(1);
    FocusAbsPosNP[0].setValue(0);

    CalibrateSP[0].fill("CALIBRATE", "Calibrate", ISS_OFF);
    CalibrateSP.fill(m_defaultDevice->getDeviceName(), "CALIBRATE", "Calibrate", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                     IPS_OK);

    ApertureAbsNP[0].fill("APERTURE_ABSOLUTE", "f-stop", "%.f", 0.0, 327.67, 0.0, 0.0);
    ApertureAbsNP.fill(getDeviceName(), "ABS_APERTURE", "Absolute Aperture", MAIN_CONTROL_TAB, IP_WO,
                       60, IPS_OK);

    ApertureRelNP[0].fill("APERTURE_RELATIVE", "f-stop", "%.f", -327.68, 327.67, 0.0, 0.0);
    ApertureRelNP.fill(getDeviceName(), "REL_APERTURE", "Relative Aperture", MAIN_CONTROL_TAB, IP_WO,
                       60, IPS_OK);

    ApertureRangeTP[0].fill("APERTURE_RANGE", "f-stop", nullptr);
    ApertureRangeTP.fill(getDeviceName(), "RANGE_APERTURE", "Aperture range", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    FocusDistanceTP[0].fill("FOCUS_DISTANCE", "meter", nullptr);
    FocusDistanceTP.fill(getDeviceName(), "FOCUS_DISTANCE", "Focus Distance", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    setDefaultPollingPeriod(50);

    return true;
}

bool PinefeatCEF::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(FocusDistanceTP);
        defineProperty(CalibrateSP);
        defineProperty(ApertureRangeTP);
        defineProperty(ApertureAbsNP);
        defineProperty(ApertureRelNP);

        int32_t pos;
        std::string dist, aper;
        if (readFocusPosition(pos) &&
                readFocusDistance(dist) &&
                readApertureRange(aper) &&
                updateProperties(pos, dist, aper))
        {
            LOG_INFO("Parameters updated, the controller is ready for use.");
        }
    }
    else
    {
        deleteProperty(FocusDistanceTP);
        deleteProperty(CalibrateSP);
        deleteProperty(ApertureRangeTP);
        deleteProperty(ApertureAbsNP);
        deleteProperty(ApertureRelNP);
    }

    return true;
}

bool PinefeatCEF::updateProperties(const int32_t pos, const std::string dist, const std::string aper)
{
    FocusAbsPosNP[0].setValue(pos);
    FocusAbsPosNP.setState(IPS_OK);
    FocusAbsPosNP.apply();

    if (FocusRelPosNP.getState() == IPS_BUSY)
    {
        FocusRelPosNP.setState(IPS_OK);
        FocusRelPosNP.apply();
    }

    if (FocusMaxPosNP.getState() == IPS_BUSY)
    {
        FocusMaxPosNP[0].setValue(pos);
        FocusMaxPosNP.setState(IPS_IDLE);
        FocusMaxPosNP.apply();

        double values[] = { FocusMaxPosNP[0].getValue() };
        char *names[] = { (char*)FocusMaxPosNP[0].getName() };
        ISNewNumber(getDeviceName(), FocusMaxPosNP.getName(), values, names, 1);
    }

    FocusDistanceTP[0].setText(dist);
    FocusDistanceTP.apply();

    ApertureRangeTP[0].setText(aper);
    ApertureRangeTP.apply();

    return true;
}

bool PinefeatCEF::Handshake()
{
    for (int i = 0; i < 3; i++)
    {
        if (readFirmwareVersion())
        {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(getCurrentPollingPeriod()));
    }

    LOG_ERROR("Can't detect the controller, please ensure the device is powered and the port is correct.");
    return false;
}

bool PinefeatCEF::readFirmwareVersion()
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("v\n", res))
        return false;

    LOGF_INFO("Detected firmware version %s.", res);

    return true;
}

bool PinefeatCEF::readFocusPosition(int32_t &pos)
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("f\n", res))
        return false;

    int rc = sscanf(res, "%d", &pos);
    if (rc <= 0)
    {
        LOGF_ERROR("Can't read focus position: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

bool PinefeatCEF::readFocusDistance(std::string &result)
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("d\n", res))
        return false;

    result = res;
    return true;
}

bool PinefeatCEF::readApertureRange(std::string &result)
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("a\n", res))
        return false;

    result = res;
    return true;
}

bool PinefeatCEF::isNotMoving()
{
    char res[CEF_BUF] = {0};
    return sendCommand("e\n", res) && strstr(res, "n");
}

bool PinefeatCEF::moveFocusAbs(uint32_t position)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "f%d\n", position);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't focus: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

bool PinefeatCEF::moveFocusRel(FocusDirection dir, uint32_t offset)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "f%s%d\n", (dir == FOCUS_INWARD) ? "-" : "+", offset);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't focus: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

bool PinefeatCEF::setSpeed(int speed)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "s%d\n", speed);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't set speed: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

bool PinefeatCEF::setApertureAbs(double value)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "a%.6g\n", value);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't set aperture: %s.", ERR_NC(res));
        return false;
    }

    LOGF_INFO("Aperture is set to f/%.6g.", value);

    return true;
}

bool PinefeatCEF::setApertureRel(double value)
{
    char cmd[CEF_BUF] = {0};
    char res[CEF_BUF] = {0};
    snprintf(cmd, CEF_BUF, "a%s%.6g\n", (value < 0) ? "" : "+", value);

    if (!sendCommand(cmd, res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't set aperture: %s.", ERR_NC(res));
        return false;
    }

    LOGF_INFO("Iris is %s by f/%.6g further.", (value > 0) ? "closed" : "opened", abs(value));

    return true;
}

bool PinefeatCEF::calibrate()
{
    char res[CEF_BUF] = {0};

    if (!sendCommand("c\n", res))
        return false;

    if (!strstr(res, "ok"))
    {
        LOGF_ERROR("Can't calibrate: %s.", ERR_NC(res));
        return false;
    }

    return true;
}

bool PinefeatCEF::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (CalibrateSP.isNameMatch(name))
        {
            CalibrateSP.reset();

            if (calibrate())
            {
                FocusAbsPosNP.setState(IPS_BUSY);
                FocusAbsPosNP.apply();

                FocusMaxPosNP.setState(IPS_BUSY);
                FocusMaxPosNP.apply();

                CalibrateSP.setState(IPS_OK);
            }
            else
                CalibrateSP.setState(IPS_ALERT);

            CalibrateSP.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool PinefeatCEF::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ApertureAbsNP.isNameMatch(name))
        {
            ApertureAbsNP.update(values, names, n);

            bool res = setApertureAbs(ApertureAbsNP[0].getValue());
            if (res)
                ApertureAbsNP.setState(IPS_OK);
            else
                ApertureAbsNP.setState(IPS_ALERT);

            ApertureAbsNP.apply();
            return res;
        }

        if (ApertureRelNP.isNameMatch(name))
        {
            ApertureRelNP.update(values, names, n);

            bool res = setApertureRel(ApertureRelNP[0].getValue());
            if (res)
                ApertureRelNP.setState(IPS_OK);
            else
                ApertureRelNP.setState(IPS_ALERT);

            ApertureRelNP.apply();
            return res;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool PinefeatCEF::SetFocuserSpeed(int speed)
{
    return setSpeed(speed);
}

IPState PinefeatCEF::MoveAbsFocuser(uint32_t targetTicks)
{
    if (!moveFocusAbs(targetTicks))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState PinefeatCEF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    if (!moveFocusRel(dir, ticks))
        return IPS_ALERT;

    return IPS_BUSY;
}

void PinefeatCEF::TimerHit()
{
    if (!isConnected())
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count();

    if ((elapsed >= 1 ||
            FocusAbsPosNP.getState() == IPS_BUSY ||
            FocusRelPosNP.getState() == IPS_BUSY ||
            FocusMaxPosNP.getState() == IPS_BUSY) &&
            isNotMoving())
    {
        int32_t pos;
        std::string dist, aper;
        if (readFocusPosition(pos)
                && readFocusDistance(dist)
                && readApertureRange(aper))
            updateProperties(pos, dist, aper);
    }

    SetTimer(getCurrentPollingPeriod());
}

bool PinefeatCEF::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
    {
        tcdrain(PortFD);
        return true;
    }

    if ((rc = tty_nread_section(PortFD, res, CEF_BUF, CEF_DEL, CEF_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
