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
}

bool PACSimulator::initProperties()
{
    INDI::DefaultDevice::initProperties();

    PACI::initProperties(MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | PAC_INTERFACE);

    OperationDurationNP[0].fill("DURATION", "Duration (s)", "%.1f", 1, 60, 1, 5);
    OperationDurationNP.fill(getDeviceName(), "OPERATION_DURATION", "Operation", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

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
    {
        defineProperty(OperationDurationNP);
        defineProperty(CorrectionGainNP);
    }
    else
    {
        deleteProperty(OperationDurationNP);
        deleteProperty(CorrectionGainNP);
    }

    PACI::updateProperties();

    return true;
}

bool PACSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (OperationDurationNP.isNameMatch(name))
        {
            OperationDurationNP.update(values, names, n);
            OperationDurationNP.setState(IPS_OK);
            OperationDurationNP.apply();
            return true;
        }

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

    OperationDurationNP.save(fp);
    CorrectionGainNP.save(fp);

    return true;
}

// ---------------------------------------------------------------------------
// Automated correction
// ---------------------------------------------------------------------------

IPState PACSimulator::StartCorrection(double azError, double altError)
{
    if (CorrectionSP.getState() == IPS_BUSY)
    {
        LOG_WARN("Alignment correction is already in progress.");
        return IPS_BUSY;
    }

    const double gain        = CorrectionGainNP[0].getValue() / 100.0;
    const double effectiveAz = azError * gain;
    const double effectiveAlt = altError * gain;

    LOGF_INFO("Starting automated alignment correction: requested az=%.4f, alt=%.4f deg; "
              "effective az=%.4f, alt=%.4f deg (gain %.0f%%).",
              azError, altError, effectiveAz, effectiveAlt, CorrectionGainNP[0].getValue());

    // Delegate to the individual axis movers (with gain applied).
    // MoveAZ: positive = East, negative = West.
    //   azError > 0 → polar axis East → move West → -effectiveAz
    // MoveALT: positive = North, negative = South.
    //   altError > 0 → altitude too high → move South → -effectiveAlt
    const IPState azState  = MoveAZ(-effectiveAz);
    const IPState altState = MoveALT(-effectiveAlt);

    if (azState == IPS_ALERT || altState == IPS_ALERT)
        return IPS_ALERT;
    if (azState == IPS_BUSY || altState == IPS_BUSY)
        return IPS_BUSY;
    return IPS_OK;
}

IPState PACSimulator::AbortCorrection()
{
    LOG_INFO("Alignment correction aborted.");
    return IPS_OK;
}

// ---------------------------------------------------------------------------
// Manual single-axis movement
// ---------------------------------------------------------------------------

IPState PACSimulator::MoveAZ(double degrees)
{
    const double duration = OperationDurationNP[0].getValue();
    const char  *direction = (degrees >= 0) ? "East" : "West";

    LOGF_INFO("Simulating azimuth move: %.4f deg %s (duration %.1f s).", std::abs(degrees), direction, duration);

    INDI::Timer::singleShot(static_cast<int>(duration * 1000), [this, degrees]()
    {
        LOGF_INFO("Azimuth move complete: %.4f deg %s.", std::abs(degrees), (degrees >= 0) ? "East" : "West");

        ManualAdjustmentNP.setState(IPS_OK);
        ManualAdjustmentNP.apply();

        // If both axes have finished, mark the overall correction complete.
        if (CorrectionSP.getState() == IPS_BUSY)
        {
            CorrectionSP.setState(IPS_OK);
            CorrectionSP.reset();
            CorrectionSP.apply();
            CorrectionStatusLP[0].setState(IPS_OK);
            CorrectionStatusLP.apply();
            LOG_INFO("Alignment correction completed successfully.");
        }
    });

    return IPS_BUSY;
}

IPState PACSimulator::MoveALT(double degrees)
{
    const double duration = OperationDurationNP[0].getValue();
    const char  *direction = (degrees >= 0) ? "North" : "South";

    LOGF_INFO("Simulating altitude move: %.4f deg %s (duration %.1f s).", std::abs(degrees), direction, duration);

    INDI::Timer::singleShot(static_cast<int>(duration * 1000), [this, degrees]()
    {
        LOGF_INFO("Altitude move complete: %.4f deg %s.", std::abs(degrees), (degrees >= 0) ? "North" : "South");

        ManualAdjustmentNP.setState(IPS_OK);
        ManualAdjustmentNP.apply();

        // If both axes have finished, mark the overall correction complete.
        if (CorrectionSP.getState() == IPS_BUSY)
        {
            CorrectionSP.setState(IPS_OK);
            CorrectionSP.reset();
            CorrectionSP.apply();
            CorrectionStatusLP[0].setState(IPS_OK);
            CorrectionStatusLP.apply();
            LOG_INFO("Alignment correction completed successfully.");
        }
    });

    return IPS_BUSY;
}
