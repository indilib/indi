/*
    Tutorial Client
    Copyright (C) 2010 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

/** \file tutorial_client.h
    \brief Construct a basic INDI client that demonstrates INDI::Client capabilities. This client must be used with tutorial_three device "Simple CCD".
    \author Jasem Mutlaq

    \example tutorial_client.h
    Construct a basic INDI client that demonstrates INDI::Client capabilities. This client must be used with tutorial_three device "Simple CCD".
    To run the example, you must first run tutorial_three:
    \code indiserver tutorial_three \endcode
    Then in another terminal, run the client:
    \code tutorial_client \endcode
    The client will connect to the CCD driver and attempts to change the CCD temperature.

*/

#include "baseclient.h"

class MyClient : public INDI::BaseClient
{
public:
    MyClient();
    ~MyClient() = default;

    void setTemperature();
    void takeExposure();

protected:
    void newDevice(INDI::BaseDevice *dp) override;
    void removeDevice(INDI::BaseDevice */*dp*/) override {}
    void newProperty(INDI::Property *property) override;
    void removeProperty(INDI::Property */*property*/) override {}
    void newBLOB(IBLOB *bp) override;
    void newSwitch(ISwitchVectorProperty */*svp*/) override {}
    void newNumber(INumberVectorProperty *nvp) override;
    void newMessage(INDI::BaseDevice *dp, int messageID) override;
    void newText(ITextVectorProperty */*tvp*/) override {}
    void newLight(ILightVectorProperty */*lvp*/) override {}
    void serverConnected() override {}
    void serverDisconnected(int /*exit_code*/) override {}

private:
    INDI::BaseDevice *ccd_simulator;
};
