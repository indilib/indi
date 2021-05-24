/*******************************************************************************
 Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "telescope_simulator.h"
#include "scopesim_helper.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

// We declare an auto pointer to ScopeSim.
static std::unique_ptr<ScopeSim> telescope_sim(new ScopeSim());

#define RA_AXIS     0
#define DEC_AXIS    1
#define GUIDE_NORTH 0
#define GUIDE_SOUTH 1
#define GUIDE_WEST  0
#define GUIDE_EAST  1

ScopeSim::ScopeSim()
{
    DBG_SCOPE = static_cast<uint32_t>(INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE"));

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_RATE,
                           4);

    /* initialize random seed: */
    srand(static_cast<uint32_t>(time(nullptr)));

    // initalise axis positions, for GEM pointing at pole, counterweight down
    axisPrimary.setDegrees(90.0);
    axisPrimary.TrackRate(Axis::SIDEREAL);
    axisSecondary.setDegrees(90.0);
}

const char *ScopeSim::getDefaultName()
{
    return "Telescope Simulator";
}

bool ScopeSim::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

#ifdef USE_SIM_TAB
    // mount type and alignment properties, these are in the Simulation tab
    IUFillSwitch(&mountTypeS[Alignment::ALTAZ], "ALTAZ", "AltAz", ISS_OFF);
    IUFillSwitch(&mountTypeS[Alignment::EQ_FORK], "EQ_FORK", "Fork (Eq)", ISS_OFF);
    IUFillSwitch(&mountTypeS[Alignment::EQ_GEM], "EQ_GEM", "GEM", ISS_ON);
    IUFillSwitchVector(&mountTypeSP, mountTypeS, 3, getDeviceName(), "MOUNT_TYPE", "Mount Type",
                       "Simulation", IP_WO, ISR_1OFMANY, 60, IPS_IDLE );

    IUFillSwitch(&simPierSideS[0], "PS_OFF", "Off", ISS_OFF);
    IUFillSwitch(&simPierSideS[1], "PS_ON", "On", ISS_ON);
    IUFillSwitchVector(&simPierSideSP, simPierSideS, 2, getDeviceName(), "SIM_PIER_SIDE", "Sim Pier Side",
                       "Simulation", IP_WO, ISR_1OFMANY, 60, IPS_IDLE );

    IUFillNumber(&mountModelN[0], "MM_IH", "Ha Zero (IH)", "%g", -5, 5, 0.01, 0);
    IUFillNumber(&mountModelN[1], "MM_ID", "Dec Zero (ID)", "%g", -5, 5, 0.01, 0);
    IUFillNumber(&mountModelN[2], "MM_CH", "Cone (CH)", "%g", -5, 5, 0.01, 0);
    IUFillNumber(&mountModelN[3], "MM_NP", "Ha/Dec (NP)", "%g", -5, 5, 0.01, 0);
    IUFillNumber(&mountModelN[4], "MM_MA", "Pole Azm (MA)", "%g", -5, 5, 0.01, 0);
    IUFillNumber(&mountModelN[5], "MM_ME", "Pole elev (ME)", "%g", -5, 5, 0.01, 0);
    IUFillNumberVector(&mountModelNP, mountModelN, 6, getDeviceName(), "MOUNT_MODEL", "Mount Model",
                       "Simulation", IP_WO, 0, IPS_IDLE);

    IUFillNumber(&flipHourAngleN[0], "FLIP_HA", "Hour Angle (deg)", "%g", -20, 20, 0.1, 0);
    IUFillNumberVector(&flipHourAngleNP, flipHourAngleN, 1, getDeviceName(), "FLIP_HA", "Flip Posn.",
                       "Simulation", IP_WO, 0, IPS_IDLE);

    IUFillNumber(&mountAxisN[0], "PRIMARY", "Primary (Ha)", "%g", -180, 180, 0.01, 0);
    IUFillNumber(&mountAxisN[1], "SECONDARY", "Secondary (Dec)", "%g", -180, 180, 0.01, 0);
    IUFillNumberVector(&mountAxisNP, mountAxisN, 2, getDeviceName(), "MOUNT_AXES", "Mount Axes",
                       "Simulation", IP_RO, 0, IPS_IDLE);
