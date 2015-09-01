/*******************************************************************************
 INDI Dome Base Class
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

 The code used calculate dome target AZ and ZD is written by Ferran Casarramona, and adapted from code from Markus Wildi.
 The transformations are based on the paper Matrix Method for Coodinates Transformation written by Toshimi Taki (http://www.asahi-net.or.jp/~zs3t-tk).

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "indidome.h"

#include <math.h>
#include <string.h>

#define DOME_MEASUREMENTS_TAB   "Measurements"
#define DOME_COORD_THRESHOLD    0.1             /* Only send debug messages if the differences between old and new values of Az/Alt excceds this value */

INDI::Dome::Dome()
{
    controller = new INDI::Controller(this);

    controller->setButtonCallback(buttonHelper);

    prev_az = prev_alt = prev_ra = prev_dec = 0;
    mountEquatorialCoords.dec=mountEquatorialCoords.ra=-1;
    mountState = IPS_ALERT;
}

INDI::Dome::~Dome()
{
}

bool INDI::Dome::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

    initDomeProperties(getDeviceName(),  MAIN_CONTROL_TAB);

    /* Port */
    IUFillText(&PortT[0], "PORT", "Port", "/dev/ttyUSB0");
    IUFillTextVector(&PortTP, PortT, 1, getDeviceName(), "DEVICE_PORT", "Ports", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Presets
    IUFillNumber(&PresetN[0], "Preset 1", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumber(&PresetN[1], "Preset 2", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumber(&PresetN[2], "Preset 3", "", "%6.2f", 0, 360.0, 1.0, 0);
    IUFillNumberVector(&PresetNP, PresetN, 3, getDeviceName(), "Presets", "", "Presets", IP_RW, 0, IPS_IDLE);

    //Preset GOTO
    IUFillSwitch(&PresetGotoS[0], "Preset 1", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[1], "Preset 2", "", ISS_OFF);
    IUFillSwitch(&PresetGotoS[2], "Preset 3", "", ISS_OFF);
    IUFillSwitchVector(&PresetGotoSP, PresetGotoS, 3, getDeviceName(), "Goto", "", "Presets", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Active Devices
    IUFillText(&ActiveDeviceT[0],"ACTIVE_TELESCOPE","Telescope","Telescope Simulator");
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,1,getDeviceName(),"ACTIVE_DEVICES","Snoop devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    // Measurements
    IUFillNumber(&DomeMeasurementsN[DM_DOME_RADIUS],"DM_DOME_RADIUS","Radius (m)","%6.2f",0.0,50.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_SHUTTER_WIDTH],"DM_SHUTTER_WIDTH","Shutter width (m)","%6.2f",0.0,10.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_NORTH_DISPLACEMENT],"DM_NORTH_DISPLACEMENT","N displacement (m)","%6.2f",-10.0,10.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_EAST_DISPLACEMENT],"DM_EAST_DISPLACEMENT","E displacement (m)","%6.2f",-10.0,10.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_UP_DISPLACEMENT],"DM_UP_DISPLACEMENT","Up displacement (m)","%6.2f",-10,10.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_OTA_OFFSET],"DM_OTA_OFFSET","OTA offset (m)","%6.2f",-10.0,10.0,1.0,0.0);
    IUFillNumberVector(&DomeMeasurementsNP,DomeMeasurementsN,6,getDeviceName(),"DOME_MEASUREMENTS","Measurements",DOME_MEASUREMENTS_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&DomeAutoSyncS[0],"DOME_AUTOSYNC_ENABLE","Enable",ISS_OFF);
    IUFillSwitch(&DomeAutoSyncS[1],"DOME_AUTOSYNC_DISABLE","Disable",ISS_ON);
    IUFillSwitchVector(&DomeAutoSyncSP,DomeAutoSyncS,2,getDeviceName(),"DOME_AUTOSYNC","Slaving",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_OK);

    addDebugControl();

    controller->mapController("Dome CW", "Dome CW", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("Dome CCW", "Dome CCW", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_2");

    controller->initProperties();

    IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");

    setInterfaceDescriptor(DOME_INTERFACE);

    return true;
}

void INDI::Dome::ISGetProperties (const char *dev)
{
    //  First we let our parent populate
    DefaultDevice::ISGetProperties(dev);

    defineText(&PortTP);

    controller->ISGetProperties(dev);
    return;
}

bool INDI::Dome::updateProperties()
{
    if(isConnected())
    {
        defineText(&ActiveDeviceTP);

        if (capability.hasShutter)
            defineSwitch(&DomeShutterSP);

        //  Now we add our Dome specific stuff
        defineSwitch(&DomeMotionSP);

        if (capability.hasVariableSpeed)
        {
            defineNumber(&DomeSpeedNP);
            defineNumber(&DomeTimerNP);
        }
        if (capability.canRelMove)
            defineNumber(&DomeRelPosNP);
        if (capability.canAbsMove)
            defineNumber(&DomeAbsPosNP);
        if (capability.canAbort)
            defineSwitch(&AbortSP);
        if (capability.canAbsMove)
        {
            defineNumber(&PresetNP);
            defineSwitch(&PresetGotoSP);
            defineNumber(&DomeParamNP);
            defineSwitch(&DomeGotoSP);
            defineSwitch(&DomeAutoSyncSP);
        }

        if (capability.canPark)
        {
            defineSwitch(&ParkSP);
            if (parkDataType != PARK_NONE)
            {
                defineNumber(&ParkPositionNP);
                defineSwitch(&ParkOptionSP);
            }
        }

        defineNumber(&DomeMeasurementsNP);
    } else
    {
        deleteProperty(ActiveDeviceTP.name);

        if (capability.hasShutter)
            deleteProperty(DomeShutterSP.name);

        deleteProperty(DomeMotionSP.name);
        if (capability.hasVariableSpeed)
        {
            deleteProperty(DomeSpeedNP.name);
            deleteProperty(DomeTimerNP.name);
        }
        if (capability.canRelMove)
            deleteProperty(DomeRelPosNP.name);
        if (capability.canAbsMove)
            deleteProperty(DomeAbsPosNP.name);
        if (capability.canAbort)
            deleteProperty(AbortSP.name);
        if (capability.canAbsMove)
        {
            deleteProperty(PresetNP.name);
            deleteProperty(PresetGotoSP.name);
            deleteProperty(DomeParamNP.name);
            deleteProperty(DomeGotoSP.name);
            deleteProperty(DomeAutoSyncSP.name);
        }

        if (capability.canPark)
        {
            deleteProperty(ParkSP.name);
            if (parkDataType != PARK_NONE)
            {
                deleteProperty(ParkPositionNP.name);
                deleteProperty(ParkOptionSP.name);
            }
        }

        deleteProperty(DomeMeasurementsNP.name);
    }

    controller->updateProperties();
    return true;
}


bool INDI::Dome::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, PresetNP.name))
        {
            IUUpdateNumber(&PresetNP, values, names, n);
            PresetNP.s = IPS_OK;
            IDSetNumber(&PresetNP, NULL);

            return true;
        }

        if (!strcmp(name, DomeMeasurementsNP.name))
        {
            IUUpdateNumber(&DomeMeasurementsNP,values, names,n);
            DomeMeasurementsNP.s = IPS_OK;
            IDSetNumber(&DomeMeasurementsNP, NULL);

            return true;
        }

        if (strstr(name, "DOME_"))
            return processDomeNumber(dev, name, values, names, n);

    }

    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool INDI::Dome::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(PresetGotoSP.name, name))
        {
            if (domeState == DOME_PARKED)
            {
                DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_ERROR, "Dome is parked. Please unpark before issuing any motion commands.");
                PresetGotoSP.s = IPS_ALERT;
                IDSetSwitch(&PresetGotoSP, NULL);
                return false;
            }

            IUUpdateSwitch(&PresetGotoSP, states, names, n);
            int index = IUFindOnSwitchIndex(&PresetGotoSP);
            IPState rc = MoveAbs(PresetN[index].value);
            if (rc == IPS_OK || rc == IPS_BUSY)
            {
                PresetGotoSP.s = IPS_OK;
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving to Preset %d (%g degrees).", index+1, PresetN[index].value);
                IDSetSwitch(&PresetGotoSP, NULL);

                if (rc == IPS_BUSY)
                {
                    DomeAbsPosNP.s = IPS_BUSY;
                    IDSetNumber(&DomeAbsPosNP, NULL);
                }
                return true;
            }

            PresetGotoSP.s = IPS_ALERT;
            IDSetSwitch(&PresetGotoSP, NULL);
            return false;
        }

        if (!strcmp(name, DomeAutoSyncSP.name))
        {
            IUUpdateSwitch(&DomeAutoSyncSP, states, names, n);
            DomeAutoSyncSP.s = IPS_OK;

            if (DomeAutoSyncS[0].s == ISS_ON)
            {
                 IDSetSwitch(&DomeAutoSyncSP, "Dome will now be synced to mount azimuth position.");
                 UpdateAutoSync();
            }
            else
            {
                IDSetSwitch(&DomeAutoSyncSP,  "Dome is no longer synced to mount azimuth position.");
                if (DomeAbsPosNP.s == IPS_BUSY || DomeRelPosNP.s == IPS_BUSY || DomeTimerNP.s == IPS_BUSY)
                    Abort();
            }

            return true;
        }

        if (strstr(name, "DOME_"))
            return processDomeSwitch(dev, name, states, names, n);

    }

    controller->ISNewSwitch(dev, name, states, names, n);

    //  Nobody has claimed this, so, ignore it
    return DefaultDevice::ISNewSwitch(dev,name,states,names,n);
}

