/*
    Rainbow Astro Focuser
    Copyright (C) 2020 Abdulaziz Bouland (boulandab@ikarustech.com)

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

/*
This is the Rainbow API. A pdf is in the driver docs folder.

Command              Send      Receive     Comment

Get Position         :Fp#      :FPsDD.DDD# s is + or -,
                                           sDD.DDD is a float number
                                           range: -08.000 to +08.000
                                           unit millimeters

Is Focus Moving      :Fs#      :FS0#       not moving
                               :FS1#       moving

Temperature          :Ft1#      :FT1sDD.D# s is + or -, DD.D temp in celcius

Move Absolute        :FmsDDDD#  :FM#       s is + or -, DDDD from -8000 to 8000

Move Relative        :FnsDDDD#  :FM#       s is + or -, DDDD from -8000 to 8000
                                           neg numbers move away from main mirror

Move Home            :Fh#       :FH#
*/

#include "rainbowRSF.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <termios.h>

static std::unique_ptr<RainbowRSF> rainbowRSF(new RainbowRSF());

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

    // Go home switch
    IUFillSwitch(&GoHomeS[0], "GO_HOME", "Go Home", ISS_OFF);
    IUFillSwitchVector(&GoHomeSP, GoHomeS, 1, getDeviceName(), "FOCUS_GO_HOME", "Home", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1,
                       60, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB,
                       IP_RO, 0, IPS_IDLE);

    // Focuser Limits
    FocusAbsPosNP[0].setMin(0);
    FocusAbsPosNP[0].setMax(16000);
    FocusAbsPosNP[0].setStep(1000);

    FocusMaxPosNP[0].setMin(0 );
    FocusMaxPosNP[0].setMax(16000);
    FocusMaxPosNP[0].setStep(1000);
    FocusMaxPosNP[0].setValue(16000);
    FocusMaxPosNP.setPermission(IP_RO);

    FocusRelPosNP[0].setMin(0);
    FocusRelPosNP[0].setMax(8000);
    FocusRelPosNP[0].setStep(1000);

    addSimulationControl();
    addDebugControl();

    m_MovementTimerActive = false;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&TemperatureNP);
        defineProperty(&GoHomeSP);
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(GoHomeSP.name);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Find Home
        if (!strcmp(name, GoHomeSP.name))
        {
            GoHomeSP.s = findHome() ? IPS_BUSY : IPS_ALERT;

            if (GoHomeSP.s == IPS_BUSY)
                LOG_INFO("Moving to home position...");
            else
                LOG_ERROR("Failed to move to home position.");

            IDSetSwitch(&GoHomeSP, nullptr);
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::Handshake()
{
    if (updateTemperature())
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

namespace
{
bool parsePosition(char *result, int *pos)
{
    const int length = strlen(result);
    if (length < 6) return false;
    // Check for a decimal/period
    char *period = strchr(result + 3, '.');
    if (period == nullptr) return false;

    float position;
    if (sscanf(result, ":FP%f#", &position) == 1)
    {
        // position is a float number between -8 and +8 that needs to be multiplied by 1000.
        *pos = position * 1000;
        return true;
    }
    return false;
}
}  // namespace


bool RainbowRSF::updatePosition()
{
    char res[DRIVER_LEN] = {0};

    /////////////////////////////////////////////////////////////////////////////
    /// Simulation
    /////////////////////////////////////////////////////////////////////////////
    if (isSimulation())
    {
        // Move the focuser
        if (FocusAbsPosNP[0].getValue() > m_TargetPosition)
        {
            FocusAbsPosNP[0].setValue(FocusAbsPosNP[0].getValue() - 500);
            if (FocusAbsPosNP[0].getValue() < m_TargetPosition)
                FocusAbsPosNP[0].setValue(m_TargetPosition);
        }
        else if (FocusAbsPosNP[0].getValue() < m_TargetPosition)
        {
            FocusAbsPosNP[0].setValue(FocusAbsPosNP[0].getValue() + 500);            
            if (FocusAbsPosNP[0].getValue() > m_TargetPosition)
                FocusAbsPosNP[0].setValue(m_TargetPosition);
        }

        // update the states
        if (FocusAbsPosNP[0].getValue() == m_TargetPosition)
        {
            if (GoHomeSP.s == IPS_BUSY)
            {
                GoHomeSP.s = IPS_OK;
                FocusAbsPosNP.setState(IPS_OK);
                FocusRelPosNP.setState(IPS_OK);
                IDSetSwitch(&GoHomeSP, nullptr);
                FocusAbsPosNP.apply();
                FocusRelPosNP.apply();
                LOG_INFO("Focuser reached home position.");
            }

            else if (FocusAbsPosNP.getState() == IPS_BUSY)
            {
                FocusAbsPosNP.setState(IPS_OK);
                FocusRelPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();
                FocusRelPosNP.apply();
                LOG_INFO("Focuser reached target position.");
            }
        }

        return true;
    }

    /////////////////////////////////////////////////////////////////////////////
    /// Real Driver
    /////////////////////////////////////////////////////////////////////////////
    else if (sendCommand(":Fp#", res, DRIVER_LEN) == false)
        return false;

    int newPosition { 0 };
    bool ok = parsePosition(res, &newPosition);
    if (ok)
    {
        FocusAbsPosNP[0].setValue(newPosition + 8000);

        constexpr int TOLERANCE = 1;  // Off-by-one position is ok given the resolution of the response.
        const int offset = std::abs(static_cast<int>(FocusAbsPosNP[0].getValue() - m_TargetPosition));
        bool focuserDone = offset <= TOLERANCE;

        // Try to hit the target position, but if it has been close for a while
        // then we believe the focuser movement is done. This is needed because it
        // sometimes stops 1 position away from the target, and occasionally 2 or 3.
        if (!focuserDone && ((GoHomeSP.s == IPS_BUSY) || (FocusAbsPosNP.getState() == IPS_BUSY)))
        {
            if (!m_MovementTimerActive)
            {
                // Waiting for motion completion. Initialize the start time for timeouts.
                m_MovementTimer.start();
                m_MovementTimerActive = true;
            }
            else
            {
                const double elapsedSeconds = m_MovementTimer.elapsed() / 1000.0;
                if ((elapsedSeconds > 5 && offset < 3) ||
                        (elapsedSeconds > 10 && offset < 6) ||
                        (elapsedSeconds > 60))
                {
                    focuserDone = true;
                    LOGF_INFO("Rainbow focuser timed out: %.1f seconds, offset %d (target %d, position %d)",
                              elapsedSeconds, offset, m_TargetPosition, static_cast<int>(FocusAbsPosNP[0].getValue()));
                }
            }
        }

        if (focuserDone)
        {
            m_MovementTimerActive = false;
            if (GoHomeSP.s == IPS_BUSY)
            {
                GoHomeSP.s = IPS_OK;
                GoHomeS[0].s = ISS_OFF;
                IDSetSwitch(&GoHomeSP, nullptr);

                FocusAbsPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();

                FocusRelPosNP.setState(IPS_OK);
                FocusRelPosNP.apply();

                LOG_INFO("Focuser reached home position.");
            }

            else if (FocusAbsPosNP.getState() == IPS_BUSY)
            {
                FocusAbsPosNP.setState(IPS_OK);
                FocusAbsPosNP.apply();

                FocusRelPosNP.setState(IPS_OK);

                FocusRelPosNP.apply();
                LOGF_INFO("Focuser reached target position %d at %d.",
                          m_TargetPosition, static_cast<int>(FocusAbsPosNP[0].getValue()));
            }
        }
        return true;
    }
    else
    {
        FocusAbsPosNP.setState(IPS_ALERT);
        return false;
    }

}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::updateTemperature()
{
    char res[DRIVER_LEN] = {0};
    float temperature = 0;

    if (isSimulation())
        strncpy(res, ":FT1+23.5#", DRIVER_LEN);

    else if (sendCommand(":Ft1#", res, DRIVER_LEN) == false)
        return false;

    if (sscanf(res, ":FT1%g", &temperature) == 1)
    {
        TemperatureN[0].value = temperature;
        TemperatureNP.s = IPS_OK;
        return true;
    }
    else
    {
        TemperatureNP.s = IPS_ALERT;
        return false;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
IPState RainbowRSF::MoveAbsFocuser(uint32_t targetTicks)
{
    m_MovementTimerActive = false;
    m_TargetPosition = targetTicks;

    char cmd[DRIVER_LEN] = {0};
    char res[DRIVER_LEN] = {0};
    int steps = targetTicks - 8000;

    snprintf(cmd, 16, ":Fm%c%04d#", steps >= 0 ? '+' : '-', std::abs(steps));

    if (isSimulation() == false)
    {
        if (sendCommand(cmd, res, DRIVER_LEN) == false)
            return IPS_ALERT;
    }
    return IPS_BUSY;
}

IPState RainbowRSF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    m_MovementTimerActive = false;
    int reversed = (FocusReverseSP.findOnSwitchIndex() == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosNP[0].getValue() + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::findHome()
{
    if (isSimulation())
    {
        MoveAbsFocuser(homePosition);
        FocusAbsPosNP.setState(IPS_BUSY);
        return true;
    }
    else
    {
        m_MovementTimerActive = false;
        m_TargetPosition = homePosition;
        FocusAbsPosNP.setState(IPS_BUSY);
        char res[DRIVER_LEN] = {0};
        return sendCommand(":Fh#", res, DRIVER_LEN);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void RainbowRSF::TimerHit()
{
    // position update
    bool rc = updatePosition();
    if (rc)
    {
        if (abs(m_LastPosition - FocusAbsPosNP[0].getValue()) > 0)
        {
            FocusAbsPosNP.apply();
            m_LastPosition = FocusAbsPosNP[0].getValue();

            if (GoHomeSP.s == IPS_BUSY && FocusAbsPosNP[0].getValue() == homePosition)
            {
                GoHomeSP.s = IPS_OK;
                LOG_INFO("Focuser arrived at home position.");
                IDSetSwitch(&GoHomeSP, nullptr);
            }
        }
    }

    // temperature update
    if (m_TemperatureCounter++ == DRIVER_TEMPERATURE_FREQ)
    {
        rc = updateTemperature();
        if (rc)
        {
            if (abs(m_LastTemperature - TemperatureN[0].value) >= 0.05)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                m_LastTemperature = TemperatureN[0].value;
            }
        }
        // Reset the counter
        m_TemperatureCounter = 0;
    }

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool RainbowRSF::sendCommand(const char * cmd, char * res, int res_len)
{
    if (cmd == nullptr || res == nullptr || res_len <= 0)
        return false;
    const int cmd_len = strlen(cmd);
    if (cmd_len <= 0)
        return false;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    int nbytes_written = 0;
    int rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    int nbytes_read = 0;
    rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR,
                           DRIVER_TIMEOUT, &nbytes_read);
    if (nbytes_read == DRIVER_LEN)
        return false;
    res[nbytes_read] = 0;
    if (rc != TTY_OK)
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
