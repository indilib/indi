#if 0
    Simple Skeleton - Tutorial Four
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

/** \file simpleskeleton.cpp
    \brief Construct a basic INDI CCD device that demonstrates ability to define properties from a skeleton file.
    \author Jasem Mutlaq
*/

#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

/* Our driver header */
#include "simpleskeleton.h"

/* Our simpleSkeleton auto pointer */
std::auto_ptr<SimpleSkeleton> simpleSkeleton(0);

const int POLLMS = 1000;				// Period of update, 1 second.

/**************************************************************************************
** Send client definitions of all properties.
***************************************************************************************/
void ISInit()
{
 static int isInit=0;

 if (isInit)
  return;
 if (simpleSkeleton.get() == 0)
 {
     isInit = 1;
     simpleSkeleton.reset(new SimpleSkeleton());
 }
}

/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties (const char *dev)
{
 ISInit(); 
 simpleSkeleton->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 ISInit();
 simpleSkeleton->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 ISInit();
 simpleSkeleton->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 ISInit();
 simpleSkeleton->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    ISInit();

    simpleSkeleton->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}
/**************************************************************************************
**
***************************************************************************************/
void ISSnoopDevice (XMLEle *root) 
{
  INDI_UNUSED(root);
}

/**************************************************************************************
**
***************************************************************************************/
SimpleSkeleton::SimpleSkeleton()
{
}

/**************************************************************************************
**
***************************************************************************************/
SimpleSkeleton::~SimpleSkeleton()
{
}

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
    if (skel)
        buildSkeleton(skel);
    else if (stat(skelFileName,&st) == 0)
        buildSkeleton(skelFileName);
    else
        IDLog("No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.\n");

    // Optional: Add aux controls for configuration, debug & simulation that get added in the Options tab
    //           of the driver.
    addAuxControls();

    std::vector<INDI::Property *> *pAll = getProperties();

    // Let's print a list of all device properties
    for (int i=0; i < pAll->size(); i++)
        IDLog("Property #%d: %s\n", i, pAll->at(i)->getName());

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
** Process Text properties
***************************************************************************************/
bool SimpleSkeleton::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
	// Ignore if not ours 
        if (strcmp (dev, getDeviceName()))
            return false;

        return false;
}

/**************************************************************************************
**
***************************************************************************************/
bool SimpleSkeleton::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	
	// Ignore if not ours
        if (strcmp (dev, getDeviceName()))
            return false;

        INumberVectorProperty *nvp = getNumber(name);

        if (!nvp)
            return false;

        if (isConnected() == false)
        {
            nvp->s = IPS_ALERT;
            IDSetNumber(nvp, "Cannot change property while device is disconnected.");
            return false;
        }

        if (!strcmp(nvp->name, "Number Property"))
        {
            IUUpdateNumber(nvp, values, names, n);
            nvp->s = IPS_OK;
            IDSetNumber(nvp, NULL);

            return true;
        }

        return false;
}

/**************************************************************************************
**
***************************************************************************************/
bool SimpleSkeleton::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int lightState=0;
    int lightIndex=0;

	// ignore if not ours //
    if (strcmp (dev, getDeviceName()))
            return false;

        if (INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n) == true)
            return true;

        ISwitchVectorProperty *svp = getSwitch(name);
        ILightVectorProperty *lvp  = getLight("Light Property");

        if (isConnected() == false)
        {
            svp->s = IPS_ALERT;
            IDSetSwitch(svp, "Cannot change property while device is disconnected.");
            return false;
        }


        if (!svp || !lvp)
            return false;

        if (!strcmp(svp->name, "Menu"))
        {
            IUUpdateSwitch(svp, states, names, n);
            ISwitch *onSW = IUFindOnSwitch(svp);
            lightIndex = IUFindOnSwitchIndex(svp);

            if (lightIndex < 0 || lightIndex > lvp->nlp)
                return false;

            if (onSW)
            {
                lightState = rand() % 4;
                svp->s = IPS_OK;
                lvp->s = IPS_OK;
                lvp->lp[lightIndex].s = (IPState) lightState;

                IDSetSwitch(svp, "Setting to switch %s is successful. Changing corresponding light property to %s.", onSW->name, pstateStr(lvp->lp[lightIndex].s));
                IDSetLight(lvp, NULL);


            }
            return true;
        }

    return false;


}

bool SimpleSkeleton::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if (strcmp (dev, getDeviceName()))
        return false;

    IBLOBVectorProperty *bvp = getBLOB(name);

    if (!bvp)
        return false;

    if (isConnected() == false)
    {
        bvp->s = IPS_ALERT;
        IDSetBLOB(bvp, "Cannot change property while device is disconnected.");
        return false;
    }

    if (!strcmp(bvp->name, "BLOB Test"))
    {

        IUUpdateBLOB(bvp, sizes, blobsizes, blobs, formats, names, n);

        IBLOB *bp = IUFindBLOB(bvp, names[0]);

        if (!bp)
            return false;

        IDLog("Received BLOB with name %s, format %s, and size %d, and bloblen %d\n", bp->name, bp->format, bp->size, bp->bloblen);

        char *blobBuffer = new char[bp->bloblen+1];
        strncpy(blobBuffer, ((char *) bp->blob), bp->bloblen);
        blobBuffer[bp->bloblen] = '\0';

        IDLog("BLOB Content:\n##################################\n%s\n##################################\n", blobBuffer);

        delete [] blobBuffer;

        bp->size=0;
        bvp->s = IPS_OK;
        IDSetBLOB(bvp, NULL);

    }

    return true;

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

const char * SimpleSkeleton::getDefaultName()
{
    return "Simple Skeleton";
}
