/*
    RoboFocus
    Copyright (C) 2006 Markus Wildi (markus.wildi@datacomm.ch)
                  2011 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "robofocus.h"

#include "indicom.h"

#include <memory>
#include <cstring>
#include <termios.h>

#define RF_MAX_CMD  9
#define RF_TIMEOUT  3

#define BACKLASH_READOUT  99999
#define MAXTRAVEL_READOUT 99999

#define currentDuty             SettingsN[0].value
#define currentDelay            SettingsN[1].value
#define currentTicks            SettingsN[2].value
#define currentMaxTravel        MaxTravelN[0].value
#define currentMinPosition      MinMaxPositionN[0].value
#define currentMaxPosition      MinMaxPositionN[1].value


#define SETTINGS_TAB "Settings"

static std::unique_ptr<RoboFocus> roboFocus(new RoboFocus());

RoboFocus::RoboFocus()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC |
                      FOCUSER_HAS_BACKLASH);
}

bool RoboFocus::initProperties()
{
    INDI::Focuser::initProperties();

    /* Focuser temperature */
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", 0, 65000., 0., 10000.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    /* Settings of the Robofocus */
    IUFillNumber(&SettingsN[0], "Duty cycle", "Duty cycle", "%6.0f", 0., 255., 0., 1.0);
    IUFillNumber(&SettingsN[1], "Step Delay", "Step delay", "%6.0f", 0., 255., 0., 1.0);
    IUFillNumber(&SettingsN[2], "Motor Steps", "Motor steps per tick", "%6.0f", 0., 255., 0., 1.0);
    IUFillNumberVector(&SettingsNP, SettingsN, 3, getDeviceName(), "FOCUS_SETTINGS", "Settings", SETTINGS_TAB, IP_RW, 0,
                       IPS_IDLE);

    /* Power Switches of the Robofocus */
    IUFillSwitch(&PowerSwitchesS[0], "1", "Switch 1", ISS_OFF);
    IUFillSwitch(&PowerSwitchesS[1], "2", "Switch 2", ISS_OFF);
    IUFillSwitch(&PowerSwitchesS[2], "3", "Switch 3", ISS_OFF);
    IUFillSwitch(&PowerSwitchesS[3], "4", "Switch 4", ISS_ON);
    IUFillSwitchVector(&PowerSwitchesSP, PowerSwitchesS, 4, getDeviceName(), "SWITCHES", "Power", SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    /* Robofocus should stay within these limits */
    IUFillNumber(&MinMaxPositionN[0], "MINPOS", "Minimum Tick", "%6.0f", 1., 65000., 0., 100.);
    IUFillNumber(&MinMaxPositionN[1], "MAXPOS", "Maximum Tick", "%6.0f", 1., 65000., 0., 55000.);
    IUFillNumberVector(&MinMaxPositionNP, MinMaxPositionN, 2, getDeviceName(), "FOCUS_MINMAXPOSITION", "Extrema",
                       SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    IUFillNumber(&MaxTravelN[0], "MAXTRAVEL", "Maximum travel", "%6.0f", 1., 64000., 0., 10000.);
    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "FOCUS_MAXTRAVEL", "Max. travel", SETTINGS_TAB,
                       IP_RW, 0, IPS_IDLE);

    // Cannot change maximum position
    FocusMaxPosNP.setPermission(IP_RO);
    FocusMaxPosNP[0].setValue(64000);

    FocusBacklashNP[0].setMin(-255);
    FocusBacklashNP[0].setMax(255);
    FocusBacklashNP[0].setStep(10);
    FocusBacklashNP[0].setValue(0);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(5000.);
    FocusRelPosNP[0].setValue(100);
    FocusRelPosNP[0].setStep(100);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(64000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    simulatedTemperature = 600.0;
    simulatedPosition    = 20000;

    addDebugControl();
    addSimulationControl();

    return true;
}

bool RoboFocus::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&TemperatureNP);
        defineProperty(&PowerSwitchesSP);
        defineProperty(&SettingsNP);
        defineProperty(&MinMaxPositionNP);
        defineProperty(&MaxTravelNP);
        GetFocusParams();

        LOG_DEBUG("RoboFocus parameters readout complete, focuser ready for use.");
    }
    else
    {
        deleteProperty(TemperatureNP.name);
        deleteProperty(SettingsNP.name);
        deleteProperty(PowerSwitchesSP.name);
        deleteProperty(MinMaxPositionNP.name);
        deleteProperty(MaxTravelNP.name);
    }

    return true;
}

