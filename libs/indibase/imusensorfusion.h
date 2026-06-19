/*
    IMU Sensor Fusion - Madgwick and Mahony AHRS filters
    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

    Based on:
    - Madgwick, S.O.H. (2010). "An efficient orientation filter for inertial and inertial/magnetic sensor arrays"
    - Mahony, R. et al. (2008). "Nonlinear Complementary Filters on the Special Orthogonal Group"

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

#pragma once

#include <cmath>

namespace INDI
{

/**
 * @brief Abstract base class for IMU sensor fusion algorithms.
 *
 * Accepts calibrated 9-DOF sensor data (accelerometer, gyroscope, magnetometer)
 * and produces a quaternion representing the orientation of the sensor in the
 * world frame. Gracefully degrades to 6-DOF if magnetometer data is not available.
 *
 * All inputs use SI units:
 *   - Gyroscope:     radians/second
 *   - Accelerometer: m/s² (does not need to be normalised)
 *   - Magnetometer:  any consistent unit (µT, mG, etc.) — only direction is used
 */
class IMUSensorFusion
{
    public:
        virtual ~IMUSensorFusion() = default;

        /**
         * @brief Perform one filter update step.
         *
         * @param gx Gyroscope X-axis angular rate (rad/s)
         * @param gy Gyroscope Y-axis angular rate (rad/s)
         * @param gz Gyroscope Z-axis angular rate (rad/s)
         * @param ax Accelerometer X-axis (m/s² or any unit — only direction used)
         * @param ay Accelerometer Y-axis
         * @param az Accelerometer Z-axis
         * @param mx Magnetometer X-axis (µT or any consistent unit)
         * @param my Magnetometer Y-axis
         * @param mz Magnetometer Z-axis
         * @param dt Time step in seconds since last update
         * @param useMag If false the magnetometer data is ignored (6-DOF mode)
         */
        virtual void update(double gx, double gy, double gz,
                            double ax, double ay, double az,
                            double mx, double my, double mz,
                            double dt, bool useMag = true) = 0;

        /**
         * @brief Return the current orientation quaternion.
         * @param w Scalar (real) component
         * @param i Vector X component
         * @param j Vector Y component
         * @param k Vector Z component
         */
        virtual void getQuaternion(double &w, double &i, double &j, double &k) const = 0;

        /**
         * @brief Reset the filter to the identity quaternion.
         */
        virtual void reset() = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
/**
 * @brief Madgwick AHRS filter (9-DOF and 6-DOF).
 *
 * A computationally efficient gradient-descent based orientation filter.
 * A single tuning parameter β (beta) controls the trade-off between
 * gyroscope integration accuracy and accelerometer/magnetometer correction:
 *
 *   - Larger β  → faster convergence to accelerometer/mag, more noise
 *   - Smaller β → slower convergence, smoother but slower to correct drift
 *
 * Recommended starting value: β = 0.1 for astronomical applications.
 *
 * Reference: Madgwick, S.O.H. (2010)
 *   "An efficient orientation filter for inertial and inertial/magnetic sensor arrays"
 */
class MadgwickFilter : public IMUSensorFusion
{
    public:
        /**
         * @param beta Algorithm gain β. Controls gyroscope measurement error
         *             (rad/s). Default 0.1 is suitable for slow-moving astronomical
         *             applications.
         */
        explicit MadgwickFilter(double beta = 0.1);
        virtual ~MadgwickFilter() = default;

        void update(double gx, double gy, double gz,
                    double ax, double ay, double az,
                    double mx, double my, double mz,
                    double dt, bool useMag = true) override;

        void getQuaternion(double &w, double &i, double &j, double &k) const override;

        void reset() override;

        /** @brief Set the filter gain β. */
        void setBeta(double beta)
        {
            m_beta = beta;
        }
        double getBeta() const
        {
            return m_beta;
        }

    private:
        double m_beta;          ///< Algorithm gain
        double m_q0, m_q1, m_q2, m_q3; ///< Quaternion (w, i, j, k)

        // 9-DOF update (with magnetometer)
        void update9DOF(double gx, double gy, double gz,
                        double ax, double ay, double az,
                        double mx, double my, double mz,
                        double dt);

        // 6-DOF update (without magnetometer)
        void update6DOF(double gx, double gy, double gz,
                        double ax, double ay, double az,
                        double dt);
};

// ─────────────────────────────────────────────────────────────────────────────
/**
 * @brief Mahony AHRS filter (9-DOF and 6-DOF).
 *
 * A complementary filter based on a Proportional-Integral (PI) controller
 * on the SO(3) Lie group. Two tuning parameters:
 *
 *   Kp — Proportional gain: how strongly the accelerometer/magnetometer
 *         corrects the gyroscope integration. Higher → faster convergence.
 *   Ki — Integral gain: compensates for systematic gyroscope bias drift.
 *         Set to 0 to disable integral correction.
 *
 * Recommended starting values: Kp = 2.0, Ki = 0.005.
 *
 * Reference: Mahony, R. et al. (2008)
 *   "Nonlinear Complementary Filters on the Special Orthogonal Group"
 */
class MahonyFilter : public IMUSensorFusion
{
    public:
        /**
         * @param kp Proportional gain. Default 2.0.
         * @param ki Integral gain.     Default 0.005.
         */
        explicit MahonyFilter(double kp = 2.0, double ki = 0.005);
        virtual ~MahonyFilter() = default;

        void update(double gx, double gy, double gz,
                    double ax, double ay, double az,
                    double mx, double my, double mz,
                    double dt, bool useMag = true) override;

        void getQuaternion(double &w, double &i, double &j, double &k) const override;

        void reset() override;

        /** @brief Set proportional gain Kp. */
        void setKp(double kp)
        {
            m_kp = kp;
        }
        double getKp() const
        {
            return m_kp;
        }

        /** @brief Set integral gain Ki. */
        void setKi(double ki)
        {
            m_ki = ki;
        }
        double getKi() const
        {
            return m_ki;
        }

    private:
        double m_kp, m_ki;                    ///< PI gains
        double m_q0, m_q1, m_q2, m_q3;       ///< Quaternion (w, i, j, k)
        double m_integralFBx, m_integralFBy, m_integralFBz; ///< Integral feedback
};

} // namespace INDI
