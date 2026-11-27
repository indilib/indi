/*
    OnStepX INDI Driver — Site/time helper (mount binary only)
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

    Plain C++ helper -- no INDI base class.  Non-owning borrowed pointers.
    Implements updateLocation() and updateTime() for OnStepXMount.

    Protocol (OnStepX v10.24c):
      :St[±DD:MM:SS.S]# — set latitude, North positive (reply '1')
      :Sg[DDD:MM:SS.S]# — set longitude, West positive (INDI convention is East; inverted here)
      :Sv[n]#           — set elevation in metres (reply '1')
      :Gt#              — get latitude (sexagesimal, North positive)
      :Gg#              — get longitude (sexagesimal, West positive)
      :Gev#             — get elevation (integer, metres)
      :SG[±HH.H]#       — set UTC offset, hours East of UTC (reply '1')
      :SL[HH:MM:SS]#    — set local time (reply '1')
      :SC[MM/DD/YY]#    — set local date (reply '1')
      :GM# :GN# :GO# :GP# — get site names 1-4 (string, no '#' in reply)
      :SM[name]# :SN# :SO# :SP# — set site name for slot 1-4 (reply '1')
                 up to 15 characters; the firmware drops spaces, so '_' is sent instead
      :W[n]#            — select active site (n=1..4, blind send)

    INDI Properties (Site Management tab, with INDI's location and time):
      OSX_SITE_SELECT  IP_RW  ISR_1OFMANY  Switch[4]: Site 1..4 (labels from :GM-GP#)
      OSX_SITE_NAME    IP_RW  Text[1]:     Active site name (editable)
*/

#pragma once

#include <libnova/ln_types.h>
#include <indipropertyswitch.h>
#include <indipropertytext.h>

#include <defaultdevice.h>

class OnStepXComm;
namespace INDI
{
class DefaultDevice;
}

// Plain C++ -- no INDI base class.  Non-owning borrowed pointers.
class OnStepXSite
{
    public:
        void setComm(OnStepXComm *comm)
        {
            m_comm = comm;
        }
        void setDevice(INDI::DefaultDevice *dev)
        {
            m_dev  = dev;
        }

        // INDI property lifecycle
        void initProperties();
        void updateProperties(bool connected);
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);
        bool handleText(const char *name, char *texts[], char *names[], int n);
        void saveConfig(FILE *fp);

        // Write latitude / longitude / elevation to the controller.
        //   latitude  : decimal degrees, -90..+90  (North positive)
        //   longitude : decimal degrees, 0..360    (East positive, INDI convention)
        //   elevation : metres above sea level
        bool writeLocation(double latitude, double longitude, double elevation);

        // Read back location from the controller.
        //   latitude  : decimal degrees, -90..+90
        //   longitude : decimal degrees, 0..360  (East positive, converted from OnStepX West-positive)
        bool readLocation(double &latitude, double &longitude, double &elevation);

        // Push UTC date/time + timezone offset to the controller.
        //   utc        : broken-down UTC date/time (libnova)
        //   utc_offset : hours east of UTC (e.g. +10 for AEST, -5 for EST)
        bool writeTime(const ln_date *utc, double utc_offset);

        // Current active site index (1-based), updated by selectSite().
        int activeSite() const
        {
            return m_activeSite;
        }

        // After handleSwitch() returns true for a site change, these hold the
        // newly active site's location so OnStepXMount can push to GEOGRAPHIC_COORD.
        double lastLat()  const
        {
            return m_lastLat;
        }
        double lastLon()  const
        {
            return m_lastLon;
        }
        double lastElev() const
        {
            return m_lastElev;
        }
        bool   locationUpdated() const
        {
            return m_locationUpdated;
        }
        void   clearLocationUpdated()
        {
            m_locationUpdated = false;
        }

        // Civil date/time <-> seconds since 1970-01-01 (Gregorian, no leap
        // seconds).  Seconds are rounded to the nearest whole second.
        static long long civilToEpochSeconds(int y, int mo, int d, int h, int mi, double s);
        static void epochSecondsToCivil(long long t, int &y, int &mo, int &d,
                                        int &h, int &mi, int &s);

        INDI::PropertyText &siteNameTP()
        {
            return m_siteNameTP;
        }

    private:
        OnStepXComm        *m_comm { nullptr };
        INDI::DefaultDevice *m_dev { nullptr };

        INDI::PropertySwitch m_siteSelectSP { 4 };  // OSX_SITE_SELECT
        INDI::PropertyText   m_siteNameTP   { 1 };  // OSX_SITE_NAME

        int    m_activeSite      { 1 };     // 1-based active site index
        bool   m_locationUpdated { false }; // set true after a site-select location read
        double m_lastLat         { 0.0 };
        double m_lastLon         { 0.0 };
        double m_lastElev        { 0.0 };

        // Query :GM#/:GN#/:GO#/:GP#; update switch labels and name field.
        void readSiteNames();

        // Send :W[n]#, re-read location, update name field.
        // Returns the callback needed by OnStepXMount to refresh GEOGRAPHIC_COORD.
        bool selectSite(int n, double &lat, double &lon, double &elev);

        const char* getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }

        void normaliseDMS(int *d, int *m, double *s);
        void readActiveSite();
};
