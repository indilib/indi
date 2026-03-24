/*******************************************************************************
  MLAstro Robotic Polar Alignment (RPA)

  Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

  ESP32-based serial protocol (MLAstroRPA custom protocol).

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
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*******************************************************************************/

#include "mlastro_rpa.h"

#include "indicom.h"
#include "indilogger.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>

static std::unique_ptr<MLAstroRPA> rpa(new MLAstroRPA());

/////////////////////////////////////////////////////////////////////////////
/// Constructor
/////////////////////////////////////////////////////////////////////////////
MLAstroRPA::MLAstroRPA()
    : PACInterface(this)
{
    setVersion(1, 0);
    SetCapability(PAC_HAS_SPEED    |
                  PAC_CAN_REVERSE  |
                  PAC_HAS_POSITION |
                  PAC_CAN_HOME     |
                  PAC_HAS_BACKLASH |
                  PAC_CAN_SYNC);
}

/////////////////////////////////////////////////////////////////////////////
/// Basic device info
/////////////////////////////////////////////////////////////////////////////
const char *MLAstroRPA::getDefaultName()
{
    return "MLAstro RPA";
}

/////////////////////////////////////////////////////////////////////////////
/// initProperties
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Initialise the PAC interface properties (main control tab)
    PACI::initProperties(MAIN_CONTROL_TAB);

    // Speed: 1–5 levels as defined by the MLAstro protocol
    SpeedNP[0].setMin(1);
    SpeedNP[0].setMax(5);
    SpeedNP[0].setStep(1);
    SpeedNP[0].setValue(3);

    // ── Main Control tab ──────────────────────────────────────────────────

    // Current machine state (read-only text)
    StatusTP[0].fill("STATUS", "State", "Unknown");
    StatusTP.fill(getDeviceName(), "RPA_STATUS", "Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    // WiFi connection light
    WiFiLP[0].fill("WIFI", "WiFi", IPS_IDLE);
    WiFiLP.fill(getDeviceName(), "RPA_WIFI", "WiFi", MAIN_CONTROL_TAB, IPS_IDLE);

    // Homed status light
    HomedLP[0].fill("HOMED", "Homed", IPS_IDLE);
    HomedLP.fill(getDeviceName(), "RPA_HOMED", "Home", MAIN_CONTROL_TAB, IPS_IDLE);

    // ── Options tab ───────────────────────────────────────────────────────

    // Azimuth soft limits
    AzSoftLimitsNP[AZ_LIMIT_MIN].fill("AZ_LIMIT_MIN", "Min (deg)", "%.2f", -90, 0, 0.5, -9.0);
    AzSoftLimitsNP[AZ_LIMIT_MAX].fill("AZ_LIMIT_MAX", "Max (deg)", "%.2f", 0, 90, 0.5, 9.0);
    AzSoftLimitsNP.fill(getDeviceName(), "AZ_SOFT_LIMITS", "AZ Limits",
                        OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // Altitude soft limits
    AltSoftLimitsNP[ALT_LIMIT_MIN].fill("ALT_LIMIT_MIN", "Min (deg)", "%.2f", -90, 0, 0.5, -9.0);
    AltSoftLimitsNP[ALT_LIMIT_MAX].fill("ALT_LIMIT_MAX", "Max (deg)", "%.2f", 0, 90, 0.5, 9.0);
    AltSoftLimitsNP.fill(getDeviceName(), "ALT_SOFT_LIMITS", "ALT Limits",
                         OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    // ── Motor Config tab ──────────────────────────────────────────────────

    // Azimuth motor settings
    AzMotorNP[AZ_RUN_CURRENT].fill("AZ_RUN_CURRENT",   "Run Current (mA)",   "%.0f", 100, 3000, 50, 800);
    AzMotorNP[AZ_HOLD_CURRENT].fill("AZ_HOLD_CURRENT", "Hold Current (mA)",  "%.0f", 0,   2000, 50, 200);
    AzMotorNP[AZ_MICROSTEPS].fill("AZ_MICROSTEPS",     "Microsteps",         "%.0f", 1,   256,  1,  64);
    AzMotorNP[AZ_ACCEL].fill("AZ_ACCEL",               "Acceleration",       "%.0f", 100, 50000, 100, 5000);
    AzMotorNP[AZ_DECEL].fill("AZ_DECEL",               "Deceleration",       "%.0f", 100, 50000, 100, 5000);
    AzMotorNP[AZ_STEPS_PER_DEG].fill("AZ_STEPS_PER_DEG", "Steps/Degree",     "%.0f", 1, 100000, 1, 3200);
    AzMotorNP[AZ_START_BOOST].fill("AZ_START_BOOST",   "Start Boost (%)",    "%.0f", 0, 100, 1, 10);
    AzMotorNP[AZ_COOL_STEP].fill("AZ_COOL_STEP",       "CoolStep (%)",       "%.0f", 0, 100, 1, 0);
    AzMotorNP[AZ_RUN_MODE].fill("AZ_RUN_MODE",         "Mode (0=SC 1=SpC)",  "%.0f", 0, 1,   1, 0);
    AzMotorNP.fill(getDeviceName(), "AZ_MOTOR_CONFIG", "Azimuth Motor",
                   MOTOR_CONFIG_TAB, IP_RW, 60, IPS_IDLE);

    // Altitude motor settings
    AltMotorNP[ALT_RUN_CURRENT].fill("ALT_RUN_CURRENT",   "Run Current (mA)",  "%.0f", 100, 3000, 50, 800);
    AltMotorNP[ALT_HOLD_CURRENT].fill("ALT_HOLD_CURRENT", "Hold Current (mA)", "%.0f", 0,   2000, 50, 200);
    AltMotorNP[ALT_MICROSTEPS].fill("ALT_MICROSTEPS",     "Microsteps",        "%.0f", 1,   256,  1,  64);
    AltMotorNP[ALT_ACCEL].fill("ALT_ACCEL",               "Acceleration",      "%.0f", 100, 50000, 100, 5000);
    AltMotorNP[ALT_DECEL].fill("ALT_DECEL",               "Deceleration",      "%.0f", 100, 50000, 100, 5000);
    AltMotorNP[ALT_STEPS_PER_DEG].fill("ALT_STEPS_PER_DEG", "Steps/Degree",   "%.0f", 1, 100000, 1, 3200);
    AltMotorNP[ALT_START_BOOST].fill("ALT_START_BOOST",   "Start Boost (%)",   "%.0f", 0, 100, 1, 10);
    AltMotorNP[ALT_COOL_STEP].fill("ALT_COOL_STEP",       "CoolStep (%)",      "%.0f", 0, 100, 1, 0);
    AltMotorNP[ALT_RUN_MODE].fill("ALT_RUN_MODE",         "Mode (0=SC 1=SpC)", "%.0f", 0, 1,   1, 0);
    AltMotorNP.fill(getDeviceName(), "ALT_MOTOR_CONFIG", "Altitude Motor",
                    MOTOR_CONFIG_TAB, IP_RW, 60, IPS_IDLE);

    // Save & Reboot button
    SaveRebootSP[0].fill("SAVE_REBOOT", "Save & Reboot", ISS_OFF);
    SaveRebootSP.fill(getDeviceName(), "RPA_SAVE_REBOOT", "Save & Reboot",
                      MOTOR_CONFIG_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // ── Info tab ──────────────────────────────────────────────────────────
    WiFiInfoTP[WIFI_INFO_AP_SSID].fill("AP_SSID",  "AP SSID",     "");
    WiFiInfoTP[WIFI_INFO_AP_IP].fill("AP_IP",      "AP IP",       "");
    WiFiInfoTP[WIFI_INFO_STA_SSID].fill("STA_SSID", "Station SSID", "");
    WiFiInfoTP[WIFI_INFO_STA_IP].fill("STA_IP",    "Station IP",  "");
    WiFiInfoTP.fill(getDeviceName(), "RPA_WIFI_INFO", "WiFi Info", INFO_TAB, IP_RO, 60, IPS_IDLE);

    setDriverInterface(AUX_INTERFACE | PAC_INTERFACE);

    // Serial connection
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    registerConnection(serialConnection);

    addAuxControls();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// updateProperties
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(StatusTP);
        defineProperty(WiFiLP);
        defineProperty(HomedLP);
        defineProperty(AzSoftLimitsNP);
        defineProperty(AltSoftLimitsNP);
        defineProperty(AzMotorNP);
        defineProperty(AltMotorNP);
        defineProperty(SaveRebootSP);
        defineProperty(WiFiInfoTP);
    }
    else
    {
        deleteProperty(StatusTP);
        deleteProperty(WiFiLP);
        deleteProperty(HomedLP);
        deleteProperty(AzSoftLimitsNP);
        deleteProperty(AltSoftLimitsNP);
        deleteProperty(AzMotorNP);
        deleteProperty(AltMotorNP);
        deleteProperty(SaveRebootSP);
        deleteProperty(WiFiInfoTP);
    }

    PACI::updateProperties();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// saveConfigItems
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    PACI::saveConfigItems(fp);
    AzSoftLimitsNP.save(fp);
    AltSoftLimitsNP.save(fp);
    AzMotorNP.save(fp);
    AltMotorNP.save(fp);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Handshake
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::Handshake()
{
    PortFD = serialConnection->getPortFD();

    // Flush any stale data
    tcflush(PortFD, TCIOFLUSH);

    // Small pause to let the device settle
    usleep(200000);

    // Send the takeover handshake command
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("[MLAstroRPA-TC]", res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("Unexpected handshake response: %s", res);
        return false;
    }

    // Lock the device into relative (angle) mode for all subsequent moves
    if (!sendCommand("JoRe:1", res))
    {
        LOG_WARN("Could not set relative mode – proceeding anyway.");
    }

    // Read initial telemetry to populate properties
    char telem[DRIVER_LEN] = {0};
    if (sendCommand("?", telem))
        parseTelemetry(telem);

    LOG_INFO("MLAstro RPA connected and ready.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// TimerHit
/////////////////////////////////////////////////////////////////////////////
void MLAstroRPA::TimerHit()
{
    if (!isConnected())
        return;

    char res[DRIVER_LEN] = {0};
    if (sendCommand("?", res))
        parseTelemetry(res);

    checkMotionComplete();

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
/// parseTelemetry
/// Format: <STATUS|Mpos:X.XXXXX,Y.YYYYY|>key:val,key:val,...
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::parseTelemetry(const char *response)
{
    if (!response || *response == '\0')
        return false;

    // ── 1. Extract STATUS ────────────────────────────────────────────────
    const char *lt = strchr(response, '<');
    const char *pipe1 = strchr(response, '|');
    if (lt && pipe1 && pipe1 > lt)
    {
        char status[64] = {0};
        int len = static_cast<int>(pipe1 - lt - 1);
        if (len > 0 && len < static_cast<int>(sizeof(status)))
        {
            strncpy(status, lt + 1, len);
            status[len] = '\0';
            if (strcmp(StatusTP[0].getText(), status) != 0)
            {
                StatusTP[0].setText(status);
                StatusTP.setState(IPS_OK);
                StatusTP.apply();
            }
            m_IsMoving = (strstr(status, "MOVING")   != nullptr ||
                          strstr(status, "ALIGNING")  != nullptr);
            m_IsHoming = (strstr(status, "HOMING")    != nullptr);
        }
    }

    // ── 2. Extract Mpos: AZ, ALT ─────────────────────────────────────────
    const char *mpos = strstr(response, "Mpos:");
    if (mpos)
    {
        double azDeg = 0.0, altDeg = 0.0;
        if (sscanf(mpos + 5, "%lf,%lf", &azDeg, &altDeg) == 2)
        {
            PositionNP[POSITION_AZ].setValue(azDeg);
            PositionNP[POSITION_ALT].setValue(altDeg);
            PositionNP.setState(m_IsMoving ? IPS_BUSY : IPS_OK);
            PositionNP.apply();
        }
    }

    // ── 3. Parse DATA_SETTING key:value pairs ────────────────────────────
    const char *dataStart = strstr(response, "|>");
    if (!dataStart)
        return true;  // No data section; that's OK.
    dataStart += 2;   // Skip "|>"

    // Work on a mutable copy
    char data[DRIVER_LEN] = {0};
    strncpy(data, dataStart, DRIVER_LEN - 1);

    char *token = strtok(data, ",");
    while (token)
    {
        char key[32] = {0};
        char val[64] = {0};

        if (sscanf(token, "%31[^:]:%63s", key, val) == 2)
        {
            double v = atof(val);

            if      (strcmp(key, "SLvl") == 0)
            {
                SpeedNP[0].setValue(v);
                SpeedNP.setState(IPS_OK);
                SpeedNP.apply();
            }
            else if (strcmp(key, "Home") == 0)
            {
                bool homed = (v != 0.0);
                if (homed != m_IsHomed)
                {
                    m_IsHomed = homed;
                    HomedLP[0].setState(homed ? IPS_OK : IPS_IDLE);
                    HomedLP.apply();
                }
            }
            else if (strcmp(key, "WSta") == 0)
            {
                WiFiLP[0].setState(v != 0.0 ? IPS_OK : IPS_IDLE);
                WiFiLP.apply();
            }
            else if (strcmp(key, "Back") == 0)
            {
                int idx = (v != 0.0) ? DefaultDevice::INDI_ENABLED : DefaultDevice::INDI_DISABLED;
                BacklashSP.reset();
                BacklashSP[idx].setState(ISS_ON);
                BacklashSP.setState(IPS_OK);
                BacklashSP.apply();
            }
            else if (strcmp(key, "AzBl") == 0)
            {
                BacklashNP[BACKLASH_AZ].setValue(v);
                BacklashNP.setState(IPS_OK);
                BacklashNP.apply();
            }
            else if (strcmp(key, "AlBl") == 0)
            {
                BacklashNP[BACKLASH_ALT].setValue(v);
                BacklashNP.setState(IPS_OK);
                BacklashNP.apply();
            }
            else if (strcmp(key, "AzPH") == 0)
            {
                // AzPH is the actual motor position — already captured via Mpos; keep in sync
                PositionNP[POSITION_AZ].setValue(v);
            }
            else if (strcmp(key, "AlPH") == 0)
            {
                PositionNP[POSITION_ALT].setValue(v);
            }
            // ── Soft limits
            else if (strcmp(key, "AzL1") == 0)
            {
                AzSoftLimitsNP[AZ_LIMIT_MIN].setValue(v);
                AzSoftLimitsNP.setState(IPS_OK);
                AzSoftLimitsNP.apply();
            }
            else if (strcmp(key, "AzL2") == 0)
            {
                AzSoftLimitsNP[AZ_LIMIT_MAX].setValue(v);
                AzSoftLimitsNP.setState(IPS_OK);
                AzSoftLimitsNP.apply();
            }
            else if (strcmp(key, "AlL1") == 0)
            {
                AltSoftLimitsNP[ALT_LIMIT_MIN].setValue(v);
                AltSoftLimitsNP.setState(IPS_OK);
                AltSoftLimitsNP.apply();
            }
            else if (strcmp(key, "AlL2") == 0)
            {
                AltSoftLimitsNP[ALT_LIMIT_MAX].setValue(v);
                AltSoftLimitsNP.setState(IPS_OK);
                AltSoftLimitsNP.apply();
            }
            // ── Azimuth motor config
            else if (strcmp(key, "AzIR")  == 0)
            {
                AzMotorNP[AZ_RUN_CURRENT].setValue(v);
                AzMotorNP.apply();
            }
            else if (strcmp(key, "AzIH")  == 0)
            {
                AzMotorNP[AZ_HOLD_CURRENT].setValue(v);
                AzMotorNP.apply();
            }
            else if (strcmp(key, "AzMS")  == 0)
            {
                AzMotorNP[AZ_MICROSTEPS].setValue(v);
                AzMotorNP.apply();
            }
            else if (strcmp(key, "AzAc")  == 0)
            {
                AzMotorNP[AZ_ACCEL].setValue(v);
                AzMotorNP.apply();
            }
            else if (strcmp(key, "AzDec") == 0)
            {
                AzMotorNP[AZ_DECEL].setValue(v);
                AzMotorNP.apply();
            }
            else if (strcmp(key, "AzSD")  == 0)
            {
                AzMotorNP[AZ_STEPS_PER_DEG].setValue(v);
                AzMotorNP.apply();
            }
            else if (strcmp(key, "AzSB")  == 0)
            {
                AzMotorNP[AZ_START_BOOST].setValue(v);
                AzMotorNP.apply();
            }
            else if (strcmp(key, "AzSC")  == 0)
            {
                AzMotorNP[AZ_COOL_STEP].setValue(v);
                AzMotorNP.apply();
            }
            else if (strcmp(key, "AzRM")  == 0)
            {
                AzMotorNP[AZ_RUN_MODE].setValue(v);
                AzMotorNP.setState(IPS_OK);
                AzMotorNP.apply();
            }
            // Azimuth reverse — sync PAC reverse property
            else if (strcmp(key, "AzRD")  == 0)
            {
                int idx = (v != 0.0) ? DefaultDevice::INDI_ENABLED : DefaultDevice::INDI_DISABLED;
                AZReverseSP.reset();
                AZReverseSP[idx].setState(ISS_ON);
                AZReverseSP.setState(IPS_OK);
                AZReverseSP.apply();
            }
            // ── Altitude motor config
            else if (strcmp(key, "AlIR")  == 0)
            {
                AltMotorNP[ALT_RUN_CURRENT].setValue(v);
                AltMotorNP.apply();
            }
            else if (strcmp(key, "AlIH")  == 0)
            {
                AltMotorNP[ALT_HOLD_CURRENT].setValue(v);
                AltMotorNP.apply();
            }
            else if (strcmp(key, "AlMS")  == 0)
            {
                AltMotorNP[ALT_MICROSTEPS].setValue(v);
                AltMotorNP.apply();
            }
            else if (strcmp(key, "AlAc")  == 0)
            {
                AltMotorNP[ALT_ACCEL].setValue(v);
                AltMotorNP.apply();
            }
            else if (strcmp(key, "AlDe")  == 0)
            {
                AltMotorNP[ALT_DECEL].setValue(v);
                AltMotorNP.apply();
            }
            else if (strcmp(key, "AlSD")  == 0)
            {
                AltMotorNP[ALT_STEPS_PER_DEG].setValue(v);
                AltMotorNP.apply();
            }
            else if (strcmp(key, "AlSB")  == 0)
            {
                AltMotorNP[ALT_START_BOOST].setValue(v);
                AltMotorNP.apply();
            }
            else if (strcmp(key, "AlSC")  == 0)
            {
                AltMotorNP[ALT_COOL_STEP].setValue(v);
                AltMotorNP.apply();
            }
            else if (strcmp(key, "AlRM")  == 0)
            {
                AltMotorNP[ALT_RUN_MODE].setValue(v);
                AltMotorNP.setState(IPS_OK);
                AltMotorNP.apply();
            }
            // Altitude reverse
            else if (strcmp(key, "AlRD")  == 0)
            {
                int idx = (v != 0.0) ? DefaultDevice::INDI_ENABLED : DefaultDevice::INDI_DISABLED;
                ALTReverseSP.reset();
                ALTReverseSP[idx].setState(ISS_ON);
                ALTReverseSP.setState(IPS_OK);
                ALTReverseSP.apply();
            }
            // ── WiFi info (string values)
            else if (strcmp(key, "APss") == 0)
            {
                WiFiInfoTP[WIFI_INFO_AP_SSID].setText(val);
                WiFiInfoTP.apply();
            }
            else if (strcmp(key, "APip") == 0)
            {
                WiFiInfoTP[WIFI_INFO_AP_IP].setText(val);
                WiFiInfoTP.apply();
            }
            else if (strcmp(key, "STAs") == 0)
            {
                WiFiInfoTP[WIFI_INFO_STA_SSID].setText(val);
                WiFiInfoTP.apply();
            }
            else if (strcmp(key, "STAi") == 0)
            {
                WiFiInfoTP[WIFI_INFO_STA_IP].setText(val);
                WiFiInfoTP.setState(IPS_OK);
                WiFiInfoTP.apply();
            }
        }
        token = strtok(nullptr, ",");
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// checkMotionComplete
/////////////////////////////////////////////////////////////////////////////
void MLAstroRPA::checkMotionComplete()
{
    // If we were busy and are no longer moving, finalize states
    if (!m_IsMoving)
    {
        if (ManualAdjustmentNP.getState() == IPS_BUSY)
        {
            ManualAdjustmentNP.setState(IPS_OK);
            ManualAdjustmentNP.apply();
            LOG_INFO("MLAstro RPA motion complete.");
        }

        if (m_IsHoming && HomeSP.getState() == IPS_BUSY)
        {
            m_IsHoming = false;
            HomeSP.setState(IPS_OK);
            HomeSP.apply();
            LOG_INFO("MLAstro RPA homing complete.");
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
/// degreesToDMS – helper
/////////////////////////////////////////////////////////////////////////////
void MLAstroRPA::degreesToDMS(double degrees, int &d, int &m, int &s, bool &positive)
{
    positive = (degrees >= 0.0);
    double abs_deg = std::fabs(degrees);
    d = static_cast<int>(abs_deg);
    double rem = (abs_deg - d) * 60.0;
    m = static_cast<int>(rem);
    s = static_cast<int>((rem - m) * 60.0 + 0.5);  // round to nearest arcsec
    // Handle carry-over
    if (s >= 60)
    {
        s -= 60;
        m += 1;
    }
    if (m >= 60)
    {
        m -= 60;
        d += 1;
    }
}

/////////////////////////////////////////////////////////////////////////////
/// sendRelativeMove – sets angle then issues the direction command
/////////////////////////////////////////////////////////////////////////////
IPState MLAstroRPA::sendRelativeMove(double degrees, const char *cmdPos, const char *cmdNeg)
{
    if (degrees == 0.0)
        return IPS_OK;

    int d, m, s;
    bool positive;
    degreesToDMS(degrees, d, m, s, positive);

    // Set degrees/minutes/seconds on the device, then fire the direction command
    char cmd[DRIVER_LEN] = {0};
    snprintf(cmd, DRIVER_LEN, "ReDe:%d,ReAM:%d,ReAS:%d,%s", d, m, s,
             positive ? cmdPos : cmdNeg);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return IPS_ALERT;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("Unexpected move response: %s", res);
        // Log protocol errors for diagnostics
        if (strstr(res, "Soft Limit"))
            LOG_ERROR("Motion rejected: soft limit reached.");
        else if (strstr(res, "Hard Limit"))
            LOG_ERROR("Motion rejected: hard limit triggered.");
        else if (strstr(res, "Not homed"))
            LOG_ERROR("Motion rejected: device is not homed.");
        else if (strstr(res, "System Locked"))
            LOG_ERROR("Motion rejected: system is locked (error state).");
        return IPS_ALERT;
    }

    LOGF_INFO("Relative move: %.4f° → D=%d M=%d S=%d dir=%s",
              degrees, d, m, s, positive ? "positive" : "negative");

    m_IsMoving = true;
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
/// MoveAZ
/////////////////////////////////////////////////////////////////////////////
IPState MLAstroRPA::MoveAZ(double degrees)
{
    // positive = East → MAzR, negative = West → MAzL
    return sendRelativeMove(degrees, "MAzR:1", "MAzL:1");
}

/////////////////////////////////////////////////////////////////////////////
/// MoveALT
/////////////////////////////////////////////////////////////////////////////
IPState MLAstroRPA::MoveALT(double degrees)
{
    // positive = Up → MAlU, negative = Down → MAlD
    return sendRelativeMove(degrees, "MAlU:1", "MAlD:1");
}

/////////////////////////////////////////////////////////////////////////////
/// AbortMotion
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::AbortMotion()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("STOP:1", res))
        return false;

    m_IsMoving = false;
    m_IsHoming = false;
    LOG_INFO("MLAstro RPA motion stopped.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SetPACSpeed  (SLvl: 1–5)
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::SetPACSpeed(uint16_t speed)
{
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), "SLvl:%u", speed);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("SetPACSpeed: unexpected response: %s", res);
        return false;
    }
    LOGF_INFO("Speed set to level %u.", speed);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// ReverseAZ
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::ReverseAZ(bool enabled)
{
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), "AzRD:%d", enabled ? 1 : 0);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("ReverseAZ: unexpected response: %s", res);
        return false;
    }
    LOGF_INFO("AZ direction %s.", enabled ? "reversed" : "normal");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// ReverseALT
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::ReverseALT(bool enabled)
{
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), "AlRD:%d", enabled ? 1 : 0);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("ReverseALT: unexpected response: %s", res);
        return false;
    }
    LOGF_INFO("ALT direction %s.", enabled ? "reversed" : "normal");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SetHome
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::SetHome()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("SetH:1", res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("SetHome: unexpected response: %s", res);
        return false;
    }
    m_IsHomed = true;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// GoHome
/////////////////////////////////////////////////////////////////////////////
IPState MLAstroRPA::GoHome()
{
    if (!m_IsHomed)
    {
        LOG_ERROR("No home position set. Please set home first.");
        return IPS_ALERT;
    }

    char res[DRIVER_LEN] = {0};
    if (!sendCommand("RetH:1", res))
        return IPS_ALERT;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("GoHome: unexpected response: %s", res);
        if (strstr(res, "Not homed"))
            LOG_ERROR("Device reports: not homed.");
        return IPS_ALERT;
    }

    m_IsHoming = true;
    m_IsMoving = true;
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
/// ResetHome
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::ResetHome()
{
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("RstH:1", res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("ResetHome: unexpected response: %s", res);
        return false;
    }
    m_IsHomed = false;
    HomedLP[0].setState(IPS_IDLE);
    HomedLP.apply();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SyncAZ  –  marks current position as the AZ reference (zero)
/// The device has no arbitrary-value position sync, so any sync
/// operation calls SetH:1 which zeros both axes simultaneously.
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::SyncAZ(double degrees)
{
    INDI_UNUSED(degrees);
    // SetH:1 is the only "sync" mechanism available; it sets (0,0).
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("SetH:1", res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("SyncAZ: unexpected response: %s", res);
        return false;
    }
    m_IsHomed = true;
    LOG_INFO("AZ synced: current position set as reference (0).");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SyncALT  –  same mechanism as SyncAZ
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::SyncALT(double degrees)
{
    INDI_UNUSED(degrees);
    char res[DRIVER_LEN] = {0};
    if (!sendCommand("SetH:1", res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("SyncALT: unexpected response: %s", res);
        return false;
    }
    m_IsHomed = true;
    LOG_INFO("ALT synced: current position set as reference (0).");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SetBacklashEnabled
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::SetBacklashEnabled(bool enabled)
{
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), "Back:%d", enabled ? 1 : 0);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("SetBacklashEnabled: unexpected response: %s", res);
        return false;
    }
    LOGF_INFO("Backlash compensation %s.", enabled ? "enabled" : "disabled");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SetBacklashAZ
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::SetBacklashAZ(int32_t steps)
{
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), "AzBl:%d", steps);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("SetBacklashAZ: unexpected response: %s", res);
        return false;
    }
    LOGF_INFO("AZ backlash set to %d steps.", steps);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SetBacklashALT
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::SetBacklashALT(int32_t steps)
{
    char cmd[64] = {0};
    snprintf(cmd, sizeof(cmd), "AlBl:%d", steps);

    char res[DRIVER_LEN] = {0};
    if (!sendCommand(cmd, res))
        return false;

    if (strncmp(res, "ok", 2) != 0)
    {
        LOGF_ERROR("SetBacklashALT: unexpected response: %s", res);
        return false;
    }
    LOGF_INFO("ALT backlash set to %d steps.", steps);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// ISNewNumber
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // ── Soft limits
        if (AzSoftLimitsNP.isNameMatch(name))
        {
            AzSoftLimitsNP.update(values, names, n);
            char cmd[DRIVER_LEN] = {0};
            snprintf(cmd, DRIVER_LEN, "AzL1:%.2f,AzL2:%.2f",
                     AzSoftLimitsNP[AZ_LIMIT_MIN].getValue(),
                     AzSoftLimitsNP[AZ_LIMIT_MAX].getValue());
            char res[DRIVER_LEN] = {0};
            if (sendCommand(cmd, res) && strncmp(res, "ok", 2) == 0)
            {
                AzSoftLimitsNP.setState(IPS_OK);
                saveConfig(AzSoftLimitsNP);
            }
            else
                AzSoftLimitsNP.setState(IPS_ALERT);
            AzSoftLimitsNP.apply();
            return true;
        }

        if (AltSoftLimitsNP.isNameMatch(name))
        {
            AltSoftLimitsNP.update(values, names, n);
            char cmd[DRIVER_LEN] = {0};
            snprintf(cmd, DRIVER_LEN, "AlL1:%.2f,AlL2:%.2f",
                     AltSoftLimitsNP[ALT_LIMIT_MIN].getValue(),
                     AltSoftLimitsNP[ALT_LIMIT_MAX].getValue());
            char res[DRIVER_LEN] = {0};
            if (sendCommand(cmd, res) && strncmp(res, "ok", 2) == 0)
            {
                AltSoftLimitsNP.setState(IPS_OK);
                saveConfig(AltSoftLimitsNP);
            }
            else
                AltSoftLimitsNP.setState(IPS_ALERT);
            AltSoftLimitsNP.apply();
            return true;
        }

        // ── Motor config (AZ)
        if (AzMotorNP.isNameMatch(name))
        {
            AzMotorNP.update(values, names, n);
            char cmd[DRIVER_LEN] = {0};
            snprintf(cmd, DRIVER_LEN,
                     "AzIR:%.0f,AzIH:%.0f,AzMS:%.0f,AzAc:%.0f,AzDec:%.0f,"
                     "AzSD:%.0f,AzSB:%.0f,AzSC:%.0f,AzRM:%.0f",
                     AzMotorNP[AZ_RUN_CURRENT].getValue(),
                     AzMotorNP[AZ_HOLD_CURRENT].getValue(),
                     AzMotorNP[AZ_MICROSTEPS].getValue(),
                     AzMotorNP[AZ_ACCEL].getValue(),
                     AzMotorNP[AZ_DECEL].getValue(),
                     AzMotorNP[AZ_STEPS_PER_DEG].getValue(),
                     AzMotorNP[AZ_START_BOOST].getValue(),
                     AzMotorNP[AZ_COOL_STEP].getValue(),
                     AzMotorNP[AZ_RUN_MODE].getValue());
            char res[DRIVER_LEN] = {0};
            if (sendCommand(cmd, res) && strncmp(res, "ok", 2) == 0)
            {
                AzMotorNP.setState(IPS_OK);
                saveConfig(AzMotorNP);
            }
            else
                AzMotorNP.setState(IPS_ALERT);
            AzMotorNP.apply();
            return true;
        }

        // ── Motor config (ALT)
        if (AltMotorNP.isNameMatch(name))
        {
            AltMotorNP.update(values, names, n);
            char cmd[DRIVER_LEN] = {0};
            snprintf(cmd, DRIVER_LEN,
                     "AlIR:%.0f,AlIH:%.0f,AlMS:%.0f,AlAc:%.0f,AlDe:%.0f,"
                     "AlSD:%.0f,AlSB:%.0f,AlSC:%.0f,AlRM:%.0f",
                     AltMotorNP[ALT_RUN_CURRENT].getValue(),
                     AltMotorNP[ALT_HOLD_CURRENT].getValue(),
                     AltMotorNP[ALT_MICROSTEPS].getValue(),
                     AltMotorNP[ALT_ACCEL].getValue(),
                     AltMotorNP[ALT_DECEL].getValue(),
                     AltMotorNP[ALT_STEPS_PER_DEG].getValue(),
                     AltMotorNP[ALT_START_BOOST].getValue(),
                     AltMotorNP[ALT_COOL_STEP].getValue(),
                     AltMotorNP[ALT_RUN_MODE].getValue());
            char res[DRIVER_LEN] = {0};
            if (sendCommand(cmd, res) && strncmp(res, "ok", 2) == 0)
            {
                AltMotorNP.setState(IPS_OK);
                saveConfig(AltMotorNP);
            }
            else
                AltMotorNP.setState(IPS_ALERT);
            AltMotorNP.apply();
            return true;
        }

        // Delegate to PACInterface for its number properties
        if (PACI::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// ISNewSwitch
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // ── Save & Reboot
        if (SaveRebootSP.isNameMatch(name))
        {
            SaveRebootSP.reset();
            char res[DRIVER_LEN] = {0};
            if (sendCommand("Save&Reboot:1", res) && strncmp(res, "ok", 2) == 0)
            {
                SaveRebootSP.setState(IPS_OK);
                LOG_INFO("Settings saved. Controller is rebooting…");
            }
            else
            {
                SaveRebootSP.setState(IPS_ALERT);
                LOG_ERROR("Save & Reboot command failed.");
            }
            SaveRebootSP.apply();
            return true;
        }

        // Delegate to PACInterface for its switch properties
        if (PACI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// Communication
/////////////////////////////////////////////////////////////////////////////
bool MLAstroRPA::sendCommand(const char *cmd, char *res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    // Append newline and send
    char formatted[DRIVER_LEN] = {0};
    snprintf(formatted, DRIVER_LEN, "%s\n", cmd);

    LOGF_DEBUG("CMD <%s>", cmd);

    rc = tty_write_string(PortFD, formatted, &nbytes_written);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);
    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    // Strip trailing CR/LF
    if (nbytes_read > 0 && (res[nbytes_read - 1] == '\n' || res[nbytes_read - 1] == '\r'))
        res[--nbytes_read] = '\0';
    if (nbytes_read > 0 && (res[nbytes_read - 1] == '\r'))
        res[--nbytes_read] = '\0';

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);
    return true;
}

void MLAstroRPA::hexDump(char *buf, const char *data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));
    if (size > 0)
        buf[3 * size - 1] = '\0';
}
