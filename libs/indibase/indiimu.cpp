/*
    IMU Driver Base Class
    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indiimu.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectioni2c.h"
#include "indicom.h"
#include "alignment/TelescopeDirectionVectorSupportFunctions.h"
#include "alignment/AlignmentSubsystemForDrivers.h"
#include <cmath>
#include <memory>

namespace INDI
{

IMU::IMU() : IMUInterface(this)
{
    // Constructor only initializes the interface base class.
    // Properties will be initialized in initProperties().
    serialConnection = nullptr;
    i2cConnection    = nullptr;
}

IMU::~IMU()
{
    delete serialConnection;
    delete i2cConnection;
}

void IMU::setSupportedConnections(const uint8_t &value)
{
    uint8_t mask = CONNECTION_SERIAL | CONNECTION_I2C | CONNECTION_NONE;

    if (value == 0 || (mask & value) == 0)
    {
        DEBUGF(Logger::DBG_ERROR, "Invalid connection mode %d", value);
        return;
    }

    imuConnection = value;
}

bool IMU::initProperties()
{
    DefaultDevice::initProperties();
    IMUInterface::initProperties(IMU_TAB);

    // Initialize driver-specific properties

    AstroCoordinatesNP[AXIS1].fill("AXIS1", "Axis 1 (deg)", "%.2f", 0, 360, 0, 0);
    AstroCoordinatesNP[AXIS2].fill("AXIS2", "Axis 2 (deg)", "%.2f", 0, 360, 0, 0);
    AstroCoordinatesNP.fill(getDeviceName(), "COORDINATES", "Coordinates", COORDINATES_TAB.c_str(), IP_RO,
                            0, IPS_IDLE);

    AstroCoordsTypeSP[COORD_EQUATORIAL].fill("EQUATORIAL", "Equatorial (HA/DEC)", ISS_ON);
    AstroCoordsTypeSP[COORD_ALTAZ].fill("ALTAZ", "Alt-Az (AZ/ALT)", ISS_OFF);
    AstroCoordsTypeSP.fill(getDeviceName(), "COORDS_TYPE", "Coordinate Type", COORDINATES_TAB.c_str(), IP_RW,
                           ISR_1OFMANY, 0, IPS_IDLE);
    AstroCoordsTypeSP.load();

    IMUFrameSP[ENU].fill("ENU", "East-North-Up", ISS_ON);
    IMUFrameSP[NWU].fill("NWU", "North-West-Up", ISS_OFF);
    IMUFrameSP[SWU].fill("SWU", "South-West-Up", ISS_OFF);
    IMUFrameSP.fill(getDeviceName(), "IMU_FRAME", "IMU Frame", IMU_TAB.c_str(), IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IMUFrameSP.load();

    OrientationAdjustmentsNP[ROLL_MULTIPLIER].fill("ROLL_M", "Roll Multiplier", "%.2f", -1.0, 1.0, 0.1, 1.0);
    OrientationAdjustmentsNP[PITCH_MULTIPLIER].fill("PITCH_M", "Pitch Multiplier", "%.2f", -1.0, 1.0, 0.1, 1.0);
    OrientationAdjustmentsNP[YAW_MULTIPLIER].fill("YAW_M", "Yaw Multiplier", "%.2f", -1.0, 1.0, 0.1, 1.0);
    OrientationAdjustmentsNP[ROLL_OFFSET].fill("ROLL_O", "Roll Offset (deg)", "%.2f", -360.0, 360.0, 10, 0.0);
    OrientationAdjustmentsNP[PITCH_OFFSET].fill("PITCH_O", "Pitch Offset (deg)", "%.2f", -360.0, 360.0, 10, 0.0);
    OrientationAdjustmentsNP[YAW_OFFSET].fill("YAW_O", "Yaw Offset (deg)", "%.2f", -360.0, 360.0, 10, 0.0);
    OrientationAdjustmentsNP.fill(getDeviceName(), "ORIENTATION_ADJUSTMENTS", "Orientation Adjustments", IMU_TAB.c_str(), IP_RW,
                                  0, IPS_IDLE);
    OrientationAdjustmentsNP.load();

    SyncAxisNP[AXIS1].fill("SYNC_AXIS1", "Sync Axis 1 (deg)", "%.2f", -360, 360, 10, 0);
    SyncAxisNP[AXIS2].fill("SYNC_AXIS2", "Sync Axis 2 (deg)", "%.2f", -360, 360, 10, 0);
    SyncAxisNP.fill(getDeviceName(), "SYNC_AXIS", "Sync Axis", COORDINATES_TAB.c_str(), IP_RW, 0, IPS_IDLE);

    TelescopeVectorNP[TELESCOPE_VECTOR_X].fill("TELESCOPE_VECTOR_X", "Telescope Vector X", "%.2f", -1.0, 1.0, 0.1, 1.0);
    TelescopeVectorNP[TELESCOPE_VECTOR_Y].fill("TELESCOPE_VECTOR_Y", "Telescope Vector Y", "%.2f", -1.0, 1.0, 0.1, 0.0);
    TelescopeVectorNP[TELESCOPE_VECTOR_Z].fill("TELESCOPE_VECTOR_Z", "Telescope Vector Z", "%.2f", -1.0, 1.0, 0.1, 0.0);
    TelescopeVectorNP.fill(getDeviceName(), "TELESCOPE_VECTOR", "Telescope Vector", IMU_TAB.c_str(), IP_RW, 0, IPS_IDLE);
    TelescopeVectorNP.load();

    GeographicCoordNP[LOCATION_LATITUDE].fill("LAT", "Lat (dd:mm:ss.s)", "%012.8m", -90, 90, 0, 0.0);
    GeographicCoordNP[LOCATION_LONGITUDE].fill("LONG", "Lon (dd:mm:ss.s)", "%012.8m", 0, 360, 0, 0.0);
    GeographicCoordNP[LOCATION_ELEVATION].fill("ELEV", "Elevation (m)", "%g", -200, 10000, 0, 0);
    GeographicCoordNP.fill(getDeviceName(), "GEOGRAPHIC_COORD", "Location", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);
    GeographicCoordNP.load();

    MagneticDeclinationNP[0].fill("MAGNETIC_DECLINATION", "Magnetic Declination", "%.4f", -180, 180, 0, 0);
    MagneticDeclinationNP.fill(getDeviceName(), "MAGNETIC_DECLINATION", "Magnetic Declination", MAIN_CONTROL_TAB, IP_RW, 0,
                               IPS_IDLE);
    MagneticDeclinationNP.load();

    if (imuConnection & CONNECTION_SERIAL)
    {
        serialConnection = new Connection::Serial(this);
        serialConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(serialConnection);
    }

    if (imuConnection & CONNECTION_I2C)
    {
        i2cConnection = new Connection::I2C(this);
        i2cConnection->setDefaultBusPath("/dev/i2c-1");
        i2cConnection->registerHandshake([&]()
        {
            return callHandshake();
        });
        registerConnection(i2cConnection);
    }

    return true;
}

bool IMU::updateProperties()
{
    DefaultDevice::updateProperties();
    IMUInterface::updateProperties();

    if (isConnected())
    {
        // Define driver-specific properties when connected
        defineProperty(AstroCoordinatesNP);
        defineProperty(AstroCoordsTypeSP);
        defineProperty(IMUFrameSP);
        defineProperty(OrientationAdjustmentsNP);
        defineProperty(SyncAxisNP);
        defineProperty(TelescopeVectorNP);
        defineProperty(GeographicCoordNP);
        defineProperty(MagneticDeclinationNP);
    }
    else
    {
        // Delete driver-specific properties when disconnected
        deleteProperty(AstroCoordinatesNP);
        deleteProperty(AstroCoordsTypeSP);
        deleteProperty(IMUFrameSP);
        deleteProperty(OrientationAdjustmentsNP);
        deleteProperty(SyncAxisNP);
        deleteProperty(TelescopeVectorNP);
        deleteProperty(GeographicCoordNP);
        deleteProperty(MagneticDeclinationNP);
    }
    return true;
}

bool IMU::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (IMUInterface::processNumber(dev, name, values, names, n))
        return true;

    if (OrientationAdjustmentsNP.isNameMatch(name))
    {
        updateProperty(OrientationAdjustmentsNP, values, names, n, [this]()
        {
            // When orientation adjustments change, recalculate astro coordinates
            // and also update the IMU's internal orientation properties.
            // This will trigger SetOrientationData to apply the new adjustments.
            SetOrientationData(last_q_i, last_q_j, last_q_k, last_q_w);
            return true;
        }, true);
        return true;
    }

    if (TelescopeVectorNP.isNameMatch(name))
    {
        updateProperty(TelescopeVectorNP, values, names, n, [this]()
        {
            RecalculateAstroCoordinates();
            return true;
        }, true);
        return true;
    }

    if (GeographicCoordNP.isNameMatch(name))
    {
        updateProperty(GeographicCoordNP, values, names, n, [this]()
        {
            RecalculateAstroCoordinates();
            return true;
        }, true);
        return true;
    }

    if (MagneticDeclinationNP.isNameMatch(name))
    {
        updateProperty(MagneticDeclinationNP, values, names, n, [&]()
        {
            RecalculateAstroCoordinates();
            return true;
        }, true);
        return true;
    }

    if (SyncAxisNP.isNameMatch(name))
    {
        updateProperty(SyncAxisNP, values, names, n, [this, values]()
        {
            INDI::AlignmentSubsystem::TelescopeDirectionVectorSupportFunctions tdvFunctions;

            // Step 1: Convert sync coordinates to a TelescopeDirectionVector in the appropriate sky frame.
            INDI::AlignmentSubsystem::TelescopeDirectionVector skyVector;
            if (AstroCoordsTypeSP[COORD_EQUATORIAL].s == ISS_ON)
            {
                INDI::IEquatorialCoordinates syncEq;
                syncEq.rightascension = values[AXIS1] / 15.0; // Convert degrees to hours
                syncEq.declination    = values[AXIS2];
                skyVector = tdvFunctions.TelescopeDirectionVectorFromLocalHourAngleDeclination(syncEq);
            }
            else
            {
                INDI::IHorizontalCoordinates syncHor;
                syncHor.azimuth  = values[AXIS1];
                syncHor.altitude = values[AXIS2];
                // This function returns a vector in the NWU frame
                skyVector = tdvFunctions.TelescopeDirectionVectorFromAltitudeAzimuth(syncHor);
            }

            // Step 2: Inverse transform the sky vector back to the local horizon (ENU) frame.
            INDI::AlignmentSubsystem::TelescopeDirectionVector enuVector;
            if (AstroCoordsTypeSP[COORD_EQUATORIAL].s == ISS_ON)
            {
                // Inverse of the transformation from ENU to Equatorial
                double latitude = GeographicCoordNP[LOCATION_LATITUDE].getValue();
                double lat_rad = DEG_TO_RAD(latitude);
                double sin_lat = sin(lat_rad);
                double cos_lat = cos(lat_rad);

                // Original transform: x_eq = z_enu*cos-y_enu*sin, y_eq = -x_enu, z_eq = z_enu*sin+y_enu*cos
                // Inverse transform:
                enuVector.x = -skyVector.y; // East
                enuVector.y = (skyVector.z - (skyVector.x * sin_lat / cos_lat)) / (cos_lat + (sin_lat * sin_lat / cos_lat)); // North
                enuVector.z = (skyVector.x + enuVector.y * sin_lat) / cos_lat; // Up
            }
            else
            {
                // The skyVector is in the NWU frame, convert it to ENU
                // NWU to ENU: X_enu = -Y_nwu, Y_enu = X_nwu, Z_enu = Z_nwu
                enuVector.x = -skyVector.y;
                enuVector.y = skyVector.x;
                enuVector.z = skyVector.z;
            }

            // Step 3: Inverse transform from the local horizon (ENU) frame to the IMU's sensor frame.
            // This gives us the "target" vector - what the telescope vector *should* be in the sensor's frame.
            INDI::AlignmentSubsystem::TelescopeDirectionVector targetIMUVector;
            IMUFrame frame = (IMUFrame)IMUFrameSP.findOnSwitchIndex();
            switch (frame)
            {
                case ENU:
                    // No conversion needed
                    targetIMUVector = enuVector;
                    break;
                case NWU:
                    // ENU to NWU: X_nwu = Y_enu, Y_nwu = -X_enu, Z_nwu = Z_enu
                    targetIMUVector.x = enuVector.y;
                    targetIMUVector.y = -enuVector.x;
                    targetIMUVector.z = enuVector.z;
                    break;
                case SWU:
                    // ENU to SWU: X_swu = -Y_enu, Y_swu = -X_enu, Z_swu = Z_enu
                    targetIMUVector.x = -enuVector.y;
                    targetIMUVector.y = -enuVector.x;
                    targetIMUVector.z = enuVector.z;
                    break;
            }

            // Step 4: Calculate the telescope's *current* pointing vector in the sensor frame.
            // This is done by rotating the configured TelescopeVector by the raw sensor quaternion.
            double vx = TelescopeVectorNP[TELESCOPE_VECTOR_X].getValue();
            double vy = TelescopeVectorNP[TELESCOPE_VECTOR_Y].getValue();
            double vz = TelescopeVectorNP[TELESCOPE_VECTOR_Z].getValue();
            double qw = last_raw_q_w, qx = last_raw_q_i, qy = last_raw_q_j, qz = last_raw_q_k;

            double current_x = vx * (1 - 2 * qy * qy - 2 * qz * qz) + vy * (2 * qx * qy - 2 * qz * qw) + vz *
                               (2 * qx * qz + 2 * qy * qw);
            double current_y = vx * (2 * qx * qy + 2 * qz * qw) + vy * (1 - 2 * qx * qx - 2 * qz * qz) + vz *
                               (2 * qy * qz - 2 * qx * qw);
            double current_z = vx * (2 * qx * qz - 2 * qy * qw) + vy * (2 * qy * qz + 2 * qx * qw) + vz *
                               (1 - 2 * qx * qx - 2 * qy * qy);
            INDI::AlignmentSubsystem::TelescopeDirectionVector currentIMUVector(current_x, current_y, current_z);

            // Step 5: Calculate the rotation required to get from the current sensor vector to the target sensor vector.
            INDI::AlignmentSubsystem::TelescopeDirectionVector rotationAxis = currentIMUVector * targetIMUVector;
            double dotProduct = std::max(-1.0, std::min(1.0, currentIMUVector ^ targetIMUVector));
            double rotationAngle = acos(dotProduct);

            if (rotationAngle < 1e-6) // Already aligned
            {
                DEBUG(Logger::DBG_DEBUG, "IMU Sync: Already aligned, no adjustment needed.");
                return true;
            }
            rotationAxis.Normalise();

            // Step 5.1: Determine the full rotation that is currently being applied to the raw data
            double rollMultiplier  = OrientationAdjustmentsNP[ROLL_MULTIPLIER].getValue();
            double pitchMultiplier = OrientationAdjustmentsNP[PITCH_MULTIPLIER].getValue();
            double yawMultiplier   = OrientationAdjustmentsNP[YAW_MULTIPLIER].getValue();
            double magneticDeclinationRad = DEG_TO_RAD(MagneticDeclinationNP[0].getValue());

            double rawRollRad, rawPitchRad, rawYawRad;
            QuaternionToEuler(qx, qy, qz, qw, rawRollRad, rawPitchRad, rawYawRad);

            rawRollRad *= rollMultiplier;
            rawPitchRad *= pitchMultiplier;
            rawYawRad *= yawMultiplier;

            double multiplied_i, multiplied_j, multiplied_k, multiplied_w;
            EulerToQuaternion(rawRollRad, rawPitchRad, rawYawRad, multiplied_i, multiplied_j, multiplied_k, multiplied_w);

            double mag_i, mag_j, mag_k, mag_w;
            EulerToQuaternion(0, 0, magneticDeclinationRad, mag_i, mag_j, mag_k, mag_w);

            // Step 5.2: The rotation from the telescope vector to the target IMU vector is the final, absolute orientation
            // that the IMU should have. Let's call it target_orientation_q.
            INDI::AlignmentSubsystem::TelescopeDirectionVector v(vx, vy, vz);
            v.Normalise();
            targetIMUVector.Normalise();
            double dot = v ^ targetIMUVector;
            INDI::AlignmentSubsystem::TelescopeDirectionVector axis = v * targetIMUVector;
            axis.Normalise();
            double angle = acos(dot);

            double s = sin(angle / 2.0);
            double target_orientation_w = cos(angle / 2.0);
            double target_orientation_i = axis.x * s;
            double target_orientation_j = axis.y * s;
            double target_orientation_k = axis.z * s;

            // Step 5.3: We have target_orientation_q = mag_q * offset_q * multiplied_q
            // We need to solve for offset_q.
            // offset_q = conjugate(mag_q) * target_orientation_q * conjugate(multiplied_q)

            // conjugate(mag_q)
            double conj_mag_i = -mag_i, conj_mag_j = -mag_j, conj_mag_k = -mag_k, conj_mag_w = mag_w;
            // conjugate(multiplied_q)
            double conj_mult_i = -multiplied_i, conj_mult_j = -multiplied_j, conj_mult_k = -multiplied_k, conj_mult_w = multiplied_w;

            // temp_q = conjugate(mag_q) * target_orientation_q
            double temp_w = conj_mag_w * target_orientation_w - conj_mag_i * target_orientation_i - conj_mag_j * target_orientation_j -
                            conj_mag_k * target_orientation_k;
            double temp_i = conj_mag_w * target_orientation_i + conj_mag_i * target_orientation_w + conj_mag_j * target_orientation_k -
                            conj_mag_k * target_orientation_j;
            double temp_j = conj_mag_w * target_orientation_j - conj_mag_i * target_orientation_k + conj_mag_j * target_orientation_w +
                            conj_mag_k * target_orientation_i;
            double temp_k = conj_mag_w * target_orientation_k + conj_mag_i * target_orientation_j - conj_mag_j * target_orientation_i +
                            conj_mag_k * target_orientation_w;

            // offset_q = temp_q * conjugate(multiplied_q)
            double offset_w = temp_w * conj_mult_w - temp_i * conj_mult_i - temp_j * conj_mult_j - temp_k * conj_mult_k;
            double offset_i = temp_w * conj_mult_i + temp_i * conj_mult_w + temp_j * conj_mult_k - temp_k * conj_mult_j;
            double offset_j = temp_w * conj_mult_j - temp_i * conj_mult_k + temp_j * conj_mult_w + temp_k * conj_mult_i;
            double offset_k = temp_w * conj_mult_k + temp_i * conj_mult_j - temp_j * conj_mult_i + temp_k * conj_mult_w;

            // Step 6: Convert the new offset quaternion to Euler angles and set the properties.
            double roll, pitch, yaw;
            QuaternionToEuler(offset_i, offset_j, offset_k, offset_w, roll, pitch, yaw);

            OrientationAdjustmentsNP[ROLL_OFFSET].setValue(RAD_TO_DEG(roll));
            OrientationAdjustmentsNP[PITCH_OFFSET].setValue(RAD_TO_DEG(pitch));
            OrientationAdjustmentsNP[YAW_OFFSET].setValue(RAD_TO_DEG(yaw));
            OrientationAdjustmentsNP.apply();

            // Step 7: Add extensive logging for debugging.
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: Syncing to Axis1=%.2f, Axis2=%.2f", values[AXIS1], values[AXIS2]);
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: Selected IMU Frame: %s", IMUFrameSP.findOnSwitch()->getLabel());
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: Telescope Vector: X=%.4f, Y=%.4f, Z=%.4f", vx, vy, vz);
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: Raw Quaternion: i=%.4f, j=%.4f, k=%.4f, w=%.4f", qx, qy, qz, qw);
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: Target IMU Vector: X=%.4f, Y=%.4f, Z=%.4f", targetIMUVector.x,
                   targetIMUVector.y, targetIMUVector.z);
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: Current IMU Vector: X=%.4f, Y=%.4f, Z=%.4f", currentIMUVector.x,
                   currentIMUVector.y, currentIMUVector.z);
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: Rotation Angle: %.4f deg", RAD_TO_DEG(rotationAngle));
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: New Offset Quaternion: i=%.4f, j=%.4f, k=%.4f, w=%.4f", offset_i, offset_j,
                   offset_k, offset_w);
            DEBUGF(Logger::DBG_DEBUG, "IMU Sync: New Offsets (deg): Roll=%.2f, Pitch=%.2f, Yaw=%.2f", RAD_TO_DEG(roll),
                   RAD_TO_DEG(pitch), RAD_TO_DEG(yaw));

            // Step 8: Trigger a full recalculation with the new offsets using the last raw sensor data.
            SetOrientationData(last_raw_q_i, last_raw_q_j, last_raw_q_k, last_raw_q_w);
            return true;
        });
        return true;
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool IMU::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (IMUInterface::processSwitch(dev, name, states, names, n))
        return true;

    if (AstroCoordsTypeSP.isNameMatch(name))
    {
        updateProperty(AstroCoordsTypeSP, states, names, n, [this]()
        {
            RecalculateAstroCoordinates();
            return true;
        }, true);
        return true;
    }

    if (IMUFrameSP.isNameMatch(name))
    {
        updateProperty(IMUFrameSP, states, names, n, [this]()
        {
            RecalculateAstroCoordinates();
            return true;
        }, true);
        return true;
    }


    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool IMU::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (IMUInterface::processText(dev, name, texts, names, n))
        return true;

    return DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool IMU::saveConfigItems(FILE *fp)
{
    IMUInterface::saveConfigItems(fp);
    DefaultDevice::saveConfigItems(fp);

    // Save driver-specific properties
    AstroCoordsTypeSP.save(fp);
    IMUFrameSP.save(fp);
    OrientationAdjustmentsNP.save(fp);
    TelescopeVectorNP.save(fp);
    GeographicCoordNP.save(fp);

    return true;
}

bool IMU::Handshake()
{
    // Default implementation does nothing and returns false
    // Concrete drivers should override this method
    return false;
}

bool IMU::callHandshake()
{
    if (imuConnection > 0)
    {
        if (getActiveConnection() == serialConnection)
            PortFD = serialConnection->getPortFD();
        else if (getActiveConnection() == i2cConnection)
            PortFD = i2cConnection->getPortFD();
    }

    return Handshake();
}

// Helper function to convert quaternion to Euler angles (roll, pitch, yaw)
// Angles are in radians
void IMU::QuaternionToEuler(double i, double j, double k, double w, double &roll, double &pitch, double &yaw)
{
    // Roll (x-axis rotation)
    double sinr_cosp = 2 * (w * i + j * k);
    double cosr_cosp = 1 - 2 * (i * i + j * j);
    roll             = std::atan2(sinr_cosp, cosr_cosp);

    // Pitch (y-axis rotation)
    double sinp = 2 * (w * j - k * i);
    if (std::abs(sinp) >= 1)
        pitch = std::copysign(M_PI / 2, sinp); // Use 90 degrees if out of range
    else
        pitch = std::asin(sinp);

    // Yaw (z-axis rotation)
    double siny_cosp = 2 * (w * k + i * j);
    double cosy_cosp = 1 - 2 * (j * j + k * k);
    yaw              = std::atan2(siny_cosp, cosy_cosp);
}

// Helper function to convert Euler angles (roll, pitch, yaw) in radians to a quaternion
void IMU::EulerToQuaternion(double roll, double pitch, double yaw, double &i, double &j, double &k, double &w)
{
    double cy = cos(yaw * 0.5);
    double sy = sin(yaw * 0.5);
    double cp = cos(pitch * 0.5);
    double sp = sin(pitch * 0.5);
    double cr = cos(roll * 0.5);
    double sr = sin(roll * 0.5);

    w = cr * cp * cy + sr * sp * sy;
    i = sr * cp * cy - cr * sp * sy;
    j = cr * sp * cy + sr * cp * sy;
    k = cr * cp * sy - sr * sp * cy;
}

void IMU::RecalculateAstroCoordinates()
{
    /*
     * COORDINATE TRANSFORMATION SYSTEM OVERVIEW
     * ========================================
     *
     * This function implements a multi-stage coordinate transformation chain to convert
     * IMU sensor orientation data into astronomical coordinates (Hour Angle/Declination
     * or Azimuth/Altitude). The transformation addresses the fundamental challenge of
     * German Equatorial Mount (GEM) axis coupling.
     *
     * GERMAN EQUATORIAL MOUNT AXIS COUPLING PROBLEM:
     * ----------------------------------------------
     * In a German Equatorial Mount, the telescope's orientation is determined by two
     * coupled axes:
     * 1. Right Ascension (RA) axis - aligned with Earth's rotation axis
     * 2. Declination (DEC) axis - perpendicular to RA axis
     *
     * The coupling issue arises because:
     * - A single IMU sensor measures the telescope's absolute orientation in space
     * - But the mount's axes are mechanically coupled - rotating one affects the other
     * - The same celestial target can be reached by multiple combinations of RA/DEC positions
     * - This creates ambiguity in determining individual axis positions from IMU data alone
     *
     * CURRENT APPROACH - DIRECT COORDINATE TRANSFORMATION:
     * ---------------------------------------------------
     * Instead of trying to decouple individual axis positions, this implementation:
     * 1. Uses the IMU to determine the telescope's absolute pointing direction
     * 2. Transforms this direction through multiple coordinate frames
     * 3. Converts the final direction to astronomical coordinates
     *
     * This approach provides accurate pointing information but doesn't solve the
     * fundamental axis decoupling problem for mount control.
     *
     * COORDINATE TRANSFORMATION CHAIN:
     * -------------------------------
     * IMU Sensor Frame → Local Horizon Frame → Equatorial Frame → Astronomical Coordinates
     *
     * Each stage is documented in detail below.
     */

    // Log raw quaternion values for debugging
    DEBUGF(Logger::DBG_DEBUG, "IMU: Recalculating Astro Coordinates from stored Quaternion: i=%.4f, j=%.4f, k=%.4f, w=%.4f",
           last_q_i, last_q_j, last_q_k, last_q_w);

    double rollRad, pitchRad, yawRad;
    QuaternionToEuler(last_q_i, last_q_j, last_q_k, last_q_w, rollRad, pitchRad, yawRad);

    /*
     * STAGE 1: IMU SENSOR FRAME TO TELESCOPE POINTING VECTOR
     * =====================================================
     *
     * The quaternion (last_q_w, last_q_i, last_q_j, last_q_k) represents the IMU's
     * orientation after applying user-defined multipliers, offsets, and magnetic declination.
     * This quaternion describes the rotation from the IMU's reference frame to the
     * local horizon frame.
     */
    double qw = last_q_w;
    double qx = last_q_i;
    double qy = last_q_j;
    double qz = last_q_k;

    /*
     * Define the telescope's pointing vector in the IMU's own reference frame.
     * This vector represents the direction the telescope is pointing relative to
     * the IMU sensor's coordinate system.
     *
     * Default: (1, 0, 0) assumes telescope points along IMU's X-axis
     * This vector is configurable via the TELESCOPE_VECTOR property to accommodate
     * different physical mounting orientations of the IMU on the telescope.
     */
    double vx = TelescopeVectorNP[TELESCOPE_VECTOR_X].getValue();
    double vy = TelescopeVectorNP[TELESCOPE_VECTOR_Y].getValue();
    double vz = TelescopeVectorNP[TELESCOPE_VECTOR_Z].getValue();

    // Log telescope vector configuration
    DEBUGF(Logger::DBG_DEBUG, "IMU: Telescope Vector: X=%.4f, Y=%.4f, Z=%.4f", vx, vy, vz);

    // Log IMU frame setting
    IMUFrame currentFrame = static_cast<IMUFrame>(IMUFrameSP.findOnSwitchIndex());
    DEBUGF(Logger::DBG_DEBUG, "IMU: IMU Frame: %s (%d)", IMUFrameSP.findOnSwitch()->getLabel(), currentFrame);

    // Log geographic location
    double latitude = GeographicCoordNP[LOCATION_LATITUDE].getValue();
    DEBUGF(Logger::DBG_DEBUG, "IMU: Geographic Latitude=%.6f°", latitude);

    // Log orientation adjustments
    double rollMultiplier = OrientationAdjustmentsNP[ROLL_MULTIPLIER].getValue();
    double pitchMultiplier = OrientationAdjustmentsNP[PITCH_MULTIPLIER].getValue();
    double yawMultiplier = OrientationAdjustmentsNP[YAW_MULTIPLIER].getValue();
    double rollOffset = OrientationAdjustmentsNP[ROLL_OFFSET].getValue();
    double pitchOffset = OrientationAdjustmentsNP[PITCH_OFFSET].getValue();
    double yawOffset = OrientationAdjustmentsNP[YAW_OFFSET].getValue();
    double magneticDeclination = MagneticDeclinationNP[0].getValue();

    DEBUGF(Logger::DBG_DEBUG, "IMU: Orientation Multipliers: Roll=%.2f, Pitch=%.2f, Yaw=%.2f",
           rollMultiplier, pitchMultiplier, yawMultiplier);
    DEBUGF(Logger::DBG_DEBUG, "IMU: Orientation Offsets (deg): Roll=%.2f, Pitch=%.2f, Yaw=%.2f",
           rollOffset, pitchOffset, yawOffset);
    DEBUGF(Logger::DBG_DEBUG, "IMU: Magnetic Declination=%.4f°", magneticDeclination);

    /*
     * STAGE 2: QUATERNION-VECTOR ROTATION
     * ===================================
     *
     * Rotate the telescope pointing vector by the IMU's adjusted quaternion to get the
     * pointing direction in the local horizon frame. This is a standard quaternion-vector
     * rotation that transforms the telescope vector from the IMU's reference frame to
     * the local horizon frame.
     *
     * Mathematical Formula: V' = q * V * q_conjugate
     * Where V is treated as a pure quaternion (0, vx, vy, vz)
     *
     * The expanded formula for rotated vector components (x', y', z') is:
     * x' = vx*(1-2*qy²-2*qz²) + vy*(2*qx*qy-2*qz*qw) + vz*(2*qx*qz+2*qy*qw)
     * y' = vx*(2*qx*qy+2*qz*qw) + vy*(1-2*qx²-2*qz²) + vz*(2*qy*qz-2*qx*qw)
     * z' = vx*(2*qx*qz-2*qy*qw) + vy*(2*qy*qz+2*qx*qw) + vz*(1-2*qx²-2*qy²)
     */
    double x_hor = vx * (1 - 2 * qy * qy - 2 * qz * qz) + vy * (2 * qx * qy - 2 * qz * qw) + vz * (2 * qx * qz + 2 * qy * qw);
    double y_hor = vx * (2 * qx * qy + 2 * qz * qw) + vy * (1 - 2 * qx * qx - 2 * qz * qz) + vz * (2 * qy * qz - 2 * qx * qw);
    double z_hor = vx * (2 * qx * qz - 2 * qy * qw) + vy * (2 * qy * qz + 2 * qx * qw) + vz * (1 - 2 * qx * qx - 2 * qy * qy);

    INDI::AlignmentSubsystem::TelescopeDirectionVector imu_vector(x_hor, y_hor, z_hor);

    // Log IMU vector before frame conversion
    DEBUGF(Logger::DBG_DEBUG, "IMU: IMU Vector (before frame conversion): X=%.4f, Y=%.4f, Z=%.4f", x_hor, y_hor, z_hor);

    /*
     * STAGE 3: IMU COORDINATE FRAME CONVERSION
     * =======================================
     *
     * Convert between different IMU coordinate frame conventions. Different IMU sensors
     * and mounting orientations may use different coordinate frame definitions:
     *
     * - ENU (East-North-Up): X=East, Y=North, Z=Up
     * - NWU (North-West-Up): X=North, Y=West, Z=Up
     * - SWU (South-West-Up): X=South, Y=West, Z=Up
     *
     * The imu_vector is initially in the IMU's native frame. We need to convert it to
     * a standardized local horizon frame for subsequent astronomical calculations.
     *
     * COORDINATE FRAME TRANSFORMATION MATRICES:
     * ----------------------------------------
     * ENU → NWU: [X_nwu, Y_nwu, Z_nwu] = [X_enu, -Y_enu, Z_enu]
     * ENU → SWU: [X_swu, Y_swu, Z_swu] = [-Y_enu, -X_enu, Z_enu]
     */
    INDI::AlignmentSubsystem::TelescopeDirectionVector horizontal_vector;

    DEBUGF(Logger::DBG_DEBUG, "IMU: Converting from ENU to %s telescope frame", IMUFrameSP.findOnSwitch()->getLabel());
    switch (IMUFrameSP.findOnSwitchIndex())
    {
        case ENU:
            // Telescope uses ENU - no conversion needed
            horizontal_vector = imu_vector;
            break;
        case NWU:
            // Convert East-North-Up to North-West-Up telescope frame
            // Since telescope NORTH is along IMU EAST:
            // X_nwu = X_enu (North = ENU_X, since telescope North is along IMU East)
            // Y_nwu = -Y_enu (West = -ENU_Y)
            // Z_nwu = Z_enu (Up = ENU_Z)
            horizontal_vector.x = imu_vector.x;
            horizontal_vector.y = -imu_vector.y;
            horizontal_vector.z = imu_vector.z;
            break;
        case SWU:
            // Convert East-North-Up to South-West-Up telescope frame
            // X_swu = -Y_enu (South = -ENU_Y)
            // Y_swu = -X_enu (West = -ENU_X)
            // Z_swu = Z_enu (Up = ENU_Z)
            horizontal_vector.x = -imu_vector.y;
            horizontal_vector.y = -imu_vector.x;
            horizontal_vector.z = imu_vector.z;
            break;
    }

    // Log the horizontal vector
    DEBUGF(Logger::DBG_DEBUG, "IMU: Horizontal Vector: X=%.4f, Y=%.4f, Z=%.4f", horizontal_vector.x, horizontal_vector.y,
           horizontal_vector.z);

    if (AstroCoordsTypeSP[COORD_EQUATORIAL].s == ISS_ON)
    {
        /*
         * STAGE 4: LOCAL HORIZON TO EQUATORIAL COORDINATE TRANSFORMATION
         * =============================================================
         *
         * Transform the horizontal vector to the equatorial frame based on the observer's
         * geographic latitude. This is a fundamental astronomical coordinate transformation
         * that accounts for the Earth's rotation and the observer's position.
         *
         * COORDINATE FRAME DEFINITIONS:
         * ----------------------------
         * Horizontal Frame (NWU):
         *   - X = North component (v_n)
         *   - Y = West component (v_w)
         *   - Z = Up component (v_u)
         *
         * Equatorial Frame:
         *   - X_eq = Points toward the meridian (local hour angle = 0)
         *   - Y_eq = Points West (same as horizontal frame)
         *   - Z_eq = Points toward the North Celestial Pole
         *
         * TRANSFORMATION MATHEMATICS:
         * --------------------------
         * The transformation rotates the horizontal frame around the East-West axis
         * by the complement of the latitude angle (90° - latitude).
         *
         * For NWU input coordinates:
         * x_eq = v_u * cos(lat) + v_n * sin(lat)  [Meridian component]
         * y_eq = v_w                              [West component unchanged]
         * z_eq = v_u * sin(lat) + v_n * cos(lat)  [Celestial pole component]
         *
         * PHYSICAL INTERPRETATION:
         * -----------------------
         * - At latitude 0° (equator): horizontal up = equatorial meridian
         * - At latitude 90° (pole): horizontal north = equatorial meridian
         * - The West component remains unchanged in both frames
         */
        double lat_rad = DEG_TO_RAD(latitude);
        double sin_lat = sin(lat_rad);
        double cos_lat = cos(lat_rad);

        // Log transformation parameters
        DEBUGF(Logger::DBG_DEBUG, "IMU: Horizontal→Equatorial transformation: lat_rad=%.6f, sin_lat=%.6f, cos_lat=%.6f",
               lat_rad, sin_lat, cos_lat);

        // Apply the latitude-based rotation transformation
        double x_eq = (horizontal_vector.z * cos_lat) + (horizontal_vector.x * sin_lat);
        double y_eq = horizontal_vector.y;
        double z_eq = (horizontal_vector.z * sin_lat) + (horizontal_vector.x * cos_lat);

        INDI::AlignmentSubsystem::TelescopeDirectionVector equatorial_vector(x_eq, y_eq, z_eq);

        // Log the equatorial vector
        DEBUGF(Logger::DBG_DEBUG, "IMU: Equatorial Vector: X=%.4f, Y=%.4f, Z=%.4f", x_eq, y_eq, z_eq);

        /*
         * STAGE 5: EQUATORIAL VECTOR TO ASTRONOMICAL COORDINATES
         * =====================================================
         *
         * Convert the equatorial direction vector to Hour Angle and Declination.
         * This uses the INDI Alignment Subsystem's support functions which implement
         * the standard astronomical coordinate conversion algorithms.
         *
         * The LocalHourAngleDeclinationFromTelescopeDirectionVector function:
         * - Expects a unit vector in the equatorial frame
         * - Returns Hour Angle in hours (not degrees)
         * - Returns Declination in degrees
         *
         * COORDINATE INTERPRETATION:
         * -------------------------
         * - Hour Angle = 0: telescope pointing at meridian (due south)
         * - Hour Angle = 6h: telescope pointing west
         * - Hour Angle = 12h: telescope pointing north
         * - Hour Angle = 18h: telescope pointing east
         * - Declination = +90°: pointing at North Celestial Pole
         * - Declination = -90°: pointing at South Celestial Pole
         */
        INDI::IEquatorialCoordinates eq_coords;
        INDI::AlignmentSubsystem::TelescopeDirectionVectorSupportFunctions tdv_functions;
        tdv_functions.LocalHourAngleDeclinationFromTelescopeDirectionVector(equatorial_vector, eq_coords);

        // Log intermediate calculation details
        DEBUGF(Logger::DBG_DEBUG,
               "IMU: LocalHourAngleDeclinationFromTelescopeDirectionVector returned: HA=%.6f hours, Dec=%.6f deg",
               eq_coords.rightascension, eq_coords.declination);

        // Update INDI properties with final astronomical coordinates
        // Convert Hour Angle from hours to degrees for the property display
        AstroCoordinatesNP[AXIS1].setValue(eq_coords.rightascension * 15.0); // HA in degrees
        AstroCoordinatesNP[AXIS2].setValue(eq_coords.declination);
        DEBUGF(Logger::DBG_DEBUG, "IMU: Calculated HA=%.2f deg, Dec=%.2f deg", eq_coords.rightascension * 15.0,
               eq_coords.declination);
    }
    else // Alt-Az calculation
    {
        /*
         * ALTERNATIVE PATH: ALTITUDE-AZIMUTH COORDINATES
         * =============================================
         *
         * For Alt-Az coordinate output, we use the horizontal frame vector directly.
         * The AltitudeAzimuthFromTelescopeDirectionVector function expects a vector
         * in the NWU (North-West-Up) frame, which matches our horizontal_vector after
         * frame conversion.
         *
         * ALTITUDE-AZIMUTH COORDINATE SYSTEM:
         * ----------------------------------
         * - Azimuth = 0°: North, 90°: East, 180°: South, 270°: West
         * - Altitude = 0°: Horizon, +90°: Zenith, -90°: Nadir
         *
         * This coordinate system is simpler than equatorial coordinates as it doesn't
         * require the latitude-based transformation. However, Alt-Az coordinates are
         * time-dependent due to Earth's rotation, while equatorial coordinates are
         * more stable for tracking celestial objects.
         */
        INDI::IHorizontalCoordinates horiz_coords;
        INDI::AlignmentSubsystem::TelescopeDirectionVectorSupportFunctions tdv_functions;
        tdv_functions.AltitudeAzimuthFromTelescopeDirectionVector(horizontal_vector, horiz_coords);

        AstroCoordinatesNP[AXIS1].setValue(horiz_coords.azimuth);
        AstroCoordinatesNP[AXIS2].setValue(horiz_coords.altitude);
        DEBUGF(Logger::DBG_DEBUG, "IMU: Horizontal Vector for Alt/Az calc: X=%.4f, Y=%.4f, Z=%.4f",
               horizontal_vector.x, horizontal_vector.y, horizontal_vector.z);
        DEBUGF(Logger::DBG_DEBUG, "IMU: Calculated Az=%.2f deg, Alt=%.2f deg", horiz_coords.azimuth, horiz_coords.altitude);
    }

    /*
     * FINAL STEP: UPDATE INDI PROPERTIES
     * ==================================
     */

    // Common code to send the update
    AstroCoordinatesNP.setState(IPS_OK);
    AstroCoordinatesNP.apply();
}

