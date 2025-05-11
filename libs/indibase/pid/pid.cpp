/**
 * Copyright 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
 * Copyright (c) 2020 Philip Salmony
 * Copyright 2019 Bradley J. Snyder <snyder.bradleyj@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <iostream>
#include <cmath>
#include "pid.h"

using namespace std;

class PIDImpl
{
    public:
        PIDImpl( double dt, double max, double min, double Kp, double Kd, double Ki );
        ~PIDImpl();
        void setIntegratorLimits(double min, double max)
        {
            m_IntegratorMin = min;
            m_IntegratorMax = max;
        }
        void setTau(double value)
        {
            m_Tau = value;
        }
        double calculate( double setpoint, double measurement );
        double propotionalTerm() const
        {
            return m_PropotionalTerm;
        }
        double integralTerm() const
        {
            return m_IntegralTerm;
        }
        double derivativeTerm() const
        {
            return m_DerivativeTerm;
        }

        void setKp(double Kp)
        {
            m_Kp = Kp;
        }
        void setKi(double Ki)
        {
            m_Ki = Ki;
        }
        void setKd(double Kd)
        {
            m_Kd = Kd;
        }
        void getGains(double &Kp, double &Ki, double &Kd) const
        {
            Kp = m_Kp;
            Ki = m_Ki;
            Kd = m_Kd;
        }

    private:
        // Sample Time
        double m_T {1};
        // Derivative Low-Pass filter time constant */
        double m_Tau {2};

        // Output limits
        double m_Max {0};
        double m_Min {0};

        // Integrator Limits
        double m_IntegratorMin {0};
        double m_IntegratorMax {0};

        // Gains
        double m_Kp {0};
        double m_Kd {0};
        double m_Ki {0};

        // Controller volatile data
        double m_PreviousError {0};
        double m_PreviousMeasurement {0};

        // Terms
        double m_PropotionalTerm {0};
        double m_IntegralTerm {0};
        double m_DerivativeTerm {0};

};


PID::PID( double dt, double max, double min, double Kp, double Kd, double Ki )
{
    pimpl = new PIDImpl(dt, max, min, Kp, Kd, Ki);
}
void PID::setIntegratorLimits(double min, double max)
{
    pimpl->setIntegratorLimits(min, max);
}
void PID::setTau(double value)
{
    pimpl->setTau(value);
}
double PID::calculate( double setpoint, double pv )
{
    return pimpl->calculate(setpoint, pv);
}
double PID::propotionalTerm() const
{
    return pimpl->propotionalTerm();
}
double PID::integralTerm() const
{
    return pimpl->integralTerm();
}
double PID::derivativeTerm() const
{
    return pimpl->derivativeTerm();
}

void PID::setKp(double Kp)
{
    pimpl->setKp(Kp);
}
void PID::setKi(double Ki)
{
    pimpl->setKi(Ki);
}
void PID::setKd(double Kd)
{
    pimpl->setKd(Kd);
}
void PID::getGains(double &Kp, double &Ki, double &Kd) const
{
    pimpl->getGains(Kp, Ki, Kd);
}

PID::~PID()
{
    delete pimpl;
}


/**
 * Implementation
 */
PIDImpl::PIDImpl( double dt, double max, double min, double Kp, double Kd, double Ki ) :
    m_T(dt),
    m_Max(max),
    m_Min(min),
    m_Kp(Kp),
    m_Kd(Kd),
    m_Ki(Ki)
{
}

double PIDImpl::calculate(double setpoint, double measurement )
{
    // Calculate error
    double error = setpoint - measurement;

    // Proportional term
    m_PropotionalTerm = m_Kp * error;

    // Integral term
    m_IntegralTerm = m_IntegralTerm + 0.5 * m_Ki * m_T * (error + m_PreviousError);

    // Clamp Integral
    if (m_IntegratorMin || m_IntegratorMax)
        m_IntegralTerm = std::min(m_IntegratorMax, std::max(m_IntegratorMin, m_IntegralTerm));

    // Derivative term (N.B. on measurement NOT error)
    m_DerivativeTerm = -(2.0f * m_Kd * (measurement - m_PreviousMeasurement) + (2.0f * m_Tau - m_T) * m_DerivativeTerm)
                       / (2.0f * m_Tau + m_T);

    // Calculate total output
    double output = m_PropotionalTerm + m_IntegralTerm + m_DerivativeTerm;

    // Clamp Output
    output = std::min(m_Max, std::max(m_Min, output));

    // Save error to previous error
    m_PreviousError = error;
    m_PreviousMeasurement = measurement;

    return output;
}

PIDImpl::~PIDImpl()
{
}
