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

#include <wordexp.h>
#include <math.h>
#include <string.h>

#define DOME_SLAVING_TAB   "Slaving"
#define DOME_COORD_THRESHOLD    0.1             /* Only send debug messages if the differences between old and new values of Az/Alt excceds this value */

INDI::Dome::Dome()
{
    controller = new INDI::Controller(this);

    controller->setButtonCallback(buttonHelper);

    prev_az = prev_alt = prev_ra = prev_dec = 0;
    mountEquatorialCoords.dec=mountEquatorialCoords.ra=-1;
    mountState   = IPS_ALERT;
    weatherState = IPS_IDLE;

    capability.canAbort=false;
    capability.canAbsMove=false;
    capability.canRelMove=true;
    capability.hasShutter=false;
    capability.hasVariableSpeed=false;

    shutterState = SHUTTER_UNKNOWN;
    domeState    = DOME_IDLE;

    parkDataType = PARK_NONE;
    Parkdatafile= "~/.indi/ParkData.xml";
    IsParked=false;
}

INDI::Dome::~Dome()
{
}

bool INDI::Dome::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

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
    IUFillText(&ActiveDeviceT[1],"ACTIVE_WEATHER","Weather","WunderGround");
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,2,getDeviceName(),"ACTIVE_DEVICES","Snoop devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    // Measurements
    IUFillNumber(&DomeMeasurementsN[DM_DOME_RADIUS],"DM_DOME_RADIUS","Radius (m)","%6.2f",0.0,50.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_SHUTTER_WIDTH],"DM_SHUTTER_WIDTH","Shutter width (m)","%6.2f",0.0,10.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_NORTH_DISPLACEMENT],"DM_NORTH_DISPLACEMENT","N displacement (m)","%6.2f",-10.0,10.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_EAST_DISPLACEMENT],"DM_EAST_DISPLACEMENT","E displacement (m)","%6.2f",-10.0,10.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_UP_DISPLACEMENT],"DM_UP_DISPLACEMENT","Up displacement (m)","%6.2f",-10,10.0,1.0,0.0);
    IUFillNumber(&DomeMeasurementsN[DM_OTA_OFFSET],"DM_OTA_OFFSET","OTA offset (m)","%6.2f",-10.0,10.0,1.0,0.0);
    IUFillNumberVector(&DomeMeasurementsNP,DomeMeasurementsN,6,getDeviceName(),"DOME_MEASUREMENTS","Measurements",DOME_SLAVING_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&DomeAutoSyncS[0],"DOME_AUTOSYNC_ENABLE","Enable",ISS_OFF);
    IUFillSwitch(&DomeAutoSyncS[1],"DOME_AUTOSYNC_DISABLE","Disable",ISS_ON);
    IUFillSwitchVector(&DomeAutoSyncSP,DomeAutoSyncS,2,getDeviceName(),"DOME_AUTOSYNC","Slaving",DOME_SLAVING_TAB,IP_RW,ISR_ATMOST1,60,IPS_OK);

    IUFillNumber(&DomeSpeedN[0],"DOME_SPEED_VALUE","RPM","%6.2f",0.0,10,0.1,1.0);
    IUFillNumberVector(&DomeSpeedNP,DomeSpeedN,1,getDeviceName(),"DOME_SPEED","Speed",MAIN_CONTROL_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&DomeMotionS[0],"DOME_CW","Dome CW",ISS_OFF);
    IUFillSwitch(&DomeMotionS[1],"DOME_CCW","Dome CCW",ISS_OFF);
    IUFillSwitchVector(&DomeMotionSP,DomeMotionS,2,getDeviceName(),"DOME_MOTION","Direction",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_OK);

    // Driver can define those to clients if there is support
    IUFillNumber(&DomeAbsPosN[0],"DOME_ABSOLUTE_POSITION","Degrees","%6.2f",0.0,360.0,1.0,0.0);
    IUFillNumberVector(&DomeAbsPosNP,DomeAbsPosN,1,getDeviceName(),"ABS_DOME_POSITION","Absolute Position",MAIN_CONTROL_TAB,IP_RW,60,IPS_OK);

    IUFillNumber(&DomeRelPosN[0],"DOME_RELATIVE_POSITION","Degrees","%6.2f",-180,180.0,10.0,0.0);
    IUFillNumberVector(&DomeRelPosNP,DomeRelPosN,1,getDeviceName(),"REL_DOME_POSITION","Relative Position",MAIN_CONTROL_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&AbortS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&AbortSP,AbortS,1,getDeviceName(),"DOME_ABORT_MOTION","Abort Motion",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillNumber(&DomeParamN[0],"AUTOSYNC_THRESHOLD","Autosync threshold (deg)","%6.2f",0.0,360.0,1.0,0.5);
    IUFillNumberVector(&DomeParamNP,DomeParamN,1,getDeviceName(),"DOME_PARAMS","Params",DOME_SLAVING_TAB,IP_RW,60,IPS_OK);

    IUFillSwitch(&ParkS[0],"PARK","Park",ISS_OFF);
    IUFillSwitch(&ParkS[1],"UNPARK","UnPark",ISS_OFF);
    IUFillSwitchVector(&ParkSP,ParkS,2,getDeviceName(),"DOME_PARK","Parking",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

    IUFillSwitch(&DomeShutterS[0],"SHUTTER_OPEN","Open",ISS_OFF);
    IUFillSwitch(&DomeShutterS[1],"SHUTTER_CLOSE","Close",ISS_ON);
    IUFillSwitchVector(&DomeShutterSP,DomeShutterS,2,getDeviceName(),"DOME_SHUTTER","Shutter",MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

    IUFillSwitch(&ParkOptionS[0],"PARK_CURRENT","Current",ISS_OFF);
    IUFillSwitch(&ParkOptionS[1],"PARK_DEFAULT","Default",ISS_OFF);
    IUFillSwitch(&ParkOptionS[2],"PARK_WRITE_DATA","Write Data",ISS_OFF);
    IUFillSwitchVector(&ParkOptionSP,ParkOptionS,3,getDeviceName(),"DOME_PARK_OPTION","Park Options", SITE_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    addDebugControl();

    controller->mapController("Dome CW", "CW/Open", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_1");
    controller->mapController("Dome CCW", "CCW/Close", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_2");

    controller->initProperties();

    IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");

    IDSnoopDevice(ActiveDeviceT[1].text,"WEATHER_STATUS");

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
            //defineNumber(&DomeTimerNP);
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

            defineSwitch(&DomeAutoSyncSP);
            defineNumber(&DomeParamNP);
            defineNumber(&DomeMeasurementsNP);            
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

    } else
    {
        deleteProperty(ActiveDeviceTP.name);

        if (capability.hasShutter)
            deleteProperty(DomeShutterSP.name);

        deleteProperty(DomeMotionSP.name);

        if (capability.hasVariableSpeed)
        {
            deleteProperty(DomeSpeedNP.name);
            //deleteProperty(DomeTimerNP.name);
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

            deleteProperty(DomeAutoSyncSP.name);
            deleteProperty(DomeParamNP.name);
            deleteProperty(DomeMeasurementsNP.name);
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

        if (!strcmp(name, DomeParamNP.name))
        {
            IUUpdateNumber(&DomeParamNP, values, names, n);
            DomeParamNP.s = IPS_OK;
            IDSetNumber(&DomeParamNP, NULL);
            return true;
        }

        if(!strcmp(name, DomeSpeedNP.name))
        {
            double newSpeed = values[0];
            Dome::SetSpeed(newSpeed);
            return true;
        }

        if(!strcmp(name, DomeAbsPosNP.name))
        {
            double newPos = values[0];
            Dome::MoveAbs(newPos);
            return true;
        }

        if(!strcmp(name, DomeRelPosNP.name))
        {
            double newPos = values[0];
            Dome::MoveRel(newPos);
            return true;
        }

        if (!strcmp(name, DomeMeasurementsNP.name))
        {
            IUUpdateNumber(&DomeMeasurementsNP,values, names,n);
            DomeMeasurementsNP.s = IPS_OK;
            IDSetNumber(&DomeMeasurementsNP, NULL);

            return true;
        }

        if(strcmp(name, ParkPositionNP.name) == 0)
        {
          IUUpdateNumber(&ParkPositionNP, values, names, n);
          ParkPositionNP.s = IPS_OK;

          Axis1ParkPosition = ParkPositionN[AXIS_RA].value;
          IDSetNumber(&ParkPositionNP, NULL);
          return true;
        }
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
                DEBUGDEVICE(getDeviceName(), INDI::Logger::DBG_ERROR, "Please unpark before issuing any motion commands.");
                PresetGotoSP.s = IPS_ALERT;
                IDSetSwitch(&PresetGotoSP, NULL);
                return false;
            }

            IUUpdateSwitch(&PresetGotoSP, states, names, n);
            int index = IUFindOnSwitchIndex(&PresetGotoSP);
            IPState rc = Dome::MoveAbs(PresetN[index].value);
            if (rc == IPS_OK || rc == IPS_BUSY)
            {
                PresetGotoSP.s = IPS_OK;
                DEBUGF(INDI::Logger::DBG_SESSION, "Moving to Preset %d (%g degrees).", index+1, PresetN[index].value);
                IDSetSwitch(&PresetGotoSP, NULL);
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
                if (DomeAbsPosNP.s == IPS_BUSY || DomeRelPosNP.s == IPS_BUSY/* || DomeTimerNP.s == IPS_BUSY*/)
                    Dome::Abort();
            }

            return true;
        }

        if(!strcmp(name, DomeMotionSP.name))
        {

            // Check if any switch is ON
            for (int i=0; i < n; i++)
            {
                if (states[i] == ISS_ON)
                {
                    if (!strcmp(DomeMotionS[DOME_CW].name, names[i]))
                        Dome::Move(DOME_CW, MOTION_START);
                    else
                        Dome::Move(DOME_CCW, MOTION_START);

                    return true;
                }
            }

            // All switches are off, so let's turn off last motion
            int current_direction = IUFindOnSwitchIndex(&DomeMotionSP);
            // Shouldn't happen
            if (current_direction < 0)
            {
                DomeMotionSP.s = IPS_IDLE;
                IDSetSwitch(&DomeMotionSP, NULL);
                return false;
            }

            Dome::Move( (DomeDirection) current_direction, MOTION_STOP);

            return true;

        }


        if(!strcmp(name, AbortSP.name))
        {
            Dome::Abort();
            return true;
        }

        // Dome Shutter
        if (!strcmp(name, DomeShutterSP.name))
        {

            // Check if any switch is ON
            for (int i=0; i < n; i++)
            {
                if (states[i] == ISS_ON)
                {
                    // Open
                    if (!strcmp(DomeShutterS[0].name, names[i]))
                    {

                        return (Dome::ControlShutter(SHUTTER_OPEN) != IPS_ALERT);
                    }
                    else
                    {

                        return (Dome::ControlShutter(SHUTTER_CLOSE) != IPS_ALERT);
                    }
                }
            }

        }

        if(!strcmp(name,ParkSP.name))
        {
            // Check if any switch is ON
            for (int i=0; i < n; i++)
            {
                if (states[i] == ISS_ON)
                {
                    if (!strcmp(ParkS[0].name, names[i]))
                    {
                        if (domeState == DOME_PARKING)
                            return false;

                        return (Dome::Park() != IPS_ALERT);
                    }
                    else
                    {
                        if (domeState == DOME_UNPARKING)
                            return false;

                        return (Dome::UnPark() != IPS_ALERT);
                    }
                }
            }

        }

        if (!strcmp(name, ParkOptionSP.name))
        {
          IUUpdateSwitch(&ParkOptionSP, states, names, n);
          ISwitch *sp = IUFindOnSwitch(&ParkOptionSP);
          if (!sp)
            return false;

          IUResetSwitch(&ParkOptionSP);

          if (!strcmp(sp->name, "PARK_CURRENT"))
          {
              SetCurrentPark();
          }
          else if (!strcmp(sp->name, "PARK_DEFAULT"))
          {
              SetDefaultPark();
          }
          else if (!strcmp(sp->name, "PARK_WRITE_DATA"))
          {
            if (WriteParkData())
              DEBUG( INDI::Logger::DBG_SESSION, "Saved Park Status/Position.");
            else
              DEBUG( INDI::Logger::DBG_WARNING, "Can not save Park Status/Position.");
          }

          ParkOptionSP.s = IPS_OK;
          IDSetSwitch(&ParkOptionSP, NULL);

          return true;
        }



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
            ActiveDeviceTP.s=IPS_OK;
            IUUpdateText(&ActiveDeviceTP,texts,names,n);
            IDSetText(&ActiveDeviceTP,NULL);

            IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text,"GEOGRAPHIC_COORD");            
            IDSnoopDevice(ActiveDeviceT[1].text,"WEATHER_STATUS");

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

    // Weather Status
    if (!strcmp("WEATHER_STATUS", propName))
    {
        weatherState = IPS_ALERT;
        crackIPState(findXMLAttValu(root, "state"), &weatherState);

        if (weatherState == IPS_ALERT)
        {
            if (capability.canPark)
            {
                if (isParked() == false)
                {
                    DEBUG(INDI::Logger::DBG_WARNING, "Weather conditions in the danger zone! Parking dome...");
                    Dome::Park();
                }
            }
            else
                DEBUG(INDI::Logger::DBG_WARNING, "Weather conditions in the danger zone! Close the dome immediately!");

            return true;
        }
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

    // Dome In
    if (!strcmp(button_n, "Dome CW"))
    {
      if (DomeMotionSP.s != IPS_BUSY)
          Dome::Move( DOME_CW, MOTION_START);
      else
          Dome::Move( DOME_CW, MOTION_STOP);
    }
    else if (!strcmp(button_n, "Dome CCW"))
    {
        if (DomeMotionSP.s != IPS_BUSY)
            Dome::Move( DOME_CCW, MOTION_START);
        else
            Dome::Move( DOME_CCW, MOTION_STOP);
    }
    else if (!strcmp(button_n, "Dome Abort"))
    {
        Dome::Abort();
    }
}

IPState INDI::Dome::getMountState() const
{
    return mountState;
}

IPState INDI::Dome::getWeatherState() const
{
    return weatherState;
}

INDI::Dome::DomeState INDI::Dome::Dome::getDomeState() const
{
    return domeState;
}

void INDI::Dome::setDomeState(const INDI::Dome::DomeState &value)
{
    switch (value)
    {
         case DOME_IDLE:
            if (DomeMotionSP.s == IPS_BUSY)
            {
                IUResetSwitch(&DomeMotionSP);
                DomeMotionSP.s = IPS_IDLE;
                IDSetSwitch(&DomeMotionSP, NULL);
            }
            if (DomeAbsPosNP.s == IPS_BUSY)
            {
                DomeAbsPosNP.s = IPS_IDLE;
                IDSetNumber(&DomeAbsPosNP, NULL);
            }
            if (DomeRelPosNP.s == IPS_BUSY)
            {
                DomeRelPosNP.s = IPS_IDLE;
                IDSetNumber(&DomeRelPosNP, NULL);
            }
            break;

        case DOME_PARKED:
            IUResetSwitch(&ParkSP);
            ParkSP.s = IPS_OK;
            ParkS[0].s = ISS_ON;
            IDSetSwitch(&ParkSP, NULL);
            IsParked = true;
            break;

         case DOME_PARKING:
            IUResetSwitch(&ParkSP);
            ParkSP.s = IPS_BUSY;
            ParkS[0].s = ISS_ON;
            IDSetSwitch(&ParkSP, NULL);
            break;

        case DOME_UNPARKING:
            IUResetSwitch(&ParkSP);
            ParkSP.s = IPS_BUSY;
            ParkS[1].s = ISS_ON;
            IDSetSwitch(&ParkSP, NULL);
            IsParked = false;
            break;
    }

    domeState = value;

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

        if (fabs(targetAz - DomeAbsPosN[0].value) > DomeParamN[0].value)
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

void INDI::Dome::SetDomeCapability(DomeCapability *cap)
{
    capability.canAbort     = cap->canAbort;
    capability.canAbsMove   = cap->canAbsMove;
    capability.canRelMove   = cap->canRelMove;
    capability.canPark      = cap->canPark;
    capability.hasShutter   = cap->hasShutter;
    capability.hasVariableSpeed= cap->hasVariableSpeed;

    if (capability.canAbort)
        controller->mapController("Dome Abort", "Dome Abort", INDI::Controller::CONTROLLER_BUTTON, "BUTTON_3");

}

const char * INDI::Dome::GetShutterStatusString(ShutterStatus status)
{
    switch (status)
    {
        case SHUTTER_OPENED:
            return "Shutter is open.";
            break;
        case SHUTTER_CLOSED:
            return "Shutter is closed.";
            break;
        case SHUTTER_MOVING:
            return "Shutter is in motion.";
            break;
        case SHUTTER_UNKNOWN:
        default:
            return "Shutter status is unknown.";
            break;
    }
}

void INDI::Dome::SetParkDataType(INDI::Dome::DomeParkData type)
{
    parkDataType = type;

    if (parkDataType != PARK_NONE)
    {
        switch (parkDataType)
        {
            case PARK_AZ:
            IUFillNumber(&ParkPositionN[AXIS_AZ],"PARK_AZ","AZ D:M:S", "%10.6m", 0.0, 360.0, 0.0, 0);
            break;

            case PARK_AZ_ENCODER:
            IUFillNumber(&ParkPositionN[AXIS_AZ],"PARK_AZ" ,"AZ Encoder","%.0f" ,0,16777215,1,0);
            break;

        default:
            break;
        }

        IUFillNumberVector(&ParkPositionNP,ParkPositionN,1,getDeviceName(),"DOME_PARK_POSITION","Park Position", SITE_TAB,IP_RW,60,IPS_IDLE);
    }
    else
    {
        strncpy(DomeMotionS[DOME_CW].label, "Open", MAXINDILABEL);
        strncpy(DomeMotionS[DOME_CCW].label, "Close", MAXINDILABEL);
    }

}

void INDI::Dome::SetParked(bool isparked)
{  
  IsParked=isparked;  

  setDomeState(DOME_IDLE);

  if (IsParked)
  {      
      setDomeState(DOME_PARKED);
      DEBUG( INDI::Logger::DBG_SESSION, "Dome is parked.");
  }
  else
  {      
      IUResetSwitch(&ParkSP);
      ParkS[1].s = ISS_ON;
      ParkSP.s = IPS_OK;
      IDSetSwitch(&ParkSP, NULL);
      DEBUG( INDI::Logger::DBG_SESSION, "Dome is unparked.");
  }  

  WriteParkData();
}

bool INDI::Dome::isParked()
{
  return IsParked;
}

bool INDI::Dome::InitPark()
{
  char *loadres;
  loadres=LoadParkData();
  if (loadres)
  {
    DEBUGF( INDI::Logger::DBG_SESSION, "InitPark: No Park data in file %s: %s", Parkdatafile, loadres);
    SetParked(false);
    return false;
  }  

  if (parkDataType != PARK_NONE)
  {
      ParkPositionN[AXIS_AZ].value = Axis1ParkPosition;
      IDSetNumber(&ParkPositionNP, NULL);

      // If parked, store the position as current azimuth angle or encoder ticks
      if (isParked() && capability.canAbsMove)
      {
          DomeAbsPosN[0].value = ParkPositionN[AXIS_AZ].value;
          IDSetNumber(&DomeAbsPosNP, NULL);
      }
  }

  return true;
}

char *INDI::Dome::LoadParkData()
{
  wordexp_t wexp;
  FILE *fp;
  LilXML *lp;
  static char errmsg[512];

  XMLEle *parkxml;
  XMLAtt *ap;
  bool devicefound=false;

  ParkDeviceName = getDeviceName();
  ParkstatusXml=NULL;
  ParkdeviceXml=NULL;
  ParkpositionXml = NULL;
  ParkpositionAxis1Xml = NULL;

  if (wordexp(Parkdatafile, &wexp, 0))
  {
    wordfree(&wexp);
    return (char *)("Badly formed filename.");
  }

  if (!(fp=fopen(wexp.we_wordv[0], "r")))
  {
    wordfree(&wexp);
    return strerror(errno);
  }
  wordfree(&wexp);

 lp = newLilXML();

 if (ParkdataXmlRoot)
    delXMLEle(ParkdataXmlRoot);

  ParkdataXmlRoot = readXMLFile(fp, lp, errmsg);

  delLilXML(lp);
  if (!ParkdataXmlRoot)
      return errmsg;

  if (!strcmp(tagXMLEle(nextXMLEle(ParkdataXmlRoot, 1)), "parkdata"))
      return (char *)("Not a park data file");

  parkxml=nextXMLEle(ParkdataXmlRoot, 1);

  while (parkxml)
  {
    if (strcmp(tagXMLEle(parkxml), "device"))
    {
        parkxml=nextXMLEle(ParkdataXmlRoot, 0);
        continue;
    }
    ap = findXMLAtt(parkxml, "name");
    if (ap && (!strcmp(valuXMLAtt(ap), ParkDeviceName)))
    {
        devicefound = true;
        break;
    }
    parkxml=nextXMLEle(ParkdataXmlRoot, 0);
  }

  if (!devicefound)
      return (char *)"No park data found for this device";

  ParkdeviceXml=parkxml;
  ParkstatusXml = findXMLEle(parkxml, "parkstatus");

  if (ParkstatusXml == NULL)
  {
      return (char *)("Park data invalid or missing.");
  }

  if (parkDataType != PARK_NONE)
  {
      ParkpositionXml = findXMLEle(parkxml, "parkposition");
      ParkpositionAxis1Xml = findXMLEle(ParkpositionXml, "axis1position");

      if (ParkpositionAxis1Xml == NULL)
      {
          return (char *)("Park data invalid or missing.");
      }
  }

  if (parkDataType != PARK_NONE)
    sscanf(pcdataXMLEle(ParkpositionAxis1Xml), "%lf", &Axis1ParkPosition);


  if (!strcmp(pcdataXMLEle(ParkstatusXml), "true"))
      SetParked(true);
  else
      SetParked(false);


  return NULL;
}

bool INDI::Dome::WriteParkData()
{
  wordexp_t wexp;
  FILE *fp;
  char pcdata[30];

  if (wordexp(Parkdatafile, &wexp, 0))
  {
    wordfree(&wexp);
    DEBUGF( INDI::Logger::DBG_SESSION, "WriteParkData: can not write file %s: Badly formed filename.", Parkdatafile);
    return false;
  }

  if (!(fp=fopen(wexp.we_wordv[0], "w")))
  {
    wordfree(&wexp);
    DEBUGF( INDI::Logger::DBG_SESSION, "WriteParkData: can not write file %s: %s", Parkdatafile, strerror(errno));
    return false;
  }

  if (!ParkdataXmlRoot)
      ParkdataXmlRoot=addXMLEle(NULL, "parkdata");

  if (!ParkdeviceXml)
  {
    ParkdeviceXml=addXMLEle(ParkdataXmlRoot, "device");
    addXMLAtt(ParkdeviceXml, "name", ParkDeviceName);
  }

  if (!ParkstatusXml)
      ParkstatusXml=addXMLEle(ParkdeviceXml, "parkstatus");

  if (parkDataType != PARK_NONE)
  {
      if (!ParkpositionXml)
          ParkpositionXml=addXMLEle(ParkdeviceXml, "parkposition");
      if (!ParkpositionAxis1Xml)
          ParkpositionAxis1Xml=addXMLEle(ParkpositionXml, "axis1position");
  }

  editXMLEle(ParkstatusXml, (IsParked?"true":"false"));

  if (parkDataType != PARK_NONE)
  {
      snprintf(pcdata, sizeof(pcdata), "%f", Axis1ParkPosition);
      editXMLEle(ParkpositionAxis1Xml, pcdata);
  }

  prXMLEle(fp, ParkdataXmlRoot, 0);
  fclose(fp);

  return true;
}

double INDI::Dome::GetAxis1Park()
{
  return Axis1ParkPosition;
}

double INDI::Dome::GetAxis1ParkDefault()
{
  return Axis1DefaultParkPosition;
}

void INDI::Dome::SetAxis1Park(double value)
{
  Axis1ParkPosition=value;
  ParkPositionN[AXIS_RA].value = value;
  IDSetNumber(&ParkPositionNP, NULL);
}

void INDI::Dome::SetAxis1ParkDefault(double value)
{
  Axis1DefaultParkPosition=value;
}

bool INDI::Dome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    // Check if it is already parked.
    if (capability.canPark)
    {
        if (parkDataType != PARK_NONE && isParked())
        {
            DEBUG( INDI::Logger::DBG_WARNING, "Please unpark the dome before issuing any motion commands.");
            DomeMotionSP.s = IPS_ALERT;
            IDSetSwitch(&DomeMotionSP, NULL);
            return false;
        }
    }

    if ( (DomeMotionSP.s != IPS_BUSY && (DomeAbsPosNP.s == IPS_BUSY || DomeRelPosNP.s == IPS_BUSY))
          || (domeState == DOME_PARKING))
    {
        DEBUG( INDI::Logger::DBG_WARNING, "Please stop dome before issuing any further motion commands.");
        DomeMotionSP.s = IPS_ALERT;
        IDSetSwitch(&DomeMotionSP, NULL);
        return false;
    }

    int current_direction = IUFindOnSwitchIndex(&DomeMotionSP);

    // if same move requested, return
    if ( DomeMotionSP.s == IPS_BUSY && current_direction == dir)
        return true;

    if (Move(dir, operation))
    {
            domeState = (operation == MOTION_START) ? DOME_MOVING : DOME_IDLE;
            IUResetSwitch(&DomeMotionSP);
            if (operation == MOTION_START)
                DomeMotionS[dir].s = ISS_ON;

            DomeMotionSP.s = IPS_BUSY;
    }
    else
        DomeMotionSP.s = IPS_ALERT;

    IDSetSwitch(&DomeMotionSP, NULL);

    return (DomeMotionSP.s == IPS_BUSY);
}

IPState INDI::Dome::MoveRel(double azDiff)
{
    if (capability.canRelMove == false)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Dome does not support relative motion.");
        return IPS_ALERT;
    }

    if (domeState == DOME_PARKED)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Please unpark before issuing any motion commands.");
        DomeRelPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeRelPosNP, NULL);
        return IPS_ALERT;
    }

    if (DomeRelPosNP.s != IPS_BUSY && DomeMotionSP.s == IPS_BUSY || (domeState == DOME_PARKING))
    {
        DEBUG( INDI::Logger::DBG_WARNING, "Please stop dome before issuing any further motion commands.");
        DomeRelPosNP.s = IPS_IDLE;
        IDSetNumber(&DomeRelPosNP, NULL);
        return IPS_ALERT;
    }

    IPState rc;

    if ( (rc=MoveRel(azDiff)) == IPS_OK)
    {
       domeState = DOME_IDLE;
       DomeRelPosNP.s=IPS_OK;
       DomeRelPosN[0].value = azDiff;
       IDSetNumber(&DomeRelPosNP, "Dome moved %g degrees %s.", azDiff, (azDiff > 0) ? "clockwise" : "counter clockwise");
       if (capability.canAbsMove)
       {
          DomeAbsPosNP.s=IPS_OK;
          IDSetNumber(&DomeAbsPosNP, NULL);
       }
       return IPS_OK;
    }
    else if (rc == IPS_BUSY)
    {
         domeState = DOME_MOVING;
         DomeRelPosN[0].value = azDiff;
         DomeRelPosNP.s=IPS_BUSY;
         IDSetNumber(&DomeRelPosNP, "Dome is moving %g degrees %s...", azDiff, (azDiff > 0) ? "clockwise" : "counter clockwise");
         if (capability.canAbsMove)
         {
            DomeAbsPosNP.s=IPS_BUSY;
            IDSetNumber(&DomeAbsPosNP, NULL);
         }
         return IPS_BUSY;
    }

    domeState = DOME_IDLE;
    DomeRelPosNP.s = IPS_ALERT;
    IDSetNumber(&DomeRelPosNP, "Dome failed to move to new requested position.");
    return IPS_ALERT;
}

IPState INDI::Dome::MoveAbs(double az)
{
    if (capability.canAbsMove == false)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Dome does not support MoveAbs(). MoveAbs() must be implemented in the child class.");
        return IPS_ALERT;
    }

    if (domeState == DOME_PARKED)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Please unpark before issuing any motion commands.");
        DomeAbsPosNP.s = IPS_ALERT;
        IDSetNumber(&DomeAbsPosNP, NULL);
        return IPS_ALERT;
    }

    if (DomeRelPosNP.s != IPS_BUSY && DomeMotionSP.s == IPS_BUSY || (domeState == DOME_PARKING))
    {
        DEBUG( INDI::Logger::DBG_WARNING, "Please stop dome before issuing any further motion commands.");
        return IPS_ALERT;
    }

    IPState rc;

    if (az < DomeAbsPosN[0].min || az > DomeAbsPosN[0].max)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error: requested azimuth angle %g is out of range.", az);
        DomeAbsPosNP.s=IPS_ALERT;
        IDSetNumber(&DomeAbsPosNP, NULL);
        return IPS_ALERT;
    }

    if ( (rc = MoveAbs(az)) == IPS_OK)
    {
       domeState = DOME_IDLE;
       DomeAbsPosNP.s=IPS_OK;
       DomeAbsPosN[0].value = az;
       DEBUGF(INDI::Logger::DBG_SESSION, "Dome moved to position %g degrees.", az);
       IDSetNumber(&DomeAbsPosNP, NULL);
       return IPS_OK;
    }
    else if (rc == IPS_BUSY)
    {
       domeState = DOME_MOVING;
       DomeAbsPosNP.s=IPS_BUSY;
       DEBUGF(INDI::Logger::DBG_SESSION, "Dome is moving to position %g degrees...", az);
       IDSetNumber(&DomeAbsPosNP, NULL);
       return IPS_BUSY;
    }

    domeState = DOME_IDLE;
    DomeAbsPosNP.s = IPS_ALERT;
    IDSetNumber(&DomeAbsPosNP, "Dome failed to move to new requested position.");
    return IPS_ALERT;
}