/*
 * ORIENTATION DATA PROCESSING AND ADJUSTMENT SYSTEM
 * =================================================
 *
 * This function processes raw IMU orientation data and applies a series of adjustments
 * to compensate for mounting orientation, sensor calibration issues, and magnetic
 * declination. The processed orientation is then used for astronomical coordinate
 * calculations.
 *
 * ADJUSTMENT PIPELINE OVERVIEW:
 * ----------------------------
 * Raw Sensor Data → Multiplier Adjustments → Offset Adjustments → Magnetic Declination → Final Orientation
 *
 * PURPOSE OF EACH ADJUSTMENT STAGE:
 * --------------------------------
 * 1. MULTIPLIER ADJUSTMENTS: Correct for sensor axis inversions or scaling issues
 *    - Roll/Pitch/Yaw multipliers can be set to -1 to flip axes
 *    - Useful when IMU is mounted in non-standard orientations
 *
 * 2. OFFSET ADJUSTMENTS: Apply fixed angular corrections
 *    - Compensate for mounting alignment errors
 *    - Can be manually set or calculated via sync operations
 *
 * 3. MAGNETIC DECLINATION: Correct for difference between magnetic and true north
 *    - Essential for accurate azimuth/bearing calculations
 *    - Varies by geographic location and changes over time
 *
 * QUATERNION PROCESSING RATIONALE:
 * -------------------------------
 * While adjustments are conceptually easier to understand in Euler angles,
 * the implementation uses quaternions for the final calculations to avoid
 * gimbal lock issues and maintain numerical stability. The process:
 * 1. Convert raw quaternion to Euler angles for multiplier application
 * 2. Convert back to quaternion for offset and magnetic declination application
 * 3. Use quaternion multiplication for final composition
 *
 * RELATIONSHIP TO COORDINATE TRANSFORMATION:
 * -----------------------------------------
 * The final adjusted quaternion (last_q_*) is used by RecalculateAstroCoordinates()
 * to transform the telescope pointing vector from the IMU frame to astronomical
 * coordinates. This creates a complete chain from raw sensor data to sky coordinates.
 */
