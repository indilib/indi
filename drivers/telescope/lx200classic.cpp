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

    ObjectInfoTP[0].fill("Info", "", "");
    ObjectInfoTP.fill(getDeviceName(), "Object Info", "", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);

    StarCatalogSP[0].fill("Star", "", ISS_ON);
    StarCatalogSP[1].fill("SAO", "", ISS_OFF);
    StarCatalogSP[2].fill("GCVS", "", ISS_OFF);
    StarCatalogSP.fill(getDeviceName(), "Star Catalogs", "", LIBRARY_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    DeepSkyCatalogSP[0].fill("NGC", "", ISS_ON);
    DeepSkyCatalogSP[1].fill("IC", "", ISS_OFF);
    DeepSkyCatalogSP[2].fill("UGC", "", ISS_OFF);
    DeepSkyCatalogSP[3].fill("Caldwell", "", ISS_OFF);
    DeepSkyCatalogSP[4].fill("Arp", "", ISS_OFF);
    DeepSkyCatalogSP[5].fill("Abell", "", ISS_OFF);
    DeepSkyCatalogSP[6].fill("Messier", "", ISS_OFF);
    DeepSkyCatalogSP.fill(getDeviceName(), "Deep Sky Catalogs", "", LIBRARY_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    SolarSP[0].fill("Select", "Select item", ISS_ON);
    SolarSP[1].fill("1", "Mercury", ISS_OFF);
    SolarSP[2].fill("2", "Venus", ISS_OFF);
    SolarSP[3].fill("3", "Moon", ISS_OFF);
    SolarSP[4].fill("4", "Mars", ISS_OFF);
    SolarSP[5].fill("5", "Jupiter", ISS_OFF);
    SolarSP[6].fill("6", "Saturn", ISS_OFF);
    SolarSP[7].fill("7", "Uranus", ISS_OFF);
    SolarSP[8].fill("8", "Neptune", ISS_OFF);
    SolarSP[9].fill("9", "Pluto", ISS_OFF);
    SolarSP.fill(getDeviceName(), "SOLAR_SYSTEM", "Solar System", LIBRARY_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    ObjectNoNP[0].fill("ObjectN", "Number", "%+03f", 1.0, 1000.0, 1.0, 0);
    ObjectNoNP.fill(getDeviceName(), "Object Number", "", LIBRARY_TAB, IP_RW, 0,
                       IPS_IDLE);

    MaxSlewRateNP[0].fill("RATE", "Rate", "%.2f", 2.0, 9.0, 1.0, 9.0);
    MaxSlewRateNP.fill(getDeviceName(), "TELESCOPE_MAX_SLEW_RATE", "Slew Rate", MOTION_TAB,
                       IP_RW, 0, IPS_IDLE);

    ElevationLimitNP[0].fill("MIN_ALT", "Min Alt.", "%+.2f", -90.0, 90.0, 0.0, 0.0);
    ElevationLimitNP[1].fill("MAX_ALT", "Max Alt", "%+.2f", -90.0, 90.0, 0.0, 0.0);
    ElevationLimitNP.fill(getDeviceName(), "TELESCOPE_ELEVATION_SLEW_LIMIT",
                       "Slew elevation Limit", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    UnparkAlignmentSP[0].fill("Polar", "", ISS_ON);
    UnparkAlignmentSP[1].fill("AltAz", "", ISS_OFF);
    UnparkAlignmentSP[2].fill("Land", "", ISS_OFF);
    UnparkAlignmentSP.fill(getDeviceName(), "Unpark Mode", "", SITE_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    return true;
}

bool LX200Classic::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        defineProperty(ElevationLimitNP);
        defineProperty(ObjectInfoTP);
        defineProperty(SolarSP);
        defineProperty(StarCatalogSP);
        defineProperty(DeepSkyCatalogSP);
        defineProperty(ObjectNoNP);
        defineProperty(MaxSlewRateNP);
        defineProperty(UnparkAlignmentSP);

        if (InitPark())
        {
            // If loading parking data is successful, we just set the default parking values.
            // Default values are poinitng to North or South Pole in AltAz coordinates.
            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].value);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(LocationNP[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2Park(LocationNP[LOCATION_LATITUDE].value);

            SetAxis1ParkDefault(LocationNP[LOCATION_LATITUDE].value >= 0 ? 0 : 180);
            SetAxis2ParkDefault(LocationNP[LOCATION_LATITUDE].value);
        }

        return true;
    }
    else
    {
        deleteProperty(ElevationLimitNP.getName());
        deleteProperty(ObjectInfoTP.getName());
        deleteProperty(SolarSP.getName());
        deleteProperty(StarCatalogSP.getName());
        deleteProperty(DeepSkyCatalogSP.getName());
        deleteProperty(ObjectNoNP.getName());
        deleteProperty(MaxSlewRateNP.getName());
        deleteProperty(UnparkAlignmentSP.getName());

        return true;
    }
}

bool LX200Classic::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (ObjectNoNP.isNameMatch(name))
        {
            char object_name[256] = {0};

            if (selectCatalogObject(PortFD, currentCatalog, (int)values[0]) < 0)
            {
                ObjectNoNP.setState(IPS_ALERT);
                ObjectNoNP.apply("Failed to select catalog object.");
                return false;
            }

            getLX200RA(PortFD, &targetRA);
            getLX200DEC(PortFD, &targetDEC);

            ObjectNoNP.setState(IPS_OK);
            ObjectNoNP.apply("Object updated.");

            if (getObjectInfo(PortFD, object_name) < 0)
                IDMessage(getDeviceName(), "Getting object info failed.");
            else
            {
                IUSaveText(&ObjectInfoTP[0], object_name);
                ObjectInfoTP.apply();
            }

            Goto(targetRA, targetDEC);
            return true;
        }

        if (MaxSlewRateNP.isNameMatch(name))
        {
            if (setMaxSlewRate(PortFD, (int)values[0]) < 0)
            {
                MaxSlewRateNP.setState(IPS_ALERT);
                MaxSlewRateNP.apply("Error setting maximum slew rate.");
                return false;
            }

            MaxSlewRateNP.setState(IPS_OK);
            MaxSlewRateNP[0].setValue(values[0]);
            MaxSlewRateNP.apply();
            return true;
        }

        if (ElevationLimitNP.isNameMatch(name))
        {
            // new elevation limits
            double minAlt = 0, maxAlt = 0;
            int i, nset;

            for (nset = i = 0; i < n; i++)
            {
                INumber *altp = ElevationLimitNP.findWidgetByName(names[i]);
                if (altp == &ElevationLimitNP[0])
                {
                    minAlt = values[i];
                    nset += minAlt >= -90.0 && minAlt <= 90.0;
                }
                else if (altp == &ElevationLimitNP[1])
                {
                    maxAlt = values[i];
                    nset += maxAlt >= -90.0 && maxAlt <= 90.0;
                }
            }
            if (nset == 2)
            {
                if (setMinElevationLimit(PortFD, (int)minAlt) < 0)
                {
                    ElevationLimitNP.setState(IPS_ALERT);
                    ElevationLimitNP.apply("Error setting elevation limit.");
                    return false;
                }

                setMaxElevationLimit(PortFD, (int)maxAlt);
                ElevationLimitNP[0].setValue(minAlt);
                ElevationLimitNP[1].setValue(maxAlt);
                ElevationLimitNP.setState(IPS_OK);
                ElevationLimitNP.apply();
                return true;
            }
            else
            {
                ElevationLimitNP.setState(IPS_IDLE);
                ElevationLimitNP.apply("elevation limit missing or invalid.");
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
        if (StarCatalogSP.isNameMatch(name))
        {
            StarCatalogSP.reset();
            StarCatalogSP.update(states, names, n);
            index = StarCatalogSP.findOnSwitchIndex();

            currentCatalog = LX200_STAR_C;

            if (selectSubCatalog(PortFD, currentCatalog, index))
            {
                currentSubCatalog = index;
                StarCatalogSP.setState(IPS_OK);
                StarCatalogSP.apply();
                return true;
            }
            else
            {
                StarCatalogSP.setState(IPS_IDLE);
                StarCatalogSP.apply("Catalog unavailable.");
                return false;
            }
        }

        // Deep sky catalog
        if (DeepSkyCatalogSP.isNameMatch(name))
        {
            DeepSkyCatalogSP.reset();
            DeepSkyCatalogSP.update(states, names, n);
            index = DeepSkyCatalogSP.findOnSwitchIndex();

            if (index == LX200_MESSIER_C)
            {
                currentCatalog     = index;
                DeepSkyCatalogSP.setState(IPS_OK);
                DeepSkyCatalogSP.apply();
            }
            else
                currentCatalog = LX200_DEEPSKY_C;

            if (selectSubCatalog(PortFD, currentCatalog, index))
            {
                currentSubCatalog  = index;
                DeepSkyCatalogSP.setState(IPS_OK);
                DeepSkyCatalogSP.apply();
            }
            else
            {
                DeepSkyCatalogSP.setState(IPS_IDLE);
                DeepSkyCatalogSP.apply("Catalog unavailable");
                return false;
            }

            return true;
        }

        // Solar system
        if (SolarSP.isNameMatch(name))
        {
            if (!SolarSP.update(states, names, n))
                return false;

            index = SolarSP.findOnSwitchIndex();

            // We ignore the first option : "Select item"
            if (index == 0)
            {
                SolarSP.setState(IPS_IDLE);
                SolarSP.apply();
                return true;
            }

            selectSubCatalog(PortFD, LX200_STAR_C, LX200_STAR);
            selectCatalogObject(PortFD, LX200_STAR_C, index + 900);

            ObjectNoNP.setState(IPS_OK);
            SolarSP.setState(IPS_OK);

            getObjectInfo(PortFD, ObjectInfoTP[0].text);
            ObjectNoNP.apply("Object updated.");
            SolarSP.apply();

            if (currentCatalog == LX200_STAR_C || currentCatalog == LX200_DEEPSKY_C)
                selectSubCatalog(PortFD, currentCatalog, currentSubCatalog);

            getObjectRA(PortFD, &targetRA);
            getObjectDEC(PortFD, &targetDEC);

            Goto(targetRA, targetDEC);

            return true;
        }

        // Unpark Alignment Mode
        if (UnparkAlignmentSP.isNameMatch(name))
        {
            UnparkAlignmentSP.update(states, names, n);
            UnparkAlignmentSP.setState(IPS_OK);
            UnparkAlignmentSP.apply();

            return true;
        }

    }

    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200Classic::saveConfigItems(FILE *fp)
{
    LX200Generic::saveConfigItems(fp);

    MaxSlewRateNP.save(fp);
    ElevationLimitNP.save(fp);

    UnparkAlignmentSP.save(fp);

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

    double parkRA  = 0.0;
    double parkDEC = 0.0;
    azAltToRaDecNow(parkAz, parkAlt, parkRA, parkDEC);
    LOGF_DEBUG("Parking to RA (%f) DEC (%f)...", parkRA, parkDEC);

    //save the current AlignmentMode to UnparkAlignment
    LX200Generic::getAlignment();
    int curAlignment = AlignmentSP.findOnSwitchIndex();
    UnparkAlignmentSP.reset();
    UnparkAlignmentSP[curAlignment].setState(ISS_ON);
    UnparkAlignmentSP.setState(IPS_OK);
    UnparkAlignmentSP.apply();
    saveConfig(true, UnparkAlignmentSP.getName());

    if (!Goto(parkRA, parkDEC))
    {
        ParkSP.setState(IPS_ALERT);
        ParkSP.apply("Parking Failed.");
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
        if (setAlignmentMode(PortFD, UnparkAlignmentSP.findOnSwitchIndex()) < 0)
        {
            LOG_ERROR("UnParking Failed.");
            AlignmentSP.setState(IPS_ALERT);
            AlignmentSP.apply("Error setting alignment mode.");
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
    azAltToRaDecNow(parkAz, parkAlt, parkRA, parkDEC);

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
    double parkAZ = 0.0;
    double parkAlt = 0.0;
    raDecToAzAltNow(currentRA, currentDEC, parkAZ, parkAlt);

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool LX200Classic::SetDefaultPark()
{
    // Az = 0 for North hemisphere
    SetAxis1Park(LocationNP[LOCATION_LATITUDE].value > 0 ? 0 : 180);

    // Alt = Latitude
    SetAxis2Park(LocationNP[LOCATION_LATITUDE].value);

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
            AlignmentSP.setState(IPS_ALERT);
            AlignmentSP.apply("Error setting alignment mode.");
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
            //coordinates intepreted in landmode.
            //Between isSlewComplete() and the beep there is nearly 3 seconds!
            settling = 3; //n iterations of default 1000ms
        }
    }

    return true;
}

void LX200Classic::azAltToRaDecNow(double az, double alt, double &ra, double &dec)
{
    ln_lnlat_posn observer;
    observer.lat = LocationNP[LOCATION_LATITUDE].getValue();
    observer.lng = LocationNP[LOCATION_LONGITUDE].getValue();
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    horizontalPos.az = az;
    horizontalPos.alt = alt;

    ln_equ_posn equatorialPos;

    get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

    ra = equatorialPos.ra / 15.0;
    dec = equatorialPos.dec;

    return;
}

void LX200Classic::raDecToAzAltNow(double ra, double dec, double &az, double &alt)
{
    ln_lnlat_posn observer;
    observer.lat = LocationNP[LOCATION_LATITUDE].getValue();
    observer.lng = LocationNP[LOCATION_LONGITUDE].getValue();
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    ln_equ_posn equatorialPos;
    equatorialPos.ra  = ra * 15;
    equatorialPos.dec = dec;
    get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);
    az = horizontalPos.az;
    alt = horizontalPos.alt;

    return;
}

