/*
    Relay Interface
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

#include "indirelayinterface.h"
#include <cstring>
#include "indilogger.h"

namespace INDI
{

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
RelayInterface::RelayInterface(DefaultDevice *defaultDevice) : m_defaultDevice(defaultDevice)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
RelayInterface::~RelayInterface()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
void RelayInterface::initProperties(const char *groupName, uint8_t relays)
{
    RelayLabelsTP.reserve(relays);

    // Initialize labels
    for (auto i = 0; i < relays; i++)
    {
        auto name = "RELAY_" + std::to_string(i);
        auto label = "Relay #" + std::to_string(i);

        INDI::WidgetText oneLabel;
        oneLabel.fill(name, label, label);
        RelayLabelsTP.push(std::move(oneLabel));
    }

    RelayLabelsTP.fill(m_defaultDevice->getDeviceName(), "RELAY_LABELS", "Labels", groupName, IP_RW, 60, IPS_IDLE);
    RelayLabelsTP.shrink_to_fit();
    RelayLabelsTP.load();

    RelaysSP.reserve(relays);
    // Initialize switches, use labels if loaded.
    for (auto i = 0; i < relays; i++)
    {
        auto name = "RELAY_" + std::to_string(i);
        auto label = "Relay #" + std::to_string(i);

        INDI::PropertySwitch oneRelay {3};
        oneRelay[Open].fill("OPEN", "Open", ISS_OFF);
        oneRelay[Close].fill("CLOSE", "Close", ISS_OFF);
        oneRelay[Flip].fill("FLIP", "Flip", ISS_OFF);

        if (i < RelayLabelsTP.count())
            label = RelayLabelsTP[i].getText();
        oneRelay.fill(m_defaultDevice->getDeviceName(), name.c_str(), label.c_str(), groupName, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
        RelaysSP.push_back(std::move(oneRelay));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool RelayInterface::updateProperties()
{
    if (m_defaultDevice->isConnected())
    {
        for (auto &oneRelay : RelaysSP)
            m_defaultDevice->defineProperty(oneRelay);
        m_defaultDevice->defineProperty(RelayLabelsTP);
    }
    else
    {
        for (auto &oneRelay : RelaysSP)
            m_defaultDevice->deleteProperty(oneRelay);
        m_defaultDevice->deleteProperty(RelayLabelsTP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool RelayInterface::processSwitch(const char *dev, const char *name, ISState states[], char *names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()))
    {
        for (auto i = 0; i < RelaysSP.size(); i++)
        {
            if (RelaysSP[i].isNameMatch(name))
            {
                auto oldState = RelaysSP[i].findOnSwitchIndex();
                RelaysSP[i].update(states, names, n);
                auto newState = RelaysSP[i].findOnSwitchIndex();
                if (oldState != newState)
                {
                    // Cast to Command and send
                    if (CommandRelay(i, static_cast<Command>(newState)))
                    {
                        RelaysSP[i].setState(IPS_OK);
                    }
                    else
                    {
                        RelaysSP[i].setState(IPS_ALERT);
                        RelaysSP[i].reset();
                        RelaysSP[i][oldState].setState(ISS_ON);
                    }

                    // Apply and return
                    RelaysSP[i].apply();
                    return true;
                }
                // No state change
                else
                {
                    RelaysSP[i].setState(IPS_OK);
                    RelaysSP[i].apply();
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
bool RelayInterface::processText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && !strcmp(dev, m_defaultDevice->getDeviceName()))
    {
        // If this call due to config loading, let's delete existing dummy property and define the full one
        if (RelayLabelsTP.isNameMatch(name))
        {
            RelayLabelsTP.update(texts, names, n);
            RelayLabelsTP.setState(IPS_OK);
            RelayLabelsTP.apply();
            m_defaultDevice->saveConfig(RelayLabelsTP);
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////////////
bool RelayInterface::saveConfigItems(FILE *fp)
{
    RelayLabelsTP.save(fp);
    return true;
}

}