/*
    LX200 Classoc
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

#include "lx200classic.h"
#include "lx200driver.h"
#include "indicom.h"

#include <cstring>

#include <libnova/transform.h>

#define LIBRARY_TAB "Library"

LX200Classic::LX200Classic() : LX200Generic()
{
    MaxReticleFlashRate = 3;

    setVersion(1, 1);
}

const char *LX200Classic::getDefaultName()
{
    return "LX200 Classic";
}

bool LX200Classic::initProperties()
{
    LX200Generic::initProperties();
    SetParkDataType(PARK_AZ_ALT);

    IUFillText(&ObjectInfoT[0], "Info", "", "");
    IUFillTextVector(&ObjectInfoTP, ObjectInfoT, 1, getDeviceName(), "Object Info", "", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);

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

    IUFillNumber(&MaxSlewRateN[0], "RATE", "Rate", "%.2f", 2.0, 9.0, 1.0, 9.0);
    IUFillNumberVector(&MaxSlewRateNP, MaxSlewRateN, 1, getDeviceName(), "TELESCOPE_MAX_SLEW_RATE", "Slew Rate", MOTION_TAB,
                       IP_RW, 0, IPS_IDLE);

    IUFillNumber(&ElevationLimitN[0], "MIN_ALT", "Min Alt.", "%+.2f", -90.0, 90.0, 0.0, 0.0);
    IUFillNumber(&ElevationLimitN[1], "MAX_ALT", "Max Alt", "%+.2f", -90.0, 90.0, 0.0, 0.0);
    IUFillNumberVector(&ElevationLimitNP, ElevationLimitN, 2, getDeviceName(), "TELESCOPE_ELEVATION_SLEW_LIMIT",
                       "Slew elevation Limit", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    IUFillSwitch(&UnparkAlignmentS[0], "Polar", "", ISS_ON);
    IUFillSwitch(&UnparkAlignmentS[1], "AltAz", "", ISS_OFF);
    IUFillSwitch(&UnparkAlignmentS[2], "Land", "", ISS_OFF);
    IUFillSwitchVector(&UnparkAlignmentSP, UnparkAlignmentS, 3, getDeviceName(), "Unpark Mode", "", SITE_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    MountTypeSP.reset();
    MountTypeSP[MOUNT_ALTAZ].setState(ISS_ON);

    return true;
}

bool LX200Classic::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineProperty(&ElevationLimitNP);
        defineProperty(&ObjectInfoTP);
        defineProperty(&SolarSP);
        defineProperty(&StarCatalogSP);
        defineProperty(&DeepSkyCatalogSP);
        defineProperty(&ObjectNoNP);
        defineProperty(&MaxSlewRateNP);
        defineProperty(&UnparkAlignmentSP);

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            // Default values are poinitng to North or South Pole in AltAz coordinates.
            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());

            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].getValue() >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].getValue());
        }

        return true;
    }
    else
    {
        deleteProperty(ElevationLimitNP.name);
        deleteProperty(ObjectInfoTP.name);
        deleteProperty(SolarSP.name);
        deleteProperty(StarCatalogSP.name);
        deleteProperty(DeepSkyCatalogSP.name);
        deleteProperty(ObjectNoNP.name);
        deleteProperty(MaxSlewRateNP.name);
        deleteProperty(UnparkAlignmentSP.name);

        return true;
    }
}

bool LX200Classic::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, ObjectNoNP.name))
        {
            char object_name[256] = {0};

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

bool LX200Classic::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    int index = 0;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
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

        // Unpark Alignment Mode
        if (!strcmp(name, UnparkAlignmentSP.name))
        {
            IUUpdateSwitch(&UnparkAlignmentSP, states, names, n);
            UnparkAlignmentSP.s = IPS_OK;
            IDSetSwitch(&UnparkAlignmentSP, nullptr);

            return true;
        }

    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200Classic::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &MaxSlewRateNP);
    IUSaveConfigNumber(fp, &ElevationLimitNP);

    IUSaveConfigSwitch(fp, &UnparkAlignmentSP);

    return true;
}

//Parking
bool LX200Classic::Park()
{
    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Parking to Az (%s) Alt (%s)...", AzStr, AltStr);

    INDI::IEquatorialCoordinates equatorialCoords {0, 0};
    INDI::IHorizontalCoordinates horizontalCoords {parkAz, parkAlt};
    INDI::HorizontalToEquatorial(&horizontalCoords, &m_Location, ln_get_julian_from_sys(), &equatorialCoords);

    //save the current AlignmentMode to UnparkAlignment
    LX200Generic::getAlignment();
    int curAlignment = IUFindOnSwitchIndex(&AlignmentSP);
    IUResetSwitch(&UnparkAlignmentSP);
    UnparkAlignmentS[curAlignment].s = ISS_ON;
    UnparkAlignmentSP.s = IPS_OK;
    IDSetSwitch(&UnparkAlignmentSP, nullptr);
    saveConfig(true, UnparkAlignmentSP.name);

    if (!Goto(equatorialCoords.rightascension, equatorialCoords.declination))
    {
        ParkSP.setState(IPS_ALERT);
        LOG_ERROR("Parking Failed.");
        ParkSP.apply();
        return false;
    }

    EqNP.setState(IPS_BUSY);
    TrackState = SCOPE_PARKING;
    LOG_INFO("Parking is in progress...");

    return true;
}

bool LX200Classic::UnPark()
{
    if (isSimulation() == false)
    {
        // Parked in Land alignment. Restore previous mode.
        if (setAlignmentMode(PortFD, IUFindOnSwitchIndex(&UnparkAlignmentSP)) < 0)
        {
            LOG_ERROR("UnParking Failed.");
            AlignmentSP.s = IPS_ALERT;
            IDSetSwitch(&AlignmentSP, "Error setting alignment mode.");
            return false;
        }
        //Update the UI
        LX200Generic::getAlignment();
    }

    // Then we sync with to our last stored position
    double parkAz  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAz, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Syncing to parked coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    double parkRA  = 0.0;
    double parkDEC = 0.0;
    //azAltToRaDecNow(parkAz, parkAlt, parkRA, parkDEC);
    INDI::IEquatorialCoordinates equatorialCoords {parkRA, parkDEC};
    INDI::IHorizontalCoordinates horizontalCoords {parkAz, parkAlt};
    INDI::HorizontalToEquatorial(&horizontalCoords, &m_Location, ln_get_julian_from_sys(), &equatorialCoords);

    if (isSimulation())
    {
        currentRA = parkRA;
        currentDEC = parkDEC;
    }
    else
    {
        if ((setObjectRA(PortFD, parkRA) < 0) || (setObjectDEC(PortFD, parkDEC) < 0))
        {
            LOG_ERROR("Error setting Unpark RA/Dec.");
            return false;
        }

        char syncString[256];
        if (::Sync(PortFD, syncString) < 0)
        {
            LOG_WARN("Unpark Sync failed.");
            return false;
        }
    }

    SetParked(false);
    return true;
}

bool LX200Classic::SetCurrentPark()
{
    INDI::IEquatorialCoordinates equatorialCoords {currentRA, currentDEC};
    INDI::IHorizontalCoordinates horizontalCoords {0, 0};
    INDI::EquatorialToHorizontal(&equatorialCoords, &m_Location, ln_get_julian_from_sys(), &horizontalCoords);

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, horizontalCoords.azimuth, 2, 3600);
    fs_sexa(AltStr, horizontalCoords.altitude, 2, 3600);
    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    SetAxis1Park(horizontalCoords.azimuth);
    SetAxis2Park(horizontalCoords.altitude);

    return true;
}

bool LX200Classic::SetDefaultPark()
{
    // Az = 0 for North hemisphere
    SetAxis1Park(LocationNP[LOCATION_LATITUDE].getValue() > 0 ? 0 : 180);

    // Alt = Latitude
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].getValue());

    return true;
}

bool LX200Classic::ReadScopeStatus()
{
    int curTrackState = TrackState;
    static int settling = -1;

    if (settling >= 0) settling--;

    if ((TrackState == SCOPE_PARKED) && (settling == 0) && !isSimulation())
    {
        settling = -1;
        if (setAlignmentMode(PortFD, LX200_ALIGN_LAND) < 0)
        {
            LOG_ERROR("Parking Failed.");
            AlignmentSP.s = IPS_ALERT;
            IDSetSwitch(&AlignmentSP, "Error setting alignment mode.");
            return false;
        }
        //Update the UI
        LX200Generic::getAlignment();
        LOG_DEBUG("Mount Land mode set. Parking completed.");
    }

    if (LX200Generic::ReadScopeStatus())
    {
        if ((TrackState == SCOPE_PARKED) && (curTrackState == SCOPE_PARKING) && !isSimulation())
        {
            //allow scope to make internal state change to settled on target.
            //otherwise changing to landmode slews the scope to same
            //coordinates interpreted in landmode.
            //Between isSlewComplete() and the beep there is nearly 3 seconds!
            settling = 3; //n iterations of default 1000ms
        }
    }

    return true;
}