bool INDI::Dome::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, PortTP.name))
        {
            IUUpdateText(&PortTP, texts, names, n);
            PortTP.s = IPS_OK;
            IDSetText(&PortTP, NULL);
            return true;
        }

        if(strcmp(name,ActiveDeviceTP.name)==0)
        {
            int rc;
            ActiveDeviceTP.s=IPS_OK;
            rc=IUUpdateText(&ActiveDeviceTP,texts,names,n);
            //  Update client display
            IDSetText(&ActiveDeviceTP,NULL);
            //saveConfig();

            // Update the property name!
            //strncpy(EqNP.device, ActiveDeviceT[0].text, MAXINDIDEVICE);
            IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");
            //  We processed this one, so, tell the world we did it
            return true;
        }
    }

    controller->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool INDI::Dome::ISSnoopDevice (XMLEle *root)
{
    XMLEle *ep=NULL;
    const char *propName = findXMLAttValu(root, "name");

    // Check EOD
    if (!strcmp("EQUATORIAL_EOD_COORD", propName))
    {
        for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
        {
            const char *elemName = findXMLAttValu(ep, "name");
            if (!strcmp(elemName, "RA"))
                mountEquatorialCoords.ra = atof(pcdataXMLEle(ep))*15.0;
            else if (!strcmp(elemName, "DEC"))
                mountEquatorialCoords.dec = atof(pcdataXMLEle(ep));

        }

        mountState = IPS_ALERT;
        crackIPState(findXMLAttValu(root, "state"), &mountState);

        // If the diff > 0.1 then the mount is in motion, so let's wait until it settles before moving the doom
        if (fabs(mountEquatorialCoords.ra - prev_ra) > DOME_COORD_THRESHOLD || fabs(mountEquatorialCoords.dec - prev_dec) > DOME_COORD_THRESHOLD)
        {
            prev_ra  = mountEquatorialCoords.ra;
            prev_dec = mountEquatorialCoords.dec;
            DEBUGF(INDI::Logger::DBG_DEBUG, "Snooped RA: %g - DEC: %g", mountEquatorialCoords.ra, mountEquatorialCoords.dec);
        }
        // else mount stable, i.e. tracking, so let's update mount coords and check if we need to move
        else if (mountState == IPS_OK || mountState == IPS_IDLE)
            UpdateMountCoords();

        return true;
     }

    // Check Geographic coords
    if (!strcmp("GEOGRAPHIC_COORD", propName))
    {
        for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
        {
            const char *elemName = findXMLAttValu(ep, "name");
            if (!strcmp(elemName, "LONG"))
            {
                double indiLong = atof(pcdataXMLEle(ep));
                if (indiLong > 180)
                    indiLong -= 360;
                observer.lng = indiLong;
            }
            else if (!strcmp(elemName, "LAT"))
                observer.lat = atof(pcdataXMLEle(ep));
        }

        DEBUGF(INDI::Logger::DBG_DEBUG, "Snooped LONG: %g - LAT: %g", observer.lng, observer.lat);

        UpdateMountCoords();

        return true;
     }

    controller->ISSnoopDevice(root);

    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool INDI::Dome::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigNumber(fp, &PresetNP);
    IUSaveConfigNumber(fp, &DomeParamNP);
    IUSaveConfigNumber(fp, &DomeMeasurementsNP);

    controller->saveConfigItems(fp);

    return true;
}