bool INDI::Dome::Abort()
{
    if (capability.canAbort == false)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Dome does not support abort.");
        return false;
    }

    IUResetSwitch(&AbortSP);

    if (Abort())
    {
        AbortSP.s = IPS_OK;

        if (domeState == DOME_PARKING || domeState == DOME_UNPARKING)
        {
            IUResetSwitch(&ParkSP);
            if (domeState == DOME_PARKING)
            {
                DEBUG( INDI::Logger::DBG_SESSION, "Parking aborted.");
                // If parking was aborted then it was UNPARKED before
                ParkS[1].s = ISS_ON;
            }
            else
            {
                DEBUG( INDI::Logger::DBG_SESSION, "UnParking aborted.");
                // If unparking aborted then it was PARKED before
                ParkS[0].s = ISS_ON;
            }

            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, NULL);
        }

        setDomeState(DOME_IDLE);
    }
    else
    {
        AbortSP.s = IPS_ALERT;

        // If alert was aborted during parking or unparking, the parking state is unknown
        if (domeState == DOME_PARKING || domeState == DOME_UNPARKING)
        {
            IUResetSwitch(&ParkSP);
            ParkSP.s = IPS_IDLE;
            IDSetSwitch(&ParkSP, NULL);
        }

    }

    IDSetSwitch(&AbortSP, NULL);

    return (AbortSP.s == IPS_OK);
}

