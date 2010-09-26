#if 0
    MyScope - Tutorial Four
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <memory>

#include <config.h>

#include "indibase/baseclient.h"

/* INDI Common Library Routines */
#include "indicom.h"

/* Our driver header */
#include "tutorial_four.h"

using namespace std;

/* Our telescope auto pointer */
auto_ptr<MyScope> telescope(0);

const int POLLMS = 1000;				// Period of update, 1 second.

/**************************************************************************************
** Send client definitions of all properties.
***************************************************************************************/
void ISInit()
{
 static int isInit=0;

 if (isInit)
  return;

 if (telescope.get() == 0)
 {
     isInit = 1;
     telescope.reset(new MyScope());
 }
 
}

/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties (const char *dev)
{
 ISInit(); 
 telescope->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 ISInit();
 telescope->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 ISInit();
 telescope->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 ISInit();
 telescope->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISSnoopDevice (XMLEle *root) 
{
  INDI_UNUSED(root);
}

/**************************************************************************************
** LX200 Basic constructor
***************************************************************************************/
MyScope::MyScope()
{
    IDLog("Initilizing from My Scope device...\n");

    myClient = new INDI::BaseClient();

    init_properties();

 }

/**************************************************************************************
**
***************************************************************************************/
MyScope::~MyScope()
{

}

/**************************************************************************************
** Initialize all properties & set default values.
***************************************************************************************/
void MyScope::init_properties()
{

    char *skel = getenv("INDISKEL");
    if (skel)
        buildSkeleton(skel);
    else
        IDLog("No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.\n");

    myClient->connect();

}

/**************************************************************************************
** Define LX200 Basic properties to clients.
***************************************************************************************/
void MyScope::ISGetProperties(const char *dev)
{
    static int configLoaded = 0;

    INDI::BaseDevice::ISGetProperties(dev);

    if (configLoaded == 0)
    {
      loadConfig(true);
      configLoaded = 1;
    }
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
void MyScope::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Ignore if not ours 
        if (strcmp (dev, deviceID))
	    return;
}

/**************************************************************************************
**
***************************************************************************************/
void MyScope::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	// Ignore if not ours
        if (strcmp (dev, deviceID))
	    return;

        INumberVectorProperty *nvp = getNumber(name);

        if (!nvp)
            return;

        if (!strcmp(nvp->name, "Tracking Accuracy"))
        {
            IUUpdateNumber(nvp, values, names, n);
            nvp->s = IPS_OK;
            IDSetNumber(nvp, NULL);

            return;
        }

}

/**************************************************************************************
**
***************************************************************************************/
void MyScope::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	// ignore if not ours //
        if (strcmp (dev, deviceID))
	    return;

        ISwitchVectorProperty *svp = getSwitch(name);

        if (!svp)
            return;

        if (!strcmp(svp->name, "CONNECTION"))
        {
            IUUpdateSwitch(svp, states, names, n);
            connect_telescope();
            return;
        }

        INDI::BaseDevice::ISNewSwitch(dev, name, states, names, n);
}


/**************************************************************************************
**
***************************************************************************************/
void MyScope::connect_telescope()
{
    ISwitchVectorProperty *svp = getSwitch("CONNECTION");

    ISwitch *sp = IUFindOnSwitch(svp);

    if (!sp)
        return;

    if (!strcmp(sp->name, "CONNECT"))
    {
        setConnected(true);
        IDSetSwitch(svp, "Connecting.... telescope is online.");
        IDLog("Connecting.... telescope is online.\n");
    }
    else
    {
        setConnected(false);
        IDSetSwitch(svp, "Disconnecting... telescope is offline.");
        IDLog("Disconnecting... telescope is offline.\n");
    }
}


