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

#pragma once

#include "indibase.h"
#include "indidriver.h"
#include "indipropertyswitch.h"
#include "indipropertynumber.h"
#include "indipropertylight.h"
#include <cstdint>

/**
 * \class PACInterface
 * \brief Provides interface to implement automated polar alignment correction.
 *
 * A client sets the measured alignment error via the ALIGNMENT_CORRECTION_ERROR number property
 * (azimuth and altitude offsets in degrees), then commands a correction through the
 * ALIGNMENT_CORRECTION switch property.  The device applies the mechanical correction and
 * reports progress through the ALIGNMENT_CORRECTION_STATUS light property.
 *
 * The interface can be embedded in any device (typically a mount) via multiple inheritance,
 * or used as the basis of a standalone alignment-correction device.
 *
 * \e IMPORTANT: initProperties() must be called before any other function to initialize the
 *               alignment correction properties.
 * \e IMPORTANT: updateProperties() must be called in your driver's updateProperties().
 * \e IMPORTANT: processSwitch() must be called in your driver's ISNewSwitch().
 * \e IMPORTANT: processNumber() must be called in your driver's ISNewNumber().
 *
 * \author Joaquin Rodriguez
 */

namespace INDI
{

class PACInterface
{
    public:
        enum
        {
            CORRECTION_START,
            CORRECTION_ABORT
        };

        enum
        {
            ERROR_AZ,
            ERROR_ALT
        };

    protected:
        explicit PACInterface(DefaultDevice *device);
        virtual ~PACInterface() = default;

        /**
         * @brief Start an alignment correction.  Must be implemented by the driver.
         * @param azError  Azimuth error in degrees (positive = clockwise when seen from above).
         * @param altError Altitude error in degrees (positive = too high).
         * @return IPS_OK if the correction completed immediately,
         *         IPS_BUSY if it is in progress (driver must update CorrectionStatusLP when done),
         *         IPS_ALERT on error.
         */
        virtual IPState StartCorrection(double azError, double altError);

        /**
         * @brief Abort a correction that is in progress.  Must be implemented by the driver.
         * @return IPS_OK if aborted successfully, IPS_ALERT on error.
         */
        virtual IPState AbortCorrection();

        /**
         * @brief Initialize alignment correction properties.
         *        Call this from your driver's initProperties().
         * @param group Group or tab name for the properties.
         */
        void initProperties(const char *group);

        /**
         * @brief Define or delete properties based on connection state.
         *        Call this from your driver's updateProperties().
         * @return true on success.
         */
        bool updateProperties();

        /** @brief Process alignment correction switch properties. */
        bool processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        /** @brief Process alignment correction number properties. */
        bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);

        INDI::PropertySwitch CorrectionSP {2};
        INDI::PropertyNumber CorrectionErrorNP {2};
        INDI::PropertyLight  CorrectionStatusLP {1};

    private:
        DefaultDevice *m_DefaultDevice { nullptr };
};

}

using PACI = INDI::PACInterface;