#endif

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[RA_AXIS], "GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.1, 0.5);
    IUFillNumber(&GuideRateN[DEC_AXIS], "GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.1, 0.5);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                       IPS_IDLE);

    IUFillSwitch(&SlewRateS[SLEW_GUIDE], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_CENTERING], "SLEW_CENTERING", "Centering", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_FIND], "SLEW_FIND", "Find", ISS_OFF);
    IUFillSwitch(&SlewRateS[SLEW_MAX], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 4, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Add Tracking Modes, the order must match the order of the TelescopeTrackMode enum
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Let's simulate it to be an F/7.5 120mm telescope
    ScopeParametersN[0].value = 120;
    ScopeParametersN[1].value = 900;
    ScopeParametersN[2].value = 120;
    ScopeParametersN[3].value = 900;

    // RA is a rotating frame, while HA or Alt/Az is not
    SetParkDataType(PARK_HA_DEC);

    initGuiderProperties(getDeviceName(), MOTION_TAB);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    setDefaultPollingPeriod(250);

    return true;
}

void ScopeSim::ISGetProperties(const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

#ifdef USE_SIM_TAB
    defineProperty(&mountTypeSP);
    loadConfig(true, mountTypeSP.name);
    defineProperty(&simPierSideSP);
    loadConfig(true, simPierSideSP.name);
    defineProperty(&mountModelNP);
    loadConfig(true, mountModelNP.name);
    defineProperty(&mountAxisNP);
    defineProperty(&flipHourAngleNP);
    loadConfig(true, flipHourAngleNP.name);
#endif
    /*
    if (isConnected())
    {
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);
        defineProperty(&EqPENV);
        defineProperty(&PEErrNSSP);
        defineProperty(&PEErrWESP);
    }
    */
}

bool ScopeSim::updateProperties()
{
    updateMountAndPierSide();

    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(&GuideNSNP);
        defineProperty(&GuideWENP);
        defineProperty(&GuideRateNP);
        loadConfig(true, GuideRateNP.name);

        if (InitPark())
        {

            if (isParked())
            {
	        // at this point there is a valid ParkData.xml available
	        double longitude, latitude;
	        IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LONG", &longitude);
	        IUGetConfigNumber(getDeviceName(), "GEOGRAPHIC_COORD", "LAT", &latitude);
	        alignment.latitude = Angle(latitude);
		alignment.longitude = Angle(longitude);

	        currentRA = (alignment.lst() - Angle(ParkPositionN[AXIS_RA].value, Angle::ANGLE_UNITS::HOURS)).Hours();
                currentDEC = ParkPositionN[AXIS_DE].value;
                Sync(currentRA, currentDEC);

            }
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(-6.);
            SetAxis2ParkDefault(0.);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(-6.);
            SetAxis2Park(0.);
            SetAxis1ParkDefault(-6.);
            SetAxis2ParkDefault(0.);
        }

        sendTimeFromSystem();
    }
    else
    {
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(GuideRateNP.name);
    }

    return true;
}

bool ScopeSim::Connect()
{
    LOG_INFO("Telescope simulator is online.");
    SetTimer(getCurrentPollingPeriod());

    return true;
}

bool ScopeSim::Disconnect()
{
    LOG_INFO("Telescope simulator is offline.");
    return true;
}

