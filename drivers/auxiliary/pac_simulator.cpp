/*******************************************************************************
  PAC Simulator (Polar Alignment Correction)

  SPDX-FileCopyrightText: 2026 Joaquin Rodriguez
  SPDX-License-Identifier: LGPL-2.0-or-later
*******************************************************************************/

#include "pac_simulator.h"
#include "indibase/timer/inditimer.h"

#include <memory>

static std::unique_ptr<PACSimulator> simulator(new PACSimulator());

PACSimulator::PACSimulator()
    : PACInterface(this)
{
    setVersion(1, 0);
    SetCapability(PAC_HAS_SPEED | PAC_CAN_REVERSE);
}

bool PACSimulator::initProperties()
{
    INDI::DefaultDevice::initProperties();

    PACI::initProperties(MAIN_CONTROL_TAB);

    // Adjust speed range for the simulator: 1 (slowest, 5 s) to 5 (fastest, 1 s)
    SpeedNP[0].setMin(1);
    SpeedNP[0].setMax(5);
    SpeedNP[0].setStep(1);
    SpeedNP[0].setValue(1);

    setDriverInterface(AUX_INTERFACE | PAC_INTERFACE);

    CorrectionGainNP[0].fill("GAIN", "Gain (%)", "%.0f", 0, 100, 10, 100);
    CorrectionGainNP.fill(getDeviceName(), "CORRECTION_GAIN", "Correction Gain", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);
    CorrectionGainNP.load();

    addAuxControls();

    return true;
}

bool PACSimulator::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
        defineProperty(CorrectionGainNP);
    else
        deleteProperty(CorrectionGainNP);

    PACI::updateProperties();

    return true;
}

bool PACSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (CorrectionGainNP.isNameMatch(name))
        {
            CorrectionGainNP.update(values, names, n);
            CorrectionGainNP.setState(IPS_OK);
            CorrectionGainNP.apply();
            return true;
        }

        if (PACI::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PACSimulator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (PACI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PACSimulator::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    CorrectionGainNP.save(fp);
    PACI::saveConfigItems(fp);
    return true;
}

// ---------------------------------------------------------------------------
// Abort
// ---------------------------------------------------------------------------

bool PACSimulator::AbortMotion()
{
    LOG_INFO("Alignment correction motion aborted.");
    m_MovingAxes = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Speed and reverse
// ---------------------------------------------------------------------------

bool PACSimulator::SetPACSpeed(uint16_t speed)
{
    LOGF_INFO("Simulator speed set to %u (move duration will be %.0f s).",
              speed, SpeedNP[0].getMax() + 1 - speed);
    return true;
}

bool PACSimulator::ReverseAZ(bool enabled)
{
    LOGF_INFO("Azimuth direction reverse %s.", enabled ? "enabled" : "disabled");
    return true;
}

bool PACSimulator::ReverseALT(bool enabled)
{
    LOGF_INFO("Altitude direction reverse %s.", enabled ? "enabled" : "disabled");
    return true;
}

// ---------------------------------------------------------------------------
// Single-axis movement
// ---------------------------------------------------------------------------

IPState PACSimulator::MoveAZ(double degrees)
{
    // Duration is inversely proportional to speed: speed 1 → max s, speed max → 1 s.
    const double duration  = SpeedNP[0].getMax() + 1 - SpeedNP[0].getValue();
    const double gain      = CorrectionGainNP[0].getValue() / 100.0;
    const double effective = degrees * gain;
    const char  *direction = (effective >= 0) ? "East" : "West";

    LOGF_INFO("Simulating azimuth move: %.4f deg %s (duration %.0f s, speed %.0f, gain %.0f%%).",
              std::abs(effective), direction, duration, SpeedNP[0].getValue(), CorrectionGainNP[0].getValue());

    m_MovingAxes++;

    INDI::Timer::singleShot(static_cast<int>(duration * 1000), [this, effective]()
    {
        LOGF_INFO("Azimuth move complete: %.4f deg %s.", std::abs(effective), (effective >= 0) ? "East" : "West");

        m_MovingAxes--;
        if (m_MovingAxes <= 0)
        {
            m_MovingAxes = 0;
            // Broadcast IPS_OK so snooping drivers (e.g. telescope simulator) can
            // read the step values and apply the polar-alignment correction.
            ManualAdjustmentNP.setState(IPS_OK);
            ManualAdjustmentNP.apply();
        }
    });

    return IPS_BUSY;
}

IPState PACSimulator::MoveALT(double degrees)
{
    // Duration is inversely proportional to speed: speed 1 → max s, speed max → 1 s.
    const double duration  = SpeedNP[0].getMax() + 1 - SpeedNP[0].getValue();
    const double gain      = CorrectionGainNP[0].getValue() / 100.0;
    const double effective = degrees * gain;
    const char  *direction = (effective >= 0) ? "North" : "South";

    LOGF_INFO("Simulating altitude move: %.4f deg %s (duration %.0f s, speed %.0f, gain %.0f%%).",
              std::abs(effective), direction, duration, SpeedNP[0].getValue(), CorrectionGainNP[0].getValue());

    m_MovingAxes++;

    INDI::Timer::singleShot(static_cast<int>(duration * 1000), [this, effective]()
    {
        LOGF_INFO("Altitude move complete: %.4f deg %s.", std::abs(effective), (effective >= 0) ? "North" : "South");

        m_MovingAxes--;
        if (m_MovingAxes <= 0)
        {
            m_MovingAxes = 0;
            ManualAdjustmentNP.setState(IPS_OK);
            ManualAdjustmentNP.apply();
        }
    });

    return IPS_BUSY;
}
