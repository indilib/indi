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

ScopeSim::ScopeSim(): GI(this)
{
    DBG_SCOPE = static_cast<uint32_t>(INDI::Logger::getInstance().addDebugLevel("Scope Verbose", "SCOPE"));

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TRACK_RATE | TELESCOPE_CAN_HOME_FIND | TELESCOPE_CAN_HOME_SET | TELESCOPE_CAN_HOME_GO,
                           4
                          );

    /* initialize random seed: */
    srand(static_cast<uint32_t>(time(nullptr)));

    // initialise axis positions
    // Note: Primary and secondary axes are always perpendicular
    switch (m_MountType)
    {
        // ALTAZ pointing at northern zenith
        case Alignment::MOUNT_TYPE::ALTAZ:
            // Primary instrument axis: looking at zenith, angle form negative PA-system, *origin HA-like*!
            axisPrimary.setDegrees(0.0);
            axisPrimary.TrackRate(Axis::SIDEREAL);
            // Secondary instrument axis: looking at east, angle form negative PA-system, origin opposite DEC-like!
            axisSecondary.setDegrees(90.0);
            axisSecondary.TrackRate(Axis::SIDEREAL);
            break;
        //EQ_XXX pointing at NCP (counterweight down)
        default:
            // Primary instrument axis: looking at SCP, angle form negative PA-system, origin opposite HA!
            axisPrimary.setDegrees(0.0);
            axisPrimary.TrackRate(Axis::SIDEREAL);
            // Secondary instrument axis: looling at east, angle form negative PA-system, origin opposite DEC!
            axisSecondary.setDegrees(90.0);
            axisSecondary.TrackRate(Axis::OFF);
    }
}

const char *ScopeSim::getDefaultName()
{
    return "Telescope Simulator";
}

