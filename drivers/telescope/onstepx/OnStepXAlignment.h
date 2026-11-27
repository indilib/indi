/*
    OnStepX INDI Driver — Alignment helper (mount binary only)
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Implements OnStepX geometric alignment (N-star).

    Protocol (OnStepX v10.24c):
      :A?#     — get status: 3 chars "mno" (m=max stars, n=current, o=target)
                 each is '0' + value, so ':' is 10 (current star can reach 10)
      :A[n]#   — start alignment with n stars (1-9), bare '1' on success, '0' on failure
      :A+#     — accept current star, bare '1' on success, '0' on failure
      :AW#     — write alignment to EEPROM, bare '1' on success
      :GX02#   — polar error altitude correction (integer arcsec)
      :GX03#   — polar error azimuth correction (integer arcsec)
      :MP#     — refine polar alignment: zero the model's polar error terms and
                 re-slew to the current position on the same pier side, so the
                 star moves by the polar error (one unframed digit like :MS#,
                 '0' = started).  Equatorial mounts only.  Refused here while a
                 model-builder session runs, and when no polar error has been
                 measured (both terms 0 or unreadable).

    INDI Properties (Alignment tab):
      OSX_ALIGN_STARS    IP_RW  ISR_1OFMANY  9 switches: "1 Star" .. "9 Stars"
      OSX_ALIGN_CONTROL  IP_RW  ISR_ATMOST1  2 switches: "Start" / "Accept Star"
      OSX_ALIGN_WRITE    IP_RW  ISR_ATMOST1  1 switch:   "Write to EEPROM"
      OSX_ALIGN_STATUS   IP_RO  4 text fields: Status / Max Stars / Current Star / Target Stars
      OSX_ALIGN_ERROR    IP_RO  2 text fields: Polar Error Alt / Polar Error Az
      OSX_POLAR_REFINE   IP_RW  ISR_ATMOST1  1 switch:   "Refine Polar Align"
                         (equatorial mounts only)
*/

#pragma once

#include <defaultdevice.h>
#include <cstdio>
#include <functional>

class OnStepXComm;
class OnStepXAlignment
{
    public:
        void setDevice(INDI::DefaultDevice *dev)
        {
            m_dev = dev;
        }
        void setComm(OnStepXComm *comm)
        {
            m_comm = comm;
        }

        // Called from device class initProperties
        void initProperties();

        // Called from device class updateProperties.  Refine Polar Align is
        // offered only on equatorial mounts.
        void updateProperties(bool connected, bool equatorial);

        // Returns true while a model-builder session owns the firmware model.
        void setModelBuildCheck(std::function<bool()> check)
        {
            m_modelBuildActive = std::move(check);
        }

        // Called from ISNewSwitch
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);

        void saveConfig(FILE *fp);

        // Refresh alignment status from :A?# and apply to OSX_ALIGN_STATUS.
        // Returns false if firmware didn't respond.
        bool updateStatus();

        // Read polar error corrections from :GX02# and :GX03#
        void updatePolarError();

    private:
        INDI::DefaultDevice *m_dev  { nullptr };
        OnStepXComm         *m_comm { nullptr };

        // Star count selector (1-9)
        INDI::PropertySwitch m_starsSP   {9};
        // Start / Accept Star
        INDI::PropertySwitch m_controlSP {2};
        // Write to EEPROM
        INDI::PropertySwitch m_writeSP   {1};
        // Status text
        INDI::PropertyText   m_statusTP  {4};
        // Polar error text
        INDI::PropertyText   m_errorTP   {2};
        // Refine polar alignment (momentary)
        INDI::PropertySwitch m_refineSP  {1};

        bool m_equatorial { false };
        std::function<bool()> m_modelBuildActive;

        bool startAlignment(int stars);
        bool acceptStar();
        bool writeAlignment();
        bool refinePolarAlign();

        const char* getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