bool RoboFocus::Handshake()
{
    char firmeware[] = "FV0000000";

    if (isSimulation())
    {
        timerID = SetTimer(getCurrentPollingPeriod());
        LOG_INFO("Simulated Robofocus is online. Getting focus parameters...");
        FocusAbsPosNP[0].setValue(simulatedPosition);
        updateRFFirmware(firmeware);
        return true;
    }

    if ((updateRFFirmware(firmeware)) < 0)
    {
        /* This would be the end*/
        LOG_ERROR("Unknown error while reading Robofocus firmware.");
        return false;
    }

    return true;
}

const char *RoboFocus::getDefaultName()
{
    return "RoboFocus";
}

unsigned char RoboFocus::CheckSum(char *rf_cmd)
{
    char substr[255];
    unsigned char val = 0;

    for (int i = 0; i < 8; i++)
        substr[i] = rf_cmd[i];

    val = CalculateSum(substr);

    if (val != (unsigned char)rf_cmd[8])
        LOGF_WARN("Checksum: Wrong (%s,%ld), %x != %x", rf_cmd, strlen(rf_cmd), val,
                  (unsigned char)rf_cmd[8]);

    return val;
}

unsigned char RoboFocus::CalculateSum(const char *rf_cmd)
{
    unsigned char val = 0;

    for (int i = 0; i < 8; i++)
        val = val + (unsigned char)rf_cmd[i];

    return val % 256;
}

int RoboFocus::SendCommand(char *rf_cmd)
{
    int nbytes_written = 0, err_code = 0;
    char rf_cmd_cks[32], robofocus_error[MAXRBUF];

    unsigned char val = 0;

    val = CalculateSum(rf_cmd);

    for (int i = 0; i < 8; i++)
        rf_cmd_cks[i] = rf_cmd[i];

    rf_cmd_cks[8] = (unsigned char)val;
    rf_cmd_cks[9] = 0;

    if (isSimulation())
        return 0;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD (%#02X %#02X %#02X %#02X %#02X %#02X %#02X %#02X %#02X)", rf_cmd_cks[0],
               rf_cmd_cks[1], rf_cmd_cks[2], rf_cmd_cks[3], rf_cmd_cks[4], rf_cmd_cks[5], rf_cmd_cks[6], rf_cmd_cks[7],
               rf_cmd_cks[8]);

    if ((err_code = tty_write(PortFD, rf_cmd_cks, RF_MAX_CMD, &nbytes_written) != TTY_OK))
    {
        tty_error_msg(err_code, robofocus_error, MAXRBUF);
        LOGF_ERROR("TTY error detected: %s", robofocus_error);
        return -1;
    }

    return nbytes_written;
}

int RoboFocus::ReadResponse(char *buf)
{
    char robofocus_error[MAXRBUF];
    char robofocus_char[1];
    int bytesRead = 0;
    int err_code;

    char motion         = 0;
    bool externalMotion = false;

    if (isSimulation())
        return RF_MAX_CMD;

    while (true)
    {
        if ((err_code = tty_read(PortFD, robofocus_char, 1, RF_TIMEOUT, &bytesRead)) != TTY_OK)
        {
            tty_error_msg(err_code, robofocus_error, MAXRBUF);
            LOGF_ERROR("TTY error detected: %s", robofocus_error);
            return -1;
        }

        switch (robofocus_char[0])
        {
            // Catch 'I'
            case 0x49:
                if (motion != 0x49)
                {
                    motion = 0x49;
                    LOG_INFO("Moving inward...");

                    if (FocusAbsPosNP.getState() != IPS_BUSY)
                    {
                        externalMotion  = true;
                        FocusAbsPosNP.setState(IPS_BUSY);
                        FocusAbsPosNP.apply();
                    }
                }
                //usleep(100000);
                break;

            // catch 'O'
            case 0x4F:
                if (motion != 0x4F)
                {
                    motion = 0x4F;
                    LOG_INFO("Moving outward...");

                    if (FocusAbsPosNP.getState() != IPS_BUSY)
                    {
                        externalMotion  = true;
                        FocusAbsPosNP.setState(IPS_BUSY);
                        FocusAbsPosNP.apply();
                    }
                }
                //usleep(100000);
                break;

            // Start of frame
            case 0x46:
                buf[0] = 0x46;
                // Read rest of frame
                if ((err_code = tty_read(PortFD, buf + 1, RF_MAX_CMD - 1, RF_TIMEOUT, &bytesRead)) != TTY_OK)
                {
                    tty_error_msg(err_code, robofocus_error, MAXRBUF);
                    LOGF_ERROR("TTY error detected: %s", robofocus_error);
                    return -1;
                }

                if (motion != 0)
                {
                    LOG_INFO("Stopped.");

                    // If we set it busy due to external motion, let's set it to OK
                    if (externalMotion)
                    {
                        FocusAbsPosNP.setState(IPS_OK);
                        FocusAbsPosNP.apply();
                    }
                }

                tcflush(PortFD, TCIOFLUSH);
                return (bytesRead + 1);
                break;

            default:
                break;
        }
    }

    return -1;
}

