#if 0
Simple Skeleton - Tutorial Four
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

/** \file simpleskeleton.cpp
    \brief Construct a basic INDI CCD device that demonstrates ability to define properties from a skeleton file.
    \author Jasem Mutlaq

    \example simpleskeleton.cpp
    A skeleton file is an external XML file with the driver properties already defined. This tutorial illustrates how to create a driver from
    a skeleton file and parse/process the properties. The skeleton file name is tutorial_four_sk.xml
    \note Please note that if you create your own skeleton file, you must append _sk postfix to your skeleton file name.
*/

#include "simpleskeleton.h"
#include <indipropertyswitch.h>
#include <indipropertynumber.h>
#include <indipropertyblob.h>

#include <cstdlib>
#include <cstring>
#include <memory>

#include <sys/stat.h>

/* Our simpleSkeleton auto pointer */
std::unique_ptr<SimpleSkeleton> simpleSkeleton(new SimpleSkeleton());

//const int POLLMS = 1000; // Period of update, 1 second.

/**************************************************************************************
** Initialize all properties & set default values.
**************************************************************************************/
bool SimpleSkeleton::initProperties()
{
    DefaultDevice::initProperties();

    // This is the default driver skeleton file location
    // Convention is: drivername_sk_xml
    // Default location is /usr/share/indi
    const char *skelFileName = "/usr/share/indi/tutorial_four_sk.xml";
    struct stat st;

    char *skel = getenv("INDISKEL");

    if (skel != nullptr)
        buildSkeleton(skel);
    else if (stat(skelFileName, &st) == 0)
        buildSkeleton(skelFileName);
    else
        IDLog(
            "No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.\n");

    // Optional: Add aux controls for configuration, debug & simulation that get added in the Options tab
    //           of the driver.
    addAuxControls();

    // Let's print a list of all device properties
    int i = 0;
    for(const auto &oneProperty : getProperties())
        IDLog("Property #%d: %s\n", i++, oneProperty.getName());

    // Set the green light (IPS_OK) for a "Number Property" if changed
    INDI::PropertyNumber number = getNumber("Number Property");
    number.onUpdate([number, this]() mutable
    {
        if (!isConnected())
        {
            number.setState(IPS_ALERT);
            number.apply("Cannot change property while device is disconnected.");
            return;
        }
        number.setState(IPS_OK);
        number.apply();
    });

    // Set random light state for selected switch index
    INDI::PropertySwitch menu = getSwitch("Menu");
    menu.onUpdate([menu, this]() mutable
    {
        if (!isConnected())
        {
            menu.setState(IPS_ALERT);
            menu.apply("Cannot change property while device is disconnected.");
            return;
        }
        auto index = menu.findOnSwitchIndex();
        if (index < 0)
            return;

        menu.setState(IPS_OK);

        INDI::PropertyLight light = getLight("Light Property");
        light[index].setState(static_cast<IPState>(rand() % 4));
        light.setState(IPS_OK);
        light.apply();
    });

    // Show blob if changed
    INDI::PropertyBlob blob = getBLOB("BLOB Test");
    blob.onUpdate([blob, this]() mutable
    {
        if (!isConnected())
        {
            blob.setState(IPS_ALERT);
            blob.apply("Cannot change property while device is disconnected.");
            return;
        }
        IDLog("Received BLOB with name %s, format %s, and size %d, and bloblen %d\n",
              blob[0].getName(), blob[0].getFormat(), blob[0].getSize(), blob[0].getBlobLen());

        IDLog("BLOB Content:\n"
              "##################################\n"
              "%s\n"
              "##################################\n",
              blob[0].getBlobAsString().c_str());

        blob[0].setSize(0);
        blob.setState(IPS_OK);
        blob.apply();
    });

    return true;
}

/**************************************************************************************
** Define Basic properties to clients.
***************************************************************************************/
void SimpleSkeleton::ISGetProperties(const char *dev)
{
    static int configLoaded = 0;

    // Ask the default driver first to send properties.
    INDI::DefaultDevice::ISGetProperties(dev);

    // If no configuration is load before, then load it now.
    if (configLoaded == 0)
    {
        loadConfig();
        configLoaded = 1;
    }
}

/**************************************************************************************
**
***************************************************************************************/
bool SimpleSkeleton::Connect()
{
    return true;
}

bool SimpleSkeleton::Disconnect()
{
    return true;
}

const char *SimpleSkeleton::getDefaultName()
{
    return "Simple Skeleton";
}