bool ScopeSim::initProperties()
{
    /* Make sure to init parent properties first */
    INDI::Telescope::initProperties();

    // Override the mount type property to make it writable in the simulator
    MountTypeSP.fill(getDeviceName(), "TELESCOPE_MOUNT_TYPE", "Mount Type", MOTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

#ifdef USE_SIM_TAB
    // alignment properties, these are in the Simulation tab

    simPierSideSP[PS_OFF].fill("PS_OFF", "Off", ISS_OFF);
    simPierSideSP[PS_ON].fill("PS_ON", "On", ISS_ON);
    simPierSideSP.fill(getDeviceName(), "SIM_PIER_SIDE", "Sim Pier Side",
                       "Simulation", IP_WO, ISR_1OFMANY, 60, IPS_IDLE);

    mountModelNP[MM_IH].fill("MM_IH", "Ha Zero (IH)", "%g", -5, 5, 0.01, 0);
    mountModelNP[MM_ID].fill("MM_ID", "Dec Zero (ID)", "%g", -5, 5, 0.01, 0);
    mountModelNP[MM_CH].fill("MM_CH", "Cone (CH)", "%g", -5, 5, 0.01, 0);
    mountModelNP[MM_NP].fill("MM_NP", "Ha/Dec (NP)", "%g", -5, 5, 0.01, 0);
    mountModelNP[MM_MA].fill("MM_MA", "Pole Azm (MA)", "%g", -5, 5, 0.01, 0);
    mountModelNP[MM_ME].fill("MM_ME", "Pole elev (ME)", "%g", -5, 5, 0.01, 0);
    mountModelNP.fill(getDeviceName(), "MOUNT_MODEL", "Mount Model",
                      "Simulation", IP_WO, 0, IPS_IDLE);

    flipHourAngleNP[0].fill("FLIP_HA", "Hour Angle (deg)", "%g", -20, 20, 0.1, 0);
    flipHourAngleNP.fill(getDeviceName(), "FLIP_HA", "Flip Posn.",
                         "Simulation", IP_WO, 0, IPS_IDLE);

    mountAxisNP[PRIMARY].fill("PRIMARY", "Primary (Ha)", "%g", -180, 180, 0.01, 0);
    mountAxisNP[SECONDARY].fill("SECONDARY", "Secondary (Dec)", "%g", -180, 180, 0.01, 0);
    mountAxisNP.fill(getDeviceName(), "MOUNT_AXES", "Mount Axes",
                     "Simulation", IP_RO, 0, IPS_IDLE);

    decBacklashNP[0].fill("DEC_BACKLASH", "DEC Backlash (ms)", "%.0f", 0, 5000, 0, 0);
    decBacklashNP.fill(getDeviceName(), "DEC_BACKLASH", "DEC Backlash",
                       "Simulation", IP_RW, 0, IPS_IDLE);

#endif

    /* How fast do we guide compared to sidereal rate */
    GuideRateNP[RA_AXIS].fill("GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.1, 0.5);
    GuideRateNP[DEC_AXIS].fill("GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.1, 0.5);
    GuideRateNP.fill(getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0,
                     IPS_IDLE);

    SlewRateSP[SLEW_GUIDE].fill("SLEW_GUIDE", "Guide", ISS_OFF);
    SlewRateSP[SLEW_CENTERING].fill("SLEW_CENTERING", "Centering", ISS_OFF);
    SlewRateSP[SLEW_FIND].fill("SLEW_FIND", "Find", ISS_OFF);
    SlewRateSP[SLEW_MAX].fill("SLEW_MAX", "Max", ISS_ON);
    SlewRateSP.fill(getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB,
                    IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Add Tracking Modes, the order must match the order of the TelescopeTrackMode enum
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // RA is a rotating frame, while HA or Alt/Az is not
    SetParkDataType(PARK_HA_DEC);

    GI::initProperties(MOTION_TAB);

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
    // Load mount type settings
    MountTypeSP.load();
    defineProperty(simPierSideSP);
    simPierSideSP.load();
    defineProperty(mountModelNP);
    mountModelNP.load();
    defineProperty(mountAxisNP);
    defineProperty(flipHourAngleNP);
    flipHourAngleNP.load();
    defineProperty(decBacklashNP);
    decBacklashNP.load();
#endif
    double Latitude = LocationNP[LOCATION_LATITUDE].getValue();
    m_sinLat = std::sin(Latitude * 0.0174533);
    m_cosLat = std::cos(Latitude * 0.0174533);
    m_currentAz = 180 + axisPrimary.position.Degrees(); // Primary to Azm
    m_currentAlt = axisSecondary.position.Degrees();
}

bool ScopeSim::updateProperties()
{
    updateMountAndPierSide();

    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineProperty(GuideRateNP);
        GuideRateNP.load();
        if (InitPark())
        {

            if (isParked())
            {
                // at this point there is a valid ParkData.xml available
                alignment.latitude = Angle(LocationNP[LOCATION_LATITUDE].getValue());
                alignment.longitude = Angle(LocationNP[LOCATION_LONGITUDE].getValue());
                // RA-parkposition in Ha full circle!!
                m_currentRA = (alignment.lst() - Angle(ParkPositionNP[AXIS_RA].getValue(), Angle::ANGLE_UNITS::HOURS)).Hours();
                m_currentDEC = ParkPositionNP[AXIS_DE].getValue();
                Sync(m_currentRA, m_currentDEC);
                m_currentAz = 180 + axisPrimary.position.Degrees(); // ALTAZ-Primary to Azm
                m_currentAlt = axisSecondary.position.Degrees();
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
        deleteProperty(GuideRateNP);
        //deleteProperty(HomeSP);
    }

    GI::updateProperties();

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


/// ALTAZ: The tracking rates of the mechanical axes vary with the angle positions.
/// (See "Deriving Field Rotation Rate for an Alt-Az Mounted Telescope" by Russell P. Patera1)
bool ScopeSim::ReadScopeStatus()
{
    if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ && TrackState == SCOPE_TRACKING)
    {
        double sinAz = std::sin(DEG_TO_RAD(m_currentAz));
        double cosAz = std::cos(DEG_TO_RAD(m_currentAz));
        double sinAlt = std::sin(DEG_TO_RAD(m_currentAlt));
        double cosAlt = std::cos(DEG_TO_RAD(m_currentAlt));
        SetTrackRate((m_sinLat - ((cosAz * sinAlt * m_cosLat) / cosAlt)) * TRACKRATE_SIDEREAL,
                      m_cosLat * sinAz * TRACKRATE_SIDEREAL);
    }

    // SetTrackRate(TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue());

    // new mechanical angle calculation
    axisPrimary.update();
    axisSecondary.update();

    // transform to new RA & DEC
    Angle ra, dec;
    alignment.mountToApparentRaDec(axisPrimary.position, axisSecondary.position, &ra, &dec);
    m_currentRA = ra.Hours();
    m_currentDEC = dec.Degrees();
    m_currentAz = 180 + axisPrimary.position.Degrees(); // ALTAZ-Primary to Azm
    m_currentAlt = axisSecondary.position.Degrees();

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
                EqNP.setState(IPS_IDLE);
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
                EqNP.setState(IPS_IDLE);

                if (HomeSP.getState() == IPS_BUSY)
                {
                    HomeSP.reset();
                    HomeSP.setState(IPS_OK);
                    HomeSP.apply();
                    LOG_INFO("Home position reached.");
                }
                else
                    LOG_INFO("Telescope slew is complete. Tracking...");

                // check the slew accuracy
                auto dRa = m_targetRA - m_currentRA;
                auto dDec = m_targetDEC - m_currentDEC;
                LOGF_DEBUG("slew accuracy %f, %f", dRa * 15 * 3600, dDec * 3600);
            }
            break;
        case SCOPE_PARKED:
            return true;
        default:
            break;
    }

    if (guidingEW && !axisPrimary.IsGuiding())
    {
        GuideWENP[0].setValue(0);
        GuideWENP[1].setValue(0);
        GuideComplete(INDI_EQ_AXIS::AXIS_RA);
        guidingEW = false;
    }

    if (guidingNS && !axisSecondary.IsGuiding())
    {
        GuideNSNP[0].setValue(0);
        GuideNSNP[1].setValue(0);
        GuideComplete(INDI_EQ_AXIS::AXIS_DE);
        guidingNS = false;
    }

#ifdef USE_SIM_TAB
    double PrimaryAngle = axisPrimary.position.Degrees();
    double SecondaryAngle = axisSecondary.position.Degrees();
    // No need to spam log until we have some actual changes.
    if (std::fabs(mountAxisNP[PRIMARY].getValue() - PrimaryAngle) > 0.0001 ||
            std::fabs(mountAxisNP[SECONDARY].getValue() - SecondaryAngle) > 0.0001)
    {
        mountAxisNP[PRIMARY].setValue(PrimaryAngle);
        mountAxisNP[SECONDARY].setValue(SecondaryAngle);

        LOGF_EXTRA1("new %s: %f, ra %f", axisPrimary.axisName, PrimaryAngle, ra.Hours());
        LOGF_EXTRA1("new %s: %f, dec %f", axisSecondary.axisName, SecondaryAngle, dec.Degrees());

        mountAxisNP.apply();
    }
#endif

    char RAStr[64], DecStr[64];
    fs_sexa(RAStr, m_currentRA, 2, 3600);
    fs_sexa(DecStr, m_currentDEC, 2, 3600);
    DEBUGF(DBG_SCOPE, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(m_currentRA, m_currentDEC);

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
    m_currentRA = r.Hours();
    m_currentDEC = d.Degrees();

    LOG_INFO("Sync is successful.");

    EqNP.setState(IPS_OK);

    NewRaDec(m_currentRA, m_currentDEC);

    return true;
}

bool ScopeSim::Park()
{
    double ra = (alignment.lst() - Angle(GetAxis1Park() * 15.)).Degrees() / 15.;
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

    m_targetRA  = ra;
    m_targetDEC = dec;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, m_targetRA, 2, 3600);
    fs_sexa(DecStr, m_targetDEC, 2, 3600);

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
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, "GUIDE_RATE") == 0)
        {
            GuideRateNP.update(values, names, n);
            GuideRateNP.setState(IPS_OK);
            GuideRateNP.apply();
            return true;
        }

#ifdef USE_SIM_TAB
        if (mountModelNP.isNameMatch(name))
        {
            if (mountModelNP.isUpdated(values, names, n))
            {
                mountModelNP.update(values, names, n);
                alignment.setCorrections(mountModelNP[MM_IH].getValue(), mountModelNP[MM_ID].getValue(),
                                         mountModelNP[MM_CH].getValue(), mountModelNP[MM_NP].getValue(),
                                         mountModelNP[MM_MA].getValue(), mountModelNP[MM_ME].getValue());
                saveConfig(mountModelNP);
            }
            mountModelNP.setState(IPS_OK);
            mountModelNP.apply();
            return true;
        }

        if (flipHourAngleNP.isNameMatch(name))
        {
            flipHourAngleNP.update(values, names, n);
            flipHourAngleNP.setState(IPS_OK);
            flipHourAngleNP.apply();
            alignment.setFlipHourAngle(flipHourAngleNP[0].getValue());
            return true;
        }
        if (decBacklashNP.isNameMatch(name))
        {
            decBacklashNP.update(values, names, n);
            decBacklashNP.setState(IPS_OK);
            decBacklashNP.apply();
            m_DecGuideBacklashMs = decBacklashNP[0].getValue();
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
        if (MountTypeSP.isNameMatch(name))
        {
            if (!MountTypeSP.update(states, names, n))
                return false;

            MountTypeSP.setState(IPS_OK);
            MountTypeSP.apply();
            updateMountAndPierSide();
            return true;
        }
        if (simPierSideSP.isNameMatch(name))
        {
            if (!simPierSideSP.update(states, names, n))
                return false;

            simPierSideSP.setState(IPS_OK);
            simPierSideSP.apply();
            updateMountAndPierSide();
            return true;
        }
#endif \
    // Slew mode
        if (SlewRateSP.isNameMatch(name))
        {
            if (SlewRateSP.update( states, names, n) == false)
                return false;

            SlewRateSP.setState(IPS_OK);
            SlewRateSP.apply();
            return true;
        }
    }

    //  Nobody has claimed this, so pass it over
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
    mcRate = static_cast<int>(SlewRateSP.findOnSwitchIndex()) + 1;
    mcRate = std::max(1, std::min(4, mcRate));

    int rate = (dir == INDI_DIR_NS::DIRECTION_NORTH) ? mcRate : -mcRate;
    if (HasPierSide() & (currentPierSide == PIER_WEST)) // see scopesim_helper.cpp: alignment
        rate = -rate;
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

    mcRate = static_cast<int>(SlewRateSP.findOnSwitchIndex()) + 1;
    mcRate = std::max(1, std::min(4, mcRate));

    int rate = (dir == INDI_DIR_WE::DIRECTION_EAST) ? -mcRate : mcRate;
    LOGF_DEBUG("MoveWE dir %d, motion %s, rate %d", dir == DIRECTION_EAST ? "E" : "W", command == 0 ? "start" : "stop", rate);

    axisPrimary.mcRate = command == MOTION_START ? rate : 0;
    return true;
}

uint32_t ScopeSim::backlashComputation(uint32_t ms)
{
    if (m_DecGuideRemainingBacklash <= 0)
    {
        m_DecGuideRemainingBacklash = 0;
    }
    else if (m_DecGuideRemainingBacklash >= ms)
    {
        m_DecGuideRemainingBacklash -= ms;
        ms = 0;
    }
    else
    {
        ms -= m_DecGuideRemainingBacklash;
        m_DecGuideRemainingBacklash = 0;
    }
    return ms;
}

uint32_t ScopeSim::applyDecBacklash(double rate, uint32_t ms)
{
    const bool changingDirections = (rate < 0 && m_GuideDecLastNorth) ||
                                    (rate > 0 && !m_GuideDecLastNorth);
    m_GuideDecLastNorth = rate >= 0;

    if (!changingDirections)
    {
        ms = backlashComputation(ms);
        return ms;
    }
    else
    {
        if (m_DecGuideRemainingBacklash <= 0)
            m_DecGuideRemainingBacklash = m_DecGuideBacklashMs;
        else if (m_DecGuideRemainingBacklash >= m_DecGuideBacklashMs)
            m_DecGuideRemainingBacklash = 0;
        else
            m_DecGuideRemainingBacklash = m_DecGuideBacklashMs - m_DecGuideRemainingBacklash;

        ms = backlashComputation(ms);
        return ms;
    }
}

IPState ScopeSim::GuideNorth(uint32_t ms)
{
    double rate = GuideRateNP[DEC_AXIS].getValue();
    if (HasPierSide() & (currentPierSide == PIER_WEST)) // see scopsim_helper.cpp: alignment
        rate = -rate;
    ms = applyDecBacklash(rate, ms);
    axisSecondary.StartGuide(rate, ms);
    guidingNS = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideSouth(uint32_t ms)
{
    double rate = GuideRateNP[DEC_AXIS].getValue();
    if (HasPierSide() & (currentPierSide == PIER_WEST)) // see scopsim_helper.cpp: alignment
        rate = -rate;
    ms = applyDecBacklash(-rate, ms);
    axisSecondary.StartGuide(-rate, ms);
    guidingNS = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideEast(uint32_t ms)
{
    double rate = GuideRateNP[RA_AXIS].getValue();
    axisPrimary.StartGuide(-rate, ms);
    guidingEW = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideWest(uint32_t ms)
{
    double rate = GuideRateNP[RA_AXIS].getValue();
    axisPrimary.StartGuide(rate, ms);
    guidingEW = true;
    return IPS_BUSY;
}

bool ScopeSim::SetCurrentPark()
{

    double ha  = (alignment.lst() - Angle(m_currentRA, Angle::ANGLE_UNITS::HOURS)).Hours();
    SetAxis1Park(ha);
    SetAxis2Park(m_currentDEC);

    return true;
}

bool ScopeSim::SetDefaultPark()
{
    // mount points to East (counterweights down) at the horizon
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
            SetTrackRate(TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue());
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
    GuideRateNP.save(fp);
    MountTypeSP.save(fp);
    simPierSideSP.save(fp);
    mountModelNP.save(fp);
    flipHourAngleNP.save(fp);
    decBacklashNP.save(fp);

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
    int mountType = MountTypeSP.findOnSwitchIndex();
    int pierSide = simPierSideSP.findOnSwitchIndex();
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
        LOG_INFO("AltAz mount type experimental");
        // return false;
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


IPState ScopeSim::ExecuteHomeAction(TelescopeHomeAction action)
{
    switch (action)
    {
        case HOME_FIND:
            LOG_INFO("Finding home position...");
            m_Home[AXIS_RA] = alignment.lst().Hours();
            m_Home[AXIS_DE] = LocationNP[LOCATION_LATITUDE].getValue() > 0 ? 90 : -90;
            Goto(m_Home[AXIS_RA], m_Home[AXIS_DE]);
            return IPS_BUSY;

        case HOME_SET:
            m_Home[AXIS_RA] = (alignment.lst() - Angle(EqNP[AXIS_RA].getValue(), Angle::HOURS)).HoursHa();
            m_Home[AXIS_DE] = EqNP[AXIS_DE].getValue();
            LOG_INFO("Setting home position to current position.");
            return IPS_OK;

        case HOME_GO:
        {
            LOG_INFO("Going to home position.");
            if (m_Home[AXIS_RA] == 0)
            {
                m_Home[AXIS_RA] = 0;
                m_Home[AXIS_DE] = LocationNP[LOCATION_LATITUDE].getValue() > 0 ? 90 : -90;
            }

            // Home HA to home RA
            auto ra = (alignment.lst() - Angle(m_Home[AXIS_RA], Angle::HOURS)).Hours();
            Goto(ra, m_Home[AXIS_DE]);
            return IPS_BUSY;
        }

        default:
            return IPS_ALERT;
    }
    return IPS_ALERT;
}

/////////////////////////////////////////////////////////////////////
