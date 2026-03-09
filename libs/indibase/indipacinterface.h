/*
    PAC Interface (Polar Alignment Correction)
    Copyright (C) 2026 Joaquin Rodriguez (jrhuerta@gmail.com)
    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
 * In addition to fully-automated correction, the interface exposes manual step controls via the
 * PAC_MANUAL_ADJUSTMENT number property.  A client can nudge the mount by a signed number of
 * degrees on either axis independently:
 *
 * \b Sign convention for manual adjustments:
 *   - Azimuth (PAC_MANUAL_AZ_STEP):  positive value → East,  negative value → West
 *   - Altitude (PAC_MANUAL_ALT_STEP): positive value → North, negative value → South
 *
 * \b Relationship between errors and corrections:
 *   - A positive azimuth error means the polar axis is displaced to the East, so the correction
 *     moves the mount West: MoveAZ(−azError).
 *   - A positive altitude error means the polar axis is too high, so the correction moves the
 *     mount South: MoveALT(−altError).
 *
 * The default implementation of StartCorrection() uses MoveAZ() and MoveALT() so that any
 * driver implementing those two primitives automatically gains automated correction.
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
        /** Indices for the CorrectionSP switch property. */
        enum
        {
            CORRECTION_START,
            CORRECTION_ABORT
        };

        /** Indices for the CorrectionErrorNP number property. */
        enum
        {
            ERROR_AZ,
            ERROR_ALT
        };

        /** Indices for the ManualAdjustmentNP number property. */
        enum
        {
            MANUAL_AZ,   ///< Azimuth step in degrees: positive = East, negative = West
            MANUAL_ALT   ///< Altitude step in degrees: positive = North, negative = South
        };

    protected:
        explicit PACInterface(DefaultDevice *device);
        virtual ~PACInterface() = default;

        /**
         * @brief Start an automated alignment correction.
         *
         * The default implementation translates the errors into movement commands:
         *   - Calls MoveAZ(−azError)  (positive azError = eastward polar-axis error → West correction)
         *   - Calls MoveALT(−altError) (positive altError = altitude too high → South correction)
         *
         * Drivers may override this method to perform the correction in a single hardware command
         * rather than relying on the individual MoveAZ / MoveALT primitives.
         *
         * @param azError  Azimuth error in degrees (positive = polar axis displaced East).
         * @param altError Altitude error in degrees (positive = polar axis too high).
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
         * @brief Move the azimuth axis by the given number of degrees.
         *
         * Sign convention: positive = East, negative = West.
         *
         * The default implementation returns IPS_ALERT.  Drivers that support motorised
         * azimuth adjustment should override this method.
         *
         * @param degrees Signed step size in degrees.
         * @return IPS_OK if completed immediately, IPS_BUSY if in progress, IPS_ALERT on error.
         */
        virtual IPState MoveAZ(double degrees);

        /**
         * @brief Move the altitude axis by the given number of degrees.
         *
         * Sign convention: positive = North (increase altitude), negative = South (decrease altitude).
         *
         * The default implementation returns IPS_ALERT.  Drivers that support motorised
         * altitude adjustment should override this method.
         *
         * @param degrees Signed step size in degrees.
         * @return IPS_OK if completed immediately, IPS_BUSY if in progress, IPS_ALERT on error.
         */
        virtual IPState MoveALT(double degrees);

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

        /**
         * Alignment correction error set by the client (e.g. Ekos) with the measured
         * azimuth and altitude polar alignment errors before triggering ALIGNMENT_CORRECTION.
         *
         * \b Important: Drivers MUST NOT update or re-broadcast this property while a
         * correction is in progress.  It is read by the driver exactly once — at correction
         * start — and must remain stable until the client sets it again for the next cycle.
         * Broadcasting intermediate "remaining correction" values through this property
         * confuses any device that snoops on it (e.g. a telescope simulator) because those
         * devices cannot distinguish a new client-commanded error from an internal progress
         * update.
         */
        INDI::PropertyNumber CorrectionErrorNP {2};
        INDI::PropertyLight  CorrectionStatusLP {1};

        /**
         * Manual adjustment property.
         * Element [MANUAL_AZ]  : azimuth step in degrees  (+East / −West).
         * Element [MANUAL_ALT] : altitude step in degrees (+North / −South).
         * Writing a non-zero value to either element immediately triggers the corresponding move.
         */
        INDI::PropertyNumber ManualAdjustmentNP {2};

    private:
        DefaultDevice *m_DefaultDevice { nullptr };
};

}

using PACI = INDI::PACInterface;
