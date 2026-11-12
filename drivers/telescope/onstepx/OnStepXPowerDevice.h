/*
    OnStepX INDI Driver — Power child device (both binaries)
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

    The controller's auxiliary outputs and USB plugin ports as their own INDI
    device, "<parent> Power", like the focusers and the rotator.  The parent
    (OnStepXMount or OnStepXAux) creates it after Handshake when the
    controller has aux features (:GXY0#) or USB ports, shares its
    OnStepXComm, and polls it from its own poll throttle.

    INDI::PowerInterface sizes its properties at initProperties(), so the
    outputs are probed (configure()) before the device is first announced.
    That is why this is a child device: the parent's initProperties() runs
    before the controller has been probed.

    Mapping (protocol in OnStepXAuxFeatures and OnStepXUsbPlugin):
      SWITCH slots (incl. momentary and cover switches, which the firmware
        reports as SWITCH)  -> PowerInterface DC outputs, POWER_CHANNELS
      USB plugin ports      -> PowerInterface USB ports, USB_PORTS
      ANALOG (PWM 0-255)    -> custom OSX_<name>_AN, Power tab
      DEW_HEATER            -> custom enable / zero+span / temp above dew
                               point, Dew Heaters tab.  Not PowerInterface dew
                               channels: OnStepX dew heaters are automatic
                               (zero/span), with no manual duty cycle, and the
                               interface's auto-dew switch needs those channels.
      INTERVALOMETER        -> custom, Intervalometer tab
    PowerInterface labels default to the firmware's feature and port names;
    they stay editable and are saved in the INDI config.
    INDI config: the firmware keeps every feature's value and settings in NV,
    so none are saved (a config load would overwrite hand controller or web UI
    changes).  The USB port states are saved: the USB plugin has no access to
    the firmware's NV, so they would otherwise reset when the controller boots.
*/

#pragma once

#include "OnStepXAuxFeatures.h"
#include "OnStepXUsbPlugin.h"

#include <defaultdevice.h>
#include <indipowerinterface.h>

#include <memory>
#include <vector>

class OnStepXComm;

class OnStepXPowerDevice : public INDI::DefaultDevice, public INDI::PowerInterface
{
    public:
        // Device name "<parentName> Power", e.g. "OnStepX Power".
        explicit OnStepXPowerDevice(const char *parentName = "OnStepX");

        // Probe the outputs.  Must run before the device is first announced:
        // the PowerInterface output counts are fixed at initProperties().
        void configure(OnStepXComm *comm, uint8_t featureMask, uint8_t portMask);

        // Periodic poll of every output — called from the parent's throttle.
        void pollStatus();

        // DefaultDevice
        const char *getDefaultName() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        bool saveConfigItems(FILE *fp) override;

        // Connection — no port to open; the parent activates us.
        bool Connect() override;
        bool Disconnect() override;

        // Parent connected: create the child the first time (configured, then
        // announced, with properties defined after a short delay so clients
        // process the new device first), or reconnect it.  Does nothing when
        // the controller has no aux features and no USB ports.
        static void connectChild(std::unique_ptr<OnStepXPowerDevice> &child, uint8_t featureMask,
                                 uint8_t portMask, OnStepXComm *comm, const char *parentName);

        // Parent disconnecting: mark the child disconnected.
        static void disconnectChild(std::unique_ptr<OnStepXPowerDevice> &child);

    protected:
        // PowerInterface
        bool SetPowerPort(size_t port, bool enabled) override;
        bool SetUSBPort(size_t port, bool enabled) override;

    private:
        char               m_name[MAXINDIDEVICE] {};   // "<parent> Power"
        OnStepXComm       *m_comm { nullptr };
        OnStepXAuxFeatures m_aux;
        OnStepXUsbPlugin   m_usb;
        uint8_t            m_featureMask { 0 };
        uint8_t            m_portMask    { 0 };

        // PowerInterface channel index -> firmware slot / port number (1-8).
        // Fixed when the device is first configured.
        std::vector<int>   m_dcSlots;
        std::vector<int>   m_usbPorts;
        bool               m_initialised { false };

        void seedLabels();
        void syncChannels();
};
