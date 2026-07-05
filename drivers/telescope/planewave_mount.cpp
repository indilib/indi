/*******************************************************************************
 Copyright(c) 2023-2027 Jasem Mutlaq. All rights reserved.

 Planewave Mount
 API Communication via PWI4 HTTP Interface

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

#include "planewave_mount.h"

#include "indicom.h"
#include "connectionplugins/connectiontcp.h"
#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#include <iomanip>

std::unique_ptr<PlaneWave> PlaneWave_mount(new PlaneWave());

/////////////////////////////////////////////////////////////////////////////
/// Constructor / Destructor
/////////////////////////////////////////////////////////////////////////////
PlaneWave::PlaneWave()
{
    setVersion(0, 2);

    SetTelescopeCapability(TELESCOPE_CAN_PARK    |
                           TELESCOPE_CAN_SYNC    |
                           TELESCOPE_CAN_GOTO    |
                           TELESCOPE_CAN_ABORT   |
                           TELESCOPE_HAS_TIME    |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_PIER_SIDE |
                           TELESCOPE_HAS_TRACK_MODE |
                           TELESCOPE_HAS_TRACK_RATE,
                           4);  // 4 slew-rate buttons

    setTelescopeConnection(CONNECTION_TCP);
}

// Must be defined here in the .cpp so that unique_ptr<httplib::Client>
// is destroyed after the complete type is known.
PlaneWave::~PlaneWave() = default;

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *PlaneWave::getDefaultName()
{
    return "PlaneWave";
}

/////////////////////////////////////////////////////////////////////////////
/// initProperties
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::initProperties()
{
    INDI::Telescope::initProperties();

    // ── Track Modes ─────────────────────────────────────────────────────────
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR",    "Solar");
    AddTrackMode("TRACK_LUNAR",    "Lunar");
    AddTrackMode("TRACK_CUSTOM",   "Custom");

    TrackState = SCOPE_IDLE;
    SetParkDataType(PARK_AZ_ALT);

    tcpConnection->setDefaultHost("192.168.1.1");
    tcpConnection->setDefaultPort(8220);

    // ── Info tab ─────────────────────────────────────────────────────────────

    FirmwareTP[0].fill("VERSION", "PWI4 Version", "");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware",
                    INFO_TAB, IP_RO, 60, IPS_IDLE);

    MountGeometryTP[0].fill("GEOMETRY", "Mount Type", "");
    MountGeometryTP.fill(getDeviceName(), "MOUNT_GEOMETRY", "Geometry",
                         INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Az / Alt — derived from status on every poll
    AltAzNP[AZ_VALUE].fill("AZ",  "Azimuth (°)",  "%.4f",  0.0, 360.0, 0.0, 0.0);
    AltAzNP[ALT_VALUE].fill("ALT", "Altitude (°)", "%.4f", -90.0, 90.0, 0.0, 0.0);
    AltAzNP.fill(getDeviceName(), "MOUNT_ALTAZ", "Az/Alt",
                 MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Axis 0 (AZ / RA) servo telemetry
    Axis0TelemetryNP[TELEMETRY_RMS_ERROR].fill(
        "AXIS0_RMS",     "RMS Error (\")",       "%.3f", 0,    1000, 0, 0);
    Axis0TelemetryNP[TELEMETRY_DIST_TO_TARGET].fill(
        "AXIS0_DIST",    "Dist to Target (\")",  "%.3f", -1e6, 1e6,  0, 0);
    Axis0TelemetryNP[TELEMETRY_SERVO_ERROR].fill(
        "AXIS0_SERVO",   "Servo Error (\")",     "%.3f", -1e6, 1e6,  0, 0);
    Axis0TelemetryNP[TELEMETRY_MOTOR_CURRENT].fill(
        "AXIS0_CURRENT", "Motor Current (A)",    "%.3f", 0,    100,  0, 0);
    Axis0TelemetryNP.fill(getDeviceName(), "AXIS0_TELEMETRY", "Axis 0 (AZ/RA) Telemetry",
                          INFO_TAB, IP_RO, 0, IPS_IDLE);

    // Axis 1 (ALT / Dec) servo telemetry
    Axis1TelemetryNP[TELEMETRY_RMS_ERROR].fill(
        "AXIS1_RMS",     "RMS Error (\")",       "%.3f", 0,    1000, 0, 0);
    Axis1TelemetryNP[TELEMETRY_DIST_TO_TARGET].fill(
        "AXIS1_DIST",    "Dist to Target (\")",  "%.3f", -1e6, 1e6,  0, 0);
    Axis1TelemetryNP[TELEMETRY_SERVO_ERROR].fill(
        "AXIS1_SERVO",   "Servo Error (\")",     "%.3f", -1e6, 1e6,  0, 0);
    Axis1TelemetryNP[TELEMETRY_MOTOR_CURRENT].fill(
        "AXIS1_CURRENT", "Motor Current (A)",    "%.3f", 0,    100,  0, 0);
    Axis1TelemetryNP.fill(getDeviceName(), "AXIS1_TELEMETRY", "Axis 1 (ALT/Dec) Telemetry",
                          INFO_TAB, IP_RO, 0, IPS_IDLE);

    // Angular separation from current pointing to the Sun (safety indicator)
    DistanceToSunNP[0].fill("DIST_TO_SUN", "Distance (°)", "%.2f", 0, 180, 0, 0);
    DistanceToSunNP.fill(getDeviceName(), "DISTANCE_TO_SUN", "Sun Distance",
                         MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // ── Main Control tab ─────────────────────────────────────────────────────

    // Motor enable / disable — axis 0
    Axis0MotorSP[INDI_ENABLED].fill("AXIS0_ENABLE",  "Enable",  ISS_OFF);
    Axis0MotorSP[INDI_DISABLED].fill("AXIS0_DISABLE", "Disable", ISS_OFF);
    Axis0MotorSP.fill(getDeviceName(), "AXIS0_MOTOR", "Axis 0 (AZ/RA) Motor",
                      MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Motor enable / disable — axis 1
    Axis1MotorSP[INDI_ENABLED].fill("AXIS1_ENABLE",  "Enable",  ISS_OFF);
    Axis1MotorSP[INDI_DISABLED].fill("AXIS1_DISABLE", "Disable", ISS_OFF);
    Axis1MotorSP.fill(getDeviceName(), "AXIS1_MOTOR", "Axis 1 (ALT/Dec) Motor",
                      MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Find Home — relevant for L-series mounts with incremental encoders
    FindHomeSP[0].fill("FIND_HOME", "Find Home", ISS_OFF);
    FindHomeSP.fill(getDeviceName(), "MOUNT_HOME", "Homing",
                    MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // ── Pointing Model tab ────────────────────────────────────────────────────

    // Active model filename (read-only, pulled from status)
    ModelFilenameTP[0].fill("MODEL_FILE", "Active Model", "");
    ModelFilenameTP.fill(getDeviceName(), "MODEL_FILENAME", "Model File",
                         MODEL_TAB, IP_RO, 60, IPS_IDLE);

    // Model quality statistics (read-only, pulled from status)
    ModelInfoNP[MODEL_POINTS_TOTAL].fill("MODEL_TOTAL",   "Total Points",    "%.0f", 0, 10000, 1, 0);
    ModelInfoNP[MODEL_POINTS_ENABLED].fill("MODEL_ENABLED", "Enabled Points", "%.0f", 0, 10000, 1, 0);
    ModelInfoNP[MODEL_RMS_ERROR].fill("MODEL_RMS",       "RMS Error (\")",   "%.3f", 0, 10000, 0, 0);
    ModelInfoNP.fill(getDeviceName(), "MODEL_INFO", "Model Statistics",
                     MODEL_TAB, IP_RO, 0, IPS_IDLE);

    // One-click model operations
    ModelOperationSP[MODEL_SAVE_AS_DEFAULT].fill("MODEL_SAVE_DEFAULT", "Save as Default", ISS_OFF);
    ModelOperationSP[MODEL_CLEAR_POINTS].fill("MODEL_CLEAR",         "Clear All Points", ISS_OFF);
    ModelOperationSP.fill(getDeviceName(), "MODEL_OPERATION", "Model Operations",
                          MODEL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Load model: user types filename → Apply → driver sends /mount/model/load
    ModelLoadFilenameTP[0].fill("MODEL_LOAD_FILE", "Filename", "DefaultModel.pxp");
    ModelLoadFilenameTP.fill(getDeviceName(), "MODEL_LOAD", "Load Model",
                             MODEL_TAB, IP_RW, 60, IPS_IDLE);

    // Save model: user types filename → Apply → driver sends /mount/model/save
    ModelSaveFilenameTP[0].fill("MODEL_SAVE_FILE", "Filename", "MyModel.pxp");
    ModelSaveFilenameTP.fill(getDeviceName(), "MODEL_SAVE", "Save Model",
                             MODEL_TAB, IP_RW, 60, IPS_IDLE);

    // ── Options tab ───────────────────────────────────────────────────────────

    SlewTimeConstantNP[0].fill("SLEW_TIME_CONSTANT", "Time Constant (s)",
                               "%.2f", 0.1, 5.0, 0.1, 0.5);
    SlewTimeConstantNP.fill(getDeviceName(), "MOUNT_SLEW_CONSTANT", "Slew Aggressiveness",
                            OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    addAuxControls();
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// updateProperties
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        // Info tab
        defineProperty(FirmwareTP);
        defineProperty(MountGeometryTP);
        defineProperty(AltAzNP);
        defineProperty(Axis0TelemetryNP);
        defineProperty(Axis1TelemetryNP);
        defineProperty(DistanceToSunNP);

        // Main Control tab
        defineProperty(Axis0MotorSP);
        defineProperty(Axis1MotorSP);
        defineProperty(FindHomeSP);

        // Pointing Model tab
        defineProperty(ModelFilenameTP);
        defineProperty(ModelInfoNP);
        defineProperty(ModelOperationSP);
        defineProperty(ModelLoadFilenameTP);
        defineProperty(ModelSaveFilenameTP);

        // Options tab
        defineProperty(SlewTimeConstantNP);

        // Parking data
        if (InitPark())
        {
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(0);
        }
        else
        {
            SetAxis1Park(0);
            SetAxis2Park(0);
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(0);
        }
    }
    else
    {
        deleteProperty(FirmwareTP);
        deleteProperty(MountGeometryTP);
        deleteProperty(AltAzNP);
        deleteProperty(Axis0TelemetryNP);
        deleteProperty(Axis1TelemetryNP);
        deleteProperty(DistanceToSunNP);

        deleteProperty(Axis0MotorSP);
        deleteProperty(Axis1MotorSP);
        deleteProperty(FindHomeSP);

        deleteProperty(ModelFilenameTP);
        deleteProperty(ModelInfoNP);
        deleteProperty(ModelOperationSP);
        deleteProperty(ModelLoadFilenameTP);
        deleteProperty(ModelSaveFilenameTP);

        deleteProperty(SlewTimeConstantNP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Handshake — build the persistent client and verify the mount is live
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Handshake()
{
    // Build (or rebuild on reconnect) the persistent client
    m_HttpClient = std::make_unique<httplib::Client>(tcpConnection->host(),
                   tcpConnection->port());
    m_HttpClient->set_connection_timeout(5, 0);
    m_HttpClient->set_read_timeout(10, 0);

    // Fetch initial status
    if (!getStatus())
    {
        LOG_ERROR("Cannot communicate with PWI4. Is PWI4 running and reachable?");
        return false;
    }

    // If PWI4 is not connected to the mount hardware, try to connect it
    bool mountConnected = m_Status["mount.is_connected"].as<bool>();
    if (!mountConnected)
    {
        LOG_INFO("Mount not connected to PWI4. Sending /mount/connect …");
        if (!dispatch("/mount/connect"))
        {
            LOG_ERROR("Failed to connect mount via PWI4.");
            return false;
        }
        mountConnected = m_Status["mount.is_connected"].as<bool>();
        if (!mountConnected)
        {
            LOG_ERROR("Mount still not connected after /mount/connect.");
            return false;
        }
    }

    // ── Populate read-only info properties from the first status fetch ────────

    // PWI4 version
    std::string version = m_Status["pwi4.version"].as<std::string>();
    FirmwareTP[0].setText(version.c_str());
    FirmwareTP.setState(IPS_OK);

    // Mount geometry
    int geometry = m_Status["mount.geometry"].as<int>();
    const char *geomStr = "Unknown";
    switch (geometry)
    {
        case 0:
            geomStr = "Alt-Az";
            break;
        case 1:
            geomStr = "Equatorial Fork";
            break;
        case 2:
            geomStr = "German Equatorial";
            break;
    }
    MountGeometryTP[0].setText(geomStr);
    MountGeometryTP.setState(IPS_OK);

    // Slew time constant
    SlewTimeConstantNP[0].setValue(m_Status["mount.slew_time_constant"].as<double>());
    SlewTimeConstantNP.setState(IPS_OK);

    LOGF_INFO("Connected to PWI4 %s — mount geometry: %s", version.c_str(), geomStr);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// getStatus — thin wrapper around dispatch("/status")
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::getStatus()
{
    return dispatch("/status");
}

/////////////////////////////////////////////////////////////////////////////
/// ReadScopeStatus — called every polling interval
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::ReadScopeStatus()
{
    if (!getStatus())
        return false;

    // ── Coordinates ────────────────────────────────────────────────────────
    double ra = m_Status["mount.ra_apparent_hours"].as<double>();
    double de = m_Status["mount.dec_apparent_degs"].as<double>();

    // ── Flags ──────────────────────────────────────────────────────────────
    bool isSlewing  = m_Status["mount.is_slewing"].as<bool>();
    bool isTracking = m_Status["mount.is_tracking"].as<bool>();

    // ── State machine ──────────────────────────────────────────────────────
    switch (TrackState)
    {
        case SCOPE_PARKING:
            // Park is complete when the mount has stopped slewing AND is no
            // longer tracking (it will be in the "stopped" state).
            if (!isSlewing && !isTracking)
                SetParked(true);
            break;

        case SCOPE_SLEWING:
            // Slewing is complete once mount.is_slewing drops to false;
            // it then either begins tracking or goes idle.
            if (!isSlewing)
            {
                if (isTracking)
                {
                    TrackState = SCOPE_TRACKING;
                    SetTrackEnabled(true);
                }
                else
                {
                    TrackState = SCOPE_IDLE;
                }
            }
            break;

        default:
            // If the mount starts tracking externally (e.g. via PWI4 GUI),
            // reflect that in the INDI state.
            if (isTracking && TrackState == SCOPE_IDLE)
                TrackState = SCOPE_TRACKING;
            break;
    }

    NewRaDec(ra, de);

    // ── Az / Alt ───────────────────────────────────────────────────────────
    double az  = m_Status["mount.azimuth_degs"].as<double>();
    double alt = m_Status["mount.altitude_degs"].as<double>();
    AltAzNP[AZ_VALUE].setValue(az);
    AltAzNP[ALT_VALUE].setValue(alt);
    AltAzNP.setState(IPS_OK);
    AltAzNP.apply();

    // ── Pier side (GEM only) ───────────────────────────────────────────────
    // On a German Equatorial, the HA decides which side the scope is on.
    // PWI4 does not expose pier side directly; we infer it from axis0 position.
    int geometry = m_Status["mount.geometry"].as<int>();
    if (geometry == 2) // GEM
    {
        double axis0Pos = m_Status["mount.axis0.position_degs"].as<double>();
        // Conventional: axis0 > 180° → pointing West → scope on East (pier west)
        setPierSide((axis0Pos > 180.0) ? PIER_WEST : PIER_EAST);
    }

    // ── Axis 0 telemetry ───────────────────────────────────────────────────
    Axis0TelemetryNP[TELEMETRY_RMS_ERROR].setValue(
        m_Status["mount.axis0.rms_error_arcsec"].as<double>());
    Axis0TelemetryNP[TELEMETRY_DIST_TO_TARGET].setValue(
        m_Status["mount.axis0.dist_to_target_arcsec"].as<double>());
    Axis0TelemetryNP[TELEMETRY_SERVO_ERROR].setValue(
        m_Status["mount.axis0.servo_error_arcsec"].as<double>());
    Axis0TelemetryNP[TELEMETRY_MOTOR_CURRENT].setValue(
        m_Status["mount.axis0.measured_current_amps"].as<double>());
    Axis0TelemetryNP.setState(IPS_OK);
    Axis0TelemetryNP.apply();

    // ── Axis 1 telemetry ───────────────────────────────────────────────────
    Axis1TelemetryNP[TELEMETRY_RMS_ERROR].setValue(
        m_Status["mount.axis1.rms_error_arcsec"].as<double>());
    Axis1TelemetryNP[TELEMETRY_DIST_TO_TARGET].setValue(
        m_Status["mount.axis1.dist_to_target_arcsec"].as<double>());
    Axis1TelemetryNP[TELEMETRY_SERVO_ERROR].setValue(
        m_Status["mount.axis1.servo_error_arcsec"].as<double>());
    Axis1TelemetryNP[TELEMETRY_MOTOR_CURRENT].setValue(
        m_Status["mount.axis1.measured_current_amps"].as<double>());
    Axis1TelemetryNP.setState(IPS_OK);
    Axis1TelemetryNP.apply();

    // ── Motor enabled state ────────────────────────────────────────────────
    bool axis0Enabled = m_Status["mount.axis0.is_enabled"].as<bool>();
    bool axis1Enabled = m_Status["mount.axis1.is_enabled"].as<bool>();

    Axis0MotorSP[INDI_ENABLED].setState(axis0Enabled ? ISS_ON : ISS_OFF);
    Axis0MotorSP[INDI_DISABLED].setState(axis0Enabled ? ISS_OFF : ISS_ON);
    Axis0MotorSP.setState(IPS_OK);
    Axis0MotorSP.apply();

    Axis1MotorSP[INDI_ENABLED].setState(axis1Enabled ? ISS_ON : ISS_OFF);
    Axis1MotorSP[INDI_DISABLED].setState(axis1Enabled ? ISS_OFF : ISS_ON);
    Axis1MotorSP.setState(IPS_OK);
    Axis1MotorSP.apply();

    // ── Distance to Sun ────────────────────────────────────────────────────
    double sunDist = m_Status["mount.distance_to_sun_degs"].as<double>();
    DistanceToSunNP[0].setValue(sunDist);
    // Warn when pointing within 30° of the Sun
    DistanceToSunNP.setState(sunDist < 30.0 ? IPS_ALERT : IPS_OK);
    DistanceToSunNP.apply();

    // ── Pointing model info ────────────────────────────────────────────────
    std::string modelFile = m_Status["mount.model.filename"].as<std::string>();
    if (modelFile != std::string(ModelFilenameTP[0].getText()))
    {
        ModelFilenameTP[0].setText(modelFile.c_str());
        ModelFilenameTP.setState(modelFile.empty() ? IPS_IDLE : IPS_OK);
        ModelFilenameTP.apply();
    }

    ModelInfoNP[MODEL_POINTS_TOTAL].setValue(
        m_Status["mount.model.num_points_total"].as<double>());
    ModelInfoNP[MODEL_POINTS_ENABLED].setValue(
        m_Status["mount.model.num_points_enabled"].as<double>());
    ModelInfoNP[MODEL_RMS_ERROR].setValue(
        m_Status["mount.model.rms_error_arcsec"].as<double>());
    ModelInfoNP.setState(IPS_OK);
    ModelInfoNP.apply();

    // ── Site location readback ─────────────────────────────────────────────
    // PWI4 is the authority on site location; push its values into INDI so
    // they are always consistent with what the pointing model uses.
    double pwi4Lat = m_Status["site.latitude_degs"].as<double>();
    double pwi4Lon = m_Status["site.longitude_degs"].as<double>();
    double pwi4Elv = m_Status["site.height_meters"].as<double>();
    if (std::fabs(pwi4Lat - LocationNP[LOCATION_LATITUDE].getValue())  > 0.0001 ||
            std::fabs(pwi4Lon - LocationNP[LOCATION_LONGITUDE].getValue()) > 0.0001 ||
            std::fabs(pwi4Elv - LocationNP[LOCATION_ELEVATION].getValue()) > 1.0)
    {
        LocationNP[LOCATION_LATITUDE].setValue(pwi4Lat);
        LocationNP[LOCATION_LONGITUDE].setValue(pwi4Lon);
        LocationNP[LOCATION_ELEVATION].setValue(pwi4Elv);
        LocationNP.apply();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Goto — J2000 RA/Dec (INDI convention)
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Goto(double ra, double dec)
{
    std::ostringstream req;
    req << std::fixed << std::setprecision(9)
        << "/mount/goto_ra_dec_j2000?ra_hours=" << ra
        << "&dec_degs=" << dec;

    if (!dispatch(req.str()))
        return false;

    TrackState = SCOPE_SLEWING;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Sync — adds a pointing-model calibration point at the current position
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Sync(double ra, double dec)
{
    // PWI4 does not have a direct "sync" command.  The closest equivalent is
    // /mount/model/add_point, which maps the current encoder position to the
    // provided J2000 coordinates and immediately updates the pointing model.
    std::ostringstream req;
    req << std::fixed << std::setprecision(9)
        << "/mount/model/add_point?ra_j2000_hours=" << ra
        << "&dec_j2000_degs=" << dec;

    if (!dispatch(req.str()))
        return false;

    LOG_INFO("Sync: added a pointing-model calibration point at the current position.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Park
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Park()
{
    if (!dispatch("/mount/park"))
        return false;

    TrackState = SCOPE_PARKING;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// UnPark — re-enable both motors before clearing the parked state
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::UnPark()
{
    // Enable axis 0 (AZ / RA)
    if (!enableAxis(0, true))
    {
        LOG_ERROR("Failed to enable axis 0 motor.");
        return false;
    }
    // Enable axis 1 (ALT / Dec)
    if (!enableAxis(1, true))
    {
        LOG_ERROR("Failed to enable axis 1 motor.");
        return false;
    }

    SetParked(false);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SetCurrentPark — tell PWI4 to store the current position as the park point
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetCurrentPark()
{
    if (!dispatch("/mount/set_park_here"))
        return false;

    LOG_INFO("Park position set to current position.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SetDefaultPark
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetDefaultPark()
{
    SetAxis1Park(0);
    SetAxis2Park(0);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Abort
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::Abort()
{
    return dispatch("/mount/stop");
}

/////////////////////////////////////////////////////////////////////////////
/// MoveNS — jog in Dec using a continuous offset rate
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    double rate = getSlewRateArcsecPerSec();
    if (dir == DIRECTION_SOUTH)
        rate = -rate;  // South → negative Dec offset

    std::ostringstream req;
    req << std::fixed << std::setprecision(4);

    if (command == MOTION_START)
        req << "/mount/offset?dec_set_rate_arcsec_per_sec=" << rate;
    else
        req << "/mount/offset?dec_set_rate_arcsec_per_sec=0";

    return dispatch(req.str());
}

/////////////////////////////////////////////////////////////////////////////
/// MoveWE — jog in RA using a continuous offset rate
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (TrackState == SCOPE_PARKED)
    {
        LOG_ERROR("Please unpark the mount before issuing any motion commands.");
        return false;
    }

    double rate = getSlewRateArcsecPerSec();
    if (dir == DIRECTION_EAST)
        rate = -rate;  // East → negative RA offset (RA increases westward)

    std::ostringstream req;
    req << std::fixed << std::setprecision(4);

    if (command == MOTION_START)
        req << "/mount/offset?ra_set_rate_arcsec_per_sec=" << rate;
    else
        req << "/mount/offset?ra_set_rate_arcsec_per_sec=0";

    return dispatch(req.str());
}

/////////////////////////////////////////////////////////////////////////////
/// updateLocation — PWI4 is the authority; we just accept the INDI call
/// and note that the actual site location is configured inside PWI4.
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);

    // PWI4 manages its own site configuration.  The driver keeps INDI's
    // location in sync by reading back site.* values in ReadScopeStatus().
    LOG_INFO("Note: site location is managed by PWI4. "
             "INDI displays the location configured in PWI4.");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// updateTime — PWI4 uses the host PC clock; nothing to push.
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc);
    INDI_UNUSED(utc_offset);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// SetTrackMode
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetTrackMode(uint8_t mode)
{
    // First ensure tracking is on
    if (!dispatch("/mount/tracking_on"))
        return false;

    // For custom mode, apply the offsets that simulate non-sidereal rates.
    // The rates stored in TrackRateNP are in arcsec/sec relative to sidereal.
    if (mode == TRACK_CUSTOM)
    {
        double dRA = TrackRateNP[AXIS_RA].getValue();
        double dDE = TrackRateNP[AXIS_DE].getValue();

        std::ostringstream req;
        req << std::fixed << std::setprecision(6)
            << "/mount/offset?ra_set_rate_arcsec_per_sec=" << dRA
            << "&dec_set_rate_arcsec_per_sec=" << dDE;

        return dispatch(req.str());
    }

    // For Sidereal / Solar / Lunar the offset rate is zero; standard tracking
    // on already handles those.  Reset any previous custom offsets.
    return dispatch("/mount/offset?ra_reset=0&dec_reset=0");
}

/////////////////////////////////////////////////////////////////////////////
/// SetTrackRate — called when the user changes TrackRateNP
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetTrackRate(double raRate, double deRate)
{
    std::ostringstream req;
    req << std::fixed << std::setprecision(6)
        << "/mount/offset?ra_set_rate_arcsec_per_sec=" << raRate
        << "&dec_set_rate_arcsec_per_sec=" << deRate;

    return dispatch(req.str());
}

/////////////////////////////////////////////////////////////////////////////
/// SetTrackEnabled
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::SetTrackEnabled(bool enabled)
{
    if (enabled)
        return SetTrackMode(TrackModeSP.findOnSwitchIndex());

    // Disable tracking and clear any active offset rates
    if (!dispatch("/mount/tracking_off"))
        return false;

    dispatch("/mount/offset?ra_reset=0&dec_reset=0");
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// saveConfigItems
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);
    SlewTimeConstantNP.save(fp);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// ISNewSwitch
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // ── Axis 0 motor control ──────────────────────────────────────────
        if (Axis0MotorSP.isNameMatch(name))
        {
            Axis0MotorSP.update(states, names, n);
            bool enable = (Axis0MotorSP[INDI_ENABLED].getState() == ISS_ON);
            if (enableAxis(0, enable))
            {
                LOGF_INFO("Axis 0 motor %s.", enable ? "enabled" : "disabled");
                Axis0MotorSP.setState(IPS_OK);
            }
            else
            {
                LOG_ERROR("Failed to change axis 0 motor state.");
                Axis0MotorSP.setState(IPS_ALERT);
            }
            Axis0MotorSP.apply();
            return true;
        }

        // ── Axis 1 motor control ──────────────────────────────────────────
        if (Axis1MotorSP.isNameMatch(name))
        {
            Axis1MotorSP.update(states, names, n);
            bool enable = (Axis1MotorSP[INDI_ENABLED].getState() == ISS_ON);
            if (enableAxis(1, enable))
            {
                LOGF_INFO("Axis 1 motor %s.", enable ? "enabled" : "disabled");
                Axis1MotorSP.setState(IPS_OK);
            }
            else
            {
                LOG_ERROR("Failed to change axis 1 motor state.");
                Axis1MotorSP.setState(IPS_ALERT);
            }
            Axis1MotorSP.apply();
            return true;
        }

        // ── Find Home ──────────────────────────────────────────────────────
        if (FindHomeSP.isNameMatch(name))
        {
            if (dispatch("/mount/find_home"))
            {
                LOG_INFO("Home finding sequence started.");
                FindHomeSP.setState(IPS_BUSY);
            }
            else
            {
                LOG_ERROR("Failed to start home finding.");
                FindHomeSP.setState(IPS_ALERT);
            }
            // Reset the button to Off (momentary action)
            FindHomeSP[0].setState(ISS_OFF);
            FindHomeSP.apply();
            return true;
        }

        // ── Pointing model operations ──────────────────────────────────────
        if (ModelOperationSP.isNameMatch(name))
        {
            ModelOperationSP.update(states, names, n);
            bool ok = false;

            if (ModelOperationSP[MODEL_SAVE_AS_DEFAULT].getState() == ISS_ON)
            {
                ok = dispatch("/mount/model/save_as_default");
                if (ok)
                    LOG_INFO("Pointing model saved as default.");
                else
                    LOG_ERROR("Failed to save pointing model as default.");
            }
            else if (ModelOperationSP[MODEL_CLEAR_POINTS].getState() == ISS_ON)
            {
                ok = dispatch("/mount/model/clear_points");
                if (ok)
                    LOG_INFO("All pointing model calibration points cleared.");
                else
                    LOG_ERROR("Failed to clear pointing model points.");
            }

            // Reset buttons (momentary)
            ModelOperationSP[MODEL_SAVE_AS_DEFAULT].setState(ISS_OFF);
            ModelOperationSP[MODEL_CLEAR_POINTS].setState(ISS_OFF);
            ModelOperationSP.setState(ok ? IPS_OK : IPS_ALERT);
            ModelOperationSP.apply();
            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// ISNewText
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // ── Load model by filename ─────────────────────────────────────────
        if (ModelLoadFilenameTP.isNameMatch(name))
        {
            ModelLoadFilenameTP.update(texts, names, n);
            const char *filename = ModelLoadFilenameTP[0].getText();
            std::string req = "/mount/model/load?filename=";
            req += filename;

            if (dispatch(req))
            {
                LOGF_INFO("Pointing model '%s' loaded.", filename);
                ModelLoadFilenameTP.setState(IPS_OK);
            }
            else
            {
                LOGF_ERROR("Failed to load pointing model '%s'.", filename);
                ModelLoadFilenameTP.setState(IPS_ALERT);
            }
            ModelLoadFilenameTP.apply();
            return true;
        }

        // ── Save model with filename ───────────────────────────────────────
        if (ModelSaveFilenameTP.isNameMatch(name))
        {
            ModelSaveFilenameTP.update(texts, names, n);
            const char *filename = ModelSaveFilenameTP[0].getText();
            std::string req = "/mount/model/save?filename=";
            req += filename;

            if (dispatch(req))
            {
                LOGF_INFO("Pointing model saved as '%s'.", filename);
                ModelSaveFilenameTP.setState(IPS_OK);
            }
            else
            {
                LOGF_ERROR("Failed to save pointing model as '%s'.", filename);
                ModelSaveFilenameTP.setState(IPS_ALERT);
            }
            ModelSaveFilenameTP.apply();
            return true;
        }
    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// ISNewNumber
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // ── Slew time constant ─────────────────────────────────────────────
        if (SlewTimeConstantNP.isNameMatch(name))
        {
            SlewTimeConstantNP.update(values, names, n);
            double tc = SlewTimeConstantNP[0].getValue();

            std::ostringstream req;
            req << std::fixed << std::setprecision(3)
                << "/mount/set_slew_time_constant?value=" << tc;

            if (dispatch(req.str()))
            {
                LOGF_INFO("Slew time constant set to %.2f s.", tc);
                SlewTimeConstantNP.setState(IPS_OK);
            }
            else
            {
                LOG_ERROR("Failed to set slew time constant.");
                SlewTimeConstantNP.setState(IPS_ALERT);
            }
            SlewTimeConstantNP.apply();
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
/// enableAxis — helper to send /mount/enable or /mount/disable
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::enableAxis(int axis, bool enable)
{
    std::ostringstream req;
    req << (enable ? "/mount/enable?axis=" : "/mount/disable?axis=") << axis;
    return dispatch(req.str());
}

/////////////////////////////////////////////////////////////////////////////
/// getSlewRateArcsecPerSec — map the selected slew-rate button to arcsec/s
/////////////////////////////////////////////////////////////////////////////
double PlaneWave::getSlewRateArcsecPerSec() const
{
    int idx = SlewRateSP.findOnSwitchIndex();
    if (idx >= 0 && idx < 4)
        return SLEW_RATES[idx];
    return SLEW_RATES[3]; // default to fast if unknown
}

/////////////////////////////////////////////////////////////////////////////
/// dispatch — the single HTTP entry-point
///
/// Sends a GET request via the persistent client, checks the HTTP status
/// code, parses the key=value body into m_Status, and returns true on
/// success (HTTP 200 with a non-empty body).
/////////////////////////////////////////////////////////////////////////////
bool PlaneWave::dispatch(const std::string &request)
{
    if (!m_HttpClient)
    {
        LOG_ERROR("HTTP client not initialized. Is the mount connected?");
        return false;
    }

    auto res = m_HttpClient->Get(request);

    if (!res)
    {
        LOGF_ERROR("HTTP GET %s failed: %s",
                   request.c_str(),
                   httplib::to_string(res.error()).c_str());
        return false;
    }

    if (res->status != 200)
    {
        LOGF_ERROR("HTTP GET %s returned HTTP %d: %s",
                   request.c_str(), res->status,
                   res->body.empty() ? "(no body)" : res->body.c_str());
        return false;
    }

    if (res->body.empty())
    {
        LOGF_ERROR("HTTP GET %s returned an empty body.", request.c_str());
        return false;
    }

    try
    {
        // PWI4 returns a flat key=value format.  Wrap it in a dummy [status]
        // section so that inicpp can parse it as a standard INI file.
        ini::IniFile inif;
        inif.decode("[status]\n" + res->body);

        if (inif["status"].size() == 0)
        {
            LOGF_ERROR("Parsed status for %s is empty.", request.c_str());
            return false;
        }

        m_Status = inif["status"];
        return true;
    }
    catch (const std::exception &e)
    {
        LOGF_ERROR("Failed to parse PWI4 response for %s: %s",
                   request.c_str(), e.what());
        return false;
    }
}
