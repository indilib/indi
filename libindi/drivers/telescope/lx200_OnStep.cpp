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

#include "lx200driver.h"
#include "indicom.h"

#include <cstring>
#include <unistd.h>
#include <termios.h>

#define LIBRARY_TAB  "Library"
#define FIRMWARE_TAB "Firmware data"
#define STATUS_TAB "ONStep Status"

#define ONSTEP_TIMEOUT  3

LX200_OnStep::LX200_OnStep() : LX200Generic()
{
    currentCatalog    = LX200_STAR_C;
    currentSubCatalog = 0;

    setVersion(1, 3);

    SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_CAN_CONTROL_TRACK);
}

const char *LX200_OnStep::getDefaultName()
{
    return (const char *)"LX200 OnStep";
}

bool LX200_OnStep::initProperties()
{
    LX200Generic::initProperties();
    // ============== MAIN_CONTROL_TAB
    IUFillText(&ObjectInfoT[0], "Info", "", "");
    IUFillTextVector(&ObjectInfoTP, ObjectInfoT, 1, getDeviceName(), "Object Info", "", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);

    IUFillNumber(&ElevationLimitN[0], "minAlt", "Speed", "%+03f", -90.0, 90.0, 0.0, 0.0); // double % typo
    IUFillNumber(&ElevationLimitN[1], "maxAlt", "Speed", "%+03f", -90.0, 90.0, 0.0, 0.0);
    IUFillNumberVector(&ElevationLimitNP, ElevationLimitN, 1, getDeviceName(), "Slew elevation Limit", "",
                       MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    /*
    IUFillSwitch(&EnaTrackS[0], "ENABLE", "TrackOn/Off", ISS_OFF);
    IUFillSwitchVector(&EnaTrackSP, EnaTrackS, 1, getDeviceName(), "TELESCOPE_TRACK_ENABLE", "Tracking On/Off",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
                       */

    IUFillSwitch(&AlignS[0], "AlignOn", "Align On", ISS_OFF);
    IUFillSwitch(&AlignS[1], "AlignOff", "Align Off", ISS_OFF);
    IUFillSwitchVector(&AlignSP, AlignS, 2, getDeviceName(), "alignOnStep", "Alignment Start", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&ReticS[0], "PLUS", "Light", ISS_OFF);
    IUFillSwitch(&ReticS[1], "MOINS", "Dark", ISS_OFF);
    IUFillSwitchVector(&ReticSP, ReticS, 2, getDeviceName(), "RETICULE_BRIGHTNESS", "Reticule +/-", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // ============== CONNECTION_TAB

    // ============== OPTION_TAB

    // ============== MOTION_CONTROL_TAB
    IUFillNumber(&MaxSlewRateN[0], "maxSlew", "Rate", "%g", 2.0, 9.0, 1.0, 9.0);
    IUFillNumberVector(&MaxSlewRateNP, MaxSlewRateN, 1, getDeviceName(), "Max slew Rate", "", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    // ============== SITE_MANAGEMENT_TAB

    // ============== GUIDE_TAB

    // ============== FOCUSER_TAB

    // ============== FIRMWARE_TAB
    IUFillText(&VersionT[0], "Date", "", "");
    IUFillText(&VersionT[1], "Time", "", "");
    IUFillText(&VersionT[2], "Number", "", "");
    IUFillText(&VersionT[3], "Name", "", "");
    //IUFillText(&VersionT[4], "Full", "", ""); //ToDo is not supported by OnStep Firmware
    IUFillTextVector(&VersionTP, VersionT, 4, getDeviceName(), "Firmware Info", "", FIRMWARE_TAB, IP_RO, 0, IPS_IDLE);

    // ============== LIBRARY_TAB
    IUFillSwitch(&StarCatalogS[0], "Star", "", ISS_ON);
    IUFillSwitch(&StarCatalogS[1], "SAO", "", ISS_OFF);
    IUFillSwitch(&StarCatalogS[2], "GCVS", "", ISS_OFF);
    IUFillSwitchVector(&StarCatalogSP, StarCatalogS, 3, getDeviceName(), "Star Catalogs", "", LIBRARY_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&DeepSkyCatalogS[0], "NGC", "", ISS_ON);
    IUFillSwitch(&DeepSkyCatalogS[1], "IC", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[2], "UGC", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[3], "Caldwell", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[4], "Arp", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[5], "Abell", "", ISS_OFF);
    IUFillSwitch(&DeepSkyCatalogS[6], "Messier", "", ISS_OFF);
    IUFillSwitchVector(&DeepSkyCatalogSP, DeepSkyCatalogS, 7, getDeviceName(), "Deep Sky Catalogs", "", LIBRARY_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

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
    IUFillSwitchVector(&SolarSP, SolarS, 10, getDeviceName(), "SOLAR_SYSTEM", "Solar System", LIBRARY_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&ObjectNoN[0], "ObjectN", "Number", "%+03f", 1.0, 1000.0, 1.0, 0);
    IUFillNumberVector(&ObjectNoNP, ObjectNoN, 1, getDeviceName(), "Object Number", "", LIBRARY_TAB, IP_RW, 0,
                       IPS_IDLE);

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
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    LX200Generic::ISGetProperties(dev);
}

bool LX200_OnStep::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        //defineSwitch(&EnaTrackSP);
        defineSwitch(&ReticSP);
        defineSwitch(&AlignSP);
        defineText(&VersionTP);
        defineNumber(&ElevationLimitNP);
        defineText(&ObjectInfoTP);
        defineSwitch(&SolarSP);
        defineSwitch(&StarCatalogSP);
        defineSwitch(&DeepSkyCatalogSP);
        defineNumber(&ObjectNoNP);
        defineNumber(&MaxSlewRateNP);
        defineText(&OnstepStatTP);
        return true;
    }
    else
    {
        //deleteProperty(EnaTrackSP.name);
        deleteProperty(ReticSP.name);
        deleteProperty(AlignSP.name);
        deleteProperty(VersionTP.name);
        deleteProperty(ElevationLimitNP.name);
        deleteProperty(ObjectInfoTP.name);
        deleteProperty(SolarSP.name);
        deleteProperty(StarCatalogSP.name);
        deleteProperty(DeepSkyCatalogSP.name);
        deleteProperty(ObjectNoNP.name);
        deleteProperty(MaxSlewRateNP.name);
        deleteProperty(OnstepStatTP.name);
        return true;
    }
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

        if (!strcmp(name, MaxSlewRateNP.name))
        {
            if (setMaxSlewRate(PortFD, (int)values[0]) < 0)
            {
                MaxSlewRateNP.s = IPS_ALERT;
                IDSetNumber(&MaxSlewRateNP, "Error setting maximum slew rate.");
                return false;
            }

            MaxSlewRateNP.s           = IPS_OK;
            MaxSlewRateNP.np[0].value = values[0];
            IDSetNumber(&MaxSlewRateNP, nullptr);
            return true;
        }

        if (!strcmp(name, ElevationLimitNP.name))
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
                    nset += minAlt >= -90.0 && minAlt <= 90.0;
                }
                else if (altp == &ElevationLimitN[1])
                {
                    maxAlt = values[i];
                    nset += maxAlt >= -90.0 && maxAlt <= 90.0;
                }
            }
            if (nset == 2)
            {
                if (setMinElevationLimit(PortFD, (int)minAlt) < 0)
                {
                    ElevationLimitNP.s = IPS_ALERT;
                    IDSetNumber(&ElevationLimitNP, "Error setting elevation limit.");
                    return false;
                }

                setMaxElevationLimit(PortFD, (int)maxAlt);
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
        // Track Enable Button
        /*
        if (!strcmp(name, EnaTrackSP.name))
        {
            int ret = 0;

            if (TrackState == SCOPE_PARKED)
            {
                IDSetSwitch(&EnaTrackSP, "Telescope is Parked, Unpark before tracking");
                return false;
            }
            if (TrackState == SCOPE_TRACKING)
            {
                IUResetSwitch(&EnaTrackSP);
                EnaTrackSP.s = IPS_IDLE;
                ret = DisTrackOnStep(PortFD);
                TrackState = SCOPE_IDLE;
                IDSetSwitch(&EnaTrackSP, "Idle");
            }
            else
            {
                IUResetSwitch(&EnaTrackSP);
                EnaTrackSP.s = IPS_OK;
                ret = EnaTrackOnStep(PortFD);
                TrackState = SCOPE_TRACKING;
                IDSetSwitch(&EnaTrackSP, "Tracking");
            }
            LX200_OnStep::OnStepStat();
            return true;
        }*/

        // Align Buttons
        if (!strcmp(name, AlignSP.name))
        {
            int ret = 0;

            IUUpdateSwitch(&AlignSP, states, names, n);
            AlignSP.s = IPS_OK;

            if (AlignS[0].s == ISS_ON)
            {
                ret = OnStepalign(PortFD);
                IDSetSwitch(&AlignSP, "Olign On");
            }
            else
            {
                ret = OnStepalign(PortFD);
                IDSetSwitch(&AlignSP, "Align Off");
            }

            IUResetSwitch(&AlignSP);
            //            AlignSP.s = IPS_IDLE;
            IDSetSwitch(&AlignSP, nullptr);
            return true;
        }


        // Reticlue +/- Buttons
        if (!strcmp(name, ReticSP.name))
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

        LX200_OnStep::OnStepStat();

        IDSetText(&VersionTP, nullptr);
    }
    //DEBUG(INDI::Logger::DBG_ERROR, "OnStep GetBasicData");
}

