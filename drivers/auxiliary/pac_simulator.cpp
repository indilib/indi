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

IPState PACSimulator::StartCorrection(double azError, double altError)
{
    if (CorrectionSP.getState() == IPS_BUSY)
    {
        LOG_WARN("Alignment correction is already in progress.");
        return IPS_BUSY;
    }

    double gain = CorrectionGainNP[0].getValue() / 100.0;
    double effectiveAz = azError * gain;
    double effectiveAlt = altError * gain;

    double duration = OperationDurationNP[0].getValue();
    LOGF_INFO("Simulating alignment correction: requested az=%.4f, alt=%.4f deg; "
              "effective az=%.4f, alt=%.4f deg (gain %.0f%%). Duration %.1f s.",
              azError, altError, effectiveAz, effectiveAlt, CorrectionGainNP[0].getValue(), duration);

    INDI::Timer::singleShot(static_cast<int>(duration * 1000), [this, effectiveAz, effectiveAlt]()
    {
        CorrectionErrorNP[ERROR_AZ].setValue(effectiveAz);
        CorrectionErrorNP[ERROR_ALT].setValue(effectiveAlt);
        CorrectionErrorNP.setState(IPS_OK);
        CorrectionErrorNP.apply();

        LOG_INFO("Alignment correction completed successfully.");
        CorrectionSP.setState(IPS_OK);
        CorrectionSP.reset();
        CorrectionSP.apply();
        CorrectionStatusLP[0].setState(IPS_OK);
        CorrectionStatusLP.apply();
    });

    return IPS_BUSY;
}

IPState PACSimulator::AbortCorrection()
{
    LOG_INFO("Alignment correction aborted.");
    return IPS_OK;
}
