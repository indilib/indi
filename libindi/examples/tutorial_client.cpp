#if 0
    Simple Client Tutorial
    Demonstration of libindi v0.7 capabilities.

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

#endif


#include <string>
#include <iostream>

#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>

#include <config.h>

#include "indibase/baseclient.h"
#include "indibase/basedevice.h"
#include "indibase/indiproperty.h"

/* INDI Common Library Routines */
#include "indicom.h"

#include "tutorial_client.h"

using namespace std;

#define MYCCD "CCD Simulator"

/* Our client auto pointer */
auto_ptr<MyClient> camera_client(0);

int main(int argc, char *argv[])
{

    if (camera_client.get() == 0)
        camera_client.reset(new MyClient());


  camera_client->setServer("localhost", 7624);

  camera_client->watchDevice(MYCCD);

  camera_client->connectServer();

  cout << "Press any key to terminate the client.\n";
  string term;
  cin >> term;


}


/**************************************************************************************
**
***************************************************************************************/
MyClient::MyClient()
{
    ccd_simulator = NULL;
}

/**************************************************************************************
**
***************************************************************************************/
MyClient::~MyClient()
{

}

/**************************************************************************************
**
***************************************************************************************/
void MyClient::setTemperature()
{
   INumberVectorProperty *ccd_temperature = NULL;

   ccd_temperature = ccd_simulator->getNumber("CCD_TEMPERATURE");

   if (ccd_temperature == NULL)
   {
       IDLog("Error: unable to find CCD Simulator CCD_TEMPERATURE property...\n");
       return;
   }

   ccd_temperature->np[0].value = -20;
   sendNewNumber(ccd_temperature);
}

/**************************************************************************************
**
***************************************************************************************/
void MyClient::newDevice(INDI::BaseDevice *dp)
{
    if (!strcmp(dp->getDeviceName(), "CCD Simulator"))
       IDLog("Receiving CCD Simulator Device...\n");

    ccd_simulator = dp;

}

/**************************************************************************************
**
*************************************************************************************/
void MyClient::newProperty(INDI::Property *property)
{

     if (!strcmp(property->getDeviceName(), "CCD Simulator") && !strcmp(property->getName(), "CCD_TEMPERATURE"))
     {
             IDLog("CCD_TEMPERATURE standard property defined. Attempting connection to CCD...\n");
             connectDevice(MYCCD);
     }
}

/**************************************************************************************
**
***************************************************************************************/
void MyClient::newSwitch(ISwitchVectorProperty *svp)
{
    // Let's check if we're ON
    if (!strcmp(svp->name, "CONNECTION"))
    {
        if (svp->sp[0].s == ISS_ON)
        {
            IDLog("CCD is connected. Setting temperature to -20 C.\n");
            setTemperature();
        }
    }
}

/**************************************************************************************
**
***************************************************************************************/
void MyClient::newNumber(INumberVectorProperty *nvp)
{
    // Let's check if we get any new values for CCD_TEMPERATURE
    if (!strcmp(nvp->name, "CCD_TEMPERATURE"))
    {
       IDLog("Receving new CCD Temperature: %g C\n", nvp->np[0].value);

       if (nvp->np[0].value == -20)
           IDLog("CCD temperature reached desired value!\n");
   }
}

/**************************************************************************************
**
***************************************************************************************/
void MyClient::newMessage(INDI::BaseDevice *dp)
{
     if (strcmp(dp->getDeviceName(), "CCD Simulator"))
         return;

     IDLog("Recveing message from Server:\n\n########################\n%s\n########################\n\n", dp->lastMessage());

}