int RoboFocus::updateRFPosition(double *value)
{
    float temp;
    char rf_cmd[RF_MAX_CMD];
    int robofocus_rc;

    LOG_DEBUG("Querying Position...");

    if (isSimulation())
    {
        *value = simulatedPosition;
        return 0;
    }

    strncpy(rf_cmd, "FG000000", 9);

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    if ((robofocus_rc = ReadResponse(rf_cmd)) < 0)
        return robofocus_rc;

    if (sscanf(rf_cmd, "FD%6f", &temp) < 1)
        return -1;

    *value = (double)temp;

    LOGF_DEBUG("Position: %g", *value);

    return 0;
}

int RoboFocus::updateRFTemperature(double *value)
{
    LOGF_DEBUG("Update Temperature: %g", value);

    float temp;
    char rf_cmd[32];
    int robofocus_rc;

    strncpy(rf_cmd, "FT000000", 9);

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    if (isSimulation())
        snprintf(rf_cmd, 32, "FT%6g", simulatedTemperature);
    else if ((robofocus_rc = ReadResponse(rf_cmd)) < 0)
        return robofocus_rc;

    if (sscanf(rf_cmd, "FT%6f", &temp) < 1)
        return -1;

    *value = (double)temp / 2. - 273.15;

    return 0;
}

int RoboFocus::updateRFBacklash(double *value)
{
    LOGF_DEBUG("Update Backlash: %g", value);

    float temp;
    char rf_cmd[32];
    char vl_tmp[4];
    int robofocus_rc;
    int sign = 0;

    if (isSimulation())
    {
        *value = FocusBacklashNP[0].getValue();
        return 0;
    }

    if (*value == BACKLASH_READOUT)
    {
        strncpy(rf_cmd, "FB000000", 9);
    }
    else
    {
        rf_cmd[0] = 'F';
        rf_cmd[1] = 'B';

        if (*value > 0)
        {
            rf_cmd[2] = '3';
        }
        else
        {
            *value    = -*value;
            rf_cmd[2] = '2';
        }
        rf_cmd[3] = '0';
        rf_cmd[4] = '0';

        if (*value > 99)
        {
            sprintf(vl_tmp, "%3d", (int)*value);
        }
        else if (*value > 9)
        {
            sprintf(vl_tmp, "0%2d", (int)*value);
        }
        else
        {
            sprintf(vl_tmp, "00%1d", (int)*value);
        }
        rf_cmd[5] = vl_tmp[0];
        rf_cmd[6] = vl_tmp[1];
        rf_cmd[7] = vl_tmp[2];
    }

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    if ((robofocus_rc = ReadResponse(rf_cmd)) < 0)
        return robofocus_rc;

    if (sscanf(rf_cmd, "FB%1d%5f", &sign, &temp) < 1)
        return -1;

    *value = (double)temp;

    if ((sign == 2) && (*value > 0))
    {
        *value = -(*value);
    }

    return 0;
}

int RoboFocus::updateRFFirmware(char *rf_cmd)
{
    int robofocus_rc;

    LOG_DEBUG("Querying RoboFocus Firmware...");

    strncpy(rf_cmd, "FV000000", 9);

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    if (isSimulation())
        strncpy(rf_cmd, "SIM", 4);
    else if ((robofocus_rc = ReadResponse(rf_cmd)) < 0)
        return robofocus_rc;

    return 0;
}

int RoboFocus::updateRFMotorSettings(double *duty, double *delay, double *ticks)
{
    LOGF_DEBUG("Update Motor Settings: Duty (%g), Delay (%g), Ticks(%g)", *duty, *delay, *ticks);

    char rf_cmd[32];
    int robofocus_rc;

    if (isSimulation())
    {
        *duty  = 100;
        *delay = 0;
        *ticks = 0;
        return 0;
    }

    if ((*duty == 0) && (*delay == 0) && (*ticks == 0))
    {
        strncpy(rf_cmd, "FC000000", 9);
    }
    else
    {
        rf_cmd[0] = 'F';
        rf_cmd[1] = 'C';
        rf_cmd[2] = (char) * duty;
        rf_cmd[3] = (char) * delay;
        rf_cmd[4] = (char) * ticks;
        rf_cmd[5] = '0';
        rf_cmd[6] = '0';
        rf_cmd[7] = '0';
        rf_cmd[8] = 0;
    }

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    if ((robofocus_rc = ReadResponse(rf_cmd)) < 0)
        return robofocus_rc;

    *duty  = (float)rf_cmd[2];
    *delay = (float)rf_cmd[3];
    *ticks = (float)rf_cmd[4];

    return 0;
}

