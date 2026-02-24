/*******************************************************************************
  Alignment Correction Simulator

  SPDX-FileCopyrightText: 2026 Joaquin Rodriguez
  SPDX-License-Identifier: LGPL-2.0-or-later
*******************************************************************************/

#include "alignment_correction_simulator.h"
#include "indibase/timer/inditimer.h"

#include <memory>

static std::unique_ptr<AlignmentCorrectionSimulator> simulator(new AlignmentCorrectionSimulator());

AlignmentCorrectionSimulator::AlignmentCorrectionSimulator()
    : AlignmentCorrectionInterface(this)
{
    setVersion(1, 0);
}

bool AlignmentCorrectionSimulator::initProperties()
{
    INDI::DefaultDevice::initProperties();

    ACI::initProperties(MAIN_CONTROL_TAB);

    setDriverInterface(AUX_INTERFACE | ALIGNMENT_CORRECTION_INTERFACE);

    OperationDurationNP[0].fill("DURATION", "Duration (s)", "%.1f", 1, 60, 1, 5);
    OperationDurationNP.fill(getDeviceName(), "OPERATION_DURATION", "Operation", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    addAuxControls();

    return true;
}

bool AlignmentCorrectionSimulator::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineProperty(OperationDurationNP);
    }
    else
    {
        deleteProperty(OperationDurationNP);
    }

    ACI::updateProperties();

    return true;
}

bool AlignmentCorrectionSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
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

        if (ACI::processNumber(dev, name, values, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool AlignmentCorrectionSimulator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ACI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool AlignmentCorrectionSimulator::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    OperationDurationNP.save(fp);

    return true;
}

IPState AlignmentCorrectionSimulator::StartCorrection(double azError, double altError)
{
    if (CorrectionSP.getState() == IPS_BUSY)
    {
        LOG_WARN("Alignment correction is already in progress.");
        return IPS_BUSY;
    }

    double duration = OperationDurationNP[0].getValue();
    LOGF_INFO("Simulating alignment correction: az=%.4f deg, alt=%.4f deg. This will take %.1f seconds.",
              azError, altError, duration);

    INDI::Timer::singleShot(static_cast<int>(duration * 1000), [this]()
    {
        LOG_INFO("Alignment correction completed successfully.");
        CorrectionSP.setState(IPS_OK);
        CorrectionSP.reset();
        CorrectionSP.apply();
        CorrectionStatusLP[0].setState(IPS_OK);
        CorrectionStatusLP.apply();
    });

    return IPS_BUSY;
}

IPState AlignmentCorrectionSimulator::AbortCorrection()
{
    LOG_INFO("Alignment correction aborted.");
    return IPS_OK;
}