void INDI::Dome::buttonHelper(const char *button_n, ISState state, void *context)
{
     static_cast<INDI::Dome *>(context)->processButton(button_n, state);
}

void INDI::Dome::processButton(const char * button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    int rc=0;
    // Dome In
    if (!strcmp(button_n, "Dome CW"))
    {
        if (capability.hasVariableSpeed)
        {
           rc = Move(DOME_CW, DomeSpeedN[0].value, DomeTimerN[0].value);
            if (rc == 0)
                DomeTimerNP.s = IPS_OK;
            else if (rc == 1)
                DomeTimerNP.s = IPS_BUSY;
            else
                DomeTimerNP.s = IPS_ALERT;

            IDSetNumber(&DomeTimerNP,NULL);
        }
        else if (capability.canRelMove)
        {
            rc=MoveRel(DOME_CW, DomeRelPosN[0].value);
            if (rc == 0)
            {
               DomeRelPosNP.s=IPS_OK;
               IDSetNumber(&DomeRelPosNP, "Dome moved %g degrees CW", DomeRelPosN[0].value);
               IDSetNumber(&DomeAbsPosNP, NULL);
            }
            else if (rc == 1)
            {
                 DomeRelPosNP.s=IPS_BUSY;
                 IDSetNumber(&DomeAbsPosNP, "Dome is moving %g degrees CW...", DomeRelPosN[0].value);
            }
        }
    }
    else if (!strcmp(button_n, "Dome CCW"))
    {
        if (capability.hasVariableSpeed)
        {
           rc = Move(DOME_CCW, DomeSpeedN[0].value, DomeTimerN[0].value);
            if (rc == 0)
                DomeTimerNP.s = IPS_OK;
            else if (rc == 1)
                DomeTimerNP.s = IPS_BUSY;
            else
                DomeTimerNP.s = IPS_ALERT;

            IDSetNumber(&DomeTimerNP,NULL);
        }
        else if (capability.canRelMove)
        {
            rc=MoveRel(DOME_CCW, DomeRelPosN[0].value);
            if (rc == 0)
            {
               DomeRelPosNP.s=IPS_OK;
               IDSetNumber(&DomeRelPosNP, "Dome moved %g degrees CCW", DomeRelPosN[0].value);
               IDSetNumber(&DomeAbsPosNP, NULL);
            }
            else if (rc == 1)
            {
                 DomeRelPosNP.s=IPS_BUSY;
                 IDSetNumber(&DomeAbsPosNP, "Dome is moving %g degrees CCW...", DomeRelPosN[0].value);
            }
        }
    }
}