bool ScopeSim::ReadScopeStatus()
{
    // new axis control
    axisPrimary.update();
    axisSecondary.update();

    Angle ra, dec;
    alignment.mountToApparentRaDec(axisPrimary.position, axisSecondary.position, &ra, &dec);

    // move both axes
    currentRA = ra.Hours();
    currentDEC = dec.Degrees();

    // update properties from the axis
    if (alignment.mountType == Alignment::MOUNT_TYPE::EQ_GEM)
    {
        setPierSide(fabs(axisSecondary.position.Degrees()) > 90 ? PIER_WEST : PIER_EAST);
    }

    bool slewing = axisPrimary.isSlewing || axisSecondary.isSlewing;
    switch (TrackState)
    {
        case SCOPE_PARKING:
            if (!slewing)
            {
                SetParked(true);
                EqNP.s = IPS_IDLE;
                LOG_INFO("Telescope slew is complete. Parked");
            }
            break;
        case SCOPE_SLEWING:
            if (!slewing)
            {
                // It seems to be required that tracking is enabled when a slew finishes but is it correct?
                // if the mount was not tracking before the slew should it remain not tracking?
                TrackState = SCOPE_TRACKING;
                SetTrackEnabled(true);
                EqNP.s = IPS_IDLE;
                LOG_INFO("Telescope slew is complete. Tracking...");

                // check the slew accuracy
                auto dRa = targetRA - currentRA;
                auto dDec = targetDEC - currentDEC;
                LOGF_DEBUG("slew accuracy %f, %f", dRa * 15 * 3600, dDec * 3600);
            }
            break;
        default:
            break;
    }

    if (guidingEW && !axisPrimary.IsGuiding())
    {
        GuideWENP.np[0].value = 0;
        GuideWENP.np[1].value = 0;
        GuideComplete(INDI_EQ_AXIS::AXIS_RA);
        guidingEW = false;
    }

    if (guidingNS && !axisSecondary.IsGuiding())
    {
        GuideNSNP.np[0].value = 0;
        GuideNSNP.np[1].value = 0;
        GuideComplete(INDI_EQ_AXIS::AXIS_DE);
        guidingNS = false;
    }

#ifdef USE_SIM_TAB
    double axisRA = axisPrimary.position.Degrees();
    double axisDE = axisSecondary.position.Degrees();
    // No need to spam log until we have some actual changes.
    if (std::fabs(mountAxisN[AXIS_RA].value - axisRA) > 0.0001 ||
            std::fabs(mountAxisN[AXIS_DE].value - axisDE) > 0.0001)
    {
        mountAxisN[AXIS_RA].value = axisRA;
        mountAxisN[AXIS_DE].value = axisDE;

        LOGF_EXTRA1("%s: %f, ra %f", axisPrimary.axisName, axisPrimary.position.Degrees(), ra.Hours());
        LOGF_EXTRA1("%s: %f, dec %f", axisSecondary.axisName, axisSecondary.position.Degrees(), dec.Degrees());

        IDSetNumber(&mountAxisNP, nullptr);
    }
#endif

    char RAStr[64], DecStr[64];
    fs_sexa(RAStr, currentRA, 2, 3600);
    fs_sexa(DecStr, currentDEC, 2, 3600);
    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool ScopeSim::Goto(double r, double d)
{
    StartSlew(r, d, SCOPE_SLEWING);
    return true;
}

bool ScopeSim::Sync(double ra, double dec)
{
    Angle a1, a2;
    // set the mount axes to the position that will cause it to report the sync position
    alignment.apparentRaDecToMount(Angle(ra * 15.0), Angle(dec), &a1, &a2);
    axisPrimary.setDegrees(a1.Degrees());
    axisSecondary.setDegrees(a2.Degrees());

    Angle r, d;
    alignment.mountToApparentRaDec(a1, a2, &r, &d);
    LOGF_DEBUG("sync to %f, %f, reached %f, %f", ra, dec, r.Hours(), d.Degrees());
    currentRA = r.Hours();
    currentDEC = d.Degrees();

    LOG_INFO("Sync is successful.");

    EqNP.s = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool ScopeSim::Park()
{
    double ra = (alignment.lst() - Angle(GetAxis1Park() * 15.)).Degrees()/15.;
    StartSlew(ra, GetAxis2Park(), SCOPE_PARKING);
    return true;
}

// common code for GoTo and park
void ScopeSim::StartSlew(double ra, double dec, TelescopeStatus status)
{
    Angle primary, secondary;
    alignment.apparentRaDecToMount(Angle(ra * 15.0), Angle(dec), &primary, &secondary);

    axisPrimary.StartSlew(primary);
    axisSecondary.StartSlew(secondary);

    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    const char * statusStr;
    switch (status)
    {
        case SCOPE_PARKING:
            statusStr = "Parking";
            break;
        case SCOPE_SLEWING:
            statusStr = "Slewing";
            break;
        default:
            statusStr = "unknown";
    }
    TrackState = status;

    LOGF_INFO("%s to RA: %s - DEC: %s", statusStr, RAStr, DecStr);
}

bool ScopeSim::UnPark()
{
    SetParked(false);
    return true;
}

bool ScopeSim::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "GUIDE_RATE") == 0)
        {
            IUUpdateNumber(&GuideRateNP, values, names, n);
            GuideRateNP.s = IPS_OK;
            IDSetNumber(&GuideRateNP, nullptr);
            return true;
        }

        if (strcmp(name, GuideNSNP.name) == 0 || strcmp(name, GuideWENP.name) == 0)
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }

#ifdef USE_SIM_TAB
        if (strcmp(name, mountModelNP.name) == 0)
        {
            IUUpdateNumber(&mountModelNP, values, names, n);
            mountModelNP.s = IPS_OK;
            IDSetNumber(&mountModelNP, nullptr);
            alignment.setCorrections(mountModelN[0].value, mountModelN[1].value,
                                     mountModelN[2].value, mountModelN[3].value,
                                     mountModelN[4].value, mountModelN[5].value);

            return true;
        }

        if (strcmp(name, flipHourAngleNP.name) == 0)
        {
            IUUpdateNumber(&flipHourAngleNP, values, names, n);
            flipHourAngleNP.s = IPS_OK;
            IDSetNumber(&flipHourAngleNP, nullptr);
            alignment.setFlipHourAngle(flipHourAngleN[0].value);
            return true;
        }
#endif
    }

    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool ScopeSim::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
#ifdef USE_SIM_TAB
        if (strcmp(name, mountTypeSP.name) == 0)
        {
            if (IUUpdateSwitch(&mountTypeSP, states, names, n) < 0)
                return false;

            mountTypeSP.s = IPS_OK;
            IDSetSwitch(&mountTypeSP, nullptr);
            updateMountAndPierSide();
            return true;
        }
        if (strcmp(name, simPierSideSP.name) == 0)
        {
            if (IUUpdateSwitch(&simPierSideSP, states, names, n) < 0)
                return false;

            simPierSideSP.s = IPS_OK;
            IDSetSwitch(&simPierSideSP, nullptr);
            updateMountAndPierSide();
            return true;
        }
#endif
        // Slew mode
        if (strcmp(name, SlewRateSP.name) == 0)
        {
            if (IUUpdateSwitch(&SlewRateSP, states, names, n) < 0)
                return false;

            SlewRateSP.s = IPS_OK;
            IDSetSwitch(&SlewRateSP, nullptr);
            return true;
        }
    }

    //  Nobody has claimed this, so, ignore it
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool ScopeSim::Abort()
{
    axisPrimary.Abort();
    axisSecondary.Abort();
    return true;
}

bool ScopeSim::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }
    mcRate = static_cast<int>(IUFindOnSwitchIndex(&SlewRateSP)) + 1;

    int rate = (dir == INDI_DIR_NS::DIRECTION_NORTH) ? mcRate : -mcRate;
    LOGF_DEBUG("MoveNS dir %s, motion %s, rate %d", dir == DIRECTION_NORTH ? "N" : "S", command == 0 ? "start" : "stop", rate);

    axisSecondary.mcRate = command == MOTION_START ? rate : 0;

    return true;
}

bool ScopeSim::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    mcRate = static_cast<int>(IUFindOnSwitchIndex(&SlewRateSP)) + 1;
    int rate = (dir == INDI_DIR_WE::DIRECTION_EAST) ? -mcRate : mcRate;
    LOGF_DEBUG("MoveWE dir %d, motion %s, rate %d", dir == DIRECTION_EAST ? "E" : "W", command == 0 ? "start" : "stop", rate);

    axisPrimary.mcRate = command == MOTION_START ? rate : 0;
    return true;
}

