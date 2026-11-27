/*
    OnStepX INDI Driver — Mount device class
    Copyright (C) 2026 Ed Lee

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

// Debug only

#include <signal.h>
#include <unistd.h>

// Debug only end

#include "OnStepXMount.h"
#include "OnStepXVersion.h"
#include "OnStepXConfig.h"
#include "OnStepXFormat.h"

#include <indicom.h>

#include <cmath>
#include <cstring>

#define WEATHER_TAB "Weather"   // tab name for WeatherInterface properties

// ---------------------------------------------------------------------------
// Track mode indices — must match AddTrackMode() call order in initProperties()
// ---------------------------------------------------------------------------
// NOTE: deliberately prefixed.  INDI::Telescope declares its own
// TRACK_SIDEREAL/SOLAR/LUNAR/CUSTOM (0-3, different order) which shadow
// unprefixed file-scope names inside member functions — that previously
// swapped Lunar and Solar.
enum { OSX_TRACK_SIDEREAL = 0, OSX_TRACK_LUNAR = 1, OSX_TRACK_SOLAR = 2, OSX_TRACK_KING = 3, OSX_TRACK_CUSTOM = 4 };

static constexpr double SIDEREAL_ARCSEC_PER_SEC = OnStepXTracking::SIDEREAL_ARCSEC_PER_SEC;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
OnStepXMount::OnStepXMount() : INDI::GuiderInterface(this),
    INDI::WeatherInterface(this)
{
    // Debug only

    // Halts the process at this point. Allows remote debugger to attach which is required
    // when launching the driver from a client eg. Ekos
    //     kill(getpid(), SIGSTOP);

    // Debug only end

    setVersion(ONSTEPX_VERSION_MAJOR, ONSTEPX_VERSION_MINOR);
    m_alignment.setDevice(this);
    m_alignment.setModelBuildCheck([this]()
    {
        return m_modelBuilder.isBuilding();
    });
    m_modelBuilder.setDevice(this);
    m_core.setDevice(this);
    m_guide.setDevice(this);
    m_guide.setGuiderInterface(this);
    m_info.setDevice(this);
    m_limits.setDevice(this);
    m_meridian.setDevice(this);
    m_pec.setDevice(this);
    m_site.setDevice(this);
    m_tracking.setDevice(this);
    m_weather.setDevice(this);
    m_driverStatus.setDevice(this);
    // comm pointers set in updateProperties after connect

    // Provisional capability set — refined in Handshake() once probed.
    SetTelescopeCapability(
        TELESCOPE_CAN_GOTO        |
        TELESCOPE_CAN_SYNC        |
        TELESCOPE_CAN_PARK        |
        TELESCOPE_CAN_ABORT       |
        TELESCOPE_HAS_TIME        |
        TELESCOPE_HAS_LOCATION    |
        TELESCOPE_HAS_TRACK_MODE  |
        TELESCOPE_CAN_CONTROL_TRACK |
        TELESCOPE_HAS_TRACK_RATE  |
        TELESCOPE_CAN_HOME_FIND   |
        TELESCOPE_CAN_HOME_SET,
        4);   // 4 slew rates
}

// ---------------------------------------------------------------------------
// Identification
// ---------------------------------------------------------------------------
const char *OnStepXMount::getDefaultName()
{
    return "OnStepX";
}

// ---------------------------------------------------------------------------
// initProperties
// ---------------------------------------------------------------------------
bool OnStepXMount::initProperties()
{
    INDI::Telescope::initProperties();
    SetParkDataType(PARK_RA_DEC);

    // Track modes — order must match OSX_TRACK_* constants above
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_LUNAR",    "Lunar");
    AddTrackMode("TRACK_SOLAR",    "Solar");
    AddTrackMode("TRACK_KING",     "King");
    AddTrackMode("TRACK_CUSTOM",   "Custom");   // uses TELESCOPE_TRACK_RATE

    // Slew rate labels are set by the base class (inditelescope.cpp) when
    // SetTelescopeCapability(caps, 4) is called; SetSlewRate() maps them.

    // Limits and home
    m_limits.initProperties();

    // Site profiles
    m_site.initProperties();

    // Advanced tracking properties
    m_tracking.initProperties();
    m_meridian.initProperties();

    // PEC
    m_pec.initProperties();

    // Alignment
    m_alignment.initProperties();
    m_modelBuilder.initProperties();

    // The rotator is a child device, OnStepXRotatorDevice ("<name> Rotator").

    // Guider interface — standard TELESCOPE_TIMED_GUIDE_NS/WE properties
    GI::initProperties(MOTION_TAB);
    // Ekos lists ST4 guiders ("guide via mount") by GUIDER_INTERFACE.
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    // Guide rate and pulse helper
    m_guide.initProperties();

    // Info (firmware display, status text, reticle)
    m_info.initProperties();

    // Weather interface — tab name, parameter group name
    WI::initProperties(WEATHER_TAB, WEATHER_TAB);
    m_weather.initProperties();

    return true;
}

// ---------------------------------------------------------------------------
// updateProperties — register/deregister properties on connect/disconnect
// ---------------------------------------------------------------------------
bool OnStepXMount::updateProperties()
{
    INDI::Telescope::updateProperties();

    // OnStepX parks only at a position saved from the current one (:hQ#,
    // TELESCOPE_PARK_OPTION Current); it can't be given park coordinates and
    // doesn't report them, so INDI's park position would only mislead.
    if (isConnected())
        deleteProperty(ParkPositionNP);

    GI::updateProperties();

    if (isConnected())
    {
        m_alignment.setComm(&m_core.comm());
        m_modelBuilder.setComm(&m_core.comm());
        m_guide.setComm(&m_core.comm());
        m_info.setComm(&m_core.comm());
        m_pec.setComm(&m_core.comm());
        m_site.setComm(&m_core.comm());
        m_limits.setComm(&m_core.comm());
        m_meridian.setComm(&m_core.comm());
        m_tracking.setComm(&m_core.comm());
        m_weather.setComm(&m_core.comm());
        m_driverStatus.setComm(&m_core.comm());
    }

    if (m_core.caps().hasWeatherRead)
    {
        if (isConnected())
        {
            // addParameter() is deferred to here because hasWeatherRead is
            // not known until Handshake()/probeController().
            // WeatherInterface::addParameter() appends, so add exactly once
            // per process; reconnects reuse the same parameters.
            if (!m_weatherParamsAdded)
            {
                addParameter("WEATHER_TEMPERATURE", "Temperature (C)", -40,   80, 15);
                addParameter("WEATHER_PRESSURE",    "Pressure (hPa)",  800, 1100, 15);
                addParameter("WEATHER_HUMIDITY",    "Humidity (%)",      0,  100, 15);
                addParameter("WEATHER_DEWPOINT",    "Dew Point (C)",   -40,   40, 15);
                m_weatherParamsAdded = true;
            }

            WI::updateProperties();

            // Weather is polled from ReadScopeStatus (every 30 polls) so all
            // controller I/O happens in one predictable place.  With the
            // period at 0 the WI timer only re-arms itself to retry after a
            // failed read.
            UpdatePeriodNP[0].setValue(0);
            UpdatePeriodNP.apply();
        }
        else
        {
            WI::updateProperties();   // disconnect path — deletes WI properties
        }
    }
    if (isConnected())
    {
        m_site.updateProperties(true);

        // Publish the site the controller is actually using.  INDI::Telescope
        // only copies a saved location into GEOGRAPHIC_COORD at start-up and
        // never sends it to the mount, so without this the property can show
        // a location the controller is not using.  A client that sets the
        // location still writes it to the controller via updateLocation().
        double lat = 0, lon = 0, elev = 0;
        if (m_site.readLocation(lat, lon, elev))
        {
            LocationNP[LOCATION_LATITUDE].setValue(lat);
            LocationNP[LOCATION_LONGITUDE].setValue(lon);
            LocationNP[LOCATION_ELEVATION].setValue(elev);
            LocationNP.setState(IPS_OK);
            LocationNP.apply();
            updateObserverLocation(lat, lon, elev);
        }
        // Manual weather values and DUT1 are always offered: without a sensor
        // the readings are "nan" until the manual values are set.
        m_weather.updateProperties(true);
        m_limits.updateProperties(true, m_core.caps().hasHomeSense, m_core.caps().mountType == MountType::GEM);
        m_tracking.updateProperties(true, m_core.caps().hasMeridianFlips);
        syncTrackModeFromFirmware();
        m_meridian.updateProperties(true, m_core.caps().hasMeridianFlips);
        m_guide.updateProperties(true);
        m_info.updateProperties(true, m_core.caps());
        m_weather.updateMcuTemp(true, m_core.caps().hasMcuTemp);   // below firmware and status
        m_info.updateReticle(true);
        createFocusers();

        OnStepXRotatorDevice::connectChild(m_rotatorDev, m_core.caps().hasRotator, m_core.caps().hasDerotator,
                                           &m_core.comm(), getDeviceName());

        OnStepXPowerDevice::connectChild(m_powerDev, m_core.caps().featureMask, m_core.caps().portMask,
                                         &m_core.comm(), getDeviceName());

        m_driverStatus.discoverAndDefine(m_core.caps().driverStatusMask);

        if (m_core.caps().hasPec)
            m_pec.updateProperties(true);

        const MountType mt = m_core.caps().mountType;
        m_alignment.updateProperties(true, mt == MountType::GEM || mt == MountType::FORK);
        m_modelBuilder.updateProperties(true);

        // Put back a model left suspended by a session the driver could not
        // finish (crash or kill).  The two enums list the same mount types.
        static_assert(static_cast<int>(MountType::ALTALT) == static_cast<int>(MountStatus::MountType::ALTALT) &&
                      static_cast<int>(MountType::UNKNOWN) == static_cast<int>(MountStatus::MountType::UNKNOWN),
                      "Capabilities and MountStatus mount types differ");
        m_modelBuilder.setBackupPath(OnStepXModelBuilder::defaultBackupPath(getDeviceName()));
        m_modelBuilder.recoverInterruptedSession(static_cast<MountStatus::MountType>(mt));
    }
    else
    {
        m_alignment.updateProperties(false, false);
        m_modelBuilder.updateProperties(false);
        m_limits.updateProperties(false, false);
        m_tracking.updateProperties(false);
        m_meridian.updateProperties(false, false);
        m_guide.updateProperties(false);
        m_info.updateProperties(false, {});
        m_weather.updateMcuTemp(false, false);
        m_info.updateReticle(false);
        m_site.updateProperties(false);

        m_weather.updateProperties(false);

        if (m_core.caps().hasPec)
            m_pec.updateProperties(false);

        m_driverStatus.deleteAll();
    }

    return true;
}

// ---------------------------------------------------------------------------
// Handshake
// ---------------------------------------------------------------------------
bool OnStepXMount::Handshake()
{
    m_core.setFd(PortFD);

    if (!m_core.probeController())
    {
        LOG_ERROR("Not an OnStepX controller. Aborting connection.");
        return false;
    }

    if (!m_core.probeMount())
    {
        LOG_ERROR("No mount detected. For mount-less OnStepX use indi_onstepx_aux instead.");
        return false;
    }

    const Capabilities &cap = m_core.caps();

    // Home find/set are always available on OnStepX; pier side is probed.
    uint32_t telescopeCaps =
        TELESCOPE_CAN_GOTO       | TELESCOPE_CAN_SYNC       | TELESCOPE_CAN_PARK  |
        TELESCOPE_CAN_ABORT      | TELESCOPE_HAS_TIME       | TELESCOPE_HAS_LOCATION |
        TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK |
        TELESCOPE_HAS_TRACK_RATE |
        TELESCOPE_CAN_HOME_FIND  | TELESCOPE_CAN_HOME_SET;

    if (cap.hasPierSide)
        telescopeCaps |= TELESCOPE_HAS_PIER_SIDE;

    // Adds FLIP to ON_COORD_SET; Flip() sends :MN#.
    if (cap.hasMeridianFlips)
        telescopeCaps |= TELESCOPE_CAN_FLIP;

    SetTelescopeCapability(telescopeCaps, 4);

    // TELESCOPE_MOUNT_TYPE defaults to EQ_GEM and is defined after Handshake().
    // INDI has no alt-alt entry, so ALTALT reports as the non-equatorial ALTAZ.
    MountTypeSP.reset();
    switch (cap.mountType)
    {
        case MountType::FORK:
            MountTypeSP[MOUNT_EQ_FORK].setState(ISS_ON);
            break;
        case MountType::ALTAZM:
        case MountType::ALTALT:
            MountTypeSP[MOUNT_ALTAZ].setState(ISS_ON);
            break;
        default:
            MountTypeSP[MOUNT_EQ_GEM].setState(ISS_ON);
            break;
    }

    m_homingSeen = false;
    return true;
}

// ---------------------------------------------------------------------------
// Disconnect — stop all controller I/O before the port is closed, so neither
// this device nor the focuser children can write to a stale (possibly reused)
// file descriptor.
// ---------------------------------------------------------------------------
bool OnStepXMount::Disconnect()
{
    // Restore the firmware pointing model if a build session is active.
    m_modelBuilder.abortSession("disconnecting");

    m_core.setFd(-1);
    disconnectFocusers();
    OnStepXRotatorDevice::disconnectChild(m_rotatorDev);
    OnStepXPowerDevice::disconnectChild(m_powerDev);
    return INDI::Telescope::Disconnect();
}

// ---------------------------------------------------------------------------
// ReadScopeStatus — ≤80 lines, delegates to named updaters
// ---------------------------------------------------------------------------
void OnStepXMount::TimerHit()
{
    if (isConnected() && m_core.comm().linkLost())
    {
        LOGF_ERROR("No reply from the controller for %.0f s; disconnecting. Check its power and cable, then reconnect.",
                   m_core.comm().secondsSinceReply());
        m_core.setFd(-1);   // so the clean-up in Disconnect() does not wait on the dead link
        Disconnect();
        setConnected(false, IPS_ALERT);
        updateProperties();
        return;             // no further polls
    }
    INDI::Telescope::TimerHit();
}

// An error for the first failure in a run; after that the comm layer has
// warned that the controller is not replying, so stay at debug level.
void OnStepXMount::logPollFailure(const char *msg)
{
    if (m_core.comm().failStreak() > 1)
        LOGF_DEBUG("%s", msg);
    else
        LOGF_ERROR("%s", msg);
}

bool OnStepXMount::ReadScopeStatus()
{
    if (!refreshMountStatus())
        return false;

    // TrackState first: updateCoordinates() calls NewRaDec(), which sets the
    // EQUATORIAL_EOD_COORD state from TrackState.  The other way round it
    // lags the firmware by one poll, e.g. Busy again just after an Abort.
    updateTrackingState(m_status);

    if (!updateCoordinates())
        return false;

    if (m_core.caps().hasPierSide)
    {
        if (m_status.pierSide == MountStatus::PierSide::EAST)
            setPierSide(PIER_EAST);
        else if (m_status.pierSide == MountStatus::PierSide::WEST)
            setPierSide(PIER_WEST);
    }

    // Throttled subsystems, staggered so a poll carries at most one of them.
    // Run together they took ~650 ms at 9600 baud, and an Abort waits for
    // the poll in progress.  Periods: 5 polls (focusers, rotator, outputs),
    // 10 (PEC, alignment, driver status) and 30 (weather, MCU temperature).
    m_pollCount++;
    switch (m_pollCount % 5)
    {
        case 0:
            updateFocuserStates();
            break;
        case 1:
            updateRotatorState();
            break;
        case 2:
            updatePowerStates();
            break;
        case 3:
            if (m_pollCount % 10 == 3)
                updatePecStatus();
            else
                updateAlignmentStatus();
            break;
        default:
            if (m_pollCount % 10 == 4)
                m_driverStatus.poll();
            else if (m_pollCount % 30 == 9)
                updateWeatherDirect();
            else if (m_pollCount % 30 == 19)
                m_weather.pollMcuTemp();
            break;
    }
    m_info.updateStatus(m_status);
    m_meridian.update(m_status, EqNP[AXIS_RA].getValue());

    return true;
}

// ---------------------------------------------------------------------------
// refreshMountStatus — :Gu# preferred; falls back to :GU#
// ---------------------------------------------------------------------------
bool OnStepXMount::refreshMountStatus()
{
    if (m_core.caps().hasBinaryStatus)
    {
        uint8_t bin[OnStepXStatus::GU_REPLY_LEN];
        // Quiet: a miss here (e.g. a controller stall) is recovered by the
        // :GU# fallback below, which logs an error if it also fails.
        if (m_core.comm().sendReadN(":Gu#", bin, OnStepXStatus::GU_REPLY_LEN, 2000, true))
        {
            if (OnStepXStatus::parseGuReply(bin, OnStepXStatus::GU_REPLY_LEN, m_status))
                return true;
            // Usually line noise (0xFF bursts around NV writes on ESP32) read in
            // place of the reply; the real bytes may still be arriving.
            LOG_DEBUG("Binary :Gu# reply invalid -- resyncing and falling back to :GU#");
            m_core.comm().requestResync();
        }
    }

    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_core.comm().sendText(":GU#", reply))
    {
        logPollFailure("Failed to read mount status (:GU#)");
        return false;
    }

    if (!OnStepXStatus::parseGU(reply, m_status))
    {
        LOGF_ERROR("Failed to parse :GU# reply: '%s'", reply);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// updateCoordinates
//
// :GRH#/:GDH# return the firmware's model-corrected RA/Dec for every mount
// type (Mount::getPosition() -> Transform::mountToNative(), which converts
// AltAz/AltAlt to equatorial), so no client-side transform is needed.
// ---------------------------------------------------------------------------
bool OnStepXMount::updateCoordinates()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    double ra = 0, dec = 0;

    if (!m_core.comm().sendText(":GRH#", reply))
    {
        logPollFailure("Failed to read RA (:GRH#)");
        return false;
    }
    if (f_scansexa(reply, &ra) < 0)
    {
        LOGF_ERROR("Failed to parse RA: '%s'", reply);
        return false;
    }

    if (!m_core.comm().sendText(":GDH#", reply))
    {
        logPollFailure("Failed to read Dec (:GDH#)");
        return false;
    }
    if (f_scansexa(reply, &dec) < 0)
    {
        LOGF_ERROR("Failed to parse Dec: '%s'", reply);
        return false;
    }

    NewRaDec(ra, dec);
    return true;
}

// ---------------------------------------------------------------------------
// updateTrackingState
//
// The firmware is the authority on park state: it can be parked or unparked
// from the hand controller or web UI, and may already be parked when the
// driver connects.  Mirror any difference into INDI's park property.
// ---------------------------------------------------------------------------
void OnStepXMount::updateTrackingState(const MountStatus &s)
{
    // Keep the model-builder lifecycle state current even when the INDI
    // TrackState handling below takes an early return.
    m_modelBuilder.updateTrackingState(s.tracking);
    m_modelBuilder.updateMountType(s.mountType);

    updateHomeState(s);
    m_limits.syncAutoHome(s.autoHomeAtBoot);

    const bool fwParked = (s.parkState == MountStatus::ParkState::PARKED);
    if (fwParked != isParked())
        SetParked(fwParked);    // also sets TrackState and ParkSP
    else if (ParkSP.findOnSwitchIndex() < 0)
        // TELESCOPE_PARK starts with neither switch on, and the base class's
        // abort-while-parking path leaves it that way.  Telescope::ISNewSwitch
        // restores ParkSP[findOnSwitchIndex()] when Park() fails, so a refused
        // park would index -1 and kill the driver.
        SyncParkStatus(fwParked);

    if (fwParked)
    {
        TrackState = SCOPE_PARKED;
        return;
    }

    if (s.parkState == MountStatus::ParkState::PARKING)
    {
        TrackState = SCOPE_PARKING;
        return;
    }

    if (s.parkState == MountStatus::ParkState::FAILED && TrackState == SCOPE_PARKING)
    {
        LOG_ERROR("Park failed");
        ParkSP.setState(IPS_ALERT);
        ParkSP.apply();
    }

    if (s.homing || s.gotoActive)
        TrackState = SCOPE_SLEWING;
    else
        TrackState = s.tracking ? SCOPE_TRACKING : SCOPE_IDLE;

    // Sync advanced tracking properties from status
    m_tracking.syncStatus(s);
}

// ---------------------------------------------------------------------------
// updateHomeState — complete the TELESCOPE_HOME switch when homing ends.
// ---------------------------------------------------------------------------
void OnStepXMount::updateHomeState(const MountStatus &s)
{
    if (HomeSP.getState() != IPS_BUSY)
    {
        m_homingSeen = false;
        return;
    }

    if (s.homing)
    {
        m_homingSeen = true;
        return;
    }

    // Without a home sensor :hC# is an ordinary goto to the home position,
    // so it can finish without ever reporting 'homing'; accept "at home".
    if (s.gotoActive)
        return;

    if (m_homingSeen || s.atHome)
    {
        HomeSP.reset();
        HomeSP.setState(s.atHome ? IPS_OK : IPS_ALERT);
        HomeSP.apply();
        if (s.atHome)
        {
            LOG_INFO("Mount is at home.");
            reapplyCustomTrackRate();   // homing clears the firmware offsets
        }
        else
            LOG_ERROR("Homing ended without reaching home.");
        m_homingSeen = false;
    }
}

// ---------------------------------------------------------------------------
// setTarget — :Sr# / :Sd# (bare '1' on success, '0' on a bad value)
// ---------------------------------------------------------------------------
bool OnStepXMount::setTarget(double ra, double dec, const char *op,
                             char *raStr, char *decStr, size_t len)
{
    // RA as HH:MM:SS.SSSS, Dec as sDD:MM:SS.SSS (zero-padded, explicit sign).
    // Do not use fs_sexa() here: its space padding is rejected by the firmware.
    if (!OnStepXFormat::formatRA(ra, raStr, len) || !OnStepXFormat::formatDec(dec, decStr, len))
    {
        LOGF_ERROR("%s: invalid target RA %f Dec %f", op, ra, dec);
        return false;
    }

    char cmd[OnStepXComm::CMD_MAX_LEN];

    snprintf(cmd, sizeof(cmd), ":Sr%s#", raStr);
    if (!m_core.comm().sendBool(cmd))
    {
        LOGF_ERROR("%s: firmware rejected RA '%s'", op, raStr);
        return false;
    }

    snprintf(cmd, sizeof(cmd), ":Sd%s#", decStr);
    if (!m_core.comm().sendBool(cmd))
    {
        LOGF_ERROR("%s: firmware rejected Dec '%s'", op, decStr);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Goto — :MS# replies a single unframed digit, '0' = goto started
// ---------------------------------------------------------------------------
bool OnStepXMount::Goto(double ra, double dec)
{
    char raStr[32], decStr[32];
    if (!setTarget(ra, dec, "Goto", raStr, decStr, sizeof(raStr)))
        return false;

    char ms = '\0';
    if (!m_core.comm().sendChar(":MS#", ms))
    {
        LOG_ERROR("Goto: no reply to :MS#");
        return false;
    }

    if (ms != '0')
    {
        LOGF_ERROR("Goto rejected by firmware: %s (code '%c')", OnStepXStatus::gotoErrorText(ms), ms);
        return false;
    }

    TrackState = SCOPE_SLEWING;
    LOGF_INFO("Slewing to RA: %s  Dec: %s", raStr, decStr);
    return true;
}

// ---------------------------------------------------------------------------
// Flip — ON_COORD_SET FLIP.  :MN# re-slews to the current position on the
// other side of the pier, whatever the pier preference.
// ---------------------------------------------------------------------------
bool OnStepXMount::Flip(double ra, double dec)
{
    if (!m_meridian.flip(ra, dec, EqNP[AXIS_RA].getValue(), EqNP[AXIS_DE].getValue()))
        return false;
    TrackState = SCOPE_SLEWING;
    return true;
}

// ---------------------------------------------------------------------------
// Sync
// ---------------------------------------------------------------------------
bool OnStepXMount::Sync(double ra, double dec)
{
    switch (m_modelBuilder.captureSync(ra, dec, m_status.pierSide, m_status.mountType))
    {
        case OnStepXModelBuilder::SyncCapture::CAPTURED:
            return true;
        case OnStepXModelBuilder::SyncCapture::REJECTED:
            return false;   // the client sees the Sync fail
        case OnStepXModelBuilder::SyncCapture::NOT_BUILDING:
            break;
    }

    char raStr[32], decStr[32];
    if (!setTarget(ra, dec, "Sync", raStr, decStr, sizeof(raStr)))
        return false;

    // :CM# rather than :CS#: :CS# replies with nothing (sync failures are
    // silent), whereas :CM# replies "N/A#" on success or "En#" where n is
    // the :MS# error code.  Both perform the same sync in firmware.
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    if (!m_core.comm().sendText(":CM#", reply))
    {
        LOG_ERROR("Sync: no reply to :CM#");
        return false;
    }
    if (reply[0] == 'E')
    {
        LOGF_ERROR("Sync rejected by firmware: %s (reply '%s')",
                   OnStepXStatus::gotoErrorText(reply[1]), reply);
        return false;
    }

    LOGF_INFO("Sync to RA: %s  Dec: %s", raStr, decStr);
    return true;
}

// ---------------------------------------------------------------------------
// Abort — :Q# stops gotos, guiding and manual moves; replies nothing
// ---------------------------------------------------------------------------
bool OnStepXMount::Abort()
{
    // Urgent: skip any resync drain still pending from a controller stall.
    if (!m_core.comm().sendUrgent(":Q#"))
    {
        LOG_ERROR("Abort failed");
        return false;
    }
    // TrackState is set from firmware status on the next poll.
    return true;
}

// ---------------------------------------------------------------------------
// Park / UnPark / SetCurrentPark — all reply bare '1'/'0'.
// INDI park state is mirrored from firmware status in updateTrackingState().
// ---------------------------------------------------------------------------
bool OnStepXMount::Park()
{
    if (!m_core.comm().sendBool(":hP#"))
    {
        LOG_ERROR("Park rejected by firmware");
        return false;
    }
    TrackState = SCOPE_PARKING;
    return true;
}

bool OnStepXMount::UnPark()
{
    if (!m_core.comm().sendBool(":hR#"))
    {
        LOG_ERROR("Unpark rejected by firmware");
        return false;
    }
    SetParked(false);
    reapplyCustomTrackRate();
    return true;
}

bool OnStepXMount::SetCurrentPark()
{
    if (!m_core.comm().sendBool(":hQ#"))
    {
        LOG_ERROR("Set park position rejected by firmware");
        return false;
    }
    return true;
}

bool OnStepXMount::SetDefaultPark()
{
    // OnStepX has no default park position, and treating Default as Current
    // would overwrite the saved one.
    LOG_ERROR("OnStepX has no default park position. Slew to the park position and choose Current.");
    return false;
}

// ---------------------------------------------------------------------------
// Tracking
// ---------------------------------------------------------------------------
bool OnStepXMount::SetTrackEnabled(bool enabled)
{
    // Bare '1'/'0'; :Te# fails while parked.
    if (!m_core.comm().sendBool(enabled ? ":Te#" : ":Td#"))
    {
        LOGF_ERROR("Tracking %s rejected by firmware", enabled ? "on" : "off");
        return false;
    }
    if (enabled)
        reapplyCustomTrackRate();
    return true;
}

bool OnStepXMount::SetTrackMode(uint8_t mode)
{
    // Rate selections reply nothing.
    const char *cmd = nullptr;
    switch (mode)
    {
        case OSX_TRACK_SIDEREAL:
        case OSX_TRACK_CUSTOM:   // custom = sidereal base + RA/Dec offsets
            // :TQ# selects the sidereal rate.  NOT :T+#, which speeds up the
            // master sidereal clock by 0.02 Hz and persists it to EEPROM.
            cmd = ":TQ#";
            break;
        case OSX_TRACK_LUNAR:
            cmd = ":TL#";
            break;
        case OSX_TRACK_SOLAR:
            cmd = ":TS#";
            break;
        case OSX_TRACK_KING:
            cmd = ":TK#";
            break;
        default:
            LOGF_ERROR("SetTrackMode: unknown mode %d", mode);
            return false;
    }
    if (!m_core.comm().sendNoReply(cmd))
    {
        LOGF_ERROR("SetTrackMode(%d) command failed", mode);
        return false;
    }

    // Offsets persist in firmware until changed, so clear them when leaving
    // custom mode and (re)apply TELESCOPE_TRACK_RATE when entering it.
    if (mode == OSX_TRACK_CUSTOM)
        return applyCustomTrackRate(TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue());
    return applyCustomTrackRate(SIDEREAL_ARCSEC_PER_SEC, 0.0);
}

// ---------------------------------------------------------------------------
// Track mode mirrors the firmware, which may have been set by the hand
// controller, the web UI or an earlier session: INDI must not show Sidereal
// while RA/Dec rate offsets (RAM, kept until reboot) make the mount drift.
// ---------------------------------------------------------------------------
// Base rate -> OSX_TRACK_* index.  The status reports the base rate only
// while tracking compensation is off (Status.command.cpp), so it is used
// only then; :GT# gives it in Hz while tracking.
int OnStepXMount::trackModeFor(OnStepXTracking::BaseRate rate)
{
    switch (rate)
    {
        case OnStepXTracking::BaseRate::LUNAR:
            return OSX_TRACK_LUNAR;
        case OnStepXTracking::BaseRate::SOLAR:
            return OSX_TRACK_SOLAR;
        case OnStepXTracking::BaseRate::KING:
            return OSX_TRACK_KING;
        case OnStepXTracking::BaseRate::SIDEREAL:
            return OSX_TRACK_SIDEREAL;
        default:
            return -1;
    }
}

void OnStepXMount::showTrackMode(int mode)
{
    if (TrackModeSP.findOnSwitchIndex() == mode)
        return;
    TrackModeSP.reset();
    TrackModeSP[mode].setState(ISS_ON);
    TrackModeSP.setState(IPS_OK);
    TrackModeSP.apply();
}

// At connect only: custom if the firmware has rate offsets, else its base
// rate where the firmware reports it.  Not per poll: with compensation on the
// status does not report the base rate, so a per-poll mirror would undo the
// client's selection.
void OnStepXMount::syncTrackModeFromFirmware()
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    double raOff = 0, deOff = 0;
    const bool haveOffsets = m_core.comm().sendText(":GXTR#", reply) && OnStepXFormat::parseNumber(reply, raOff) &&
                             m_core.comm().sendText(":GXTD#", reply) && OnStepXFormat::parseNumber(reply, deOff);
    if (haveOffsets && (std::fabs(raOff) > 1e-4 || std::fabs(deOff) > 1e-4))
    {
        TrackRateNP[AXIS_RA].setValue(OnStepXTracking::raRateFromOffset(raOff));
        TrackRateNP[AXIS_DE].setValue(OnStepXTracking::deRateFromOffset(deOff));
        TrackRateNP.setState(IPS_OK);
        TrackRateNP.apply();
        showTrackMode(OSX_TRACK_CUSTOM);
        LOGF_INFO("The controller has custom tracking rates set (RA %.4f, Dec %.4f arcsec/s); track mode shown as Custom.",
                  TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue());
        return;
    }
    double hz = 0;
    int mode = -1;
    if (m_core.comm().sendText(":GT#", reply, 2000, true) && OnStepXFormat::parseNumber(reply, hz))
        mode = trackModeFor(OnStepXTracking::baseRateFromHz(hz));          // tracking
    else if (refreshMountStatus() && m_status.trackComp == MountStatus::TrackComp::NONE)
        mode = trackModeFor(OnStepXTracking::baseRateFromStatus(m_status.trackRate));
    if (mode >= 0)
        showTrackMode(mode);
}

// ---------------------------------------------------------------------------
// SetTrackRate — INDI calls this for TELESCOPE_TRACK_RATE edits while
// tracking in TRACK_CUSTOM mode.  Rates are absolute, arcsec per SI second
// (RA sidereal = 15.041067).
// ---------------------------------------------------------------------------
bool OnStepXMount::SetTrackRate(double raRate, double deRate)
{
    return applyCustomTrackRate(raRate, deRate);
}

// The firmware clears the RA/Dec rate offsets in Home::reset() (homing,
// Set Home, alignment start), so re-send them while TRACK_CUSTOM is selected.
void OnStepXMount::reapplyCustomTrackRate()
{
    if (TrackModeSP.findOnSwitchIndex() == OSX_TRACK_CUSTOM)
        applyCustomTrackRate(TrackRateNP[AXIS_RA].getValue(), TrackRateNP[AXIS_DE].getValue());
}

// OnStepX models custom rates as offsets from the selected base rate:
//   :SXTR,n#  RA offset,  arcsec per sidereal second; +ve = target RA
//             increasing (mount tracks slower than sidereal)
//   :SXTD,n#  Dec offset, arcsec per sidereal second; +ve = northward
// Both reply bare '1'/'0' and are clamped by the firmware to +/-1800.
bool OnStepXMount::applyCustomTrackRate(double raRate, double deRate)
{
    const double raOffset = OnStepXTracking::raOffsetFromRate(raRate);
    const double deOffset = OnStepXTracking::deOffsetFromRate(deRate);

    char cmd[OnStepXComm::CMD_MAX_LEN];

    snprintf(cmd, sizeof(cmd), ":SXTR,%.5f#", raOffset);
    if (!m_core.comm().sendBool(cmd))
    {
        LOGF_ERROR("Setting RA tracking rate offset failed ('%s')", cmd);
        return false;
    }

    snprintf(cmd, sizeof(cmd), ":SXTD,%.5f#", deOffset);
    if (!m_core.comm().sendBool(cmd))
    {
        LOGF_ERROR("Setting Dec tracking rate offset failed ('%s')", cmd);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Slew rate (manual-move rate) — :R3# :RC# :RM# :RS# reply nothing.
// INDI's four labels are Guide / Centering / Find / Max; the firmware rates
// are 2x / 8x / 20x / half the goto rate.  Guide is :R3# (2x), not :RG# (1x):
// any rate of 1x or less also becomes the pulse-guide rate and is written to
// NV (GUIDE_SEPARATE_PULSE_RATE, Guide.command.cpp), which would silently
// replace the GUIDE_RATE the client set.
// ---------------------------------------------------------------------------
bool OnStepXMount::SetSlewRate(int index)
{
    const char *cmds[] = { ":R3#", ":RC#", ":RM#", ":RS#" };
    if (index < 0 || index > 3)
    {
        LOGF_ERROR("SetSlewRate: invalid index %d", index);
        return false;
    }
    if (!m_core.comm().sendNoReply(cmds[index]))
    {
        LOGF_ERROR("SetSlewRate(%d) command failed", index);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Manual motion (arrow buttons).  :Mn/:Ms/:Me/:Mw and the per-axis stops
// :Qn/:Qs (axis 2) and :Qe/:Qw (axis 1) reply nothing.  Stopping one axis
// must not use :Q#, which also aborts gotos and the other axis.
// ---------------------------------------------------------------------------
bool OnStepXMount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    const bool north = (dir == DIRECTION_NORTH);
    const char *cmd = (command == MOTION_START) ? (north ? ":Mn#" : ":Ms#")
                      : (north ? ":Qn#" : ":Qs#");
    if (!m_core.comm().sendNoReply(cmd))
    {
        LOGF_ERROR("MoveNS %s failed", command == MOTION_START ? "start" : "stop");
        return false;
    }
    return true;
}

bool OnStepXMount::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    const bool west = (dir == DIRECTION_WEST);
    const char *cmd = (command == MOTION_START) ? (west ? ":Mw#" : ":Me#")
                      : (west ? ":Qw#" : ":Qe#");
    if (!m_core.comm().sendNoReply(cmd))
    {
        LOGF_ERROR("MoveWE %s failed", command == MOTION_START ? "start" : "stop");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Location
// ---------------------------------------------------------------------------
bool OnStepXMount::updateLocation(double latitude, double longitude, double elevation)
{
    return m_site.writeLocation(latitude, longitude, elevation);
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------
bool OnStepXMount::updateTime(ln_date *utc, double utc_offset)
{
    return m_site.writeTime(utc, utc_offset);
}

// ---------------------------------------------------------------------------
// updateWeatherState — called from ReadScopeStatus throttle (every ~30 polls)
// ---------------------------------------------------------------------------
void OnStepXMount::updateWeatherDirect()
{
    // checkWeatherUpdate() calls updateWeather() once and publishes the
    // parameter and safety states.
    if (m_core.caps().hasWeatherRead)
        WI::checkWeatherUpdate();
}

// ---------------------------------------------------------------------------
// updateWeather — WeatherInterface callback; queries sensors from firmware
// ---------------------------------------------------------------------------
IPState OnStepXMount::updateWeather()
{
    SensorData data = m_weather.readSensors();
    if (data.temp.ok)      setParameterValue("WEATHER_TEMPERATURE", data.temp.value);
    if (data.pressure.ok)  setParameterValue("WEATHER_PRESSURE",    data.pressure.value);
    if (data.humidity.ok)  setParameterValue("WEATHER_HUMIDITY",    data.humidity.value);
    if (data.dewpoint.ok)  setParameterValue("WEATHER_DEWPOINT",    data.dewpoint.value);
    return data.anyOk() ? IPS_OK : IPS_ALERT;
}

// ---------------------------------------------------------------------------
// Guide pulse methods — delegate to OnStepXGuide
// ---------------------------------------------------------------------------
IPState OnStepXMount::GuideNorth(uint32_t ms)
{
    return m_guide.guideNorth(ms);
}
IPState OnStepXMount::GuideSouth(uint32_t ms)
{
    return m_guide.guideSouth(ms);
}
IPState OnStepXMount::GuideEast(uint32_t ms)
{
    return m_guide.guideEast(ms);
}
IPState OnStepXMount::GuideWest(uint32_t ms)
{
    return m_guide.guideWest(ms);
}

// ---------------------------------------------------------------------------
// Focuser child devices
// ---------------------------------------------------------------------------
void OnStepXMount::createFocusers()
{
    OnStepXFocuser::connectChildren(m_focusers, m_core.caps().focuserMask, &m_core.comm(), getDeviceName());
}

void OnStepXMount::disconnectFocusers()
{
    OnStepXFocuser::disconnectChildren(m_focusers);
}

// ---------------------------------------------------------------------------
// updatePecStatus — poll :$QZ?# state (~10 poll throttle)
// ---------------------------------------------------------------------------
void OnStepXMount::updatePecStatus()
{
    if (m_core.caps().hasPec)
        m_pec.pollStatus();
}

// ---------------------------------------------------------------------------
// updateAlignmentStatus — refresh :A?# status (~30 poll throttle)
// ---------------------------------------------------------------------------
void OnStepXMount::updateAlignmentStatus()
{
    m_alignment.updateStatus();
}

// ---------------------------------------------------------------------------
// updatePowerStates — poll the power child's outputs (~5 poll throttle)
// ---------------------------------------------------------------------------
void OnStepXMount::updatePowerStates()
{
    if (m_powerDev)
        m_powerDev->pollStatus();
}

// ---------------------------------------------------------------------------
// updateRotatorState — poll rotator angle and motion status (~10 s throttle)
// ---------------------------------------------------------------------------
void OnStepXMount::updateRotatorState()
{
    if (m_rotatorDev)
        m_rotatorDev->pollStatus();
}

// ---------------------------------------------------------------------------
// updateFocuserStates — poll each active focuser (position + temperature)
// ---------------------------------------------------------------------------
void OnStepXMount::updateFocuserStates()
{
    for (auto &f : m_focusers)
    {
        if (f)
            f->pollStatus();
    }
}

// ---------------------------------------------------------------------------
// ISNew* — forward unhandled events to base class
// ---------------------------------------------------------------------------
bool OnStepXMount::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (WI::processSwitch(dev, name, states, names, n))
        return true;
    if (isConnected() && m_info.handleSwitch(name, states, names, n))
        return true;
    if (isConnected() && m_site.handleSwitch(name, states, names, n))
    {
        // Site switch — re-read location and push to GEOGRAPHIC_COORD
        if (m_site.locationUpdated())
        {
            LocationNP[LOCATION_LATITUDE].setValue(m_site.lastLat());
            LocationNP[LOCATION_LONGITUDE].setValue(m_site.lastLon());
            LocationNP[LOCATION_ELEVATION].setValue(m_site.lastElev());
            LocationNP.setState(IPS_OK);
            LocationNP.apply();
            updateObserverLocation(m_site.lastLat(), m_site.lastLon(), m_site.lastElev());
            m_site.clearLocationUpdated();
        }
        return true;
    }
    if (isConnected() && m_limits.handleSwitch(name, states, names, n))
        return true;
    if (isConnected() && m_tracking.handleSwitch(name, states, names, n))
        return true;
    if (isConnected() && m_meridian.handleSwitch(name, states, names, n))
        return true;
    if (isConnected() && m_core.caps().hasPec && m_pec.handleSwitch(name, states, names, n))
        return true;
    if (isConnected() && m_alignment.handleSwitch(name, states, names, n))
        return true;
    if (isConnected() && m_modelBuilder.handleSwitch(name, states, names, n))
        return true;
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool OnStepXMount::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (GI::processNumber(dev, name, values, names, n))
        return true;
    if (WI::processNumber(dev, name, values, names, n))
        return true;
    if (isConnected() && m_guide.handleNumber(name, values, names, n))
    {
        // :R[n]# also changes the manual-move rate; restore the selected one.
        const int slew = SlewRateSP.findOnSwitchIndex();
        if (slew >= 0)
            SetSlewRate(slew);
        return true;
    }
    if (isConnected() && m_tracking.handleNumber(name, values, names, n))
        return true;
    if (isConnected() && m_weather.handleNumber(name, values, names, n))
        return true;
    if (isConnected() && m_limits.handleNumber(name, values, names, n))
        return true;
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool OnStepXMount::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (isConnected() && m_site.handleText(name, texts, names, n))
        return true;
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool OnStepXMount::saveConfigItems(FILE *fp)
{
    // INDI::Telescope's list, without the entry that a config load (Ekos
    // sends one on connect) must not replay:
    //   GEOGRAPHIC_COORD     the firmware's site, in NV (:St/:Sg); read at connect
    // Track mode, track rate and slew rate stay: RAM only in the firmware
    // (SetSlewRate() avoids the rates that also set the NV pulse-guide rate).
    OnStepXConfig::saveExcept(fp, { "GEOGRAPHIC_COORD" }, [this](FILE * f)
    {
        return INDI::Telescope::saveConfigItems(f);
    });
    WI::saveConfigItems(fp);
    m_alignment.saveConfig(fp);
    m_modelBuilder.saveConfig(fp);
    m_limits.saveConfig(fp);
    m_site.saveConfig(fp);
    m_guide.saveConfig(fp);
    m_info.saveConfig(fp);
    m_weather.saveConfig(fp);
    m_pec.saveConfig(fp);
    m_tracking.saveConfig(fp);
    m_meridian.saveConfig(fp);
    return true;
}

// ---------------------------------------------------------------------------
// ExecuteHomeAction
// ---------------------------------------------------------------------------
IPState OnStepXMount::ExecuteHomeAction(TelescopeHomeAction action)
{
    switch (action)
    {
        case HOME_FIND:
            return m_limits.homeFind() ? IPS_BUSY : IPS_ALERT;
        case HOME_SET:
            return m_limits.homeSet() ? IPS_OK : IPS_ALERT;
        default:
            return IPS_ALERT;
    }
}

