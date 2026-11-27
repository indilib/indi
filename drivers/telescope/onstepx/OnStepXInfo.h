/*
    OnStepX INDI Driver — Firmware info, mount status text, reticle (mount binary only)
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

    Plain C++ helper -- no INDI base class.  Extracted from OnStepXMount in
    Stage 15 to keep OnStepXMount.cpp within a manageable size.

    Protocol (OnStepX v10.24c):
      :B+#  — reticle brighter (blind send, no reply)
      :B-#  — reticle dimmer   (blind send, no reply)
      Firmware version/date/time/config supplied from Capabilities at connect;
      no runtime commands needed.

    INDI Properties ("OnStepX" tab):
      OSX_FIRMWARE  IP_RO  Text[4]    Version / Date / Time / Config
      OSX_STATUS    IP_RO  Text[11]   Decoded human-readable mount status

    The firmware's general error (last field of the status) is logged when it
    changes (seen on two polls in a row): an error for a limit or fault that stopped the mount, a warning
    for a start-up failure, and an info message when it clears.
      OSX_RETICLE   IP_RW  Switch[2]  Brighter / Dimmer (momentary buttons, ISR_ATMOST1)
*/

#pragma once

#include "OnStepXCapabilities.h"
#include "OnStepXStatus.h"

#include <indipropertyswitch.h>
#include <indipropertytext.h>

class OnStepXComm;
namespace INDI
{
class DefaultDevice;
}

class OnStepXInfo
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

        void initProperties();
        // caps used only on connect (populated = true) to fill firmware fields
        // OSX_FIRMWARE and OSX_STATUS; updateReticle() defines OSX_RETICLE.
        void updateProperties(bool connected, const Capabilities &caps);
        void updateReticle(bool connected);
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);
        void saveConfig(FILE *fp);

        // Populate OSX_STATUS from a freshly polled MountStatus (every poll)
        void updateStatus(const MountStatus &s);

    private:
        INDI::DefaultDevice *m_dev  { nullptr };
        OnStepXComm         *m_comm { nullptr };

        INDI::PropertyText   m_firmwareTP { 4 };
        INDI::PropertyText   m_statusTP   { 11 };
        INDI::PropertySwitch m_reticleSP  { 2 };

        int m_lastError    { 0 };   // last error logged
        int m_pendingError { 0 };   // error seen on the previous poll
        void reportError(const MountStatus &s);

        const char *getDeviceName() const;
};
