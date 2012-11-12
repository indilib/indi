/*
   INDI Developers Manual
   Tutorial #2

   "Simple Telescope Driver"

   We develop a simple telescope simulator.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simplescope.cpp
    \brief Construct a basic INDI telescope device that simulates GOTO commands.
    \author Jasem Mutlaq
*/

#include <sys/time.h>
#include <math.h>
#include <memory>

#include "simplescope.h"
#include "indicom.h"

const float SIDRATE  = 0.004178;			/* sidereal rate, degrees/s */
const int   SLEW_RATE = 	1;  				/* slew rate, degrees/s */
const int   POLLMS	 =	250;    			/* poll period, ms */

std::auto_ptr<SimpleScope> simpleScope(0);

/**************************************************************************************
** Initilize SimpleScope object
***************************************************************************************/
void ISInit()
{
 static int isInit=0;
 if (isInit)
  return;
 if (simpleScope.get() == 0)
 {
     isInit = 1;
     simpleScope.reset(new SimpleScope());
 }
}

/**************************************************************************************
** Return properties of device.
***************************************************************************************/
void ISGetProperties (const char *dev)
{
 ISInit();
 simpleScope->ISGetProperties(dev);
}

/**************************************************************************************
** Process new switch from client
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
 ISInit();
 simpleScope->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Process new text from client
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
 ISInit();
 simpleScope->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Process new number from client
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
 ISInit();
 simpleScope->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Process new blob from client
***************************************************************************************/
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    ISInit();
    simpleScope->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
** Process snooped property from another driver
***************************************************************************************/
void ISSnoopDevice (XMLEle *root)
{
  INDI_UNUSED(root);
}

SimpleScope::SimpleScope()
{
    currentRA  = 15;
    currentDEC = 15;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool SimpleScope::Connect()
{
    IDMessage(getDeviceName(), "Simple Scope connected successfully!");

    // Let's set a timer that checks telescopes status every POLLMS milliseconds.
    SetTimer(POLLMS);

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool SimpleScope::Disconnect()
{
    IDMessage(getDeviceName(), "Simple Scope disconnected successfully!");
    return true;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char * SimpleScope::getDefaultName()
{
    return "Simple Scope";
}

/**************************************************************************************
** Client is asking us to slew to a new position
***************************************************************************************/
bool SimpleScope::Goto(double ra, double dec)
{
    targetRA=ra;
    targetDEC=dec;
    char RAStr[64], DecStr[64];

    // Parse the RA/DEC into strings
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // Mark state as slewing
    TrackState = SCOPE_SLEWING;

    // Inform client we are slewing to a new position
    IDMessage(getDeviceName(), "Slewing to RA: %s - DEC: %s", RAStr, DecStr);

    // Success!
    return true;
}

/**************************************************************************************
** Client is asking us to abort our motion
***************************************************************************************/
bool SimpleScope::Abort()
{

    TrackState = SCOPE_IDLE;

    IDMessage(getDeviceName(), "Simple Scope stopped.");

    return true;

}
/**************************************************************************************
** Client is asking us to report telescope status
***************************************************************************************/
bool SimpleScope::ReadScopeStatus()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt=0, da_ra=0, da_dec=0, dx=0, dy=0;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday (&tv, NULL);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec)/1e6;
    ltv = tv;

    // Calculate how much we moved since last time
    da_ra = SLEW_RATE *dt;
    da_dec = SLEW_RATE *dt;

    /* Process per current state. We check the state of EQUATORIAL_EOD_COORDS_REQUEST and act acoordingly */
    switch (TrackState)
    {
    case SCOPE_SLEWING:
        // Wait until we are "locked" into positon for both RA & DEC axis
        nlocked = 0;

        // Calculate diff in RA
        dx = targetRA - currentRA;

        // If diff is very small, i.e. smaller than how much we changed since last time, then we reached target RA.
        if (fabs(dx)*15. <= da_ra)
        {
            currentRA = targetRA;
            nlocked++;
        }
        // Otherwise, increase RA
        else if (dx > 0)
            currentRA += da_ra/15.;
        // Otherwise, decrease RA
        else
            currentRA -= da_ra/15.;

        // Calculate diff in DEC
        dy = targetDEC - currentDEC;

        // If diff is very small, i.e. smaller than how much we changed since last time, then we reached target DEC.
        if (fabs(dy) <= da_dec)
        {
            currentDEC = targetDEC;
            nlocked++;
        }
        // Otherwise, increase DEC
        else if (dy > 0)
          currentDEC += da_dec;
        // Otherwise, decrease DEC
        else
          currentDEC -= da_dec;

        // Let's check if we recahed position for both RA/DEC
        if (nlocked == 2)
        {
            // Let's set state to TRACKING
            TrackState = SCOPE_TRACKING;

            IDMessage(getDeviceName(), "Telescope slew is complete. Tracking...");
        }
        break;

    default:
        break;
    }

    NewRaDec(currentRA, currentDEC);
    return true;
}

