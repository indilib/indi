/*
    LX200 LX200_OnStep
    Based on LX200 classic, azwing (alain@zwingelstein.org)
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "lx200_OnStep.h"

#define LIBRARY_TAB  "Library"
#define FIRMWARE_TAB "Firmware data"
#define STATUS_TAB "ONStep Status"

#define ONSTEP_TIMEOUT  3
#define RB_MAX_LEN 64

LX200_OnStep::LX200_OnStep() : LX200Generic()
{
    currentCatalog    = LX200_STAR_C;
    currentSubCatalog = 0;

    setVersion(1, 3);
    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_PEC | TELESCOPE_HAS_PIER_SIDE | TELESCOPE_HAS_TRACK_RATE, 4 );
    //CAN_ABORT, CAN_GOTO ,CAN_PARK ,CAN_SYNC ,HAS_LOCATION ,HAS_TIME ,HAS_TRACK_MODEAlready inherited from lx200generic,
    // 4 stands for the number of Slewrate Buttons as defined in Inditelescope.cpp
}

const char *LX200_OnStep::getDefaultName()
{
    return (const char *)"LX200 OnStep";
}

bool LX200_OnStep::initProperties()
{
    LX200Generic::initProperties();

    SetParkDataType(PARK_RA_DEC);

    // ============== MAIN_CONTROL_TAB
    IUFillSwitch(&ReticS[0], "PLUS", "Light", ISS_OFF);
    IUFillSwitch(&ReticS[1], "MOINS", "Dark", ISS_OFF);
    IUFillSwitchVector(&ReticSP, ReticS, 2, getDeviceName(), "RETICULE_BRIGHTNESS", "Reticule +/-", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_ALERT);

    IUFillSwitch(&OSAlignS[0], "1", "1 Star", ISS_OFF);
    IUFillSwitch(&OSAlignS[1], "2", "2 Stars", ISS_OFF);
    IUFillSwitch(&OSAlignS[2], "3", "3 Stars", ISS_OFF);
    IUFillSwitchVector(&OSAlignSP, OSAlignS, 3, getDeviceName(), "AlignStar", "Align using n stars", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&OSAlignT[0], "OSStarAlign", "Align x Star(s)", "");
    IUFillTextVector(&OSAlignTP, OSAlignT, 1, getDeviceName(), "Process Align", "", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&ElevationLimitN[0], "minAlt", "Elev Min", "%+03f", -90.0, 90.0, 1.0, -30.0);
    IUFillNumber(&ElevationLimitN[1], "maxAlt", "Elev Max", "%+03f", -90.0, 90.0, 1.0, 89.0);
    IUFillNumberVector(&ElevationLimitNP, ElevationLimitN, 2, getDeviceName(), "Slew elevation Limit", "", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    IUFillText(&ObjectInfoT[0], "Info", "", "");
    IUFillTextVector(&ObjectInfoTP, ObjectInfoT, 1, getDeviceName(), "Object Info", "", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // ============== CONNECTION_TAB

    // ============== OPTION_TAB
    IUFillNumber(&BacklashN[0], "Backlash DEC", "DE", "%g", 0, 999, 1, 15);    //test
    IUFillNumber(&BacklashN[1], "Backlash RA", "RA", "%g", 0, 999, 1, 15);    //test
    IUFillNumberVector(&BacklashNP, BacklashN, 2, getDeviceName(), "Backlash", "", MOTION_TAB, IP_RW, 0,IPS_IDLE);    //test

    // ============== MOTION_CONTROL_TAB

    IUFillNumber(&MaxSlewRateN[0], "maxSlew", "Rate", "%g", 1.0, 9.0, 1.0, 5.0);    //2.0, 9.0, 1.0, 9.0
    IUFillNumberVector(&MaxSlewRateNP, MaxSlewRateN, 1, getDeviceName(), "Max slew Rate", "", MOTION_TAB, IP_RW, 0,IPS_IDLE);

    IUFillSwitch(&TrackCompS[0], "1", "Full Compensation", ISS_OFF);
    IUFillSwitch(&TrackCompS[1], "2", "Refraction", ISS_OFF);
    IUFillSwitch(&TrackCompS[2], "3", "Off", ISS_OFF);
    IUFillSwitchVector(&TrackCompSP, TrackCompS, 3, getDeviceName(), "Compensation", "Compensation Tracking", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // ============== SITE_MANAGEMENT_TAB
    IUFillSwitch(&SetHomeS[0], "COLD_START", "Cold Start", ISS_OFF);
    IUFillSwitch(&SetHomeS[1], "WARM_START", "Init Home", ISS_OFF);
    IUFillSwitchVector(&SetHomeSP, SetHomeS, 2, getDeviceName(), "HOME_INIT", "Homing", SITE_TAB, IP_RW, ISR_1OFMANY, 60, IPS_ALERT);

    // ============== GUIDE_TAB

    // ============== FOCUSER_TAB

    // ============== FIRMWARE_TAB
    IUFillText(&VersionT[0], "Date", "", "");
    IUFillText(&VersionT[1], "Time", "", "");
    IUFillText(&VersionT[2], "Number", "", "");
    IUFillText(&VersionT[3], "Name", "", "");
    IUFillTextVector(&VersionTP, VersionT, 4, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    // ============== LIBRARY_TAB
    IUFillSwitch(&StarCatalogS[0], "Star", "", ISS_ON);
    IUFillSwitch(&StarCatalogS[1], "SAO", "", ISS_OFF);
    IUFillSwitch(&StarCatalogS[2], "GCVS", "", ISS_OFF);
    IUFillSwitchVector(&StarCatalogSP, StarCatalogS, 3, getDeviceName(), "Star Catalogs", "", LIBRARY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&DeepSkyCatalogS[0], "NGC", "", ISS_ON);
    IUFillSwitch(&DeepSkyCatalogS[1], "IC", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[2], "UGC", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[3], "Caldwell", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[4], "Arp", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[5], "Abell", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[6], "Messier", "", ISS_OFF);
    IUFillSwitchVector(&DeepSkyCatalogSP, DeepSkyCatalogS, 7, getDeviceName(), "Deep Sky Catalogs", "", LIBRARY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&SolarS[0], "Select", "Select item", ISS_ON);
    IUFillSwitch(&SolarS[1], "1", "Mercury", ISS_OFF);
    IUFillSwitch(&SolarS[2], "2", "Venus", ISS_OFF);
    IUFillSwitch(&SolarS[3], "3", "Moon", ISS_OFF);
    IUFillSwitch(&SolarS[4], "4", "Mars", ISS_OFF);
    IUFillSwitch(&SolarS[5], "5", "Jupiter", ISS_OFF);
    IUFillSwitch(&SolarS[6], "6", "Saturn", ISS_OFF);
    IUFillSwitch(&SolarS[7], "7", "Uranus", ISS_OFF);
    IUFillSwitch(&SolarS[8], "8", "Neptune", ISS_OFF);
    IUFillSwitch(&SolarS[9], "9", "Pluto", ISS_OFF);
    IUFillSwitchVector(&SolarSP, SolarS, 10, getDeviceName(), "SOLAR_SYSTEM", "Solar System", LIBRARY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&ObjectNoN[0], "ObjectN", "Number", "%+03f", 1.0, 1000.0, 1.0, 0);
    IUFillNumberVector(&ObjectNoNP, ObjectNoN, 1, getDeviceName(), "Object Number", "", LIBRARY_TAB, IP_RW, 0, IPS_IDLE);

    // ============== STATUS_TAB
    IUFillText(&OnstepStat[0], ":GU# return", "", "");
    IUFillText(&OnstepStat[1], "Tracking", "", "");
    IUFillText(&OnstepStat[2], "Refractoring", "", "");
    IUFillText(&OnstepStat[3], "Park", "", "");
    IUFillText(&OnstepStat[4], "Pec", "", "");
    IUFillText(&OnstepStat[5], "TimeSync", "", "");
    IUFillText(&OnstepStat[6], "Mount Type", "", "");
    IUFillText(&OnstepStat[7], "Error", "", "");
    IUFillTextVector(&OnstepStatTP, OnstepStat, 8, getDeviceName(), "OnStep Status", "", STATUS_TAB, IP_RO, 0, IPS_IDLE);

    return true;
}

void LX200_OnStep::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0) return;
    LX200Generic::ISGetProperties(dev);
}

bool LX200_OnStep::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineSwitch(&ReticSP);
        defineSwitch(&OSAlignSP);
        defineText(&OSAlignTP);
        defineText(&VersionTP);
        defineNumber(&ElevationLimitNP);
        defineText(&ObjectInfoTP);
        defineSwitch(&SolarSP);
        defineSwitch(&StarCatalogSP);
        defineSwitch(&DeepSkyCatalogSP);
        defineNumber(&ObjectNoNP);
        defineNumber(&MaxSlewRateNP);
        defineText(&OnstepStatTP);
        defineSwitch(&ParkOptionSP);
        defineSwitch(&SetHomeSP);
        defineSwitch(&TrackCompSP);
        defineNumber(&BacklashNP);  //test

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value);

            SetAxis1ParkDefault(LocationN[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationN[LOCATION_LATITUDE].value);
         }

         double longitude=-1000, latitude=-1000;
         // Get value from config file if it exists.
         IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
         IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
         if (longitude != -1000 && latitude != -1000)
         {
             updateLocation(latitude, longitude, 0);
         }
    }
    else
    {
        deleteProperty(ReticSP.name);
        deleteProperty(OSAlignSP.name);
        deleteProperty(OSAlignTP.name);
        deleteProperty(VersionTP.name);
        deleteProperty(ElevationLimitNP.name);
        deleteProperty(ObjectInfoTP.name);
        deleteProperty(SolarSP.name);
        deleteProperty(StarCatalogSP.name);
        deleteProperty(DeepSkyCatalogSP.name);
        deleteProperty(ObjectNoNP.name);
        deleteProperty(MaxSlewRateNP.name);
        deleteProperty(OnstepStatTP.name);
        deleteProperty(ParkOptionSP.name);
        deleteProperty(SetHomeSP.name);
        deleteProperty(TrackCompSP.name);
        deleteProperty(BacklashNP.name);    //test
    }
    return true;
}

bool LX200_OnStep::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, ObjectNoNP.name))
        {
            char object_name[256];

            if (selectCatalogObject(PortFD, currentCatalog, (int)values[0]) < 0)
            {
                ObjectNoNP.s = IPS_ALERT;
                IDSetNumber(&ObjectNoNP, "Failed to select catalog object.");
                return false;
            }

            getLX200RA(PortFD, &targetRA);
            getLX200DEC(PortFD, &targetDEC);

            ObjectNoNP.s = IPS_OK;
            IDSetNumber(&ObjectNoNP, "Object updated.");

            if (getObjectInfo(PortFD, object_name) < 0)
                IDMessage(getDeviceName(), "Getting object info failed.");
            else
            {
                IUSaveText(&ObjectInfoTP.tp[0], object_name);
                IDSetText(&ObjectInfoTP, nullptr);
            }
            Goto(targetRA, targetDEC);
            return true;
        }

        if (!strcmp(name, MaxSlewRateNP.name))      // Tested
        {
            int ret;
            char cmd[4];
            snprintf(cmd, 4, ":R%d#", (int)values[0]);
            ret = sendOnStepCommandBlind(cmd);

            //if (setMaxSlewRate(PortFD, (int)values[0]) < 0) //(int) MaxSlewRateN[0].value
            if (ret == -1)
            {
                DEBUGF(INDI::Logger::DBG_DEBUG, "Pas OK Return value =%d", ret);
                DEBUGF(INDI::Logger::DBG_DEBUG, "Setting Max Slew Rate to %f\n", values[0]);
                MaxSlewRateNP.s = IPS_ALERT;
                IDSetNumber(&MaxSlewRateNP, "Setting Max Slew Rate Failed");
                return false;
            }
            DEBUGF(INDI::Logger::DBG_DEBUG, "OK Return value =%d", ret);
            MaxSlewRateNP.s           = IPS_OK;
            MaxSlewRateNP.np[0].value = values[0];
            IDSetNumber(&MaxSlewRateNP, "Slewrate set to %04.1f", values[0]);
            return true;
        }

        if (!strcmp(name, BacklashNP.name))      // tested
        {
            char cmd[9];
            int i, nset;
            double bklshdec=0, bklshra=0;

            for (nset = i = 0; i < n; i++)
            {
                INumber *bktp = IUFindNumber(&BacklashNP, names[i]);
                if (bktp == &BacklashN[0])
                {
                    bklshdec = values[i];
                    //DEBUGF(INDI::Logger::DBG_SESSION, "===CMD==> Backlash DEC= %f", bklshdec);
                    nset += bklshdec >= 0 && bklshdec <= 999;  //range 0 to 999
                }
                else if (bktp == &BacklashN[1])
                {
                    bklshra = values[i];
                    //DEBUGF(INDI::Logger::DBG_SESSION, "===CMD==> Backlash RA= %f", bklshra);
                    nset += bklshra >= 0 && bklshra <= 999;   //range 0 to 999
                }
            }
            if (nset == 2)
            {
                snprintf(cmd, 9, ":$BD%d#", (int)bklshdec);
                if (sendOnStepCommand(cmd))
                {
                    BacklashNP.s = IPS_ALERT;
                    IDSetNumber(&BacklashNP, "Error Backlash DEC limit.");
                }

                usleep(100000);

                snprintf(cmd, 9, ":$BR%d#", (int)bklshra);
                if (sendOnStepCommand(cmd))
                {
                    BacklashNP.s = IPS_ALERT;
                    IDSetNumber(&BacklashNP, "Error Backlash RA limit.");
                }

                BacklashNP.np[0].value = bklshdec;
                BacklashNP.np[1].value = bklshra;
                BacklashNP.s           = IPS_OK;
                IDSetNumber(&BacklashNP, nullptr);
                return true;
            }
            else
            {
                BacklashNP.s = IPS_ALERT;
                IDSetNumber(&BacklashNP, "Backlash invalid.");
                return false;
            }
        }

        if (!strcmp(name, ElevationLimitNP.name))       // Tested
        {
            // new elevation limits
            double minAlt = 0, maxAlt = 0;
            int i, nset;

            for (nset = i = 0; i < n; i++)
            {
                INumber *altp = IUFindNumber(&ElevationLimitNP, names[i]);
                if (altp == &ElevationLimitN[0])
                {
                    minAlt = values[i];
                    nset += minAlt >= -30.0 && minAlt <= 30.0;  //range -30 to 30
                }
                else if (altp == &ElevationLimitN[1])
                {
                    maxAlt = values[i];
                    nset += maxAlt >= 60.0 && maxAlt <= 90.0;   //range 60 to 90
                }
            }
            if (nset == 2)
            {
                if (setMinElevationLimit(PortFD, (int)minAlt) < 0)
                {
                    ElevationLimitNP.s = IPS_ALERT;
                    IDSetNumber(&ElevationLimitNP, "Error setting min elevation limit.");
                }

                if (setMaxElevationLimit(PortFD, (int)maxAlt) < 0)
                {
                    ElevationLimitNP.s = IPS_ALERT;
                    IDSetNumber(&ElevationLimitNP, "Error setting max elevation limit.");
                    return false;
                }
                ElevationLimitNP.np[0].value = minAlt;
                ElevationLimitNP.np[1].value = maxAlt;
                ElevationLimitNP.s           = IPS_OK;
                IDSetNumber(&ElevationLimitNP, nullptr);
                return true;
            }
            else
            {
                ElevationLimitNP.s = IPS_IDLE;
                IDSetNumber(&ElevationLimitNP, "elevation limit missing or invalid.");
                return false;
            }
        }
    }
    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200_OnStep::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        // Align Buttons
        if (!strcmp(name, OSAlignSP.name))      // Tested
        {
            int ret = 0;

            if (IUUpdateSwitch(&OSAlignSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&OSAlignSP);

            if (index == 0)
            {
                //IDSetText(&OSAlignTP,"1 Stars Align");
                IUSaveText(&OSAlignT[0],"1 Star Align: choose a star => Goto => center => sync");
                ret = OnStepalign1(PortFD);
                DEBUG(INDI::Logger::DBG_WARNING, "choix1");
            }
            if (index == 1)
            {
                IUSaveText(&OSAlignT[0],"2 Stars Align: choose a star => Goto => center => sync");
                ret = OnStepalign2(PortFD);
                DEBUG(INDI::Logger::DBG_WARNING, "choix2");
            }
            if (index == 2)
            {
                IUSaveText(&OSAlignT[0],"3 Star Align: choose a star => Goto => center => sync");
                ret = OnStepalign3(PortFD);
                DEBUG(INDI::Logger::DBG_WARNING, "choix3");
            }
            OSAlignSP.s = IPS_OK;
            IDSetSwitch(&OSAlignSP, nullptr);
            IDSetText(&OSAlignTP, nullptr);

        }

        // Reticlue +/- Buttons
        if (!strcmp(name, ReticSP.name))      // Tested
        {
            int ret = 0;

            IUUpdateSwitch(&ReticSP, states, names, n);
            ReticSP.s = IPS_OK;

            if (ReticS[0].s == ISS_ON)
            {
                ret = ReticPlus(PortFD);
                IDSetSwitch(&ReticSP, "Bright");
            }
            else
            {
                ret = ReticMoins(PortFD);
                IDSetSwitch(&ReticSP, "Dark");
            }

            IUResetSwitch(&ReticSP);
            //            ReticSP.s = IPS_IDLE;
            IDSetSwitch(&ReticSP, nullptr);
            return true;
        }

        // Homing, Cold and Warm Init
        if (!strcmp(name, SetHomeSP.name))      // Tested
        {
            IUUpdateSwitch(&SetHomeSP, states, names, n);
            SetHomeSP.s = IPS_OK;

            if (SetHomeS[0].s == ISS_ON)
            {
                if(sendOnStepCommandBlind(":hC#"))
                IDSetSwitch(&SetHomeSP, "Cold Start");
                return false;
            }
            else
            {
                if(sendOnStepCommandBlind(":hF#"))
                IDSetSwitch(&SetHomeSP, "Home Init");
                return false;
            }
            IUResetSwitch(&ReticSP);
            SetHomeSP.s = IPS_IDLE;
            IDSetSwitch(&SetHomeSP, nullptr);
            return true;
        }

        // Tracking Compensation selection
        if (!strcmp(name, TrackCompSP.name))      // Tested
        {
            IUUpdateSwitch(&TrackCompSP, states, names, n);
            TrackCompSP.s = IPS_OK;

            if (TrackCompS[0].s == ISS_ON)
            {
                if (!sendOnStepCommand(":To#"))
                {
                    IDSetSwitch(&TrackCompSP, "Full Compensated Tracking On");
                    return true;
                }
            }
            if (TrackCompS[1].s == ISS_ON)
            {
                if (!sendOnStepCommand(":Tr#"))
                {
                    IDSetSwitch(&TrackCompSP, "Refraction Tracking On");
                    return true;
                }
            }
            if (TrackCompS[2].s == ISS_ON)
            {
                if (!sendOnStepCommand(":Tn#"))
                {
                    IDSetSwitch(&TrackCompSP, "Refraction Tracking Disabled");
                    return true;
                }
            }
            IUResetSwitch(&TrackCompSP);
            TrackCompSP.s = IPS_IDLE;
            IDSetSwitch(&TrackCompSP, nullptr);
            return true;
        }

        // Star Catalog
        if (!strcmp(name, StarCatalogSP.name))
        {
            IUResetSwitch(&StarCatalogSP);
            IUUpdateSwitch(&StarCatalogSP, states, names, n);
            index = IUFindOnSwitchIndex(&StarCatalogSP);

            currentCatalog = LX200_STAR_C;

            if (selectSubCatalog(PortFD, currentCatalog, index))
            {
                currentSubCatalog = index;
                StarCatalogSP.s   = IPS_OK;
                IDSetSwitch(&StarCatalogSP, nullptr);
                return true;
            }
            else
            {
                StarCatalogSP.s = IPS_IDLE;
                IDSetSwitch(&StarCatalogSP, "Catalog unavailable.");
                return false;
            }
        }

        // Deep sky catalog
        if (!strcmp(name, DeepSkyCatalogSP.name))
        {
            IUResetSwitch(&DeepSkyCatalogSP);
            IUUpdateSwitch(&DeepSkyCatalogSP, states, names, n);
            index = IUFindOnSwitchIndex(&DeepSkyCatalogSP);

            if (index == LX200_MESSIER_C)
            {
                currentCatalog     = index;
                DeepSkyCatalogSP.s = IPS_OK;
                IDSetSwitch(&DeepSkyCatalogSP, nullptr);
            }
            else
                currentCatalog = LX200_DEEPSKY_C;

            if (selectSubCatalog(PortFD, currentCatalog, index))
            {
                currentSubCatalog  = index;
                DeepSkyCatalogSP.s = IPS_OK;
                IDSetSwitch(&DeepSkyCatalogSP, nullptr);
            }
            else
            {
                DeepSkyCatalogSP.s = IPS_IDLE;
                IDSetSwitch(&DeepSkyCatalogSP, "Catalog unavailable");
                return false;
            }

            return true;
        }

        // Solar system
        if (!strcmp(name, SolarSP.name))
        {
            if (IUUpdateSwitch(&SolarSP, states, names, n) < 0)
                return false;

            index = IUFindOnSwitchIndex(&SolarSP);

            // We ignore the first option : "Select item"
            if (index == 0)
            {
                SolarSP.s = IPS_IDLE;
                IDSetSwitch(&SolarSP, nullptr);
                return true;
            }

            selectSubCatalog(PortFD, LX200_STAR_C, LX200_STAR);
            selectCatalogObject(PortFD, LX200_STAR_C, index + 900);

            ObjectNoNP.s = IPS_OK;
            SolarSP.s    = IPS_OK;

            getObjectInfo(PortFD, ObjectInfoTP.tp[0].text);
            IDSetNumber(&ObjectNoNP, "Object updated.");
            IDSetSwitch(&SolarSP, nullptr);

            if (currentCatalog == LX200_STAR_C || currentCatalog == LX200_DEEPSKY_C)
                selectSubCatalog(PortFD, currentCatalog, currentSubCatalog);

            getObjectRA(PortFD, &targetRA);
            getObjectDEC(PortFD, &targetDEC);

            Goto(targetRA, targetDEC);
             return true;
        }
    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

void LX200_OnStep::getBasicData()
{
    // process parent
    LX200Generic::getBasicData();

    if (!isSimulation())
    {
        char buffer[128];
        getVersionDate(PortFD, buffer);
        IUSaveText(&VersionT[0], buffer);
        getVersionTime(PortFD, buffer);
        IUSaveText(&VersionT[1], buffer);
        getVersionNumber(PortFD, buffer);
        IUSaveText(&VersionT[2], buffer);
        getProductName(PortFD, buffer);
        IUSaveText(&VersionT[3], buffer);

        IDSetText(&VersionTP, nullptr);

            if (InitPark())
            {
                // If loading parking data is successful, we just set the default parking values.
                DEBUG(INDI::Logger::DBG_SESSION, "=============== Parkdata loaded");
                //SetAxis1ParkDefault(currentRA);
                //SetAxis2ParkDefault(currentDEC);
            }
            else
            {
                // Otherwise, we set all parking data to default in case no parking data is found.
                DEBUG(INDI::Logger::DBG_SESSION, "=============== Parkdata Load Failed");
                //SetAxis1Park(currentRA);
                //SetAxis2Park(currentDEC);
                //SetAxis1ParkDefault(currentRA);
                //SetAxis2ParkDefault(currentDEC);
            }
    }
}

//======================== Parking =======================
bool LX200_OnStep::SetCurrentPark()      // Tested
{
    char response[32];

    if(!getCommandString(PortFD, response, ":hQ#"))
        {
            DEBUGF(INDI::Logger::DBG_WARNING, "===CMD==> Set Park Pos %s", response);
            return false;
        }
    SetAxis1Park(currentRA);
    SetAxis2Park(currentDEC);
    DEBUG(INDI::Logger::DBG_WARNING, "Park Value set to current postion");
    return true;
}

bool LX200_OnStep::SetDefaultPark()      // Tested
{
    IDMessage(getDeviceName(), "Setting Park Data to Default.");
    SetAxis1Park(20);
    SetAxis2Park(80);
    DEBUG(INDI::Logger::DBG_WARNING, "Park Position set to Default value, 20/80");
    return true;
}

bool LX200_OnStep::UnPark()      // Tested
{
    char response[32];

    if (!isSimulation())
    {
        if(!getCommandString(PortFD, response, ":hR#"))
        {
            return false;
        }
     }
    return true;
}

bool LX200_OnStep::Park()      // Tested
{
    if (!isSimulation())
    {
        // If scope is moving, let's stop it first.
        if (EqNP.s == IPS_BUSY)
        {
            if (!isSimulation() && abortSlew(PortFD) < 0)
            {
                AbortSP.s = IPS_ALERT;
                IDSetSwitch(&AbortSP, "Abort slew failed.");
                return false;
            }
            AbortSP.s = IPS_OK;
            EqNP.s    = IPS_IDLE;
            IDSetSwitch(&AbortSP, "Slew aborted.");
            IDSetNumber(&EqNP, nullptr);

            if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
            {
                MovementNSSP.s = MovementWESP.s = IPS_IDLE;
                EqNP.s                          = IPS_IDLE;
                IUResetSwitch(&MovementNSSP);
                IUResetSwitch(&MovementWESP);

                IDSetSwitch(&MovementNSSP, nullptr);
                IDSetSwitch(&MovementWESP, nullptr);
            }
        }
        if (!isSimulation() && slewToPark(PortFD) < 0)
        {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "Parking Failed.");
            return false;
        }
    }
    ParkSP.s   = IPS_BUSY;
    return true;
}

// Periodically Polls OnStep Parameter from controller
bool LX200_OnStep::ReadScopeStatus()      // Tested
{
    char OSbacklashDEC[5];
    char OSbacklashRA[5];
    Errors Lasterror = ERR_NONE;

    if (isSimulation()) //if Simulation is selected
    {
        mountSim();
        return true;
    }

    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0) // Update actual position
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    NewRaDec(currentRA, currentDEC);    // Update Scope Position

    getCommandString(PortFD,OSStat,":GU#"); // :GU# returns a string containg controller status
    if (strcmp(OSStat,OldOSStat) != 0)  //if status changed
    {
    // ============= Telescope Status
    strcpy(OldOSStat ,OSStat);

    IUSaveText(&OnstepStat[0],OSStat);
    if (strstr(OSStat,"n") && strstr(OSStat,"N")) {IUSaveText(&OnstepStat[1],"Tracking Off"); }
    if (strstr(OSStat,"n") && !strstr(OSStat,"N"))
    {
        IUSaveText(&OnstepStat[1],"Sleewing");
        TrackState=SCOPE_SLEWING;
    }
    if (strstr(OSStat,"N") && !strstr(OSStat,"n"))
    {
        IUSaveText(&OnstepStat[1],"Tracking");
        TrackState=SCOPE_TRACKING;
    }

    // ============= Refractoring
    if (strstr(OSStat,"r")) {IUSaveText(&OnstepStat[2],"Refractoring On"); }
    if (strstr(OSStat,"s")) {IUSaveText(&OnstepStat[2],"Refractoring Off"); }
    if (strstr(OSStat,"r") && strstr(OSStat,"t")) {IUSaveText(&OnstepStat[2],"Full Comp"); }
    if (strstr(OSStat,"r") && !strstr(OSStat,"t")) { IUSaveText(&OnstepStat[2],"Refractory Comp"); }

    // ============= Parkstatus
    if(FirstRead)   // it is the first time I read the status so I need to update
    {
        if (strstr(OSStat,"P"))
        {
            TrackState=SCOPE_PARKED;
            SetParked(true);
            IUSaveText(&OnstepStat[3],"Parked");
         }
        if (strstr(OSStat,"F"))
        {
            TrackState=SCOPE_IDLE;
            SetParked(false);
            IUSaveText(&OnstepStat[3],"Parking Failed");
        }
        if (strstr(OSStat,"I"))
        {
            TrackState=SCOPE_PARKING;
            SetParked(false);
            IUSaveText(&OnstepStat[3],"Park in Progress");
        }
        if (strstr(OSStat,"p"))
        {
            TrackState=SCOPE_IDLE;
            SetParked(false);
            IUSaveText(&OnstepStat[3],"UnParked");
        }
    FirstRead=false;
    }
    else
    {
        if (!isParked())
        {
            if(strstr(OSStat,"P"))
            {
                TrackState=SCOPE_PARKED;
                SetParked(true);
                IUSaveText(&OnstepStat[3],"Parked");
                //DEBUG(INDI::Logger::DBG_SESSION, "OnStep Parking Succeded");
            }
            if (strstr(OSStat,"I"))
            {
                TrackState=SCOPE_PARKING;
                SetParked(false);
                IUSaveText(&OnstepStat[3],"Park in Progress");
                DEBUG(INDI::Logger::DBG_SESSION, "OnStep Parking in Progress...");
            }
        }
        if (isParked())
        {
            if (strstr(OSStat,"F"))
            {
                TrackState=SCOPE_IDLE;
                SetParked(false);
                IUSaveText(&OnstepStat[3],"Parking Failed");
                DEBUG(INDI::Logger::DBG_ERROR, "OnStep Parking failed, need to re Init OnStep at home");
            }
            if (strstr(OSStat,"p"))
            {
                //TrackState=SCOPE_IDLE;
                SetParked(false);
                IUSaveText(&OnstepStat[3],"UnParked");
                //DEBUG(INDI::Logger::DBG_SESSION, "OnStep Unparked...");
            }
        }
    }

      
    if (strstr(OSStat,"H")) { IUSaveText(&OnstepStat[3],"At Home"); }
    if (strstr(OSStat,"W")) { IUSaveText(&OnstepStat[3],"Waiting at Home"); }

    // ============= Pec Status
    if (!strstr(OSStat,"R") && !strstr(OSStat,"W")) { IUSaveText(&OnstepStat[4],"N/A"); }
    if (strstr(OSStat,"R")) { IUSaveText(&OnstepStat[4],"Recorded"); }
    if (strstr(OSStat,"W")) { IUSaveText(&OnstepStat[4],"Autorecord"); }

    // ============= Time Sync Status
    if (!strstr(OSStat,"S")) { IUSaveText(&OnstepStat[5],"N/A"); }
    if (strstr(OSStat,"S")) { IUSaveText(&OnstepStat[5],"PPS / GPS Sync Ok"); }

    // ============= Mount Types
    if (strstr(OSStat,"E")) { IUSaveText(&OnstepStat[6],"German Mount"); }
    if (strstr(OSStat,"K")) { IUSaveText(&OnstepStat[6],"Fork Mount"); }
    if (strstr(OSStat,"k")) { IUSaveText(&OnstepStat[6],"Fork Alt Mount"); }
    if (strstr(OSStat,"A")) { IUSaveText(&OnstepStat[6],"AltAZ Mount"); }

    // ============= Error Code
    Lasterror=(Errors)(OSStat[strlen(OSStat)-1]-'0');
    if (Lasterror==ERR_NONE) { IUSaveText(&OnstepStat[7],"None"); }
    if (Lasterror==ERR_MOTOR_FAULT) { IUSaveText(&OnstepStat[7],"Motor Fault"); }
    if (Lasterror==ERR_ALT) { IUSaveText(&OnstepStat[7],"Altitude Min/Max"); }
    if (Lasterror==ERR_LIMIT_SENSE) { IUSaveText(&OnstepStat[7],"Limit Sense"); }
    if (Lasterror==ERR_DEC) { IUSaveText(&OnstepStat[7],"Dec Limit Exceeded"); }
    if (Lasterror==ERR_AZM) { IUSaveText(&OnstepStat[7],"Azm Limit Exceeded"); }
    if (Lasterror==ERR_UNDER_POLE) { IUSaveText(&OnstepStat[7],"Under Pole Limit Exceeded"); }
    if (Lasterror==ERR_MERIDIAN) { IUSaveText(&OnstepStat[7],"Meridian Limit (W) Exceeded"); }
    if (Lasterror==ERR_SYNC) { IUSaveText(&OnstepStat[7],"Sync. ignored >30&deg;"); }
    }

    // Get actual Pier Side
    getCommandString(PortFD,OSPier,":Gm#");
    if (strcmp(OSPier, OldOSPier) !=0)  // any change ?
    {
        strcpy(OldOSPier, OSPier);
        switch(OSPier[0])
        {
        case 'E':
            setPierSide(PIER_EAST);
        break;

        case 'W':
            setPierSide(PIER_WEST);
        break;

        case 'N':
            setPierSide(PIER_UNKNOWN);
        break;

        case '?':
            setPierSide(PIER_UNKNOWN);
        break;
        }
    }

    //========== Get actual Backlash values
    getCommandString(PortFD,OSbacklashDEC, ":%BD#");
    getCommandString(PortFD,OSbacklashRA, ":%BR#");
    BacklashNP.np[0].value = atof(OSbacklashDEC);
    BacklashNP.np[1].value = atof(OSbacklashRA);
    IDSetNumber(&BacklashNP, nullptr);

    // Update OnStep Status TAB
    IDSetText(&OnstepStatTP, "==> Update OnsTep Status");
    return true;
}


bool LX200_OnStep::SetTrackEnabled(bool enabled) //track On/Off events handled by inditelescope       Tested
{
    char response[32];

    if (enabled)
    {
        if(!getCommandString(PortFD, response, ":Te#"))
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "===CMD==> Track On %s", response);
            return false;
        }
    }
    else
    {
    if(!getCommandString(PortFD, response, ":Td#"))
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "===CMD==> Track Off %s", response);
            return false;
        }
    }
    return true;
}

bool LX200_OnStep::setLocalDate(uint8_t days, uint8_t months, uint16_t years)      // Tested
{
    years = years % 100;
    char cmd[32];

    snprintf(cmd, 32, ":SC%02d/%02d/%02d#", months, days, years);

    if (!sendOnStepCommand(cmd)) return true;
    return false;
}

bool LX200_OnStep::sendOnStepCommandBlind(const char *cmd)
{
    int error_type;
    int nbytes_write = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    return 1;
}

bool LX200_OnStep::sendOnStepCommand(const char *cmd)      // Tested
{
    char response[1];
    int error_type;
    int nbytes_write = 0, nbytes_read = 0;

    DEBUGF(DBG_SCOPE, "CMD <%s>", cmd);

    tcflush(PortFD, TCIFLUSH);

    if ((error_type = tty_write_string(PortFD, cmd, &nbytes_write)) != TTY_OK)
        return error_type;

    error_type = tty_read(PortFD, response, 1, ONSTEP_TIMEOUT, &nbytes_read);

    tcflush(PortFD, TCIFLUSH);

    if (nbytes_read < 1)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Unable to parse response.");
        return error_type;
    }

    return (response[0] == '0');
}

bool LX200_OnStep::updateLocation(double latitude, double longitude, double elevation)      // Tested
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    double onstep_long = 360 - longitude ;
    if (onstep_long < -180)
        onstep_long += 360;
    if (onstep_long > 180)
        onstep_long -= 360;


    if (!isSimulation() && setSiteLongitude(PortFD, onstep_long) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting site longitude coordinates");
        return false;
    }

    if (!isSimulation() && setSiteLatitude(PortFD, latitude) < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Error setting site latitude coordinates");
        return false;
    }

    char l[32]={0}, L[32]={0};
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    DEBUGF(INDI::Logger::DBG_SESSION, "Site location updated to Lat %.32s - Long %.32s", l, L);

    return true;
}

int LX200_OnStep::setMaxElevationLimit(int fd, int max)   // According to standard command is :SoDD*#       Tested
{
    DEBUGF(INDI::Logger::DBG_SESSION, "<%s>", __FUNCTION__);

    char read_buffer[RB_MAX_LEN]={0};

    snprintf(read_buffer, sizeof(read_buffer), ":So%02d#", max);

    return (setStandardProcedure(fd, read_buffer));
}

int LX200_OnStep::setSiteLongitude(int fd, double Long)
{
    //DEBUGFDEVICE(lx200Name, DBG_SCOPE, "<%s>", __FUNCTION__);
    int d, m, s;
    char read_buffer[32];

    getSexComponents(Long, &d, &m, &s);

    snprintf(read_buffer, sizeof(read_buffer), ":Sg%.03d:%02d#", d, m);

    return (setStandardProcedure(fd, read_buffer));
}