int RoboFocus::updateRFPositionRelativeInward(double value)
{
    char rf_cmd[32];
    int robofocus_rc;
    //float temp ;
    rf_cmd[0] = 0;

    LOGF_DEBUG("Update Relative Position Inward: %g", value);

    if (isSimulation())
    {
        simulatedPosition += value;
        //value = simulatedPosition;
        return 0;
    }

    if (value > 9999)
    {
        sprintf(rf_cmd, "FI0%5d", (int)value);
    }
    else if (value > 999)
    {
        sprintf(rf_cmd, "FI00%4d", (int)value);
    }
    else if (value > 99)
    {
        sprintf(rf_cmd, "FI000%3d", (int)value);
    }
    else if (value > 9)
    {
        sprintf(rf_cmd, "FI0000%2d", (int)value);
    }
    else
    {
        sprintf(rf_cmd, "FI00000%1d", (int)value);
    }

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    return 0;
}

int RoboFocus::updateRFPositionRelativeOutward(double value)
{
    char rf_cmd[32];
    int robofocus_rc;
    //float temp ;

    LOGF_DEBUG("Update Relative Position Outward: %g", value);

    if (isSimulation())
    {
        simulatedPosition -= value;
        //value = simulatedPosition;
        return 0;
    }

    rf_cmd[0] = 0;

    if (value > 9999)
    {
        sprintf(rf_cmd, "FO0%5d", (int)value);
    }
    else if (value > 999)
    {
        sprintf(rf_cmd, "FO00%4d", (int)value);
    }
    else if (value > 99)
    {
        sprintf(rf_cmd, "FO000%3d", (int)value);
    }
    else if (value > 9)
    {
        sprintf(rf_cmd, "FO0000%2d", (int)value);
    }
    else
    {
        sprintf(rf_cmd, "FO00000%1d", (int)value);
    }

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    return 0;
}

int RoboFocus::updateRFPositionAbsolute(double value)
{
    char rf_cmd[32];
    int robofocus_rc;

    LOGF_DEBUG("Moving Absolute Position: %g", value);

    if (isSimulation())
    {
        simulatedPosition = value;
        return 0;
    }

    rf_cmd[0] = 0;

    if (value > 9999)
    {
        sprintf(rf_cmd, "FG0%5d", (int)value);
    }
    else if (value > 999)
    {
        sprintf(rf_cmd, "FG00%4d", (int)value);
    }
    else if (value > 99)
    {
        sprintf(rf_cmd, "FG000%3d", (int)value);
    }
    else if (value > 9)
    {
        sprintf(rf_cmd, "FG0000%2d", (int)value);
    }
    else
    {
        sprintf(rf_cmd, "FG00000%1d", (int)value);
    }

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    return 0;
}

int RoboFocus::updateRFPowerSwitches(int s, int new_sn, int *cur_s1LL, int *cur_s2LR, int *cur_s3RL, int *cur_s4RR)
{
    INDI_UNUSED(s);
    char rf_cmd[32];
    char rf_cmd_tmp[32];
    int robofocus_rc;
    int i = 0;

    if (isSimulation())
    {
        return 0;
    }

    LOG_DEBUG("Get switch status...");

    /* Get first the status */
    strncpy(rf_cmd_tmp, "FP000000", 9);

    if ((robofocus_rc = SendCommand(rf_cmd_tmp)) < 0)
        return robofocus_rc;

    if ((robofocus_rc = ReadResponse(rf_cmd_tmp)) < 0)
        return robofocus_rc;

    for (i = 0; i < 9; i++)
    {
        rf_cmd[i] = rf_cmd_tmp[i];
    }

    if (rf_cmd[new_sn + 4] == '2')
    {
        rf_cmd[new_sn + 4] = '1';
    }
    else
    {
        rf_cmd[new_sn + 4] = '2';
    }

    rf_cmd[8] = 0;

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    if ((robofocus_rc = ReadResponse(rf_cmd)) < 0)
        return robofocus_rc;

    *cur_s1LL = *cur_s2LR = *cur_s3RL = *cur_s4RR = ISS_OFF;

    if (rf_cmd[4] == '2')
    {
        *cur_s1LL = ISS_ON;
    }

    if (rf_cmd[5] == '2')
    {
        *cur_s2LR = ISS_ON;
    }

    if (rf_cmd[6] == '2')
    {
        *cur_s3RL = ISS_ON;
    }

    if (rf_cmd[7] == '2')
    {
        *cur_s4RR = ISS_ON;
    }
    return 0;
}