bool IMU::SetOrientationData(double i, double j, double k, double w)
{
    /*
     * STAGE 1: STORE RAW SENSOR DATA
     * =============================
     *
     * Preserve the original, unmodified quaternion from the IMU sensor.
     * This raw data is essential for:
     * - Sync operations that need to calculate correction offsets
     * - Debugging and diagnostics
     * - Potential future recalibration without losing original data
     */
    last_raw_q_i = i;
    last_raw_q_j = j;
    last_raw_q_k = k;
    last_raw_q_w = w;

    DEBUGF(Logger::DBG_DEBUG, "IMU: Raw Quaternion: i=%.4f, j=%.4f, k=%.4f, w=%.4f", i, j, k, w);

    /*
     * STAGE 2: RETRIEVE USER-CONFIGURED ADJUSTMENTS
     * =============================================
     *
     * Load all adjustment parameters from INDI properties. These values can be:
     * - Set manually by the user through the client interface
     * - Calculated automatically by sync operations
     * - Loaded from saved configuration files
     */
    double rollMultiplier  = OrientationAdjustmentsNP[ROLL_MULTIPLIER].getValue();
    double pitchMultiplier = OrientationAdjustmentsNP[PITCH_MULTIPLIER].getValue();
    double yawMultiplier   = OrientationAdjustmentsNP[YAW_MULTIPLIER].getValue();
    double rollOffset      = DEG_TO_RAD(OrientationAdjustmentsNP[ROLL_OFFSET].getValue());
    double pitchOffset     = DEG_TO_RAD(OrientationAdjustmentsNP[PITCH_OFFSET].getValue());
    double yawOffset       = DEG_TO_RAD(OrientationAdjustmentsNP[YAW_OFFSET].getValue());
    double magneticDeclinationRad = DEG_TO_RAD(MagneticDeclinationNP[0].getValue());

    /*
     * STAGE 3: APPLY MULTIPLIER ADJUSTMENTS
     * =====================================
     *
     * Multipliers are applied in Euler angle space to handle axis inversions
     * and scaling corrections. This is necessary because:
     * - IMU mounting orientation may not match telescope coordinate system
     * - Some sensors may have inverted axes that need correction
     * - Multipliers of -1 effectively flip axis directions
     *
     * MATHEMATICAL PROCESS:
     * 1. Convert quaternion to Euler angles (roll, pitch, yaw)
     * 2. Apply multipliers: angle_adjusted = angle_raw * multiplier
     * 3. Convert back to quaternion for subsequent processing
     */
    double rawRollRad, rawPitchRad, rawYawRad;
    QuaternionToEuler(i, j, k, w, rawRollRad, rawPitchRad, rawYawRad);

    // Apply axis multipliers to handle mounting orientation corrections
    rawRollRad *= rollMultiplier;
    rawPitchRad *= pitchMultiplier;
    rawYawRad *= yawMultiplier;

    // Convert multiplier-adjusted Euler angles back to quaternion
    double temp_i, temp_j, temp_k, temp_w;
    EulerToQuaternion(rawRollRad, rawPitchRad, rawYawRad, temp_i, temp_j, temp_k, temp_w);

    /*
     * STAGE 4: PREPARE OFFSET AND MAGNETIC DECLINATION QUATERNIONS
     * ============================================================
     *
     * Convert the offset angles and magnetic declination to quaternion form
     * for proper quaternion multiplication. This approach:
     * - Avoids gimbal lock issues that can occur with Euler angle arithmetic
     * - Maintains numerical precision for small angle corrections
     * - Allows proper composition of multiple rotations
     */

    // Convert user-defined offset angles to a quaternion
    double offset_i, offset_j, offset_k, offset_w;
    EulerToQuaternion(rollOffset, pitchOffset, yawOffset, offset_i, offset_j, offset_k, offset_w);

    // Convert magnetic declination (yaw-only rotation) to a quaternion
    double mag_i, mag_j, mag_k, mag_w;
    EulerToQuaternion(0, 0, magneticDeclinationRad, mag_i, mag_j, mag_k, mag_w);

    /*
     * STAGE 5: QUATERNION COMPOSITION
     * ===============================
     *
     * Apply offset and magnetic declination corrections using quaternion multiplication.
     * The order of operations is critical:
     *
     * COMPOSITION ORDER: final_q = mag_q * offset_q * temp_q
     * Where:
     * - temp_q = multiplier-adjusted raw quaternion
     * - offset_q = user-defined offset corrections
     * - mag_q = magnetic declination correction
     *
     * MATHEMATICAL JUSTIFICATION:
     * Quaternion multiplication represents rotation composition. The rightmost
     * quaternion is applied first, so this order means:
     * 1. First apply multiplier adjustments (temp_q)
     * 2. Then apply offset corrections (offset_q)
     * 3. Finally apply magnetic declination (mag_q)
     *
     * QUATERNION MULTIPLICATION FORMULA:
     * For q1 * q2 = (w1*w2 - x1*x2 - y1*y2 - z1*z2,
     *                w1*x2 + x1*w2 + y1*z2 - z1*y2,
     *                w1*y2 - x1*z2 + y1*w2 + z1*x2,
     *                w1*z2 + x1*y2 - y1*x2 + z1*w2)
     */

    // First composition: adjusted_q = offset_q * temp_q
    double adjusted_w = offset_w * temp_w - offset_i * temp_i - offset_j * temp_j - offset_k * temp_k;
    double adjusted_i = offset_w * temp_i + offset_i * temp_w + offset_j * temp_k - offset_k * temp_j;
    double adjusted_j = offset_w * temp_j - offset_i * temp_k + offset_j * temp_w + offset_k * temp_i;
    double adjusted_k = offset_w * temp_k + offset_i * temp_j - offset_j * temp_i + offset_k * temp_w;

    // Final composition: final_q = mag_q * adjusted_q
    last_q_w = mag_w * adjusted_w - mag_i * adjusted_i - mag_j * adjusted_j - mag_k * adjusted_k;
    last_q_i = mag_w * adjusted_i + mag_i * adjusted_w + mag_j * adjusted_k - mag_k * adjusted_j;
    last_q_j = mag_w * adjusted_j - mag_i * adjusted_k + mag_j * adjusted_w + mag_k * adjusted_i;
    last_q_k = mag_w * adjusted_k + mag_i * adjusted_j - mag_j * adjusted_i + mag_k * adjusted_w;

    /*
     * STAGE 6: LOGGING AND DIAGNOSTICS
     * ================================
     *
     * Convert the final adjusted quaternion back to Euler angles for human-readable
     * logging. This helps with:
     * - Debugging orientation issues
     * - Verifying that adjustments are being applied correctly
     * - Monitoring the telescope's orientation in familiar angle units
     */
    double adjustedRollRad, adjustedPitchRad, adjustedYawRad;
    QuaternionToEuler(last_q_i, last_q_j, last_q_k, last_q_w, adjustedRollRad, adjustedPitchRad, adjustedYawRad);
    DEBUGF(Logger::DBG_DEBUG, "IMU: Adjusted Euler Angles (deg): Roll=%.2f, Pitch=%.2f, Yaw=%.2f",
           RAD_TO_DEG(adjustedRollRad), RAD_TO_DEG(adjustedPitchRad), RAD_TO_DEG(adjustedYawRad));

    /*
     * STAGE 7: UPDATE INDI PROPERTIES
     * ===============================
     *
     * Update the standard INDI orientation properties with the RAW (unadjusted) data.
     * This design choice ensures that:
     * - Users can see the actual sensor readings
     * - Diagnostic tools can access unmodified sensor data
     * - The adjustment pipeline remains transparent
     *
     * The adjusted data is used internally for coordinate calculations but not
     * directly exposed through these standard properties.
     */
    QuaternionToEuler(i, j, k, w, rawRollRad, rawPitchRad, rawYawRad);
    OrientationNP[ORIENTATION_ROLL].setValue(RAD_TO_DEG(rawRollRad));
    OrientationNP[ORIENTATION_PITCH].setValue(RAD_TO_DEG(rawPitchRad));
    OrientationNP[ORIENTATION_YAW].setValue(RAD_TO_DEG(rawYawRad));
    OrientationNP[ORIENTATION_QUATERNION_W].setValue(w);
    OrientationNP.setState(IPS_OK);
    OrientationNP.apply();

    /*
     * STAGE 8: TRIGGER COORDINATE RECALCULATION
     * =========================================
     *
     * Call RecalculateAstroCoordinates() to transform the newly adjusted orientation
     * into astronomical coordinates (Hour Angle/Declination or Azimuth/Altitude).
     *
     * This completes the full pipeline from raw IMU sensor data to usable
     * astronomical pointing information, addressing the German Equatorial Mount
     * axis coupling challenge through direct coordinate transformation rather
     * than attempting to decouple individual axis positions.
     *
     * FUTURE ENHANCEMENT PATHWAY:
     * --------------------------
     * For full axis decoupling, this system could be extended with:
     * 1. Integration with INDI Alignment Subsystem for pointing model corrections
     * 2. Machine learning algorithms to learn mount-specific coupling patterns
     * 3. Multi-point calibration systems for improved accuracy
     * 4. Real-time tracking error feedback for dynamic corrections
     */
    RecalculateAstroCoordinates();

    return true;
}

