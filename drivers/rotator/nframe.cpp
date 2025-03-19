/*
  nFrameRotator Rotator

  nFrame added by Gene N
  Modified from indi_nstep_focuser

  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#include "nframe.h"

#include "indicom.h"

#include <cstring>
#include <termios.h>
#include <memory>
#include <thread>
#include <chrono>

static std::unique_ptr<nFrameRotator> rotator(new nFrameRotator());


nFrameRotator::nFrameRotator()
{
    setVersion(1, 2);
    RI::SetCapability(ROTATOR_CAN_ABORT |
                      ROTATOR_CAN_SYNC
                     );
}

bool nFrameRotator::initProperties()
{
    INDI::Rotator::initProperties();



    // Stepping Modes
    SteppingModeSP[STEPPING_WAVE].fill("STEPPING_WAVE", "Wave", ISS_OFF);
    SteppingModeSP[STEPPING_HALF].fill( "STEPPING_HALF", "Half", ISS_OFF);
    SteppingModeSP[STEPPING_FULL].fill( "STEPPING_FULL", "Full", ISS_ON);
    SteppingModeSP.fill(getDeviceName(), "STEPPING_MODE", "Mode",
                        STEPPING_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    // Stepping Phase
    SteppingPhaseNP[0].fill("PHASES", "Wiring", "%.f", 0, 2, 1, 0);
    SteppingPhaseNP.fill(getDeviceName(), "STEPPING_PHASE", "Phase",
                         STEPPING_TAB, IP_RW, 0, IPS_OK);


    RotatorSpeedNP[0].fill("ROTATE_SPEED_VALUE", "Step Rate", "%3.0f", 0.0, 255.0, 1.0, 255.0);
    RotatorSpeedNP.fill(getDeviceName(), "ROTATE_SPEED", "Speed", MAIN_CONTROL_TAB, IP_RW, 0,
                        IPS_OK);

    // Max Speed
    MaxSpeedNP[0].fill("RATE", "Rate", "%.f", 1, 254, 10, 0);
    MaxSpeedNP.fill(getDeviceName(), "MAX_SPEED", "Max Speed", MAIN_CONTROL_TAB, IP_RW, 0,
                    IPS_OK);

    // Coil Energized Status
    CoilStatusSP[COIL_ENERGIZED_OFF].fill("COIL_ENERGIZED_OFF", "De-energized", ISS_OFF);
    CoilStatusSP[COIL_ENERGIZED_ON].fill("COIL_ENERGIZED_ON", "Energized", ISS_OFF);
    //    IUFillSwitch(&CoilStatusS[COIL_ENERGIZED_ON], "COIL_ENERGIZED_OFF", "Energized", ISS_OFF);
    CoilStatusSP.fill(getDeviceName(), "COIL_MODE", "Coil After Move",
                      OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);
    SettingNP[PARAM_STEPS_DEGREE].fill("PARAM_STEPS_DEGREE", "Steps/Degree", "%.2f", 1., 10000., 500., 1000.);
    SettingNP.fill(getDeviceName(), "ROTATOR_SETTINGS", "Parameters", SETTINGS_TAB, IP_RW, 0,
                   IPS_OK);
    // Rotator Ticks
    RotatorAbsPosNP[0].fill("ROTATOR_ABSOLUTE_POSITION", "Value", "%.f", 0., 1000000., 0., 0.);
    RotatorAbsPosNP.fill(getDeviceName(), "ABS_ROTATOR_POSITION", "Steps", MAIN_CONTROL_TAB,
                         IP_RW, 0, IPS_IDLE );


    addDebugControl();

    // Set limits as per documentation
    GotoRotatorNP[0].setMin(0);
    GotoRotatorNP[0].setMax(999999);
    GotoRotatorNP[0].setStep(1000);

    RotatorSpeedNP[0].setMin(1);
    RotatorSpeedNP[0].setMax(254);
    RotatorSpeedNP[0].setStep(10);

    return true;
}

const char *nFrameRotator::getDefaultName()
{
    return "nFrameRotator";
}

bool nFrameRotator::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        // Read these values before defining focuser interface properties
        defineProperty(RotatorAbsPosNP);
        defineProperty(SettingNP);
        defineProperty(RotatorSpeedNP);
        defineProperty(GotoRotatorNP);
        SettingNP.load();
        readPosition();
        readSpeedInfo();
        RotatorAbsPosNP.apply();
        GotoRotatorNP.apply();

        PresetNP.load();

        bool rc = getStartupValues();

        // Settings
        defineProperty(MaxSpeedNP);
        defineProperty(SteppingModeSP);
        defineProperty(SteppingPhaseNP);
        defineProperty(CoilStatusSP);

        if (rc)
            LOG_INFO("nFrameRotator is ready.");
        else
            LOG_WARN("Failed to query startup values.");
    }
    else
    {

        deleteProperty(MaxSpeedNP);
        deleteProperty(SteppingModeSP);
        deleteProperty(SteppingPhaseNP);
        deleteProperty(CoilStatusSP);
        deleteProperty(RotatorSpeedNP);
        deleteProperty(RotatorAbsPosNP);
    }

    return true;
}

bool nFrameRotator::Handshake()
{
    char cmd[NFRAME_LEN] = {0}, res[NFRAME_LEN] = {0};

    // Ack
    cmd[0] = 0x6;

    bool rc = sendCommand(cmd, res, 1, 1);
    if (rc == false)
        return false;

    return res[0] == 'S';
}

bool nFrameRotator::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[NFRAME_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
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
        rc = tty_read(PortFD, res, res_len, NFRAME_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, NFRAME_LEN, NFRAME_STOP_CHAR, NFRAME_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[NFRAME_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void nFrameRotator::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

bool nFrameRotator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        // Stepping Phase
        if (SteppingPhaseNP.isNameMatch(name))
        {
            if (setSteppingPhase(static_cast<uint8_t>(values[0])))
            {
                SteppingPhaseNP.update(values, names, n);
                SteppingPhaseNP.setState(IPS_OK);
            }
            else
                SteppingPhaseNP.setState(IPS_ALERT);

            SteppingPhaseNP.apply();
            return true;
        }

        // Current speed

        if (RotatorSpeedNP.isNameMatch(name))
        {
            if (SetRotatorSpeed(static_cast<uint8_t>(values[0])))
            {
                RotatorSpeedNP.update(values, names, n);
                RotatorSpeedNP.setState(IPS_OK);
            }
            else
            {
                RotatorSpeedNP.setState(IPS_ALERT);
            }

            RotatorSpeedNP.apply();
            return true;
        }

        // Max Speed
        if (MaxSpeedNP.isNameMatch(name))
        {
            if (setMaxSpeed(static_cast<uint8_t>(values[0])))
            {
                MaxSpeedNP.update(values, names, n);
                MaxSpeedNP.setState(IPS_OK);

                // We must update the Min/Max of focus speed
                RotatorSpeedNP[0].setMax(values[0]);
                RotatorSpeedNP.updateMinMax();
            }
            else
            {
                MaxSpeedNP.setState(IPS_ALERT);
            }

            MaxSpeedNP.apply();
            return true;
        }
        if (SettingNP.isNameMatch(name))
        {
            bool rc = true;
            std::vector<double> prevValue(SettingNP.count());
            for (uint8_t i = 0; i < SettingNP.count(); i++)
                prevValue[i] = SettingNP[i].getValue();
            SettingNP.update(values, names, n);


            if (SettingNP[PARAM_STEPS_DEGREE].value != prevValue[PARAM_STEPS_DEGREE])
            {
                GotoRotatorNP[0].setValue(calculateAngle(RotatorAbsPosNP[0].getValue()));
                GotoRotatorNP.apply();
            }

            if (!rc)
            {
                for (uint8_t i = 0; i < SettingNP.count(); i++)
                    SettingNP[i].setValue(prevValue[i]);
            }

            SettingNP.setState(rc ? IPS_OK : IPS_ALERT);
            SettingNP.apply();
            return true;
        }

    }

    return INDI::Rotator::ISNewNumber(dev, name, values, names, n);
}

bool nFrameRotator::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        // Stepping Mode
        if (SteppingModeSP.isNameMatch(name))
        {
            SteppingModeSP.update(states, names, n);
            SteppingModeSP.setState(IPS_OK);
            SteppingModeSP.apply();
            return true;
        }

        // Coil Status after Move is done
        if (CoilStatusSP.isNameMatch(name))
        {
            int prevIndex = CoilStatusSP.findOnSwitchIndex();
            LOGF_DEBUG("SETCOIL PINDEX=%d", prevIndex);
            CoilStatusSP.update(states, names, n);
            int state = CoilStatusSP.findOnSwitchIndex();
            if (setCoilStatus(state))
            {
                CoilStatusSP.setState(IPS_OK);
                if (state == COIL_ENERGIZED_ON)
                    LOG_WARN("Coil shall be kept energized after motion is complete. Watch for motor heating!");
                else
                    LOG_INFO("Coil shall be de-energized after motion is complete.");
            }
            else
            {
                CoilStatusSP.reset();
                CoilStatusSP[prevIndex].setState(ISS_ON);
                CoilStatusSP.setState(IPS_ALERT);
                LOG_ERROR("Failed to update coil energization status.");
            }

            CoilStatusSP.apply();
            return true;
        }
    }

    return INDI::Rotator::ISNewSwitch(dev, name, states, names, n);
}

bool nFrameRotator::getStartupValues()
{
    bool rc1 = readCoilStatus();
    bool rc2 = readSteppingInfo();

    return (rc1 && rc2 );
}

IPState nFrameRotator::MoveRotator(double angle)
{
    requestedAngle = angle ;

    LOGF_DEBUG("Angle = <%f> Step/Deg=<%d>", angle, (int)SettingNP[PARAM_STEPS_DEGREE].getValue());
    // Find closest distance
    double r = (angle > 180) ? 360 - angle : angle;
    int sign = (angle >= 0 && angle <= 180) ? 1 : -1;
    sign = 1;
    r = angle;

    r *= sign;
    r *= ReverseRotatorSP.findOnSwitchIndex() == INDI_ENABLED ? -1 : 1;

    double newTarget = r * SettingNP[PARAM_STEPS_DEGREE].getValue() + m_ZeroPosition;
    m_TargetDiff = (int)newTarget - (int)RotatorAbsPosNP[0].getValue();
    return IPS_BUSY;
}


bool nFrameRotator::AbortRotator()
{
    if(m_TargetDiff > 0)
    {
        m_TargetDiff = 1;
        wantAbort = true;
        return false;
    }
    return true;
}

void nFrameRotator::TimerHit()
{
    if (isConnected() == false)
        return;

    readPosition();

    // Check if we have a pending motion
    // and if we STOPPED, then let's take the next action
    if ( ((RotatorAbsPosNP.getState() == IPS_BUSY || GotoRotatorNP.getState() == IPS_BUSY) && isMoving() == false) || wantAbort == true)
    {
        LOGF_DEBUG("wantAbort = %d, diff = %d", wantAbort, m_TargetDiff);
        // Are we done moving?
        if (m_TargetDiff == 0)
        {
            RotatorAbsPosNP.setState(IPS_OK);
            GotoRotatorNP.setState(IPS_OK);
            LOGF_DEBUG("HIT reqAngle=%f diff=%d", requestedAngle, m_TargetDiff);
            RotatorAbsPosNP.apply();
            wantAbort = false;
        }
        else
        {
            // 999 is the max we can go in one command
            // so we need to go 999 or LESS
            // therefore for larger movements, we break it down.
            int nextMotion = (std::abs(m_TargetDiff) > 999) ? 999 : std::abs(m_TargetDiff);
            int direction = m_TargetDiff > 0 ? ROTATE_OUTWARD : ROTATE_INWARD;
            int mode = SteppingModeSP.findOnSwitchIndex();
            char cmd[NFRAME_LEN] = {0};
            snprintf(cmd, NFRAME_LEN, ":F%d%d%03d#", direction, mode, nextMotion);
            if (sendCommand(cmd) == false)
            {
                LOG_ERROR("Failed to issue motion command.");
                if (GotoRotatorNP.getState() == IPS_BUSY)
                {
                    GotoRotatorNP.setState(IPS_ALERT);
                    GotoRotatorNP.apply();
                }
                if (RotatorAbsPosNP.getState() == IPS_BUSY)
                {
                    RotatorAbsPosNP.setState(IPS_ALERT);
                    RotatorAbsPosNP.apply();
                }
            }
            else
            {
                // Reduce target diff depending on the motion direction
                // Negative targetDiff increases eventually to zero
                // Positive targetDiff decreases eventually to zero
                m_TargetDiff = m_TargetDiff + (nextMotion * ((direction == ROTATE_INWARD) ? 1 : -1));
            }
        }
        // Check if can update the absolute position in case it changed.
    }
    if (m_TargetDiff == 0)
    {
        RotatorAbsPosNP.apply();
    }
    RotatorAbsPosNP.apply();
    GotoRotatorNP.apply();

    SetTimer(getCurrentPollingPeriod());
}

bool nFrameRotator::isMoving()
{
    char res[NFRAME_LEN] = {0};

    bool rc = sendCommand("S", res, 1, 1);

    if (rc && res[0] == '1')
        return true;

    return false;
}


bool nFrameRotator::readPosition()
{
    char res[NFRAME_LEN] = {0};

    if (sendCommand(":RP", res, 3, 7) == false)
        return false;

    int32_t pos = 1e6;
    sscanf(res, "%d", &pos);

    if (pos == 1e6)
        return false;
    //    LOGF_DEBUG("readPosFBPNB=< %d >", pos);

    RotatorAbsPosNP[0].setValue(pos);
    //    if(m_TargetDiff != 0)
    GotoRotatorNP[0].setValue(calculateAngle(RotatorAbsPosNP[0].getValue() ));
    RotatorAbsPosNP.apply();
    GotoRotatorNP.apply();

    return true;
}


bool nFrameRotator::readSpeedInfo()
{
    char res[NFRAME_LEN] = {0};
    int32_t max_step = 1e6, current_step = 1e6;

    // Max Step
    if (sendCommand(":RS", res, 3, 3) == false)
        return false;
    sscanf(res, "%d", &max_step);
    if (max_step == 1e6)
        return false;

    // Current Step
    if (sendCommand(":RO", res, 3, 3) == false)
        return false;
    sscanf(res, "%d", &current_step);
    if (current_step == 1e6)
        return false;

    MaxSpeedNP[0].setValue(254 - max_step + 1);
    MaxSpeedNP.setState(IPS_OK);

    // nStep defines speed step rates from 1 to 254
    // when 1 being the fastest, so for speed we flip the values
    RotatorSpeedNP[0].setMax(254 - max_step + 1);
    RotatorSpeedNP[0].setValue(254 - current_step + 1);
    RotatorSpeedNP.apply();
    RotatorSpeedNP.setState(IPS_OK);
    LOGF_DEBUG("Speed= %f cs=%d", RotatorSpeedNP[0].getValue(), current_step);

    return true;
}

bool nFrameRotator::readSteppingInfo()
{
    char res[NFRAME_LEN] = {0};

    if (sendCommand(":RW", res, 3, 1) == false)
        return false;

    int32_t phase = 1e6;
    sscanf(res, "%d", &phase);

    if (phase == 1e6)
        return false;

    SteppingPhaseNP[0].setValue(phase);
    SteppingPhaseNP.setState(IPS_OK);

    return true;
}

bool nFrameRotator::readCoilStatus()
{
    char res[NFRAME_LEN] = {0};

    if (sendCommand(":RC", res, 3, 1) == false)
        return false;

    CoilStatusSP.reset();
    LOGF_DEBUG("Coil status = %c", res[0]);

    CoilStatusSP[COIL_ENERGIZED_OFF].setState((res[0] == '0') ? ISS_OFF : ISS_ON);
    CoilStatusSP[COIL_ENERGIZED_ON].setState((res[0] == '0') ? ISS_ON : ISS_OFF);
    CoilStatusSP.setState(IPS_OK);

    return true;
}

bool nFrameRotator::SyncRotator(double angle)
{
    //    double min = range360(SettingN[PARAM_MIN_LIMIT].value);
    //    double max = range360(SettingN[PARAM_MAX_LIMIT].value);
    // Clamp to range
    //    angle = std::max(min, std::min(max, angle));

    // Find closest distance
    double r = (angle > 180) ? 360 - angle : angle;
    int sign = (angle >= 0 && angle <= 180) ? 1 : -1;

    r *= sign;
    r *= ReverseRotatorSP.findOnSwitchIndex() == INDI_ENABLED ? -1 : 1;
    double newTarget = r * SettingNP[PARAM_STEPS_DEGREE].getValue() + m_ZeroPosition;

    char cmd[NFRAME_LEN] = {0};
    snprintf(cmd, NFRAME_LEN, "#:CP+%06d#", (int)newTarget);
    return sendCommand(cmd);
}

bool nFrameRotator::SetRotatorSpeed(uint8_t speed)
{
    // Speed and Current nFrameRotator steps are opposite.
    // Speed 1 is slowest, translated to 254 for nStep.
    char cmd[NFRAME_LEN] = {0};
    snprintf(cmd, NFRAME_LEN, "#:CO%03d#", 254 - speed + 1);
    return sendCommand(cmd);
}

bool nFrameRotator::setMaxSpeed(uint8_t maxSpeed)
{
    // INDI Rotate Speed and Current nFrameRotator steps are opposite.
    // INDI Speed 1 is slowest, translated to 254 for nFrame.
    // and vice versa
    char cmd[NFRAME_LEN] = {0};
    snprintf(cmd, NFRAME_LEN, ":CS%03d#", 254 - maxSpeed + 1);
    return sendCommand(cmd);
}



bool nFrameRotator::setSteppingPhase(uint8_t phase)
{
    char cmd[NFRAME_LEN] = {0};
    snprintf(cmd, NFRAME_LEN, "#:CW%01d#", phase);
    return sendCommand(cmd);
}

bool nFrameRotator::setCoilStatus(uint8_t status)
{
    char cmd[NFRAME_LEN] = {0};
    snprintf(cmd, NFRAME_LEN, "#:CC%01d#", status == COIL_ENERGIZED_OFF ? 1 : 0);
    LOGF_DEBUG("setCoil = %d hex=%x", status, status);
    LOGF_DEBUG("setCoilS = %s CEOFF=%d CEON = %d", cmd, COIL_ENERGIZED_OFF, COIL_ENERGIZED_ON);
    return sendCommand(cmd);
}

bool nFrameRotator::saveConfigItems(FILE *fp)
{
    INDI::Rotator::saveConfigItems(fp);

    SteppingModeSP.save(fp);
    SettingNP.save(fp);


    return true;
}
double nFrameRotator::calculateAngle(uint32_t steps)
{
    int diff = (static_cast<int32_t>(steps) - m_ZeroPosition) *
               (ReverseRotatorSP.findOnSwitchIndex() == INDI_ENABLED ? -1 : 1);
    //    LOGF_DEBUG("RANGE=%f",(int)(range360((float(diff)+0.5) / SettingN[PARAM_STEPS_DEGREE].value)*100)/100.);
    return range360((float(diff) + 0.5) / SettingNP[PARAM_STEPS_DEGREE].getValue());
    //   return (int)(range360((float(diff)+0.5) / SettingN[PARAM_STEPS_DEGREE].value)*100)/100.;
}

/*
bool nFrameRotator::setSpeedRange(uint32_t min, uint32_t max)
{
    return true;
}
*/

void nFrameRotator::ISGetProperties(const char *dev)
{
    INDI::Rotator::ISGetProperties(dev);

    SettingNP.load();
}