bool LX200_OnStep::UnPark()
{
    //int ret = 0;

    // First we unpark
    if (!isSimulation())
    {
            if (UnParkOnStep(PortFD) < 0)
            {
            DEBUG(INDI::Logger::DBG_ERROR, "OnsTep UnParking OnStep Failed.");
            return false;
            }
    }

    DEBUG(INDI::Logger::DBG_SESSION, "OnStep UnParking telescope in progress... ");
    SetParked(false);

    //ret = EnaTrackOnStep(PortFD);
    //ParkSP.s   = IPS_OK;
    SetTrackEnabled(true);
    TrackState = SCOPE_TRACKING;

    return true;
}

void LX200_OnStep::OnStepStat()
{
Errors Lasterror = ERR_NONE;

    getStatus(PortFD, OSStat);
    // ============= Tracking Status
    IUSaveText(&OnstepStat[0],OSStat);
    if (strstr(OSStat,"n")) IUSaveText(&OnstepStat[1],"Not Tracking");
    if (strstr(OSStat,"N")) IUSaveText(&OnstepStat[1],"Not Slewing");
    if (strstr(OSStat,"t")) IUSaveText(&OnstepStat[1],"Tracking");

    // ============= Refractoring
    if (strstr(OSStat,"r")) IUSaveText(&OnstepStat[2],"Refractoring On");
    if (strstr(OSStat,"s")) IUSaveText(&OnstepStat[2],"Refractoring Off");
    if (strstr(OSStat,"r") && strstr(OSStat,"t")) IUSaveText(&OnstepStat[2],"Full Comp");
    if (strstr(OSStat,"r") && !strstr(OSStat,"t")) IUSaveText(&OnstepStat[2],"Refractory Comp");

    // ============= Parkstatus
    if (strstr(OSStat,"P")) IUSaveText(&OnstepStat[3],"Parked");
    if (strstr(OSStat,"p")) IUSaveText(&OnstepStat[3],"UnParked");
    if (strstr(OSStat,"I")) IUSaveText(&OnstepStat[3],"Park in Progress");
    if (strstr(OSStat,"F")) IUSaveText(&OnstepStat[3],"Parking Failed");
    if (strstr(OSStat,"H")) IUSaveText(&OnstepStat[3],"At Home");
    if (strstr(OSStat,"W")) IUSaveText(&OnstepStat[3],"Waiting at Home");

    // ============= Pec Status
    if (!strstr(OSStat,"R") && !strstr(OSStat,"W")) IUSaveText(&OnstepStat[4],"N/A");
    if (strstr(OSStat,"R")) IUSaveText(&OnstepStat[4],"Recorded");
    if (strstr(OSStat,"W")) IUSaveText(&OnstepStat[4],"Autorecord");

    // ============= Time Sync Status
    if (!strstr(OSStat,"S")) IUSaveText(&OnstepStat[5],"N/A");
    if (strstr(OSStat,"S")) IUSaveText(&OnstepStat[5],"PPS / GPS Sync Ok");

    // ============= Mount Types
    if (strstr(OSStat,"E")) IUSaveText(&OnstepStat[6],"German Mount");
    if (strstr(OSStat,"F")) IUSaveText(&OnstepStat[6],"Fork Mount");
    if (strstr(OSStat,"k")) IUSaveText(&OnstepStat[6],"Fork Alt Mount");
    if (strstr(OSStat,"A")) IUSaveText(&OnstepStat[6],"AltAZ Mount");

    // ============= Error Code
    Lasterror=(Errors)(OSStat[strlen(OSStat)-1]-'0');
    if (Lasterror==ERR_NONE) IUSaveText(&OnstepStat[7],"None");
    if (Lasterror==ERR_MOTOR_FAULT) IUSaveText(&OnstepStat[7],"Motor Fault");
    if (Lasterror==ERR_ALT) IUSaveText(&OnstepStat[7],"Altitude Min/Max");
    if (Lasterror==ERR_LIMIT_SENSE) IUSaveText(&OnstepStat[7],"Limit Sense");
    if (Lasterror==ERR_DEC) IUSaveText(&OnstepStat[7],"Dec Limit Exceeded");
    if (Lasterror==ERR_AZM) IUSaveText(&OnstepStat[7],"Azm Limit Exceeded");
    if (Lasterror==ERR_UNDER_POLE) IUSaveText(&OnstepStat[7],"Under Pole Limit Exceeded");
    if (Lasterror==ERR_MERIDIAN) IUSaveText(&OnstepStat[7],"Meridian Limit (W) Exceeded");
    if (Lasterror==ERR_SYNC) IUSaveText(&OnstepStat[7],"Sync. ignored >30&deg;");

    DEBUG(INDI::Logger::DBG_DEBUG, OnStepStatus);
}

bool LX200_OnStep::SetTrackEnabled(bool enabled)
{
    int ret = 0;

    if (enabled)
        ret = EnaTrackOnStep(PortFD);
    else
        ret = DisTrackOnStep(PortFD);

    if (ret == 5)
    {
        LX200_OnStep::OnStepStat();
        return true;
    }

    return false;
}

bool LX200_OnStep::setLocalDate(uint8_t days, uint8_t months, uint8_t years)
{
    years = years % 100;
    char cmd[32];

    snprintf(cmd, 32, ":SC%02d/%02d/%02d#", months, days, years);

    return sendOnStepCommand(cmd);
}

bool LX200_OnStep::sendOnStepCommand(const char *cmd)
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

    return (response[0] == '1');
}

bool LX200_OnStep::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    if (isSimulation())
        return true;

    double onstep_long = longitude - 360;
    if (onstep_long < -180)
        onstep_long += 360;

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
