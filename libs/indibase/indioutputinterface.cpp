/*
    Output Interface
    Copyright (C) 2024 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indioutputinterface.h"
#include <cstring>
#include "indilogger.h"

namespace INDI
{

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
OutputInterface::OutputInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
OutputInterface::~OutputInterface()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
void OutputInterface::initProperties(const char *groupName, uint8_t Outputs, const std::string &prefix)
{
    OutputLabelsTP.reserve(Outputs);

    // Initialize labels
    for (auto i = 0; i < Outputs; i++)
    {
        auto name = "OUTPUT_" + std::to_string(i + 1);
        auto label = prefix + " #" + std::to_string(i + 1);

        INDI::WidgetText oneLabel;
        oneLabel.fill(name, label, label);
        OutputLabelsTP.push(std::move(oneLabel));
    }

    OutputLabelsTP.fill(m_defaultDevice->getDeviceName(), "OUTPUT_LABELS", "Labels", groupName, IP_RW, 60, IPS_IDLE);
    OutputLabelsTP.shrink_to_fit();
    OutputLabelsTP.load();

    OutputsSP.reserve(Outputs);
    // Initialize switches, use labels if loaded.
    for (size_t i = 0; i < Outputs; i++)
    {
        auto name = "OUTPUT_" + std::to_string(i + 1);
        auto label = prefix + " #" + std::to_string(i + 1);

        INDI::PropertySwitch oneOutput {3};
        oneOutput[Open].fill("OPEN", "Open", ISS_OFF);
        oneOutput[Close].fill("CLOSE", "Close", ISS_OFF);
        oneOutput[Flip].fill("FLIP", "Flip", ISS_OFF);

        if (i < OutputLabelsTP.count())
            label = OutputLabelsTP[i].getText();
        oneOutput.fill(m_defaultDevice->getDeviceName(), name.c_str(), label.c_str(), groupName, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
        OutputsSP.push_back(std::move(oneOutput));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool OutputInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        for (auto &oneOutput : OutputsSP)
            m_defaultDevice->defineProperty(oneOutput);
        m_defaultDevice->defineProperty(OutputLabelsTP);
    }
    else
    {
        for (auto &oneOutput : OutputsSP)
            m_defaultDevice->deleteProperty(oneOutput);
        m_defaultDevice->deleteProperty(OutputLabelsTP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool OutputInterface::processSwitch(const char *dev, const char *name, ISState states[], char *names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()))
    {
        for (size_t i = 0; i < OutputsSP.size(); i++)
        {
            if (OutputsSP[i].isNameMatch(name))
            {
                auto oldState = OutputsSP[i].findOnSwitchIndex();
                OutputsSP[i].update(states, names, n);
                auto newState = OutputsSP[i].findOnSwitchIndex();
                if (oldState != newState)
                {
                    // Cast to Command and send
                    if (CommandOutput(i, static_cast<Command>(newState)))
                    {
                        OutputsSP[i].setState(IPS_OK);
                    }
                    else
                    {
                        OutputsSP[i].setState(IPS_ALERT);
                        OutputsSP[i].reset();
                        OutputsSP[i][oldState].setState(ISS_ON);
                    }

                    // Apply and return
                    OutputsSP[i].apply();
                    return true;
                }
                // No state change
                else
                {
                    OutputsSP[i].setState(IPS_OK);
                    OutputsSP[i].apply();
                    return true;
                }
            }

        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool OutputInterface::processText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()))
    {
        // If this call due to config loading, let's delete existing dummy property and define the full one
        if (OutputLabelsTP.isNameMatch(name))
        {
            OutputLabelsTP.update(texts, names, n);
            OutputLabelsTP.setState(IPS_OK);
            OutputLabelsTP.apply();
            m_defaultDevice->saveConfig(OutputLabelsTP);
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool OutputInterface::saveConfigItems(FILE *fp)
{
    OutputLabelsTP.save(fp);
    return true;
}

}