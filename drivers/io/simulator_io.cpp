/*
    INDI Simulator IO
    Copyright (C) 2024 Jasem Mutlaq

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

#include "simulator_io.h"

#include <memory>
#include <unistd.h>

static std::unique_ptr<SimulatorIO> simulatorIO(new SimulatorIO());

SimulatorIO::SimulatorIO() : InputInterface(this), OutputInterface(this)
{
    setVersion(1, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Initialize Input Interface with 4 digital inputs and 0 analog inputs
    INDI::InputInterface::initProperties("Input", 4, 0, "Input");

    // Initialize Output Interface with 4 digital outputs
    INDI::OutputInterface::initProperties("Output", 4, "Output");

    // Set up simulation controls for inputs
    char swName[MAXINDINAME];
    char swLabel[MAXINDILABEL];
    for (int i = 0; i < 4; i++)
    {
        snprintf(swName, MAXINDINAME, "SIM_INPUT_%d", i + 1);
        snprintf(swLabel, MAXINDILABEL, "Input %d", i + 1);
        SimulateInputsSP[i].fill(swName, swLabel, ISS_OFF);
    }
    SimulateInputsSP.fill(getDeviceName(), "SIMULATOR_INPUT", "Inputs", "Simulation", IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
    SimulateInputsSP.load();

    setDriverInterface(AUX_INTERFACE | INPUT_INTERFACE | OUTPUT_INTERFACE);

    addAuxControls();

    setDefaultPollingPeriod(1000);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    INDI::InputInterface::updateProperties();
    INDI::OutputInterface::updateProperties();

    if (isConnected())
    {
        defineProperty(SimulateInputsSP);
        SetTimer(getPollingPeriod());
    }
    else
    {
        deleteProperty(SimulateInputsSP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *SimulatorIO::getDefaultName()
{
    return "Simulator IO";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // Check Input Properties
    if (INDI::InputInterface::processText(dev, name, texts, names, n))
        return true;

    // Check Output Properties
    if (INDI::OutputInterface::processText(dev, name, texts, names, n))
        return true;

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Check if it's one of our simulation input controls

        if (SimulateInputsSP.isNameMatch(name))
        {
            SimulateInputsSP.update(states, names, n);
            SimulateInputsSP.setState(IPS_OK);
            SimulateInputsSP.apply();
            saveConfig(SimulateInputsSP);

            for (int i = 0; i < 4; i++)
                // Update the input state based on the simulation control
                m_InputStates[i] = (SimulateInputsSP[i].getState() == ISS_ON);

            // Force an update of the inputs
            UpdateDigitalInputs();

            return true;
        }
    }

    // Check Output Properties
    if (INDI::OutputInterface::processSwitch(dev, name, states, names, n))
        return true;

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool SimulatorIO::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (INDI::OutputInterface::processNumber(dev, name, values, names, n))
        return true;

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::saveConfigItems(FILE * fp)
{
    INDI::InputInterface::saveConfigItems(fp);
    INDI::OutputInterface::saveConfigItems(fp);

    // Save simulation control states
    SimulateInputsSP.save(fp);

    return INDI::DefaultDevice::saveConfigItems(fp);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimulatorIO::TimerHit()
{
    if (!isConnected())
        return;

    UpdateDigitalInputs();
    UpdateDigitalOutputs();

    SetTimer(getCurrentPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::UpdateDigitalInputs()
{
    for (size_t i = 0; i < DigitalInputsSP.size(); i++)
    {
        auto oldState = DigitalInputsSP[i].findOnSwitchIndex();
        auto newState = m_InputStates[i] ? 1 : 0;

        if (oldState != newState)
        {
            DigitalInputsSP[i].reset();
            DigitalInputsSP[i][newState].setState(ISS_ON);
            DigitalInputsSP[i].setState(IPS_OK);
            DigitalInputsSP[i].apply();
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::UpdateAnalogInputs()
{
    // We don't have analog inputs in this simulator
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::UpdateDigitalOutputs()
{
    for (size_t i = 0; i < DigitalOutputsSP.size(); i++)
    {
        auto oldState = DigitalOutputsSP[i].findOnSwitchIndex();
        auto newState = m_OutputStates[i] ? 1 : 0;

        if (oldState != newState)
        {
            DigitalOutputsSP[i].reset();
            DigitalOutputsSP[i][newState].setState(ISS_ON);
            DigitalOutputsSP[i].setState(IPS_OK);
            DigitalOutputsSP[i].apply();
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::CommandOutput(uint32_t index, OutputState command)
{
    if (index >= 4)
        return false;

    m_OutputStates[index] = (command == OutputState::On);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::Connect()
{
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SimulatorIO::Disconnect()
{
    return true;
}