/*
The problem to get a dome azimuth given a telescope azimuth, altitude and geometry (telescope placement, mount geometry) can be seen as solve the intersection between the optical axis with the dome, that is, the intersection between a line and a sphere.
To do that we need to calculate the optical axis line taking the centre of the dome as origin of coordinates.
*/

// Returns false if it can't solve it due bad geometry of the observatory
// Returns:
// Az : Azimuth required to the dome in order to center the shutter aperture with telescope
// minAz: Minimum azimuth in order to avoid any dome interference to the full aperture of the telescope
// maxAz: Maximum azimuth in order to avoid any dome interference to the full aperture of the telescope
bool INDI::Dome::GetTargetAz(double & Az, double & Alt, double & minAz, double & maxAz)
{
    point3D MountCenter, OptCenter, OptAxis, DomeCenter, DomeIntersect;
    double hourAngle;
    double mu1, mu2;
    double yx;
    double HalfApertureChordAngle;
    double RadiusAtAlt;

    double JD  = ln_get_julian_from_sys();
    double MSD = ln_get_mean_sidereal_time(JD);

    MountCenter.x = DomeMeasurementsN[DM_NORTH_DISPLACEMENT].value;    // Positive to North
    MountCenter.y = DomeMeasurementsN[DM_EAST_DISPLACEMENT].value;     // Positive to East
    MountCenter.z = DomeMeasurementsN[DM_UP_DISPLACEMENT].value;       // Positive Up

    // Get hour angle in hours
    hourAngle = MSD + observer.lng/15.0 - mountEquatorialCoords.ra/15.0;

    // Get optical center point
    OpticalCenter(MountCenter, DomeMeasurementsN[DM_OTA_OFFSET].value, observer.lat, hourAngle, OptCenter);

    // Get optical axis point. This and the previous form the optical axis line
    OpticalVector(OptCenter, mountHoriztonalCoords.az, mountHoriztonalCoords.alt, OptAxis);

    DomeCenter.x = 0; DomeCenter.y = 0; DomeCenter.z = 0;

    if (Intersection(OptCenter, OptAxis, DomeCenter, DomeMeasurementsN[DM_DOME_RADIUS].value, mu1, mu2))
    {
        // If telescope is pointing over the horizon, the solution is mu1, else is mu2
        if (mu1 < 0)
            mu1 = mu2;

        DomeIntersect.x = OptCenter.x + mu1 * (OptAxis.x - OptCenter.x);
        DomeIntersect.y = OptCenter.y + mu1 * (OptAxis.y - OptCenter.y);
        DomeIntersect.z = OptCenter.z + mu1 * (OptAxis.z - OptCenter.z);

        if (fabs(DomeIntersect.x) > 0.001)
        {
            yx = DomeIntersect.y / DomeIntersect.x;
            Az = 90 - 180 * atan(yx) / M_PI;
            if (DomeIntersect.x < 0)
            {
                Az = Az + 180;
                if (Az >= 360) Az = Az - 360;
            }
        }
        else
        {  // Dome East-West line
            if (DomeIntersect.y > 0)
                Az = 90;
            else
                Az = 270;
        }

        if ((fabs(DomeIntersect.x) > 0.001) || (fabs(DomeIntersect.y) > 0.001))
            Alt = 180 * atan(DomeIntersect.z / sqrt((DomeIntersect.x*DomeIntersect.x) + (DomeIntersect.y*DomeIntersect.y))) / M_PI;
        else
            Alt = 90; // Dome Zenith

        // Calculate the Azimuth range in the given Altitude of the dome
        RadiusAtAlt = DomeMeasurementsN[DM_DOME_RADIUS].value * cos(M_PI * Alt/180); // Radius alt the given altitude

        if (DomeMeasurementsN[DM_SHUTTER_WIDTH].value < (2 * RadiusAtAlt))
        {
            HalfApertureChordAngle = 180 * asin(DomeMeasurementsN[DM_SHUTTER_WIDTH].value/(2 * RadiusAtAlt)) / M_PI; // Angle of a chord of half aperture length
            minAz = Az - HalfApertureChordAngle;
            if (minAz < 0)
                minAz = minAz + 360;
            maxAz = Az + HalfApertureChordAngle;
            if (maxAz >= 360)
                maxAz = maxAz - 360;
        }
        else
        {
            minAz = 0;
            maxAz = 360;
        }
        return true;
    }

    return false;
}


