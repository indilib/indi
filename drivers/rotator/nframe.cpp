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
    IUFillSwitch(&SteppingModeS[STEPPING_WAVE], "STEPPING_WAVE", "Wave", ISS_OFF);
    IUFillSwitch(&SteppingModeS[STEPPING_HALF], "STEPPING_HALF", "Half", ISS_OFF);
    IUFillSwitch(&SteppingModeS[STEPPING_FULL], "STEPPING_FULL", "Full", ISS_ON);
    IUFillSwitchVector(&SteppingModeSP, SteppingModeS, 3, getDeviceName(), "STEPPING_MODE", "Mode",
                       STEPPING_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    // Stepping Phase
    IUFillNumber(&SteppingPhaseN[0], "PHASES", "Wiring", "%.f", 0, 2, 1, 0);
    IUFillNumberVector(&SteppingPhaseNP, SteppingPhaseN, 1, getDeviceName(), "STEPPING_PHASE", "Phase",
                       STEPPING_TAB, IP_RW, 0, IPS_OK);


    IUFillNumber(&RotatorSpeedN[0], "ROTATE_SPEED_VALUE", "Step Rate", "%3.0f", 0.0, 255.0, 1.0, 255.0);
    IUFillNumberVector(&RotatorSpeedNP, RotatorSpeedN, 1, getDeviceName(), "ROTATE_SPEED", "Speed", MAIN_CONTROL_TAB, IP_RW, 0,
                       IPS_OK);

    // Max Speed
    IUFillNumber(&MaxSpeedN[0], "RATE", "Rate", "%.f", 1, 254, 10, 0);
    IUFillNumberVector(&MaxSpeedNP, MaxSpeedN, 1, getDeviceName(), "MAX_SPEED", "Max Speed", MAIN_CONTROL_TAB, IP_RW, 0,
                       IPS_OK);

    // Coil Energized Status
    IUFillSwitch(&CoilStatusS[COIL_ENERGIZED_OFF], "COIL_ENERGIZED_OFF", "De-energized", ISS_OFF);
    IUFillSwitch(&CoilStatusS[COIL_ENERGIZED_ON], "COIL_ENERGIZED_ON", "Energized", ISS_OFF);
    //    IUFillSwitch(&CoilStatusS[COIL_ENERGIZED_ON], "COIL_ENERGIZED_OFF", "Energized", ISS_OFF);
    IUFillSwitchVector(&CoilStatusSP, CoilStatusS, 2, getDeviceName(), "COIL_MODE", "Coil After Move",
                       OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);
    IUFillNumber(&SettingN[PARAM_STEPS_DEGREE], "PARAM_STEPS_DEGREE", "Steps/Degree", "%.2f", 1., 10000., 500., 1000.);
    IUFillNumberVector(&SettingNP, SettingN, 1, getDeviceName(), "ROTATOR_SETTINGS", "Parameters", SETTINGS_TAB, IP_RW, 0,
                       IPS_OK);
    // Rotator Ticks
    IUFillNumber(&RotatorAbsPosN[0], "ROTATOR_ABSOLUTE_POSITION", "Value", "%.f", 0., 1000000., 0., 0.);
    IUFillNumberVector(&RotatorAbsPosNP, RotatorAbsPosN, 1, getDeviceName(), "ABS_ROTATOR_POSITION", "Steps", MAIN_CONTROL_TAB,
                       IP_RW, 0, IPS_IDLE );


    addDebugControl();

    // Set limits as per documentation
    GotoRotatorN[0].min  = 0;
    GotoRotatorN[0].max  = 999999;
    GotoRotatorN[0].step = 1000;

    RotatorSpeedN[0].min  = 1;
    RotatorSpeedN[0].max  = 254;
    RotatorSpeedN[0].step = 10;

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
        defineProperty(&RotatorAbsPosNP);
        defineProperty(&SettingNP);
        defineProperty(&RotatorSpeedNP);
        defineProperty(&GotoRotatorNP);
        loadConfig(true, SettingNP.name);
        readPosition();
        readSpeedInfo();
        IDSetNumber(&RotatorAbsPosNP, nullptr);
        IDSetNumber(&GotoRotatorNP, nullptr);

        loadConfig(true, PresetNP.name);

        bool rc = getStartupValues();

        // Settings
        defineProperty(&MaxSpeedNP);
        defineProperty(&SteppingModeSP);
        defineProperty(&SteppingPhaseNP);
        defineProperty(&CoilStatusSP);

        if (rc)
            LOG_INFO("nFrameRotator is ready.");
        else
            LOG_WARN("Failed to query startup values.");
    }
    else
    {

        deleteProperty(MaxSpeedNP.name);
        deleteProperty(SteppingModeSP.name);
        deleteProperty(SteppingPhaseNP.name);
        deleteProperty(CoilStatusSP.name);
        deleteProperty(RotatorSpeedNP.name);
        deleteProperty(RotatorAbsPosNP.name);
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
        if (!strcmp(name, SteppingPhaseNP.name))
        {
            if (setSteppingPhase(static_cast<uint8_t>(values[0])))
            {
                IUUpdateNumber(&SteppingPhaseNP, values, names, n);
                SteppingPhaseNP.s = IPS_OK;
            }
            else
                SteppingPhaseNP.s = IPS_ALERT;

            IDSetNumber(&SteppingPhaseNP, nullptr);
            return true;
        }

        // Current speed

        if (!strcmp(name, RotatorSpeedNP.name))
        {
            if (SetRotatorSpeed(static_cast<uint8_t>(values[0])))
            {
                IUUpdateNumber(&RotatorSpeedNP, values, names, n);
                RotatorSpeedNP.s = IPS_OK;
            }
            else
            {
                RotatorSpeedNP.s = IPS_ALERT;
            }

            IDSetNumber(&RotatorSpeedNP, nullptr);
            return true;
        }

        // Max Speed
        if (!strcmp(name, MaxSpeedNP.name))
        {
            if (setMaxSpeed(static_cast<uint8_t>(values[0])))
            {
                IUUpdateNumber(&MaxSpeedNP, values, names, n);
                MaxSpeedNP.s = IPS_OK;

                // We must update the Min/Max of focus speed
                RotatorSpeedN[0].max = values[0];
                IUUpdateMinMax(&RotatorSpeedNP);
            }
            else
            {
                MaxSpeedNP.s = IPS_ALERT;
            }

            IDSetNumber(&MaxSpeedNP, nullptr);
            return true;
        }
        if (!strcmp(name, SettingNP.name))
        {
            bool rc = true;
            std::vector<double> prevValue(SettingNP.nnp);
            for (uint8_t i = 0; i < SettingNP.nnp; i++)
                prevValue[i] = SettingN[i].value;
            IUUpdateNumber(&SettingNP, values, names, n);


            if (SettingN[PARAM_STEPS_DEGREE].value != prevValue[PARAM_STEPS_DEGREE])
            {
                GotoRotatorN[0].value = calculateAngle(RotatorAbsPosN[0].value);
                IDSetNumber(&GotoRotatorNP, nullptr);
            }

            if (!rc)
            {
                for (uint8_t i = 0; i < SettingNP.nnp; i++)
                    SettingN[i].value = prevValue[i];
            }

            SettingNP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetNumber(&SettingNP, nullptr);
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
        if (!strcmp(name, SteppingModeSP.name))
        {
            IUUpdateSwitch(&SteppingModeSP, states, names, n);
            SteppingModeSP.s = IPS_OK;
            IDSetSwitch(&SteppingModeSP, nullptr);
            return true;
        }

        // Coil Status after Move is done
        if (!strcmp(name, CoilStatusSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&CoilStatusSP);
            LOGF_DEBUG("SETCOIL PINDEX=%d", prevIndex);
            IUUpdateSwitch(&CoilStatusSP, states, names, n);
            int state = IUFindOnSwitchIndex(&CoilStatusSP);
            if (setCoilStatus(state))
            {
                CoilStatusSP.s = IPS_OK;
                if (state == COIL_ENERGIZED_ON)
                    LOG_WARN("Coil shall be kept energized after motion is complete. Watch for motor heating!");
                else
                    LOG_INFO("Coil shall be de-energized after motion is complete.");
            }
            else
            {
                IUResetSwitch(&CoilStatusSP);
                CoilStatusS[prevIndex].s = ISS_ON;
                CoilStatusSP.s = IPS_ALERT;
                LOG_ERROR("Failed to update coil energization status.");
            }

            IDSetSwitch(&CoilStatusSP, nullptr);
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

    LOGF_DEBUG("Angle = <%f> Step/Deg=<%d>", angle, (int)SettingN[PARAM_STEPS_DEGREE].value);
    // Find closest distance
    double r = (angle > 180) ? 360 - angle : angle;
    int sign = (angle >= 0 && angle <= 180) ? 1 : -1;
    sign = 1;
    r = angle;

    r *= sign;
    r *= IUFindOnSwitchIndex(&ReverseRotatorSP) == INDI_ENABLED ? -1 : 1;

    double newTarget = r * SettingN[PARAM_STEPS_DEGREE].value + m_ZeroPosition;
    m_TargetDiff = (int)newTarget - (int)RotatorAbsPosN[0].value;
    return IPS_BUSY;
}


bool nFrameRotator::AbortRotator()
{
    if(m_TargetDiff > 0)
    {
	 m_TargetDiff = 1; wantAbort = true;  return false;
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
    if ( ((RotatorAbsPosNP.s == IPS_BUSY || GotoRotatorNP.s == IPS_BUSY) && isMoving() == false) || wantAbort == true)
    {
	LOGF_DEBUG("wantAbort = %d, diff = %d",wantAbort,m_TargetDiff);
        // Are we done moving?
        if (m_TargetDiff == 0)
        {
            RotatorAbsPosNP.s = IPS_OK;
            GotoRotatorNP.s = IPS_OK;
	    LOGF_DEBUG("HIT reqAngle=%f diff=%d",requestedAngle,m_TargetDiff);
            IDSetNumber(&RotatorAbsPosNP, nullptr);
	    wantAbort = false;
        }
        else
        {
            // 999 is the max we can go in one command
            // so we need to go 999 or LESS
            // therefore for larger movements, we break it down.
            int nextMotion = (std::abs(m_TargetDiff) > 999) ? 999 : std::abs(m_TargetDiff);
            int direction = m_TargetDiff > 0 ? ROTATE_OUTWARD : ROTATE_INWARD;
            int mode = IUFindOnSwitchIndex(&SteppingModeSP);
            char cmd[NFRAME_LEN] = {0};
            snprintf(cmd, NFRAME_LEN, ":F%d%d%03d#", direction, mode, nextMotion);
            if (sendCommand(cmd) == false)
            {
                LOG_ERROR("Failed to issue motion command.");
                if (GotoRotatorNP.s == IPS_BUSY)
                {
                    GotoRotatorNP.s = IPS_ALERT;
                    IDSetNumber(&GotoRotatorNP, nullptr);
                }
                if (RotatorAbsPosNP.s == IPS_BUSY)
                {
                    RotatorAbsPosNP.s = IPS_ALERT;
                    IDSetNumber(&RotatorAbsPosNP, nullptr);
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
        IDSetNumber(&RotatorAbsPosNP, nullptr);
    }
    IDSetNumber(&RotatorAbsPosNP, nullptr);
    IDSetNumber(&GotoRotatorNP, nullptr);

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

    RotatorAbsPosN[0].value = pos;
//    if(m_TargetDiff != 0) 
      GotoRotatorN[0].value = calculateAngle(RotatorAbsPosN[0].value );
      IDSetNumber(&RotatorAbsPosNP, nullptr);
      IDSetNumber(&GotoRotatorNP, nullptr);

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

    MaxSpeedN[0].value = 254 - max_step + 1;
    MaxSpeedNP.s = IPS_OK;

    // nStep defines speed step rates from 1 to 254
    // when 1 being the fastest, so for speed we flip the values
    RotatorSpeedN[0].max   = 254 - max_step + 1;
    RotatorSpeedN[0].value = 254 - current_step + 1;
    IDSetNumber(&RotatorSpeedNP, nullptr);
    RotatorSpeedNP.s = IPS_OK;
    LOGF_DEBUG("Speed= %f cs=%d", RotatorSpeedN[0].value, current_step);

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

    SteppingPhaseN[0].value = phase;
    SteppingPhaseNP.s = IPS_OK;

    return true;
}

bool nFrameRotator::readCoilStatus()
{
    char res[NFRAME_LEN] = {0};

    if (sendCommand(":RC", res, 3, 1) == false)
        return false;

    IUResetSwitch(&CoilStatusSP);
    LOGF_DEBUG("Coil status = %c", res[0]);

    CoilStatusS[COIL_ENERGIZED_OFF].s = (res[0] == '0') ? ISS_OFF : ISS_ON;
    CoilStatusS[COIL_ENERGIZED_ON].s  = (res[0] == '0') ? ISS_ON : ISS_OFF;
    CoilStatusSP.s = IPS_OK;

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
    r *= IUFindOnSwitchIndex(&ReverseRotatorSP) == INDI_ENABLED ? -1 : 1;
    double newTarget = r * SettingN[PARAM_STEPS_DEGREE].value + m_ZeroPosition;

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

    IUSaveConfigSwitch(fp, &SteppingModeSP);
    IUSaveConfigNumber(fp, &SettingNP);


    return true;
}
double nFrameRotator::calculateAngle(uint32_t steps)
{
    int diff = (static_cast<int32_t>(steps) - m_ZeroPosition) *
               (IUFindOnSwitchIndex(&ReverseRotatorSP) == INDI_ENABLED ? -1 : 1);
//    LOGF_DEBUG("RANGE=%f",(int)(range360((float(diff)+0.5) / SettingN[PARAM_STEPS_DEGREE].value)*100)/100.);
    return range360((float(diff)+0.5) / SettingN[PARAM_STEPS_DEGREE].value);
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

    loadConfig(true, SettingNP.name);
}