IPState ScopeSim::GuideNorth(uint32_t ms)
{
    double rate = GuideRateN[DEC_AXIS].value;
    axisSecondary.StartGuide(rate, ms);
    guidingNS = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideSouth(uint32_t ms)
{
    double rate = GuideRateN[DEC_AXIS].value;
    axisSecondary.StartGuide(-rate, ms);
    guidingNS = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideEast(uint32_t ms)
{
    double rate = GuideRateN[RA_AXIS].value;
    axisPrimary.StartGuide(-rate, ms);
    guidingEW = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideWest(uint32_t ms)
{
    double rate = GuideRateN[RA_AXIS].value;
    axisPrimary.StartGuide(rate, ms);
    guidingEW = true;
    return IPS_BUSY;
}

bool ScopeSim::SetCurrentPark()
{

    double ha  = (alignment.lst() - Angle(currentRA, Angle::ANGLE_UNITS::HOURS)).Hours();
    SetAxis1Park(ha);
    SetAxis2Park(currentDEC);

    return true;
}

bool ScopeSim::SetDefaultPark()
{
    // mount points to East (couter weights down) at the horizon
    // works for both hemispheres
    SetAxis1Park(-6.);
    SetAxis2Park(0.);

    return true;
}

bool ScopeSim::SetTrackMode(uint8_t mode)
{
    switch (static_cast<TelescopeTrackMode>(mode))
    {
        case TRACK_SIDEREAL:
            axisPrimary.TrackRate(Axis::SIDEREAL);
            axisSecondary.TrackRate(Axis::OFF);
            return true;
        case TRACK_SOLAR:
            axisPrimary.TrackRate(Axis::SOLAR);
            axisSecondary.TrackRate(Axis::OFF);
            return true;
        case TRACK_LUNAR:
            axisPrimary.TrackRate(Axis::LUNAR);
            axisSecondary.TrackRate(Axis::OFF);
            return true;
        case TRACK_CUSTOM:
            SetTrackRate(TrackRateN[AXIS_RA].value, TrackRateN[AXIS_DE].value);
            return true;
    }
    return false;
}

bool ScopeSim::SetTrackEnabled(bool enabled)
{
    axisPrimary.Tracking(enabled);
    axisSecondary.Tracking(enabled);
    return true;
}

bool ScopeSim::SetTrackRate(double raRate, double deRate)
{
    axisPrimary.TrackingRateDegSec = Angle(raRate / 3600.0);
    axisSecondary.TrackingRateDegSec = Angle(deRate / 3600.0);
    return true;
}

bool ScopeSim::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

#ifdef USE_SIM_TAB
    IUSaveConfigNumber(fp, &GuideRateNP);
    IUSaveConfigSwitch(fp, &mountTypeSP);
    IUSaveConfigSwitch(fp, &simPierSideSP);
    IUSaveConfigNumber(fp, &mountModelNP);
    IUSaveConfigNumber(fp, &flipHourAngleNP);
#endif
    return true;
}

bool ScopeSim::updateLocation(double latitude, double longitude, double elevation)
{
    LOGF_DEBUG("Update location %8.3f, %8.3f, %4.0f", latitude, longitude, elevation);

    alignment.latitude = Angle(latitude);
    alignment.longitude = Angle(longitude);

    INDI_UNUSED(elevation);
    return true;
}

bool ScopeSim::updateMountAndPierSide()
{
#ifdef USE_SIM_TAB
    int mountType = IUFindOnSwitchIndex(&mountTypeSP);
    int pierSide = IUFindOnSwitchIndex(&simPierSideSP);
    if (mountType < 0 || pierSide < 0)
        return false;

    // If nothing changed, return
    if (mountType == m_MountType && pierSide == m_PierSide)
        return true;

    m_MountType = mountType;
    m_PierSide = pierSide;

    LOGF_INFO("update mount and pier side: Pier Side %s, mount type %d", pierSide == 0 ? "Off" : "On", mountType);
#else
    int mountType = 2;
    int pierSide = 1;
#endif
    if ( mountType == Alignment::MOUNT_TYPE::ALTAZ)
    {
        LOG_INFO("AltAz mount type not implemented yet");
        return false;
    }

    alignment.mountType = static_cast<Alignment::MOUNT_TYPE>(mountType);
    // update the pier side capability depending on the mount type
    uint32_t cap = GetTelescopeCapability();
    if (pierSide == 1 && mountType == 2)
    {
        cap |= TELESCOPE_HAS_PIER_SIDE;
    }
    else
    {
        cap &= ~static_cast<uint32_t>(TELESCOPE_HAS_PIER_SIDE);
    }
    SetTelescopeCapability(cap, 4);

    return true;
}

/////////////////////////////////////////////////////////////////////