bool INDI::Dome::Intersection(point3D p1, point3D p2, point3D sc, double r, double & mu1, double & mu2)
{
    double a, b, c;
    double bb4ac;
    point3D dp;

    dp.x = p2.x - p1.x;
    dp.y = p2.y - p1.y;
    dp.z = p2.z - p1.z;
    a = dp.x * dp.x + dp.y * dp.y + dp.z * dp.z;
    b = 2 * (dp.x * (p1.x - sc.x) + dp.y * (p1.y - sc.y) + dp.z * (p1.z - sc.z));
    c = sc.x * sc.x + sc.y * sc.y + sc.z * sc.z;
    c = c + p1.x * p1.x + p1.y * p1.y + p1.z * p1.z;
    c = c - 2 * (sc.x * p1.x + sc.y * p1.y + sc.z * p1.z);
    c = c - r * r;
    bb4ac = b * b - 4 * a * c;
    if ((fabs(a) < 0.0000001) || (bb4ac < 0))
    {
        mu1 = 0;
        mu2 = 0;
        return false;
    }

    mu1 = (-b + sqrt(bb4ac)) / (2 * a);
    mu2 = (-b - sqrt(bb4ac)) / (2 * a);

    return true;
}


bool INDI::Dome::OpticalCenter(point3D MountCenter, double dOpticalAxis, double Lat, double Ah, point3D & OP)
{
    double q, f;
    double cosf, sinf, cosq, sinq;

    // Note: this transformation is a circle rotated around X axis -(90 - Lat) degrees
    q = M_PI * (90 - Lat) / 180;
    f = M_PI * (- Ah * 15) / 180;

    cosf = cos(f);
    sinf = sin(f);
    cosq = cos(q);
    sinq = sin(q);

    OP.x = (dOpticalAxis * cosq * (-cosf) + MountCenter.x);  // The sign of dOpticalAxis determines de side of the tube
    OP.y = (dOpticalAxis * sinf * cosq + MountCenter.y);
    OP.z = (dOpticalAxis * cosf * sinq + MountCenter.z);

    return true;
}