int RoboFocus::updateRFMaxPosition(double *value)
{
    LOG_DEBUG("Query max position...");

    float temp;
    char rf_cmd[32];
    char vl_tmp[6];
    int robofocus_rc;
    char waste[1];

    if (isSimulation())
    {
        return 0;
    }

    if (*value == MAXTRAVEL_READOUT)
    {
        strncpy(rf_cmd, "FL000000", 9);
    }
    else
    {
        rf_cmd[0] = 'F';
        rf_cmd[1] = 'L';
        rf_cmd[2] = '0';

        if (*value > 9999)
        {
            sprintf(vl_tmp, "%5d", (int)*value);
        }
        else if (*value > 999)
        {
            sprintf(vl_tmp, "0%4d", (int)*value);
        }
        else if (*value > 99)
        {
            sprintf(vl_tmp, "00%3d", (int)*value);
        }
        else if (*value > 9)
        {
            sprintf(vl_tmp, "000%2d", (int)*value);
        }
        else
        {
            sprintf(vl_tmp, "0000%1d", (int)*value);
        }
        rf_cmd[3] = vl_tmp[0];
        rf_cmd[4] = vl_tmp[1];
        rf_cmd[5] = vl_tmp[2];
        rf_cmd[6] = vl_tmp[3];
        rf_cmd[7] = vl_tmp[4];
        rf_cmd[8] = 0;
    }

    if ((robofocus_rc = SendCommand(rf_cmd)) < 0)
        return robofocus_rc;

    if ((robofocus_rc = ReadResponse(rf_cmd)) < 0)
        return robofocus_rc;

    if (sscanf(rf_cmd, "FL%1c%5f", waste, &temp) < 1)
        return -1;

    *value = (double)temp;

    LOGF_DEBUG("Max position: %g", *value);

    return 0;
}

bool RoboFocus::SyncFocuser(uint32_t ticks)
{
    char rf_cmd[32];
    char vl_tmp[6];
    int ret_read_tmp;

    if (isSimulation())
    {
        FocusAbsPosNP[0].setValue(ticks);
        return true;
    }

    rf_cmd[0] = 'F';
    rf_cmd[1] = 'S';
    rf_cmd[2] = '0';

    if (ticks > 9999)
    {
        snprintf(vl_tmp, 6, "%5d", ticks);
    }
    else if (ticks > 999)
    {
        snprintf(vl_tmp, 6, "0%4d", ticks);
    }
    else if (ticks > 99)
    {
        snprintf(vl_tmp, 6, "00%3d", ticks);
    }
    else if (ticks > 9)
    {
        snprintf(vl_tmp, 6, "000%2d", ticks);
    }
    else
    {
        snprintf(vl_tmp, 6, "0000%1d", ticks);
    }
    rf_cmd[3] = vl_tmp[0];
    rf_cmd[4] = vl_tmp[1];
    rf_cmd[5] = vl_tmp[2];
    rf_cmd[6] = vl_tmp[3];
    rf_cmd[7] = vl_tmp[4];
    rf_cmd[8] = 0;

    if ((ret_read_tmp = SendCommand(rf_cmd)) < 0)
        return false;

    return true;
}

