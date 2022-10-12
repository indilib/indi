#if 0
Simple Client Tutorial
Demonstration of libindi v0.7 capabilities.

Copyright (C) 2010 Jasem Mutlaq (mutlaqja@ikarustech.com)

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

/** \file tutorial_client.cpp
    \brief Construct a basic INDI client that demonstrates INDI::Client capabilities. This client must be used with tutorial_three device "Simple CCD".
    \author Jasem Mutlaq

    \example tutorial_client.cpp
    Construct a basic INDI client that demonstrates INDI::Client capabilities. This client must be used with tutorial_three device "Simple CCD".
    To run the example, you must first run tutorial_three:
    \code indiserver tutorial_three \endcode
    Then in another terminal, run the client:
    \code tutorial_client \endcode
    The client will connect to the CCD driver and attempts to change the CCD temperature.
*/

#include "tutorial_client.h"

#include <basedevice.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

static const char MYCCD[] = "Simple CCD";

int main(int, char *[])
{
    MyClient myClient;
    myClient.setServer("localhost", 7624);

    myClient.connectServer();

    myClient.setBLOBMode(B_ALSO, MYCCD, nullptr);

    myClient.enableDirectBlobAccess(MYCCD, nullptr);

    std::cout << "Press Enter key to terminate the client.\n";
    std::cin.ignore();
}

/**************************************************************************************
**
***************************************************************************************/
MyClient::MyClient()
{
    // wait for the availability of the device
    watchDevice(MYCCD, [this](INDI::BaseDevice device)
    {
        mCcdSimulator = device; // save device
        // wait for the availability of the "CONNECTION" property
        device.watchProperty("CONNECTION", [this](INDI::Property)
        {
            IDLog("Connecting to INDI Driver...\n");
            connectDevice(MYCCD);
        });

        // wait for the availability of the "CCD_TEMPERATURE" property
        device.watchProperty("CCD_TEMPERATURE", [this](INDI::PropertyNumber property)
        {
            if (mCcdSimulator.isConnected())
            {
                IDLog("CCD is connected.\n");
                setTemperature(-20);
            }

            // call a handler function if property changes
            property.onUpdate([property, this]()
            {
                IDLog("Receving new CCD Temperature: %g C\n", property[0].getValue());
                if (property[0].getValue() == -20)
                {
                    IDLog("CCD temperature reached desired value!\n");
                    takeExposure(1);
                }
            });
        });

        // wait for the availability of the "CCD1" property
        device.watchProperty("CCD1", [](INDI::PropertyBlob property)
        {
            // call a handler function if property changes
            property.onUpdate([property]()
            {
                // Save FITS file to disk
                std::ofstream myfile;

                myfile.open("ccd_simulator.fits", std::ios::out | std::ios::binary);

                myfile.write(static_cast<char *>(property[0].getBlob()), property[0].getBlobLen());

                myfile.close();

                IDLog("Received image, saved as mCcdSimulator.fits\n");
            });
        });
    });
}

/**************************************************************************************
**
***************************************************************************************/
void MyClient::setTemperature(double value)
{
    INDI::PropertyNumber ccdTemperature = mCcdSimulator.getProperty("CCD_TEMPERATURE");

    if (!ccdTemperature.isValid())
    {
        IDLog("Error: unable to find CCD Simulator CCD_TEMPERATURE property...\n");
        return;
    }

    IDLog("Setting temperature to %g C.\n", value);
    ccdTemperature[0].setValue(value);
    sendNewProperty(ccdTemperature);
}

/**************************************************************************************
**
***************************************************************************************/
void MyClient::takeExposure(double seconds)
{
    INDI::PropertyNumber ccdExposure = mCcdSimulator.getProperty("CCD_EXPOSURE");

    if (!ccdExposure.isValid())
    {
        IDLog("Error: unable to find CCD Simulator CCD_EXPOSURE property...\n");
        return;
    }

    // Take a 1 second exposure
    IDLog("Taking a %g second exposure.\n", seconds);
    ccdExposure[0].setValue(seconds);
    sendNewProperty(ccdExposure);
}

/**************************************************************************************
**
***************************************************************************************/
void MyClient::newMessage(INDI::BaseDevice *dp, int messageID)
{
    if (!dp->isDeviceNameMatch(MYCCD))
        return;

    IDLog("Recveing message from Server:\n"
          "\n"
          "########################\n"
          "%s\n"
          "########################\n"
          "\n",
          dp->messageQueue(messageID).c_str());
}
