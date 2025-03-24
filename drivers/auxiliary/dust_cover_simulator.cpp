/*******************************************************************************
  Dust Cover Simulator

  SPDX-FileCopyrightText: 2025 Jasem Mutlaq
  SPDX-License-Identifier: LGPL-2.0-or-later
*******************************************************************************/

#include "dust_cover_simulator.h"
#include "indibase/timer/inditimer.h"

#include <memory>

static std::unique_ptr<DustCoverSimulator> simulator(new DustCoverSimulator());

DustCoverSimulator::DustCoverSimulator() : DustCapInterface(this)
{
    setVersion(1, 0);
}

bool DustCoverSimulator::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Initialize the dust cap interface
    DI::initProperties(MAIN_CONTROL_TAB);

    // Set the driver interface
    setDriverInterface(AUX_INTERFACE | DUSTCAP_INTERFACE);

    // Operation duration property
    OperationDurationNP[0].fill("DURATION", "Duration (s)", "%.1f", 1, 60, 1, 5);
    OperationDurationNP.fill(getDeviceName(), "OPERATION_DURATION", "Operation", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    addAuxControls();

    return true;
}

bool DustCoverSimulator::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        ParkCapSP[CAP_PARK].setState(ISS_OFF);
        ParkCapSP[CAP_UNPARK].setState(ISS_ON);
        ParkCapSP.setState(IPS_OK);
        defineProperty(OperationDurationNP);
    }
    else
    {
        deleteProperty(OperationDurationNP);
    }

    DI::updateProperties();

    return true;
}

bool DustCoverSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Operation Duration
        if (OperationDurationNP.isNameMatch(name))
        {
            OperationDurationNP.update(values, names, n);
            OperationDurationNP.setState(IPS_OK);
            OperationDurationNP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool DustCoverSimulator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (DI::processSwitch(dev, name, states, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool DustCoverSimulator::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    OperationDurationNP.save(fp);

    return true;
}

IPState DustCoverSimulator::ParkCap()
{
    if (ParkCapSP.getState() == IPS_BUSY)
    {
        LOG_WARN("Dust cover is already in motion.");
        return IPS_BUSY;
    }

    // Set the timer to simulate the dust cover movement
    double duration = OperationDurationNP[0].getValue();
    LOGF_INFO("Parking dust cover. This will take %.1f seconds.", duration);

    // Use INDI::Timer for the timer
    INDI::Timer::singleShot(duration * 1000, [this]()
    {
        LOG_INFO("Dust cover parked successfully.");
        ParkCapSP.setState(IPS_OK);
        ParkCapSP.apply();
    });

    return IPS_BUSY;
}

IPState DustCoverSimulator::UnParkCap()
{
    if (ParkCapSP.getState() == IPS_BUSY)
    {
        LOG_WARN("Dust cover is already in motion.");
        return IPS_BUSY;
    }

    // Set the timer to simulate the dust cover movement
    double duration = OperationDurationNP[0].getValue();
    LOGF_INFO("Unparking dust cover. This will take %.1f seconds.", duration);

    // Use INDI::Timer for the timer
    INDI::Timer::singleShot(duration * 1000, [this]()
    {
        LOG_INFO("Dust cover unparked successfully.");
        ParkCapSP.setState(IPS_OK);
        ParkCapSP.apply();
    });

    return IPS_BUSY;
}