bool RoboFocus::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, PowerSwitchesSP.name) == 0)
        {
            int ret      = -1;
            int nset     = 0;
            int i        = 0;
            int new_s    = -1;
            int new_sn   = -1;
            int cur_s1LL = 0;
            int cur_s2LR = 0;
            int cur_s3RL = 0;
            int cur_s4RR = 0;

            ISwitch *sp;

            PowerSwitchesSP.s = IPS_BUSY;
            IDSetSwitch(&PowerSwitchesSP, nullptr);

            for (nset = i = 0; i < n; i++)
            {
                /* Find numbers with the passed names in the SettingsNP property */
                sp = IUFindSwitch(&PowerSwitchesSP, names[i]);

                /* If the state found is  (PowerSwitchesS[0]) then process it */

                if (sp == &PowerSwitchesS[0])
                {
                    new_s  = (states[i]);
                    new_sn = 0;
                    nset++;
                }
                else if (sp == &PowerSwitchesS[1])
                {
                    new_s  = (states[i]);
                    new_sn = 1;
                    nset++;
                }
                else if (sp == &PowerSwitchesS[2])
                {
                    new_s  = (states[i]);
                    new_sn = 2;
                    nset++;
                }
                else if (sp == &PowerSwitchesS[3])
                {
                    new_s  = (states[i]);
                    new_sn = 3;
                    nset++;
                }
            }
            if (nset == 1)
            {
                cur_s1LL = cur_s2LR = cur_s3RL = cur_s4RR = 0;

                if ((ret = updateRFPowerSwitches(new_s, new_sn, &cur_s1LL, &cur_s2LR, &cur_s3RL, &cur_s4RR)) < 0)
                {
                    PowerSwitchesSP.s = IPS_ALERT;
                    IDSetSwitch(&PowerSwitchesSP, "Unknown error while reading  Robofocus power switch settings");
                    return true;
                }
            }
            else
            {
                /* Set property state to idle */
                PowerSwitchesSP.s = IPS_IDLE;

                IDSetNumber(&SettingsNP, "Power switch settings absent or bogus.");
                return true;
            }
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool RoboFocus::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    int nset = 0, i = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, SettingsNP.name) == 0)
        {
            /* new speed */
            double new_duty  = 0;
            double new_delay = 0;
            double new_ticks = 0;
            int ret          = -1;

            for (nset = i = 0; i < n; i++)
            {
                /* Find numbers with the passed names in the SettingsNP property */
                INumber *eqp = IUFindNumber(&SettingsNP, names[i]);

                /* If the number found is  (SettingsN[0]) then process it */
                if (eqp == &SettingsN[0])
                {
                    new_duty = (values[i]);
                    nset += static_cast<int>(new_duty >= 0 && new_duty <= 255);
                }
                else if (eqp == &SettingsN[1])
                {
                    new_delay = (values[i]);
                    nset += static_cast<int>(new_delay >= 0 && new_delay <= 255);
                }
                else if (eqp == &SettingsN[2])
                {
                    new_ticks = (values[i]);
                    nset += static_cast<int>(new_ticks >= 0 && new_ticks <= 255);
                }
            }

            /* Did we process the three numbers? */
            if (nset == 3)
            {
                /* Set the robofocus state to BUSY */
                SettingsNP.s = IPS_BUSY;

                IDSetNumber(&SettingsNP, nullptr);

                if ((ret = updateRFMotorSettings(&new_duty, &new_delay, &new_ticks)) < 0)
                {
                    IDSetNumber(&SettingsNP, "Changing to new settings failed");
                    return false;
                }

                currentDuty  = new_duty;
                currentDelay = new_delay;
                currentTicks = new_ticks;

                SettingsNP.s = IPS_OK;
                IDSetNumber(&SettingsNP, "Motor settings are now  %3.0f %3.0f %3.0f", currentDuty, currentDelay,
                            currentTicks);
                return true;
            }
            else
            {
                /* Set property state to idle */
                SettingsNP.s = IPS_IDLE;

                IDSetNumber(&SettingsNP, "Settings absent or bogus.");
                return false;
            }
        }

        if (strcmp(name, MinMaxPositionNP.name) == 0)
        {
            /* new positions */
            double new_min = 0;
            double new_max = 0;

            for (nset = i = 0; i < n; i++)
            {
                /* Find numbers with the passed names in the MinMaxPositionNP property */
                INumber *mmpp = IUFindNumber(&MinMaxPositionNP, names[i]);

                /* If the number found is  (MinMaxPositionN[0]) then process it */
                if (mmpp == &MinMaxPositionN[0])
                {
                    new_min = (values[i]);
                    nset += static_cast<int>(new_min >= 1 && new_min <= 65000);
                }
                else if (mmpp == &MinMaxPositionN[1])
                {
                    new_max = (values[i]);
                    nset += static_cast<int>(new_max >= 1 && new_max <= 65000);
                }
            }

            /* Did we process the two numbers? */
            if (nset == 2)
            {
                /* Set the robofocus state to BUSY */
                MinMaxPositionNP.s = IPS_BUSY;

                currentMinPosition = new_min;
                currentMaxPosition = new_max;

                MinMaxPositionNP.s = IPS_OK;
                IDSetNumber(&MinMaxPositionNP, "Minimum and Maximum settings are now  %3.0f %3.0f", currentMinPosition,
                            currentMaxPosition);
                return true;
            }
            else
            {
                /* Set property state to idle */
                MinMaxPositionNP.s = IPS_IDLE;

                IDSetNumber(&MinMaxPositionNP, "Minimum and maximum limits absent or bogus.");

                return false;
            }
        }

        if (strcmp(name, MaxTravelNP.name) == 0)
        {
            double new_maxt = 0;
            int ret         = -1;

            for (nset = i = 0; i < n; i++)
            {
                /* Find numbers with the passed names in the MinMaxPositionNP property */
                INumber *mmpp = IUFindNumber(&MaxTravelNP, names[i]);

                /* If the number found is  (MaxTravelN[0]) then process it */
                if (mmpp == &MaxTravelN[0])
                {
                    new_maxt = (values[i]);
                    nset += static_cast<int>(new_maxt >= 1 && new_maxt <= 64000);
                }
            }
            /* Did we process the one number? */
            if (nset == 1)
            {
                IDSetNumber(&MinMaxPositionNP, nullptr);

                if ((ret = updateRFMaxPosition(&new_maxt)) < 0)
                {
                    MaxTravelNP.s = IPS_IDLE;
                    IDSetNumber(&MaxTravelNP, "Changing to new maximum travel failed");
                    return false;
                }

                currentMaxTravel = new_maxt;
                MaxTravelNP.s    = IPS_OK;
                IDSetNumber(&MaxTravelNP, "Maximum travel is now  %3.0f", currentMaxTravel);
                return true;
            }
            else
            {
                /* Set property state to idle */

                MaxTravelNP.s = IPS_IDLE;
                IDSetNumber(&MaxTravelNP, "Maximum travel absent or bogus.");

                return false;
            }
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

void RoboFocus::GetFocusParams()
{
    int ret      = -1;
    int cur_s1LL = 0;
    int cur_s2LR = 0;
    int cur_s3RL = 0;
    int cur_s4RR = 0;

    double currentPosition = 0;
    if ((ret = updateRFPosition(&currentPosition)) < 0)
    {
        FocusAbsPosNP.setState(IPS_ALERT);
        LOGF_ERROR("Unknown error while reading  Robofocus position: %d", ret);
        FocusAbsPosNP.apply();
        return;
    }

    FocusAbsPosNP[0].setValue(currentPosition);
    FocusAbsPosNP.setState(IPS_OK);
    FocusAbsPosNP.apply();

    double currentTemperature = 0;


    if ((ret = updateRFTemperature(&currentTemperature)) < 0)
    {
        TemperatureNP.s = IPS_ALERT;
        LOG_ERROR("Unknown error while reading Robofocus temperature.");
        IDSetNumber(&TemperatureNP, nullptr);
        return;
    }

    TemperatureN[0].value = currentTemperature;
    TemperatureNP.s = IPS_OK;
    IDSetNumber(&TemperatureNP, nullptr);

    double currentBacklash = 0;

    FocusBacklashNP[0].setValue(BACKLASH_READOUT);
    if ((ret = updateRFBacklash(&currentBacklash)) < 0)
    {
        FocusBacklashNP.setState(IPS_ALERT);
        LOG_ERROR("Unknown error while reading Robofocus backlash.");
        FocusBacklashNP.apply();
        return;
    }
    FocusBacklashNP[0].setValue(currentBacklash);
    FocusBacklashNP.setState(IPS_OK);
    FocusBacklashNP.apply();

    currentDuty = currentDelay = currentTicks = 0;

    if ((ret = updateRFMotorSettings(&currentDuty, &currentDelay, &currentTicks)) < 0)
    {
        SettingsNP.s = IPS_ALERT;
        LOG_ERROR("Unknown error while reading Robofocus motor settings.");
        IDSetNumber(&SettingsNP, nullptr);
        return;
    }

    SettingsNP.s = IPS_OK;
    IDSetNumber(&SettingsNP, nullptr);

    if ((ret = updateRFPowerSwitches(-1, -1, &cur_s1LL, &cur_s2LR, &cur_s3RL, &cur_s4RR)) < 0)
    {
        PowerSwitchesSP.s = IPS_ALERT;
        LOG_ERROR("Unknown error while reading Robofocus power switch settings.");
        IDSetSwitch(&PowerSwitchesSP, nullptr);
        return;
    }

    PowerSwitchesS[0].s = PowerSwitchesS[1].s = PowerSwitchesS[2].s = PowerSwitchesS[3].s = ISS_OFF;

    if (cur_s1LL == ISS_ON)
    {
        PowerSwitchesS[0].s = ISS_ON;
    }
    if (cur_s2LR == ISS_ON)
    {
        PowerSwitchesS[1].s = ISS_ON;
    }
    if (cur_s3RL == ISS_ON)
    {
        PowerSwitchesS[2].s = ISS_ON;
    }
    if (cur_s4RR == ISS_ON)
    {
        PowerSwitchesS[3].s = ISS_ON;
    }
    PowerSwitchesSP.s = IPS_OK;
    IDSetSwitch(&PowerSwitchesSP, nullptr);
}

IPState RoboFocus::MoveAbsFocuser(uint32_t targetTicks)
{
    int ret   = -1;
    targetPos = targetTicks;

    if (targetTicks < FocusAbsPosNP[0].getMin() || targetTicks > FocusAbsPosNP[0].getMax())
    {
        LOG_DEBUG("Error, requested position is out of range.");
        return IPS_ALERT;
    }

    if ((ret = updateRFPositionAbsolute(targetPos)) < 0)
    {
        LOGF_DEBUG("Read out of the absolute movement failed %3d", ret);
        return IPS_ALERT;
    }

    RemoveTimer(timerID);
    timerID = SetTimer(250);
    return IPS_BUSY;
}

IPState RoboFocus::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(FocusAbsPosNP[0].getValue() + (ticks * (dir == FOCUS_INWARD ? -1 : 1)));
}

