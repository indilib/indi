/*
    IMU Sensor Fusion - Madgwick and Mahony AHRS filters
    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

    Based on:
    - Madgwick, S.O.H. (2010). "An efficient orientation filter for inertial and
      inertial/magnetic sensor arrays". University of Bristol.
    - Mahony, R. et al. (2008). "Nonlinear Complementary Filters on the Special
      Orthogonal Group". IEEE Transactions on Automatic Control, 53(5):1203-1218.

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

#include "imusensorfusion.h"
#include <cmath>
#include <algorithm>

namespace INDI
{

// ─────────────────────────────────────────────────────────────────────────────
// MadgwickFilter
// ─────────────────────────────────────────────────────────────────────────────

MadgwickFilter::MadgwickFilter(double beta)
    : m_beta(beta), m_q0(1.0), m_q1(0.0), m_q2(0.0), m_q3(0.0)
{
}

void MadgwickFilter::reset()
{
    m_q0 = 1.0;
    m_q1 = 0.0;
    m_q2 = 0.0;
    m_q3 = 0.0;
}

void MadgwickFilter::getQuaternion(double &w, double &i, double &j, double &k) const
{
    w = m_q0;
    i = m_q1;
    j = m_q2;
    k = m_q3;
}

void MadgwickFilter::update(double gx, double gy, double gz,
                            double ax, double ay, double az,
                            double mx, double my, double mz,
                            double dt, bool useMag)
{
    // Sanity check: dt must be positive and reasonable
    if (dt <= 0.0 || dt > 1.0)
        dt = 0.01; // Clamp to 100 Hz default if something went wrong

    // Check if we have valid magnetometer data
    double magNorm = mx * mx + my * my + mz * mz;
    if (useMag && magNorm > 1e-12)
        update9DOF(gx, gy, gz, ax, ay, az, mx, my, mz, dt);
    else
        update6DOF(gx, gy, gz, ax, ay, az, dt);
}

void MadgwickFilter::update9DOF(double gx, double gy, double gz,
                                double ax, double ay, double az,
                                double mx, double my, double mz,
                                double dt)
{
    // Convenience aliases
    double q0 = m_q0, q1 = m_q1, q2 = m_q2, q3 = m_q3;

    // ── Step 1: Normalise accelerometer measurement ──────────────────────────
    double accNorm = std::sqrt(ax * ax + ay * ay + az * az);
    if (accNorm < 1e-12)
    {
        // Degenerate: fall back to gyro-only integration
        update6DOF(gx, gy, gz, 0, 0, 0, dt);
        return;
    }
    ax /= accNorm;
    ay /= accNorm;
    az /= accNorm;

    // ── Step 2: Normalise magnetometer measurement ───────────────────────────
    double magNorm = std::sqrt(mx * mx + my * my + mz * mz);
    mx /= magNorm;
    my /= magNorm;
    mz /= magNorm;

    // ── Step 3: Auxiliary variables ──────────────────────────────────────────
    double q0q1 = q0 * q1, q0q2 = q0 * q2, q0q3 = q0 * q3;
    double q1q1 = q1 * q1, q1q2 = q1 * q2, q1q3 = q1 * q3;
    double q2q2 = q2 * q2, q2q3 = q2 * q3;
    double q3q3 = q3 * q3;

    // ── Step 4: Reference direction of Earth's magnetic field ────────────────
    //   Rotate mag vector into global frame using current quaternion estimate
    double hx = 2.0 * (mx * (0.5 - q2q2 - q3q3) + my * (q1q2 - q0q3)        + mz * (q1q3 + q0q2));
    double hy = 2.0 * (mx * (q1q2 + q0q3)        + my * (0.5 - q1q1 - q3q3)  + mz * (q2q3 - q0q1));

    // Constrain field to lie in x-z plane (ignore hy component)
    double bx = std::sqrt(hx * hx + hy * hy);
    double bz = 2.0 * (mx * (q1q3 - q0q2) + my * (q2q3 + q0q1) + mz * (0.5 - q1q1 - q2q2));

    // ── Step 5: Gradient descent objective function Jacobian ─────────────────
    //   f_g: gravity objective
    double f1 = 2.0 * (q1q3 - q0q2)       - ax;
    double f2 = 2.0 * (q0q1 + q2q3)       - ay;
    double f3 = 2.0 * (0.5 - q1q1 - q2q2) - az;

    //   f_b: magnetic field objective
    double f4 = 2.0 * bx * (0.5 - q2q2 - q3q3) + 2.0 * bz * (q1q3 - q0q2)        - mx;
    double f5 = 2.0 * bx * (q1q2 - q0q3)        + 2.0 * bz * (q0q1 + q2q3)        - my;
    double f6 = 2.0 * bx * (q0q2 + q1q3)        + 2.0 * bz * (0.5 - q1q1 - q2q2)  - mz;

    // ── Step 6: Jacobian transposed × objective ───────────────────────────────
    //   J_g^T * f_g
    double Jt_g0 = -2.0 * q2 * f1 + 2.0 * q1 * f2;
    double Jt_g1 =  2.0 * q3 * f1 + 2.0 * q0 * f2 - 4.0 * q1 * f3;
    double Jt_g2 = -2.0 * q0 * f1 + 2.0 * q3 * f2 - 4.0 * q2 * f3;
    double Jt_g3 =  2.0 * q1 * f1 + 2.0 * q2 * f2;

    //   J_b^T * f_b
    double Jt_b0 = -2.0 * bz * q2              * f4 + (-2.0 * bx * q3 + 2.0 * bz * q1)      * f5 + 2.0 * bx * q2 * f6;
    double Jt_b1 =  2.0 * bz * q3              * f4 + ( 2.0 * bx * q2 + 2.0 * bz * q0)      * f5 +
                    (2.0 * bx * q3 - 4.0 * bz * q1) * f6;
    double Jt_b2 = (-4.0 * bx * q2 - 2.0 * bz * q0) * f4 + ( 2.0 * bx * q1 + 2.0 * bz * q3)      * f5 +
                   (2.0 * bx * q0 - 4.0 * bz * q2) * f6;
    double Jt_b3 = (-4.0 * bx * q3 + 2.0 * bz * q1) * f4 + (-2.0 * bx * q0 + 2.0 * bz * q2)      * f5 + 2.0 * bx * q1 * f6;

    // ── Step 7: Gradient (sum of both objectives) ─────────────────────────────
    double step0 = Jt_g0 + Jt_b0;
    double step1 = Jt_g1 + Jt_b1;
    double step2 = Jt_g2 + Jt_b2;
    double step3 = Jt_g3 + Jt_b3;

    // Normalise gradient
    double stepNorm = std::sqrt(step0 * step0 + step1 * step1 + step2 * step2 + step3 * step3);
    step0 /= stepNorm;
    step1 /= stepNorm;
    step2 /= stepNorm;
    step3 /= stepNorm;

    // ── Step 8: Rate of change of quaternion from gyroscope ───────────────────
    double qDot0 = 0.5 * (-q1 * gx - q2 * gy - q3 * gz);
    double qDot1 = 0.5 * ( q0 * gx + q2 * gz - q3 * gy);
    double qDot2 = 0.5 * ( q0 * gy - q1 * gz + q3 * gx);
    double qDot3 = 0.5 * ( q0 * gz + q1 * gy - q2 * gx);

    // ── Step 9: Apply feedback and integrate ─────────────────────────────────
    qDot0 -= m_beta * step0;
    qDot1 -= m_beta * step1;
    qDot2 -= m_beta * step2;
    qDot3 -= m_beta * step3;

    q0 += qDot0 * dt;
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;

    // ── Step 10: Normalise quaternion ─────────────────────────────────────────
    double qNorm = std::sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    m_q0 = q0 / qNorm;
    m_q1 = q1 / qNorm;
    m_q2 = q2 / qNorm;
    m_q3 = q3 / qNorm;
}

void MadgwickFilter::update6DOF(double gx, double gy, double gz,
                                double ax, double ay, double az,
                                double dt)
{
    double q0 = m_q0, q1 = m_q1, q2 = m_q2, q3 = m_q3;

    // ── Step 1: Rate of change from gyroscope ────────────────────────────────
    double qDot0 = 0.5 * (-q1 * gx - q2 * gy - q3 * gz);
    double qDot1 = 0.5 * ( q0 * gx + q2 * gz - q3 * gy);
    double qDot2 = 0.5 * ( q0 * gy - q1 * gz + q3 * gx);
    double qDot3 = 0.5 * ( q0 * gz + q1 * gy - q2 * gx);

    // ── Step 2: Accelerometer correction (if valid) ──────────────────────────
    double accNorm = std::sqrt(ax * ax + ay * ay + az * az);
    if (accNorm > 1e-12)
    {
        ax /= accNorm;
        ay /= accNorm;
        az /= accNorm;

        // Gravity objective function
        double f1 = 2.0 * (q1 * q3 - q0 * q2) - ax;
        double f2 = 2.0 * (q0 * q1 + q2 * q3) - ay;
        double f3 = 2.0 * (0.5 - q1 * q1 - q2 * q2) - az;

        // Jacobian transposed × objective
        double Jt0 = -2.0 * q2 * f1 + 2.0 * q1 * f2;
        double Jt1 =  2.0 * q3 * f1 + 2.0 * q0 * f2 - 4.0 * q1 * f3;
        double Jt2 = -2.0 * q0 * f1 + 2.0 * q3 * f2 - 4.0 * q2 * f3;
        double Jt3 =  2.0 * q1 * f1 + 2.0 * q2 * f2;

        double stepNorm = std::sqrt(Jt0 * Jt0 + Jt1 * Jt1 + Jt2 * Jt2 + Jt3 * Jt3);
        qDot0 -= m_beta * Jt0 / stepNorm;
        qDot1 -= m_beta * Jt1 / stepNorm;
        qDot2 -= m_beta * Jt2 / stepNorm;
        qDot3 -= m_beta * Jt3 / stepNorm;
    }

    // ── Step 3: Integrate and normalise ──────────────────────────────────────
    q0 += qDot0 * dt;
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;

    double qNorm = std::sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    m_q0 = q0 / qNorm;
    m_q1 = q1 / qNorm;
    m_q2 = q2 / qNorm;
    m_q3 = q3 / qNorm;
}

// ─────────────────────────────────────────────────────────────────────────────
// MahonyFilter
// ─────────────────────────────────────────────────────────────────────────────

MahonyFilter::MahonyFilter(double kp, double ki)
    : m_kp(kp), m_ki(ki),
      m_q0(1.0), m_q1(0.0), m_q2(0.0), m_q3(0.0),
      m_integralFBx(0.0), m_integralFBy(0.0), m_integralFBz(0.0)
{
}

void MahonyFilter::reset()
{
    m_q0 = 1.0;
    m_q1 = 0.0;
    m_q2 = 0.0;
    m_q3 = 0.0;
    m_integralFBx = 0.0;
    m_integralFBy = 0.0;
    m_integralFBz = 0.0;
}

void MahonyFilter::getQuaternion(double &w, double &i, double &j, double &k) const
{
    w = m_q0;
    i = m_q1;
    j = m_q2;
    k = m_q3;
}

void MahonyFilter::update(double gx, double gy, double gz,
                          double ax, double ay, double az,
                          double mx, double my, double mz,
                          double dt, bool useMag)
{
    // Sanity check: dt must be positive and reasonable
    if (dt <= 0.0 || dt > 1.0)
        dt = 0.01;

    double q0 = m_q0, q1 = m_q1, q2 = m_q2, q3 = m_q3;

    double halfex = 0.0, halfey = 0.0, halfez = 0.0;

    // ── Step 1: Use magnetometer correction if available and valid ────────────
    double magNorm = mx * mx + my * my + mz * mz;
    if (useMag && magNorm > 1e-12)
    {
        // Normalise accelerometer measurement
        double accNorm = std::sqrt(ax * ax + ay * ay + az * az);
        if (accNorm > 1e-12)
        {
            double axn = ax / accNorm, ayn = ay / accNorm, azn = az / accNorm;

            // Normalise magnetometer measurement
            double mxn = mx / std::sqrt(magNorm);
            double myn = my / std::sqrt(magNorm);
            double mzn = mz / std::sqrt(magNorm);

            // Reference direction of Earth's magnetic field in global frame
            double hx = 2.0 * (mxn * (0.5 - q2 * q2 - q3 * q3) + myn * (q1 * q2 - q0 * q3) + mzn * (q1 * q3 + q0 * q2));
            double hy = 2.0 * (mxn * (q1 * q2 + q0 * q3)        + myn * (0.5 - q1 * q1 - q3 * q3) + mzn * (q2 * q3 - q0 * q1));
            double bx = std::sqrt(hx * hx + hy * hy);
            double bz = 2.0 * (mxn * (q1 * q3 - q0 * q2) + myn * (q2 * q3 + q0 * q1) + mzn * (0.5 - q1 * q1 - q2 * q2));

            // Estimated direction of gravity and magnetic field
            double vx = 2.0 * (q1 * q3 - q0 * q2);
            double vy = 2.0 * (q0 * q1 + q2 * q3);
            double vz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;
            double wx = 2.0 * bx * (0.5 - q2 * q2 - q3 * q3) + 2.0 * bz * (q1 * q3 - q0 * q2);
            double wy = 2.0 * bx * (q1 * q2 - q0 * q3)        + 2.0 * bz * (q0 * q1 + q2 * q3);
            double wz = 2.0 * bx * (q0 * q2 + q1 * q3)        + 2.0 * bz * (0.5 - q1 * q1 - q2 * q2);

            // Error is cross product between estimated and measured direction
            halfex = (ayn * vz - azn * vy) + (myn * wz - mzn * wy);
            halfey = (azn * vx - axn * vz) + (mzn * wx - mxn * wz);
            halfez = (axn * vy - ayn * vx) + (mxn * wy - myn * wx);
        }
    }
    else
    {
        // ── Step 2: Accelerometer-only correction (6-DOF) ─────────────────────
        double accNorm = std::sqrt(ax * ax + ay * ay + az * az);
        if (accNorm > 1e-12)
        {
            double axn = ax / accNorm, ayn = ay / accNorm, azn = az / accNorm;

            // Estimated direction of gravity from current quaternion
            double vx = 2.0 * (q1 * q3 - q0 * q2);
            double vy = 2.0 * (q0 * q1 + q2 * q3);
            double vz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

            halfex = ayn * vz - azn * vy;
            halfey = azn * vx - axn * vz;
            halfez = axn * vy - ayn * vx;
        }
    }

    // ── Step 3: Apply PI controller ──────────────────────────────────────────
    if (m_ki > 0.0)
    {
        m_integralFBx += m_ki * halfex * dt;
        m_integralFBy += m_ki * halfey * dt;
        m_integralFBz += m_ki * halfez * dt;
    }
    else
    {
        m_integralFBx = 0.0;
        m_integralFBy = 0.0;
        m_integralFBz = 0.0;
    }

    // Apply integral and proportional feedback to gyro measurement
    gx += m_kp * halfex + m_integralFBx;
    gy += m_kp * halfey + m_integralFBy;
    gz += m_kp * halfez + m_integralFBz;

    // ── Step 4: Integrate rate of change of quaternion ────────────────────────
    double qDot0 = 0.5 * (-q1 * gx - q2 * gy - q3 * gz);
    double qDot1 = 0.5 * ( q0 * gx + q2 * gz - q3 * gy);
    double qDot2 = 0.5 * ( q0 * gy - q1 * gz + q3 * gx);
    double qDot3 = 0.5 * ( q0 * gz + q1 * gy - q2 * gx);

    q0 += qDot0 * dt;
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;

    // ── Step 5: Normalise quaternion ─────────────────────────────────────────
    double qNorm = std::sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    m_q0 = q0 / qNorm;
    m_q1 = q1 / qNorm;
    m_q2 = q2 / qNorm;
    m_q3 = q3 / qNorm;
}

} // namespace INDI
