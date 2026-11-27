/*
    OnStepX INDI Driver — Limits and Home helper (mount binary only)
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

    Protocol (OnStepX v10.24c):
      :Gh#         — get altitude limit (integer, deg)
      :Sh[n]#      — set altitude limit (reply '1')
      :Go#         — get overhead limit (integer, deg)
      :So[n]#      — set overhead limit (reply '1')
      :GXE9#       — get meridian limit East (integer, minutes past the meridian)
      :GXEA#       — get meridian limit West (integer, minutes past the meridian)
      :SXE9,[n]#   — set meridian limit East (reply '1')
      :SXEA,[n]#   — set meridian limit West (reply '1')
                     Meridian limits apply to a GEM only (firmware Limits.cpp).
      :hC#         — find home (no reply, even on failure)
      :hF#         — set current position as home (no reply, even on failure)
      :hA0/1#      — home-at-boot: 0=off, 1=on (no reply)
      :hC1,[n]#    — home offset axis 1 in arcsec (requires hasHomeSense; no reply)
      :hC2,[n]#    — home offset axis 2 in arcsec (requires hasHomeSense; no reply)
      :h?#         — "hasSense,offset1,offset2" (offsets in arcsec)

    Find home and set home are INDI's standard TELESCOPE_HOME (Main Control),
    which OnStepXMount::ExecuteHomeAction() routes to homeFind()/homeSet().

    INDI Properties:
      Limits tab:
        HORIZON_LIMITS      IP_RW  2 numbers: Minimum Alt / Maximum Alt (deg)
        MERIDIAN_LIMITS     IP_RW  2 numbers: E of Meridian / W of Meridian (min); GEM only
      Site Management tab (home setup, beside INDI's park setup):
        AUTO_HOME           IP_RW  ISR_1OFMANY  2 switches: Enabled / Disabled
        HOME_OFFSETS        IP_RW  2 numbers: Axis 1 / Axis 2 (arcsec; hasHomeSense only)
      Motion Control tab:
        OSX_MOUNT_BACKLASH  IP_RW  2 numbers: Axis1 (RA/Az) / Axis2 (Dec/Alt) arcsec
        Read:  :%BR# -> Axis1,  :%BD# -> Axis2
        Write: :$BR[n]# / :$BD[n]# (each expects '1' reply)
*/

#pragma once

#include <defaultdevice.h>
#include <indipropertyswitch.h>
#include <indipropertynumber.h>

class OnStepXComm;
namespace INDI
{
class DefaultDevice;
}

class OnStepXLimits
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

        // Called from OnStepXMount::initProperties().
        void initProperties();

        // Called from OnStepXMount::updateProperties() after connect/disconnect.
        // isGem: define the meridian limits (the firmware applies them to a GEM only).
        void updateProperties(bool connected, bool hasHomeSense, bool isGem = true);

        // Called from OnStepXMount::ISNewSwitch().
        // Returns true if the event was consumed.
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);

        // Called from OnStepXMount::ISNewNumber().
        // Returns true if the event was consumed.
        bool handleNumber(const char *name, double values[], char *names[], int n);

        // Called from OnStepXMount::saveConfigItems().
        void saveConfig(FILE *fp);

        // Read horizon and meridian limits from the controller into properties.
        bool readLimits();

        // Execute a home action: FIND -> :hC#, SET -> :hF#
        // GO is not a separate command on OnStepX -- homing is always FIND.
        bool homeFind();
        bool homeSet();

        // Send auto-boot setting: enabled -> :hA1# / disabled -> :hA0#
        bool setAutoHome(bool enabled);

        // Mirror the firmware's auto-home-at-boot flag from the status poll,
        // so a change from the hand controller or web UI shows here too.
        void syncAutoHome(bool enabled);

        // Write home axis offsets (axis1, axis2) via :hC1,[n]# and :hC2,[n]#
        bool writeHomeOffsets(double axis1, double axis2);

        // Property references (used by OnStepXMount for state updates)
        INDI::PropertySwitch &autoBootSP()
        {
            return m_autoBootSP;
        }
        INDI::PropertyNumber &homeOffsetNP()
        {
            return m_homeOffsetNP;
        }
        INDI::PropertyNumber &horizonLimitNP()
        {
            return m_horizonLimitNP;
        }
        INDI::PropertyNumber &meridianLimitNP()
        {
            return m_meridianLimitNP;
        }
        INDI::PropertyNumber &backlashNP()
        {
            return m_backlashNP;
        }

    private:
        OnStepXComm        *m_comm { nullptr };
        INDI::DefaultDevice *m_dev { nullptr };
        bool                 m_hasHomeSense { false };
        bool                 m_isGem { true };

        INDI::PropertySwitch m_autoBootSP    { 2 };  // Auto-home on boot: On/Off
        INDI::PropertyNumber m_homeOffsetNP  { 2 };  // Axis1, Axis2 home offsets
        INDI::PropertyNumber m_horizonLimitNP  { 2 };// Min alt, Max alt (degrees)
        INDI::PropertyNumber m_meridianLimitNP { 2 };// East, West meridian (minutes)
        INDI::PropertyNumber m_backlashNP      { 2 };// Axis1 (RA/Az) / Axis2 (Dec/Alt) arcsec

        const char* getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
