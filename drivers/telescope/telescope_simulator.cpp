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
#include "lilxml.h"

#include <libnova/julian_day.h>

using namespace INDI::AlignmentSubsystem;

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

    // Initialize axes to a safe default (EQ pointing at NCP, counterweight down).
    // updateMountAndPierSide() sets mount-type-dependent rates when config is loaded.
    axisPrimary.setDegrees(0.0);
    axisPrimary.TrackRate(Axis::SIDEREAL);
    axisSecondary.setDegrees(90.0);
    axisSecondary.TrackRate(Axis::OFF);
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

    mountModelArcminNP[MM_IH].fill("MM_IH", "Ha Zero (IH) '", "%.1f", -10800, 10800, 0.1, 0);
    mountModelArcminNP[MM_ID].fill("MM_ID", "Dec Zero (ID) '", "%.1f", -10800, 10800, 0.1, 0);
    mountModelArcminNP[MM_CH].fill("MM_CH", "Cone (CH) '", "%.1f", -300, 300, 0.1, 0);
    mountModelArcminNP[MM_NP].fill("MM_NP", "Ha/Dec (NP) '", "%.1f", -300, 300, 0.1, 0);
    mountModelArcminNP[MM_MA].fill("MM_MA", "Pole Azm (MA) '", "%.1f", -300, 300, 0.1, 0);
    mountModelArcminNP[MM_ME].fill("MM_ME", "Pole elev (ME) '", "%.1f", -300, 300, 0.1, 0);
    mountModelArcminNP.fill(getDeviceName(), "MOUNT_MODEL_ARCMIN", "Mount Model",
                            "Simulation", IP_RW, 0, IPS_IDLE);

    mountModelNP[MM_IH].fill("MM_IH", "Ha Zero (IH) deg", "%g", -180, 180, 0.01, 0);
    mountModelNP[MM_ID].fill("MM_ID", "Dec Zero (ID) deg", "%g", -180, 180, 0.01, 0);
    mountModelNP[MM_CH].fill("MM_CH", "Cone (CH) deg", "%g", -5, 5, 0.01, 0);
    mountModelNP[MM_NP].fill("MM_NP", "Ha/Dec (NP) deg", "%g", -5, 5, 0.01, 0);
    mountModelNP[MM_MA].fill("MM_MA", "Pole Azm (MA) deg", "%g", -5, 5, 0.01, 0);
    mountModelNP[MM_ME].fill("MM_ME", "Pole elev (ME) deg", "%g", -5, 5, 0.01, 0);
    mountModelNP.fill(getDeviceName(), "MOUNT_MODEL", "Mount Model (deg)",
                      "Simulation", IP_RO, 0, IPS_IDLE);

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

    PACDeviceTP[0].fill("PAC_DEVICE", "Alignment Corrector", "Alignment Correction Simulator");
    PACDeviceTP.fill(getDeviceName(), "ACTIVE_PAC", "Polar Alignment Corrector", "Simulation", IP_RW, 60, IPS_IDLE);
    PACDeviceTP.load();

    const char *pacDevice = PACDeviceTP[0].getText();
    if (pacDevice && strlen(pacDevice) > 0)
        IDSnoopDevice(pacDevice, "PAC_MANUAL_ADJUSTMENT");

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

    // Park data type is set in updateMountAndPierSide() based on the mount type.

    GI::initProperties(MOTION_TAB);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    setDefaultPollingPeriod(250);

    InitAlignmentProperties(this);

    EqPENP[PE_RA].fill("RA_PE",  "RA (hh:mm:ss)",  "%010.6m", 0,   24, 0, 0);
    EqPENP[PE_DEC].fill("DEC_PE", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    EqPENP.fill(getDeviceName(), "EQUATORIAL_PE", "True Pointing", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    return true;
}

void ScopeSim::ISGetProperties(const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

#ifdef USE_SIM_TAB
    // Load mount type settings and apply immediately so axis track rates and
    // alignment.mountType are correct before the first ReadScopeStatus tick.
    MountTypeSP.load();
    MountTypeSP.apply();
    defineProperty(simPierSideSP);
    simPierSideSP.load();
    updateMountAndPierSide();
    defineProperty(mountModelArcminNP);
    mountModelArcminNP.load();
    // Apply loaded arcmin values to the degrees mirror and alignment
    for (int i = 0; i < 6; i++)
        mountModelNP[i].setValue(mountModelArcminNP[i].getValue() / 60.0);
    alignment.setCorrections(mountModelNP[MM_IH].getValue(), mountModelNP[MM_ID].getValue(),
                             mountModelNP[MM_CH].getValue(), mountModelNP[MM_NP].getValue(),
                             mountModelNP[MM_MA].getValue(), mountModelNP[MM_ME].getValue());
    defineProperty(mountModelNP);
    defineProperty(mountAxisNP);
    defineProperty(flipHourAngleNP);
    flipHourAngleNP.load();
    defineProperty(decBacklashNP);
    decBacklashNP.load();
    defineProperty(PACDeviceTP);
    PACDeviceTP.load();
#endif
    m_currentAz  = axisPrimary.position.Degrees();
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
        // Always set lat/lon before axis init so instrumentHaDecToMount uses the right location.
        // updateLocation() is called later by the framework; this ensures it's set now from config.
        alignment.latitude  = Angle(LocationNP[LOCATION_LATITUDE].getValue());
        alignment.longitude = Angle(LocationNP[LOCATION_LONGITUDE].getValue());

        if (InitPark())
        {
            if (isParked())
            {
                // at this point there is a valid ParkData.xml available.
                // For ALTAZ, Axis1=Az (degrees), Axis2=Alt (degrees).
                // For EQ, Axis1=HA (hours), Axis2=Dec (degrees).
                if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
                {
                    // Convert stored INDI Az/Alt park position to instrument RA/Dec.
                    Angle instHA, instDec;
                    alignment.mountToInstrumentHaDec(
                        Angle(ParkPositionNP[AXIS_RA].getValue()),
                        Angle(ParkPositionNP[AXIS_DE].getValue()),
                        &instHA, &instDec);
                    m_currentRA  = (alignment.lst() - instHA).Hours();
                    m_currentDEC = instDec.Degrees();
                }
                else
                {
                    m_currentRA  = (alignment.lst() - Angle(ParkPositionNP[AXIS_RA].getValue(), Angle::ANGLE_UNITS::HOURS)).Hours();
                    m_currentDEC = ParkPositionNP[AXIS_DE].getValue();
                }
            }
            // If loading parking data is successful, we just set the default parking values.
            // ALTAZ: Az=0 (North horizon), Alt=0.  EQ: HA=-6h (counterweight down), Dec=0.
            double defaultAxis1 = (m_MountType == Alignment::MOUNT_TYPE::ALTAZ) ? 0. : -6.;
            SetAxis1ParkDefault(defaultAxis1);
            SetAxis2ParkDefault(0.);
        }
        else
        {
            // Otherwise, we set all parking data to default in case no parking data is found.
            double defaultAxis1 = (m_MountType == Alignment::MOUNT_TYPE::ALTAZ) ? 0. : -6.;
            SetAxis1Park(defaultAxis1);
            SetAxis2Park(0.);
            SetAxis1ParkDefault(defaultAxis1);
            SetAxis2ParkDefault(0.);
        }

        // Initialize axis positions from the current RA/Dec (park position if parked,
        // last known position otherwise) using naive (no Wallace) conversion.
        // Do NOT call Sync() here — it would add a spurious identity entry to the INDI
        // alignment database that conflicts with real sync points.
        // Also seed m_targetRA/Dec so the ALTAZ tracking servo has a valid target from the start.
        {
            Angle ha = alignment.lst() - Angle(m_currentRA * 15.0);
            Angle primary, secondary;
            alignment.instrumentHaDecToMount(ha, Angle(m_currentDEC), &primary, &secondary);
            axisPrimary.position   = primary;
            axisSecondary.position = secondary;
            m_currentAz  = axisPrimary.position.Degrees();
            m_currentAlt = axisSecondary.position.Degrees();
            m_targetRA   = m_currentRA;
            m_targetDEC  = m_currentDEC;
        }

        sendTimeFromSystem();

#ifdef USE_SIM_TAB
        const char *pacDevice = PACDeviceTP[0].getText();
        if (pacDevice && strlen(pacDevice) > 0)
            IDSnoopDevice(pacDevice, "PAC_MANUAL_ADJUSTMENT");
#endif

        defineProperty(EqPENP);
        SetAlignmentSubsystemActive(true);
    }
    else
    {
        deleteProperty(EqPENP);
        deleteProperty(GuideRateNP);
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


void ScopeSim::updateCurrentCoordsFromAxes()
{
    Angle instHA, instDec;
    alignment.mountToInstrumentHaDec(axisPrimary.position, axisSecondary.position, &instHA, &instDec);
    m_currentRA  = (alignment.lst() - instHA).Hours();
    m_currentDEC = instDec.Degrees();

    if (GetAlignmentDatabase().size() >= 1)
    {
        double corrRA, corrDec;
        bool corrected = false;
        if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
        {
            corrected = TelescopeAltAzToSky(axisSecondary.position.Degrees(),
                                            axisPrimary.position.Degrees(), corrRA, corrDec);
        }
        else
        {
            INDI::IEquatorialCoordinates haCoords{ instHA.Hours(), instDec.Degrees() };
            TelescopeDirectionVector tdv =
                TelescopeDirectionVectorFromLocalHourAngleDeclination(haCoords);
            corrected = TransformTelescopeToCelestial(tdv, corrRA, corrDec);
        }
        if (corrected)
        {
            m_currentRA  = corrRA;
            m_currentDEC = corrDec;
        }
    }
}

void ScopeSim::setTargetFromAxisPosition(Angle primary, Angle secondary)
{
    Angle instHA, instDec;
    alignment.mountToInstrumentHaDec(primary, secondary, &instHA, &instDec);
    m_targetRA  = (alignment.lst() - instHA).Hours();
    m_targetDEC = instDec.Degrees();
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

    // new mechanical angle calculation
    axisPrimary.update();
    axisSecondary.update();

    // True pointing: apply Wallace error injection to get where the scope physically points.
    // Published as EQUATORIAL_PE so the CCD simulator generates star fields at this position.
    Angle trueRa, trueDec;
    alignment.mountToApparentRaDec(axisPrimary.position, axisSecondary.position, &trueRa, &trueDec);
    EqPENP[PE_RA].setValue(trueRa.Hours());
    EqPENP[PE_DEC].setValue(trueDec.Degrees());
    EqPENP.setState(IPS_OK);
    EqPENP.apply();

    // Raw encoder position: what the mount "reports" without knowing its own errors.
    // This is what EQUATORIAL_EOD_COORD carries and what the INDI alignment module uses.
    // Apply INDI alignment correction if sync points exist.
    updateCurrentCoordsFromAxes();

    // Az/Alt for the ALTAZ tracking servo.  The servo (top of ReadScopeStatus) drives the
    // axis motors; it must compare axis-space positions against axis-space targets.  Using
    // Wallace-corrected (true) Az/Alt here would inject the Wallace error as a permanent
    // position offset, causing the servo to oscillate every tick.  Use raw axis positions.
    // axisPrimary stores INDI Az (0=North through East), same convention as m_currentAz.
    m_currentAz  = axisPrimary.position.Degrees();
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

                // Check slew accuracy in axis space so the result is frame-independent:
                // m_targetRA/Dec are instrument-frame; m_currentRA/Dec may be alignment-corrected.
                {
                    Angle targetHa = alignment.lst() - Angle(m_targetRA * 15.0);
                    Angle tPrimary, tSecondary;
                    alignment.instrumentHaDecToMount(targetHa, Angle(m_targetDEC), &tPrimary, &tSecondary);
                    double dPri = (axisPrimary.position   - tPrimary).Degrees();
                    double dSec = (axisSecondary.position - tSecondary).Degrees();
                    LOGF_DEBUG("slew accuracy pri %f arcsec, sec %f arcsec", dPri * 3600, dSec * 3600);
                }

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

        LOGF_EXTRA1("new %s: %f, ra %f", axisPrimary.axisName, PrimaryAngle, trueRa.Hours());
        LOGF_EXTRA1("new %s: %f, dec %f", axisSecondary.axisName, SecondaryAngle, trueDec.Degrees());

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
    // When the INDI alignment subsystem has learned sync points, transform the celestial
    // target through the alignment database so the mount ends up at the right sky position
    // despite the injected mount errors.
    //
    // ALTAZ mounts: the math plugin uses Az/Alt encoding.  Decode via SkyToTelescopeAltAz
    // and set axis slew targets directly in Az/Alt space.
    //
    // EQ mounts: the math plugin uses HA/Dec encoding.  Decode via LocalHourAngle... and
    // convert back through instrumentHaDecToMount inside StartSlew.
    if (GetAlignmentDatabase().size() >= 1 && m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
    {
        double instrumentAlt, instrumentAz_INDI;
        if (SkyToTelescopeAltAz(r, d, instrumentAlt, instrumentAz_INDI))
        {
            axisPrimary.StartSlew(Angle(instrumentAz_INDI));
            axisSecondary.StartSlew(Angle(instrumentAlt));

            // m_targetRA/Dec must be the instrument position (not celestial) so the ALTAZ
            // tracking servo's getAltAz() computes instrument Az/Alt rates, not celestial.
            setTargetFromAxisPosition(Angle(instrumentAz_INDI), Angle(instrumentAlt));

            TrackState = SCOPE_SLEWING;
            EqNP.setState(IPS_BUSY);
            LOGF_INFO("Slewing to RA: %.4f Dec: %.4f", m_targetRA, m_targetDEC);
            return true;
        }
    }

    double targetRA = r, targetDec = d;
    if (GetAlignmentDatabase().size() >= 1)
    {
        TelescopeDirectionVector tdv;
        if (TransformCelestialToTelescope(r, d, 0.0, tdv))
        {
            INDI::IEquatorialCoordinates haCoords;
            LocalHourAngleDeclinationFromTelescopeDirectionVector(tdv, haCoords);
            targetRA  = (alignment.lst() - Angle(haCoords.rightascension, Angle::HOURS)).Hours();
            targetDec = haCoords.declination;
        }
    }

    StartSlew(targetRA, targetDec, SCOPE_SLEWING);
    return true;
}

bool ScopeSim::Sync(double ra, double dec)
{
    // The INDI alignment math plugins encode telescope directions as:
    //   - ALTAZ (ZENITH) mounts: Az/Alt via TelescopeDirectionVectorFromAltitudeAzimuth
    //   - EQ mounts: HA/Dec via TelescopeDirectionVectorFromLocalHourAngleDeclination
    // Using the wrong encoding causes the correction to be applied in the wrong coordinate
    // system, which after Goto's instrumentHaDecToMount re-rotation produces garbage altitudes.

    if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
    {
        double indi_az  = axisPrimary.position.Degrees();
        double indi_alt = axisSecondary.position.Degrees();
        AddAlignmentEntryAltAz(ra, dec, indi_alt, indi_az);  // handles DB + Initialise
    }
    else
    {
        // EQ mounts: encode the raw encoder HA/Dec as the telescope direction.
        Angle instHA, instDec;
        alignment.mountToInstrumentHaDec(axisPrimary.position, axisSecondary.position,
                                         &instHA, &instDec);

        INDI::IEquatorialCoordinates haCoords{ instHA.Hours(), instDec.Degrees() };
        TelescopeDirectionVector tdv =
            TelescopeDirectionVectorFromLocalHourAngleDeclination(haCoords);

        AlignmentDatabaseEntry entry;
        entry.ObservationJulianDate = ln_get_julian_from_sys();
        entry.RightAscension        = ra;
        entry.Declination           = dec;
        entry.TelescopeDirection    = tdv;
        entry.PrivateDataSize       = 0;

        if (!CheckForDuplicateSyncPoint(entry))
        {
            GetAlignmentDatabase().push_back(entry);
            UpdateSize();
            Initialise(this);
        }
    }

    // Keep the ALTAZ tracking servo on the current instrument position after sync.
    setTargetFromAxisPosition(axisPrimary.position, axisSecondary.position);

    LOGF_DEBUG("Sync at RA %f Dec %f  m_targetRA %f m_targetDEC %f",
               ra, dec, m_targetRA, m_targetDEC);

    LOG_INFO("Sync is successful.");

    EqNP.setState(IPS_OK);
    NewRaDec(m_currentRA, m_currentDEC);

    return true;
}

bool ScopeSim::Park()
{
    if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
    {
        // For ALTAZ, park position is stored as INDI Az (degrees) / Alt (degrees).
        // Convert directly to axis positions and slew; do not go through StartSlew's
        // HA/Dec path which treats Axis1 as hours.
        Angle indi_az = Angle(GetAxis1Park());
        Angle alt     = Angle(GetAxis2Park());
        axisPrimary.StartSlew(indi_az);
        axisSecondary.StartSlew(alt);
        setTargetFromAxisPosition(indi_az, alt);
        TrackState  = SCOPE_PARKING;
        EqNP.setState(IPS_BUSY);
        LOG_INFO("Parking...");
    }
    else
    {
        double ra = (alignment.lst() - Angle(GetAxis1Park() * 15.)).Degrees() / 15.;
        StartSlew(ra, GetAxis2Park(), SCOPE_PARKING);
    }
    return true;
}

// common code for GoTo and park
void ScopeSim::StartSlew(double ra, double dec, TelescopeStatus status)
{
    // Naive axis conversion: no Wallace corrections.  The mount slews to where its
    // encoders say, unaware of its own IH/ID/etc. errors — just like a real mount.
    Angle ha = alignment.lst() - Angle(ra * 15.0);
    Angle primary, secondary;
    alignment.instrumentHaDecToMount(ha, Angle(dec), &primary, &secondary);

    axisPrimary.StartSlew(primary);
    axisSecondary.StartSlew(secondary);

    // Store instrument-frame RA/Dec so the ALTAZ tracking servo always works
    // in a consistent coordinate frame, regardless of how StartSlew was called.
    setTargetFromAxisPosition(primary, secondary);
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

    ProcessAlignmentNumberProperties(this, name, values, names, n);

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
            saveConfig(MountTypeSP);
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

    ProcessAlignmentSwitchProperties(this, name, states, names, n);

    // Persist alignment plugin and active state immediately when changed, since
    // SaveAlignmentConfigProperties() is only called during a full saveConfigItems() run.
    if (strcmp(name, "ALIGNMENT_SUBSYSTEM_MATH_PLUGINS") == 0 ||
        strcmp(name, "ALIGNMENT_SUBSYSTEM_ACTIVE") == 0)
        saveConfig(true, name);

    //  Nobody has claimed this, so pass it over
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool ScopeSim::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
#ifdef USE_SIM_TAB
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (PACDeviceTP.isNameMatch(name))
        {
            PACDeviceTP.setState(IPS_OK);
            PACDeviceTP.update(texts, names, n);
            PACDeviceTP.apply();

            const char *pacDevice = PACDeviceTP[0].getText();
            if (pacDevice && strlen(pacDevice) > 0)
                IDSnoopDevice(pacDevice, "PAC_MANUAL_ADJUSTMENT");

            saveConfig(PACDeviceTP);
            return true;
        }
    }
#endif
    ProcessAlignmentTextProperties(this, name, texts, names, n);
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool ScopeSim::ISSnoopDevice(XMLEle *root)
{
#ifdef USE_SIM_TAB
    const char *propName   = findXMLAttValu(root, "name");
    const char *deviceName = findXMLAttValu(root, "device");
    const char *pacDevice  = PACDeviceTP[0].getText();

    if (pacDevice && strlen(pacDevice) > 0 && deviceName && strcmp(deviceName, pacDevice) == 0)
    {
        if (!strcmp(propName, "PAC_MANUAL_ADJUSTMENT"))
        {
            // Capture the step values (AZ and ALT) whenever the property is updated.
            // PAC_MANUAL_ADJUSTMENT[MANUAL_AZ_STEP]  > 0 → East,  < 0 → West
            // PAC_MANUAL_ADJUSTMENT[MANUAL_ALT_STEP] > 0 → North, < 0 → South
            XMLEle *ep = nullptr;
            for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
            {
                const char *elemName = findXMLAttValu(ep, "name");
                if (!strcmp(elemName, "MANUAL_AZ_STEP"))
                    m_snoopedAzError = atof(pcdataXMLEle(ep));
                else if (!strcmp(elemName, "MANUAL_ALT_STEP"))
                    m_snoopedAltError = atof(pcdataXMLEle(ep));
            }

            // Only update the mount model once the physical move has completed (state = Ok).
            const char *stateStr = findXMLAttValu(root, "state");
            if (!strcmp(stateStr, "Ok"))
            {
                // The PAA's azStep and MM_MA use opposite sign conventions:
                //   azStep > 0 (correction East) ↔ MM_MA decreases in the model
                // Adding m_snoopedAzError to the current MM_MA correctly simulates
                // the physical delta correction applied to the mount.
                double oldMA = mountModelNP[MM_MA].getValue();
                double oldME = mountModelNP[MM_ME].getValue();
                double newMA = oldMA + m_snoopedAzError;
                double newME = oldME + m_snoopedAltError;

                mountModelNP[MM_MA].setValue(newMA);
                mountModelNP[MM_ME].setValue(newME);
                alignment.setCorrections(mountModelNP[MM_IH].getValue(), mountModelNP[MM_ID].getValue(),
                                         mountModelNP[MM_CH].getValue(), mountModelNP[MM_NP].getValue(),
                                         mountModelNP[MM_MA].getValue(), mountModelNP[MM_ME].getValue());

                mountModelNP.setState(IPS_OK);
                mountModelNP.apply();

                LOGF_INFO("Applied PAC correction: MA %.4f -> %.4f, ME %.4f -> %.4f",
                          oldMA, newMA, oldME, newME);

                // Force an immediate coordinate update so that the next CCD capture
                // uses the corrected pointing rather than waiting up to one full
                // polling cycle (default 500 ms) for ReadScopeStatus to run.
                updateCurrentCoordsFromAxes();
                NewRaDec(m_currentRA, m_currentDEC);
            }
            return true;
        }
    }
#endif
    return INDI::Telescope::ISSnoopDevice(root);
}

bool ScopeSim::Abort()
{
    axisPrimary.Abort();
    axisSecondary.Abort();
    if (TrackState == SCOPE_PARKING || TrackState == SCOPE_SLEWING)
        TrackState = SCOPE_IDLE;
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
    if (HasPierSide() && (currentPierSide == PIER_WEST)) // see scopesim_helper.cpp: alignment
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

void ScopeSim::guideAltAzDecomposed(double dNS, double dEW, uint32_t ms)
{
    // Decompose a celestial N/S/E/W guide pulse into Az/Alt axis components using
    // the parallactic angle q.  sinQ = sin(q)/cos(alt), cosQ = cos(q)/cos(alt).
    // The azimuth rate for celestial North is -sin(q)/cos(alt); since sinQ already
    // carries one 1/cos(alt), dAz_N = -sinQ/cosAlt gives -sin(q)/cos^2(alt), which
    // is the correct field rotation formula.
    //
    // Celestial North in Az/Alt:  dAlt = cos(q),   dAz = -sin(q)/cos(alt)
    // Celestial East  in Az/Alt:  dAlt = sin(q),   dAz =  cos(q)/cos(alt)

    double H      = (alignment.lst() - Angle(m_currentRA * 15.0)).radians();
    double dec    = Angle(m_currentDEC).radians();
    double lat    = alignment.latitude.radians();
    double alt    = m_currentAlt * M_PI / 180.0;
    double cosAlt = std::cos(alt);

    if (std::abs(cosAlt) > 1e-4)
    {
        double sinQ = std::sin(H) * std::cos(lat) / cosAlt;
        double cosQ = (std::sin(lat) - std::sin(dec) * std::sin(alt))
                      / (std::cos(dec) * cosAlt);

        double dAlt_N =  cosQ;
        double dAz_N  = -sinQ / cosAlt;
        double dAlt_E =  sinQ;
        double dAz_E  =  cosQ / cosAlt;

        axisPrimary.StartGuide(dNS * dAz_N + dEW * dAz_E, ms);
        axisSecondary.StartGuide(dNS * dAlt_N + dEW * dAlt_E, ms);
    }
    else
    {
        // Near zenith: Az is degenerate; fall back to direct axis motion.
        axisPrimary.StartGuide(dEW, ms);
        axisSecondary.StartGuide(dNS, ms);
    }
}

IPState ScopeSim::GuideNorth(uint32_t ms)
{
    if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
    {
        guideAltAzDecomposed(GuideRateNP[DEC_AXIS].getValue(), 0.0, ms);
    }
    else
    {
        double rate = GuideRateNP[DEC_AXIS].getValue();
        if (HasPierSide() && (currentPierSide == PIER_WEST)) // see scopsim_helper.cpp: alignment
            rate = -rate;
        ms = applyDecBacklash(rate, ms);
        axisSecondary.StartGuide(rate, ms);
    }
    guidingNS = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideSouth(uint32_t ms)
{
    if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
    {
        guideAltAzDecomposed(-GuideRateNP[DEC_AXIS].getValue(), 0.0, ms);
    }
    else
    {
        double rate = GuideRateNP[DEC_AXIS].getValue();
        if (HasPierSide() && (currentPierSide == PIER_WEST)) // see scopsim_helper.cpp: alignment
            rate = -rate;
        ms = applyDecBacklash(-rate, ms);
        axisSecondary.StartGuide(-rate, ms);
    }
    guidingNS = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideEast(uint32_t ms)
{
    if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
        guideAltAzDecomposed(0.0, -GuideRateNP[RA_AXIS].getValue(), ms);
    else
        axisPrimary.StartGuide(-GuideRateNP[RA_AXIS].getValue(), ms);
    guidingEW = true;
    return IPS_BUSY;
}

IPState ScopeSim::GuideWest(uint32_t ms)
{
    if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
        guideAltAzDecomposed(0.0, GuideRateNP[RA_AXIS].getValue(), ms);
    else
        axisPrimary.StartGuide(GuideRateNP[RA_AXIS].getValue(), ms);
    guidingEW = true;
    return IPS_BUSY;
}

bool ScopeSim::SetCurrentPark()
{
    if (m_MountType == Alignment::MOUNT_TYPE::ALTAZ)
    {
        // Store INDI Az (0=North) and Alt in degrees.
        SetAxis1Park(m_currentAz);
        SetAxis2Park(m_currentAlt);
    }
    else
    {
        double ha  = (alignment.lst() - Angle(m_currentRA, Angle::ANGLE_UNITS::HOURS)).Hours();
        SetAxis1Park(ha);
        SetAxis2Park(m_currentDEC);
    }
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
            if (m_MountType != Alignment::MOUNT_TYPE::ALTAZ)
                axisSecondary.TrackRate(Axis::OFF);
            return true;
        case TRACK_SOLAR:
            axisPrimary.TrackRate(Axis::SOLAR);
            if (m_MountType != Alignment::MOUNT_TYPE::ALTAZ)
                axisSecondary.TrackRate(Axis::OFF);
            return true;
        case TRACK_LUNAR:
            axisPrimary.TrackRate(Axis::LUNAR);
            if (m_MountType != Alignment::MOUNT_TYPE::ALTAZ)
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

    SaveAlignmentConfigProperties(fp);

#ifdef USE_SIM_TAB
    GuideRateNP.save(fp);
    MountTypeSP.save(fp);
    simPierSideSP.save(fp);
    mountModelNP.save(fp);
    flipHourAngleNP.save(fp);
    decBacklashNP.save(fp);
    PACDeviceTP.save(fp);

#endif
    return true;
}

bool ScopeSim::updateLocation(double latitude, double longitude, double elevation)
{
    LOGF_DEBUG("Update location %8.3f, %8.3f, %4.0f", latitude, longitude, elevation);

    alignment.latitude = Angle(latitude);
    alignment.longitude = Angle(longitude);

    UpdateLocation(latitude, longitude, elevation);
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
    int mountType = static_cast<int>(Alignment::MOUNT_TYPE::EQ_GEM);
    int pierSide  = PS_ON;
#endif
    if ( mountType == Alignment::MOUNT_TYPE::ALTAZ)
    {
        LOG_INFO("AltAz mount type experimental");
        // return false;
    }

    alignment.mountType = static_cast<Alignment::MOUNT_TYPE>(mountType);

    // Tell the INDI alignment subsystem whether this is an equatorial or ALTAZ mount.
    // Without this, the math plugin always fits an ALTAZ model regardless of mount type.
    SetApproximateMountAlignmentFromMountType(
        alignment.mountType == Alignment::MOUNT_TYPE::ALTAZ
            ? INDI::AlignmentSubsystem::MathPluginManagement::ALTAZ
            : INDI::AlignmentSubsystem::MathPluginManagement::EQUATORIAL);

    // Set the park data type appropriate for the mount geometry.
    if (mountType == static_cast<int>(Alignment::MOUNT_TYPE::ALTAZ))
        SetParkDataType(PARK_AZ_ALT);
    else
        SetParkDataType(PARK_HA_DEC);

    // Reinitialize the secondary axis track rate for the selected mount type.
    // ALTAZ: both axes need sidereal rate (overridden each tick by parabolic tracking).
    // EQ: primary tracks sidereal, secondary (Dec) is driven only when slewing.
    if (mountType == Alignment::MOUNT_TYPE::ALTAZ)
        axisSecondary.TrackRate(Axis::SIDEREAL);
    else
        axisSecondary.TrackRate(Axis::OFF);

    // update the pier side capability depending on the mount type
    uint32_t cap = GetTelescopeCapability();
    if (pierSide == PS_ON && mountType == static_cast<int>(Alignment::MOUNT_TYPE::EQ_GEM))
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
