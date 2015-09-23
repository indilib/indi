#if 0
    LX200 Basic Driver
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200driver.h"
#include "lx200basic.h"
#include "indicom.h"

#define POLLMS 1000

/* Simulation Parameters */
#define	SLEWRATE	1		/* slew rate, degrees/s */
#define SIDRATE		0.004178	/* sidereal rate, degrees/s */


using namespace std;

/* Our telescope auto pointer */
auto_ptr<LX200Basic> telescope(0);

/**************************************************************************************
** Send client definitions of all properties.
***************************************************************************************/
void ISInit()
{
 static int isInit=0;

 if (isInit)
  return;

 if (telescope.get() == 0) telescope.reset(new LX200Basic());

 isInit = 1;
  
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
LX200Basic::LX200Basic()
{    
    setVersion(2, 0);

    DBG_SCOPE = INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE");

    currentRA=ln_get_apparent_sidereal_time(ln_get_julian_from_sys());
    currentDEC=90;   

    SetTelescopeCapability(TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT);

    DEBUG(INDI::Logger::DBG_DEBUG, "Initializing from LX200 Basic device...");

 }

/**************************************************************************************
**
***************************************************************************************/
LX200Basic::~LX200Basic()
{

}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::debugTriggered(bool enable)
{
   INDI_UNUSED(enable);
   setLX200Debug(getDeviceName(), DBG_SCOPE);
}

/**************************************************************************************
**
***************************************************************************************/
const char * LX200Basic::getDefaultName()
{
    return (char *)"LX200 Basic";
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    // Slew threshold
    IUFillNumber(&SlewAccuracyN[0], "SlewRA",  "RA (arcmin)", "%10.6m",  0., 60., 1., 3.0);
    IUFillNumber(&SlewAccuracyN[1], "SlewDEC", "Dec (arcmin)", "%10.6m", 0., 60., 1., 3.0);
    IUFillNumberVector(&SlewAccuracyNP, SlewAccuracyN, NARRAY(SlewAccuracyN), getDeviceName(), "Slew Accuracy", "", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    addAuxControls();

    return true;
}

/**************************************************************************************
** Define LX200 Basic properties to clients.
***************************************************************************************/
void LX200Basic::ISGetProperties(const char *dev)
{
    if(dev && strcmp(dev,getDeviceName()))
        return;

    INDI::Telescope::ISGetProperties(dev);

    if (isConnected())
        defineNumber(&SlewAccuracyNP);
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineNumber(&SlewAccuracyNP);

        // We don't support NSWE controls
        deleteProperty(MovementNSSP.name);
        deleteProperty(MovementWESP.name);

        getBasicData();
    }
    else
    {
        deleteProperty(SlewAccuracyNP.name);
    }

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::Connect()
{
    bool rc=false;

    if(isConnected()) return true;

    rc=Connect(PortT[0].text, atoi(IUFindOnSwitch(&BaudRateSP)->name));

    if(rc)
        SetTimer(POLLMS);

    return rc;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::Connect(const char *port, uint16_t baud)
{
    if (isSimulation())
    {
        DEBUGF (INDI::Logger::DBG_SESSION, "Simulated %s is online.", getDeviceName());
        return true;
    }

    if (tty_connect(port, baud, 8, 0, 1, &PortFD) != TTY_OK)
    {
      DEBUGF(INDI::Logger::DBG_ERROR, "Error connecting to port %s. Make sure you have BOTH write and read permission to your port.", port);
      return false;
    }


    if (check_lx200_connection(PortFD))
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error connecting to Telescope. Telescope is offline.");
        return false;
    }

   DEBUGF (INDI::Logger::DBG_SESSION, "%s is online. Retrieving basic data...", getDeviceName());

   return true;
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::Disconnect()
{
    if (isSimulation() == false)
        tty_disconnect(PortFD);
    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::isSlewComplete()
{
    const double dx = targetRA - currentRA;
    const double dy = targetDEC - currentDEC;
    return fabs(dx) <= (SlewAccuracyN[0].value/(900.0)) && fabs(dy) <= (SlewAccuracyN[1].value/60.0);
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::ReadScopeStatus()
{
    if (isConnected() == false)
     return false;

    if (isSimulation())
    {
        mountSim();
        return true;
    }

    if ( getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
      EqNP.s = IPS_ALERT;
      IDSetNumber(&EqNP, "Error reading RA/DEC.");
      return false;
    }

    if (TrackState == SCOPE_SLEWING)
    {
        // Check if LX200 is done slewing
        if (isSlewComplete())
        {
            TrackState=SCOPE_TRACKING;
            IDMessage(getDeviceName(),"Slew is complete. Tracking...");

        }
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::Goto(double r,double d)
{
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
         if (!isSimulation() && abortSlew(PortFD) < 0)
         {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
         }

         AbortSP.s = IPS_OK;
         EqNP.s       = IPS_IDLE;
         IDSetSwitch(&AbortSP, "Slew aborted.");
         IDSetNumber(&EqNP, NULL);

       // sleep for 100 mseconds
       usleep(100000);
    }

    if (isSimulation() == false)
    {
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC.");
            return false;
        }

        int err=0;
        /* Slew reads the '0', that is not the end of the slew */
        if (err = Slew(PortFD))
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            slewError(err);
            return  false;
        }
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s    = IPS_BUSY;

    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::Sync(double ra, double dec)
{
    char syncString[256];

    if (isSimulation() == false &&
            (setObjectRA(PortFD, ra) < 0 || (setObjectDEC(PortFD, dec)) < 0))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
        return false;
    }

    if (isSimulation() == false &&  ::Sync(PortFD, syncString) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP , "Synchronization failed.");
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    DEBUG(INDI::Logger::DBG_SESSION, "Synchronization successful.");

    TrackState = SCOPE_IDLE;
    EqNP.s    = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{	
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, SlewAccuracyNP.name))
        {
            if (IUUpdateNumber(&SlewAccuracyNP, values, names, n) < 0)
                return false;

            SlewAccuracyNP.s = IPS_OK;

            if (SlewAccuracyN[0].value < 3 || SlewAccuracyN[1].value < 3)
                IDSetNumber(&SlewAccuracyNP, "Warning: Setting the slew accuracy too low may result in a dead lock");

            IDSetNumber(&SlewAccuracyNP, NULL);
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Basic::Abort()
{
     if (isSimulation() == false && abortSlew(PortFD) < 0)
     {
         DEBUG(INDI::Logger::DBG_ERROR, "Failed to abort slew.");
         return false;
     }

     EqNP.s = IPS_IDLE;
     TrackState = SCOPE_IDLE;
     IDSetNumber(&EqNP, NULL);

     DEBUG(INDI::Logger::DBG_SESSION, "Slew aborted.");
     return true;
}



/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::getBasicData()
{

  // Make sure short
  checkLX200Format(PortFD);

  // Get current RA/DEC
  getLX200RA(PortFD, &currentRA);
  getLX200DEC(PortFD, &currentDEC);

  IDSetNumber (&EqNP, NULL);
}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::mountSim ()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, da, dx;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;
    da = SLEWRATE*dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {

    case SCOPE_TRACKING:
        /* RA moves at sidereal, Dec stands still */
        currentRA += (SIDRATE*dt/15.);
        break;

    case SCOPE_SLEWING:
        /* slewing - nail it when both within one pulse @ SLEWRATE */
        nlocked = 0;

        dx = targetRA - currentRA;

        if (fabs(dx) <= da)
        {
        currentRA = targetRA;
        nlocked++;
        }
        else if (dx > 0)
            currentRA += da/15.;
        else
            currentRA -= da/15.;

        dx = targetDEC - currentDEC;
        if (fabs(dx) <= da)
        {
        currentDEC = targetDEC;
        nlocked++;
        }
        else if (dx > 0)
          currentDEC += da;
        else
          currentDEC -= da;

        if (nlocked == 2)
        {
                TrackState = SCOPE_TRACKING;

        }

        break;

    default:
        break;
    }

    NewRaDec(currentRA, currentDEC);


}

/**************************************************************************************
**
***************************************************************************************/
void LX200Basic::slewError(int slewCode)
{
    EqNP.s = IPS_ALERT;

    if (slewCode == 1)
    IDSetNumber(&EqNP, "Object below horizon.");
    else if (slewCode == 2)
    IDSetNumber(&EqNP, "Object below the minimum elevation limit.");
    else
    IDSetNumber(&EqNP, "Slew failed.");

}
