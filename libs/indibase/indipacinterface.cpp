/*
    PAC Interface (Polar Alignment Correction)
    Copyright (C) 2026 Joaquin Rodriguez (jrhuerta@gmail.com)

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

#include "indipacinterface.h"
#include "defaultdevice.h"

#include <cstring>

namespace INDI
{

PACInterface::PACInterface(DefaultDevice *defaultDevice)
    : m_DefaultDevice(defaultDevice)
{
}

void PACInterface::initProperties(const char *group)
{
    CorrectionSP[CORRECTION_START].fill("CORRECT", "Correct", ISS_OFF);
    CorrectionSP[CORRECTION_ABORT].fill("ABORT", "Abort", ISS_OFF);
    CorrectionSP.fill(m_DefaultDevice->getDeviceName(), "ALIGNMENT_CORRECTION", "Alignment Correction",
                      group, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    CorrectionErrorNP[ERROR_AZ].fill("AZ_ERROR", "Azimuth Error (deg)", "%.4f", -10, 10, 0, 0);
    CorrectionErrorNP[ERROR_ALT].fill("ALT_ERROR", "Altitude Error (deg)", "%.4f", -10, 10, 0, 0);
    CorrectionErrorNP.fill(m_DefaultDevice->getDeviceName(), "ALIGNMENT_CORRECTION_ERROR", "Correction Error",
                           group, IP_RW, 0, IPS_IDLE);

    CorrectionStatusLP[0].fill("STATUS", "Status", IPS_IDLE);
    CorrectionStatusLP.fill(m_DefaultDevice->getDeviceName(), "ALIGNMENT_CORRECTION_STATUS", "Correction Status",
                            group, IPS_IDLE);
}

bool PACInterface::updateProperties()
{
    if (m_DefaultDevice->isConnected())
    {
        m_DefaultDevice->defineProperty(CorrectionErrorNP);
        m_DefaultDevice->defineProperty(CorrectionSP);
        m_DefaultDevice->defineProperty(CorrectionStatusLP);
    }
    else
    {
        m_DefaultDevice->deleteProperty(CorrectionErrorNP);
        m_DefaultDevice->deleteProperty(CorrectionSP);
        m_DefaultDevice->deleteProperty(CorrectionStatusLP);
    }

    return true;
}

bool PACInterface::processSwitch(const char *dev, const char *name,
                                                  ISState *states, char *names[], int n)
{
    INDI_UNUSED(dev);

    if (CorrectionSP.isNameMatch(name))
    {
        CorrectionSP.update(states, names, n);

        if (CorrectionSP[CORRECTION_START].getState() == ISS_ON)
        {
            double azError  = CorrectionErrorNP[ERROR_AZ].getValue();
            double altError = CorrectionErrorNP[ERROR_ALT].getValue();

            auto state = StartCorrection(azError, altError);
            CorrectionSP.setState(state);

            if (state == IPS_BUSY)
            {
                CorrectionStatusLP[0].setState(IPS_BUSY);
            }
            else if (state == IPS_OK)
            {
                CorrectionStatusLP[0].setState(IPS_OK);
                CorrectionSP.reset();
            }
            else
            {
                CorrectionStatusLP[0].setState(IPS_ALERT);
                CorrectionSP.reset();
            }
        }
        else if (CorrectionSP[CORRECTION_ABORT].getState() == ISS_ON)
        {
            auto state = AbortCorrection();
            CorrectionSP.setState(state);
            CorrectionSP.reset();

            if (state == IPS_OK)
                CorrectionStatusLP[0].setState(IPS_IDLE);
            else
                CorrectionStatusLP[0].setState(IPS_ALERT);
        }

        CorrectionSP.apply();
        CorrectionStatusLP.apply();
        return true;
    }

    return false;
}

bool PACInterface::processNumber(const char *dev, const char *name,
                                                  double values[], char *names[], int n)
{
    INDI_UNUSED(dev);

    if (CorrectionErrorNP.isNameMatch(name))
    {
        CorrectionErrorNP.update(values, names, n);
        CorrectionErrorNP.setState(IPS_OK);
        CorrectionErrorNP.apply();
        return true;
    }

    return false;
}

IPState PACInterface::StartCorrection(double azError, double altError)
{
    INDI_UNUSED(azError);
    INDI_UNUSED(altError);
    return IPS_ALERT;
}

IPState PACInterface::AbortCorrection()
{
    return IPS_ALERT;
}

}