bool INDI::Dome::OpticalVector(point3D OP, double Az, double Alt, point3D & OV)
{
    double q, f;

    q = M_PI * Alt / 180;
    f = M_PI * (90 - Az) / 180;
    OV.x = OP.x + cos(q) * cos(f);
    OV.y = OP.y + cos(q) * sin(f);
    OV.z = OP.z + sin(q);

    return true;
}

double INDI::Dome::Csc(double x)
{
    return 1.0 / sin(x);
}

double INDI::Dome::Sec(double x)
{
    return 1.0 / cos(x);
}

bool INDI::Dome::CheckHorizon(double HA, double dec, double lat)
{
    double sinh_value;

    sinh_value = cos(lat) * cos(HA) * cos(dec) + sin(lat) * sin(dec);

    if ( sinh_value >= 0.0)
        return true;

    return false;
}

void INDI::Dome::UpdateMountCoords()
{
    // If not initialized yet, return.
    if (mountEquatorialCoords.ra == -1)
        return;

    ln_get_hrz_from_equ(&mountEquatorialCoords, &observer, ln_get_julian_from_sys(), &mountHoriztonalCoords);

    mountHoriztonalCoords.az += 180;
    if (mountHoriztonalCoords.az > 360)
        mountHoriztonalCoords.az -= 360;
    if (mountHoriztonalCoords.az < 0)
        mountHoriztonalCoords.az += 360;

    // Control debug flooding
    if (fabs(mountHoriztonalCoords.az - prev_az) > DOME_COORD_THRESHOLD || fabs(mountHoriztonalCoords.alt - prev_alt) > DOME_COORD_THRESHOLD)
    {
        prev_az  = mountHoriztonalCoords.az;
        prev_alt = mountHoriztonalCoords.alt;
        DEBUGF(INDI::Logger::DBG_DEBUG, "Updated telescope Az: %g - Alt: %g", prev_az, prev_alt);
    }

    // Check if we need to move
    UpdateAutoSync();
}

void INDI::Dome::UpdateAutoSync()
{
    if ((mountState == IPS_OK || mountState == IPS_IDLE) && DomeAbsPosNP.s != IPS_BUSY && DomeAutoSyncS[0].s == ISS_ON)
    {
        double targetAz, targetAlt, minAz, maxAz;
        GetTargetAz(targetAz, targetAlt, minAz, maxAz);

        DEBUGF(INDI::Logger::DBG_DEBUG, "Calculated target azimuth is %g. MinAz: %g, MaxAz: %g", targetAz, minAz, maxAz);

        if (fabs(targetAz - DomeAbsPosN[0].value) > DomeParamN[DOME_AUTOSYNC].value)
        {
            int ret = 0;
            if ( (ret = MoveAbs(targetAz)) == 0)
            {
               DomeAbsPosNP.s=IPS_OK;
               IDSetNumber(&DomeAbsPosNP, "Dome synced to position %g degrees.", targetAz);
            }
            else if (ret == 1)
            {
               DomeAbsPosNP.s=IPS_BUSY;
               IDSetNumber(&DomeAbsPosNP, "Dome is syncing to position %g degrees...", targetAz);
            }
            else
            {
                DomeAbsPosNP.s = IPS_ALERT;
                IDSetNumber(&DomeAbsPosNP, "Dome failed to sync to new requested position.");
            }
       }
    }
}