bool IMU::SetAccelerationData(double x, double y, double z)
{
    AccelerationNP[ACCELERATION_X].setValue(x);
    AccelerationNP[ACCELERATION_Y].setValue(y);
    AccelerationNP[ACCELERATION_Z].setValue(z);
    AccelerationNP.setState(IPS_OK);
    AccelerationNP.apply();
    return true;
}

bool IMU::SetGyroscopeData(double x, double y, double z)
{
    GyroscopeNP[GYROSCOPE_X].setValue(x);
    GyroscopeNP[GYROSCOPE_Y].setValue(y);
    GyroscopeNP[GYROSCOPE_Z].setValue(z);
    GyroscopeNP.setState(IPS_OK);
    GyroscopeNP.apply();
    return true;
}

bool IMU::SetMagnetometerData(double x, double y, double z)
{
    MagnetometerNP[MAGNETOMETER_X].setValue(x);
    MagnetometerNP[MAGNETOMETER_Y].setValue(y);
    MagnetometerNP[MAGNETOMETER_Z].setValue(z);
    MagnetometerNP.setState(IPS_OK);
    MagnetometerNP.apply();
    return true;
}
bool IMU::SetCalibrationStatus(int sys, int gyro, int accel, int mag)
{
    INDI_UNUSED(sys);
    INDI_UNUSED(gyro);
    INDI_UNUSED(accel);
    INDI_UNUSED(mag);
    return false;
}
bool IMU::StartCalibration()
{
    return false;
}
bool IMU::SaveCalibrationData()
{
    return false;
}
bool IMU::LoadCalibrationData()
{
    return false;
}
bool IMU::ResetCalibration()
{
    return false;
}
bool IMU::SetPowerMode(const std::string &mode)
{
    INDI_UNUSED(mode);
    return false;
}
bool IMU::SetOperationMode(const std::string &mode)
{
    INDI_UNUSED(mode);
    return false;
}
bool IMU::SetDistanceUnits(bool metric)
{
    INDI_UNUSED(metric);
    return false;
}
bool IMU::SetAngularUnits(bool degrees)
{
    INDI_UNUSED(degrees);
    return false;
}
bool IMU::SetUpdateRate(double rate)
{
    INDI_UNUSED(rate);
    return false;
}
bool IMU::SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion, const std::string &sensorStatus)
{
    INDI_UNUSED(chipID);
    INDI_UNUSED(firmwareVersion);
    INDI_UNUSED(sensorStatus);
    return false;
}
bool IMU::SetTemperature(double temperature)
{
    INDI_UNUSED(temperature);
    return false;
}
bool IMU::SetStabilityMonitoring(double vibrationLevel, double stabilityThreshold)
{
    INDI_UNUSED(vibrationLevel);
    INDI_UNUSED(stabilityThreshold);
    return false;
}

}