bool RoboFocus::saveConfigItems(FILE *fp)
{
    IUSaveConfigNumber(fp, &SettingsNP);
    FocusBacklashNP.save(fp);

    return INDI::Focuser::saveConfigItems(fp);
}

void RoboFocus::TimerHit()
{
    if (!isConnected())
        return;

    double prevPos = FocusAbsPosNP[0].getValue();
    double newPos  = 0;

    if (FocusAbsPosNP.getState() == IPS_OK || FocusAbsPosNP.getState() == IPS_IDLE)
    {
        int rc = updateRFPosition(&newPos);
        if (rc >= 0)
        {
            FocusAbsPosNP[0].setValue(newPos);
            if (prevPos != FocusAbsPosNP[0].getValue())
                FocusAbsPosNP.apply();
        }
    }
    else if (FocusAbsPosNP.getState() == IPS_BUSY)
    {
        float newPos            = 0;
        int nbytes_read         = 0;
        char rf_cmd[RF_MAX_CMD] = { 0 };

        //nbytes_read= ReadUntilComplete(rf_cmd, RF_TIMEOUT) ;

        nbytes_read = ReadResponse(rf_cmd);

        rf_cmd[nbytes_read - 1] = 0;

        if (nbytes_read != 9 || (sscanf(rf_cmd, "FD0%5f", &newPos) <= 0))
        {
            DEBUGF(INDI::Logger::DBG_WARNING,
                   "Bogus position: (%#02X %#02X %#02X %#02X %#02X %#02X %#02X %#02X %#02X) - Bytes read: %d",
                   rf_cmd[0], rf_cmd[1], rf_cmd[2], rf_cmd[3], rf_cmd[4], rf_cmd[5], rf_cmd[6], rf_cmd[7], rf_cmd[8],
                   nbytes_read);
            timerID = SetTimer(getCurrentPollingPeriod());
            return;
        }
        else if (nbytes_read < 0)
        {
            FocusAbsPosNP.setState(IPS_ALERT);
            LOG_ERROR("Read error! Reconnect and try again.");
            FocusAbsPosNP.apply();
            return;
        }

        FocusAbsPosNP[0].setValue(newPos);

        if (FocusAbsPosNP[0].getValue() == targetPos)
        {
            FocusAbsPosNP.setState(IPS_OK);

            if (FocusRelPosNP.getState() == IPS_BUSY)
            {
                FocusRelPosNP.setState(IPS_OK);
                FocusRelPosNP.apply();
            }
        }

        FocusAbsPosNP.apply();
        if (FocusAbsPosNP.getState() == IPS_BUSY)
        {
            timerID = SetTimer(250);
            return;
        }
    }

    timerID = SetTimer(getCurrentPollingPeriod());
}

bool RoboFocus::AbortFocuser()
{
    LOG_DEBUG("Aborting focuser...");

    int nbytes_written;
    const char *buf = "\r";

    return tty_write(PortFD, buf, strlen(buf), &nbytes_written) == TTY_OK;
}

bool RoboFocus::SetFocuserBacklash(int32_t steps)
{
    double value = steps;
    return (updateRFBacklash(&value) == 0);
}
