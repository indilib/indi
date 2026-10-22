/*******************************************************************************
  Copyright(c) 2026 Jasem Mutlaq. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU Library General Public License
  version 2 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indicom.h"
#include "indipropertytext.h"

#include <libnova/julian_day.h>
#include <libnova/sidereal_time.h>

#include <cstring>

/**
 * @brief The SimulatorClock class is the clock of a camera simulator: the system clock, shifted to the time
 * the client last wrote to TIME_UTC.
 *
 * The client (KStars) sets TIME_UTC on every device that has it, so the camera simulators follow the same
 * clock as the telescope simulator. With no TIME_UTC set, the clock is the system clock.
 */
class SimulatorClock
{
    public:
        void initProperties(const char *deviceName, const char *group)
        {
            TimeTP[UTC].fill("UTC", "UTC Time", nullptr);
            TimeTP[OFFSET].fill("OFFSET", "UTC Offset", nullptr);
            TimeTP.fill(deviceName, "TIME_UTC", "UTC", group, IP_RW, 60, IPS_IDLE);
        }

        // Defines TIME_UTC while connected. A new connection starts from the system clock.
        void updateProperties(INDI::DefaultDevice *device, bool connected)
        {
            if (connected)
            {
                m_OffsetDays = 0;
                device->defineProperty(TimeTP);
            }
            else
                device->deleteProperty(TimeTP);
        }

        // Handles a new TIME_UTC. Returns false when the property is not TIME_UTC.
        bool processText(const char *name, char *texts[], char *names[], int n)
        {
            if (!TimeTP.isNameMatch(name))
                return false;

            const char *utc = nullptr;
            const char *offset = nullptr;
            for (int i = 0; i < n; i++)
            {
                if (!strcmp(names[i], "UTC"))
                    utc = texts[i];
                else if (!strcmp(names[i], "OFFSET"))
                    offset = texts[i];
            }

            ln_date utcDate;
            if (utc == nullptr || extractISOTime(utc, &utcDate) == -1)
            {
                TimeTP.setState(IPS_ALERT);
                TimeTP.apply("Date/Time is invalid: %s.", utc ? utc : "");
                return true;
            }

            m_OffsetDays = ln_get_julian_day(&utcDate) - ln_get_julian_from_sys();
            TimeTP[UTC].setText(utc);
            if (offset)
                TimeTP[OFFSET].setText(offset);
            TimeTP.setState(IPS_OK);
            TimeTP.apply();
            return true;
        }

        // Julian date of the simulated clock
        double julianDate() const
        {
            return ln_get_julian_from_sys() + m_OffsetDays;
        }

        // Local sidereal time in hours at this longitude (degrees, east positive)
        double localSiderealTime(double longitude) const
        {
            return range24(ln_get_apparent_sidereal_time(julianDate()) + longitude / 15.0);
        }

    private:
        enum { UTC, OFFSET };
        INDI::PropertyText TimeTP {2};
        // Days to add to the system clock
        double m_OffsetDays {0};
};