bool INDI::Dome::SetSpeed(double speed)
{
    if (capability.hasVariableSpeed == false)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Dome does not support variable speed.");
        return false;
    }

    if (SetSpeed(speed))
    {
        DomeSpeedNP.s = IPS_OK;
        DomeSpeedN[0].value = speed;
    }
    else
        DomeSpeedNP.s = IPS_ALERT;

    IDSetNumber(&DomeSpeedNP,NULL);

    return (DomeSpeedNP.s == IPS_OK);
}

IPState INDI::Dome::ControlShutter(ShutterOperation operation)
{
    if (capability.hasShutter == false)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Dome does not have shutter control.");
        return IPS_ALERT;
    }

    if (weatherState == IPS_ALERT && operation == SHUTTER_OPEN)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Weather is in the danger zone! Cannot open shutter.");
        return IPS_ALERT;
    }

    int currentStatus = IUFindOnSwitchIndex(&DomeShutterSP);

    // No change of status, let's return
    if (DomeShutterSP.s == IPS_BUSY && currentStatus == operation)
    {
        IDSetSwitch(&DomeShutterSP,NULL);
        return DomeShutterSP.s;
    }

    DomeShutterSP.s = ControlShutter(operation);

    if (DomeShutterSP.s == IPS_OK)
    {
       IUResetSwitch(&DomeShutterSP);
       DomeShutterS[operation].s = ISS_ON;
       IDSetSwitch(&DomeShutterSP, "Shutter is %s.", (operation == SHUTTER_OPEN ? "open" : "closed"));
       DomeShutterSP.s;
    }
    else if (DomeShutterSP.s == IPS_BUSY)
    {
         IUResetSwitch(&DomeShutterSP);
         DomeShutterS[operation].s = ISS_ON;
         IDSetSwitch(&DomeShutterSP, "Shutter is %s...", (operation == 0 ? "opening" : "closing"));
         return DomeShutterSP.s;
    }

    IDSetSwitch(&DomeShutterSP, "Shutter failed to %s.", (operation == 0 ? "open" : "close"));
    return IPS_ALERT;
}

IPState INDI::Dome::Park()
{
    if (capability.canPark == false)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Dome does not support parking.");
        return IPS_ALERT;
    }

    if (domeState == DOME_PARKED)
    {
        IUResetSwitch(&ParkSP);
        ParkS[0].s = ISS_ON;
        DEBUG( INDI::Logger::DBG_SESSION, "Dome already parked.");
        IDSetSwitch(&ParkSP, NULL);
        return IPS_OK;
    }

    ParkSP.s = Park();

    if (ParkSP.s == IPS_OK)
        SetParked(true);
    else if (ParkSP.s == IPS_BUSY)
    {
      domeState = DOME_PARKING;

      if (capability.canAbsMove)
                 DomeAbsPosNP.s=IPS_BUSY;

      IUResetSwitch(&ParkSP);
      ParkS[0].s = ISS_ON;
    }
    else
        IDSetSwitch(&ParkSP, NULL);

    return ParkSP.s;
}

IPState INDI::Dome::UnPark()
{
    if (capability.canPark == false)
    {
        DEBUG( INDI::Logger::DBG_ERROR, "Dome does not support parking.");
        return IPS_ALERT;
    }   

    if (domeState != DOME_PARKED)
    {
        IUResetSwitch(&ParkSP);
        ParkS[1].s = ISS_ON;
        DEBUG( INDI::Logger::DBG_SESSION, "Dome already unparked.");
        IDSetSwitch(&ParkSP, NULL);
        return IPS_OK;
    }

    if (weatherState == IPS_ALERT)
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Weather is in the danger zone! Cannot unpark dome.");
        ParkSP.s == IPS_OK;
        IDSetSwitch(&ParkSP, NULL);
        return IPS_ALERT;
    }

    ParkSP.s = UnPark();

   if (ParkSP.s == IPS_OK)
      SetParked(false);
   else if (ParkSP.s == IPS_BUSY)
         setDomeState(DOME_UNPARKING);
   else
       IDSetSwitch(&ParkSP, NULL);

    return ParkSP.s;
}

void INDI::Dome::SetCurrentPark()
{
    DEBUG( INDI::Logger::DBG_WARNING, "Parking is not supported.");
}

void INDI::Dome::SetDefaultPark()
{
    DEBUG( INDI::Logger::DBG_WARNING, "Parking is not supported.");
}



