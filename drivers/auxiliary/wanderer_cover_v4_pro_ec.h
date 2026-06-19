/*******************************************************************************
  Copyright(c) 2024 Frank Wang/Jérémie Klein. All rights reserved.

  WandererCover V4 Pro-EC

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indidustcapinterface.h"
#include "indilightboxinterface.h"
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace Connection
{
class Serial;
}

// Forward declarations
class WandererCoverV4ProEC;

// Protocol handler interface
class IWandererCoverProtocol
{
public:
    virtual ~IWandererCoverProtocol() = default;
    
    // Protocol identification
    virtual std::string getProtocolName() const = 0;
    virtual int getProtocolVersion() const = 0;
    virtual int getMinFirmwareVersion() const = 0;
    virtual bool supportsFeature(const std::string& feature) const = 0;
    
    // Data parsing
    virtual bool parseDeviceData(const char* data, WandererCoverV4ProEC* device) = 0;
    virtual bool detectProtocol(const char* data) = 0;
    
    // Command generation
    virtual std::string generateOpenCommand() const = 0;
    virtual std::string generateCloseCommand() const = 0;
    virtual std::string generateSetBrightnessCommand(uint16_t value) const = 0;
    virtual std::string generateTurnOffLightCommand() const = 0;
    virtual std::string generateSetOpenPositionCommand(double value) const = 0;
    virtual std::string generateSetClosePositionCommand(double value) const = 0;
    virtual std::string generateASIAIRControlCommand(bool enable) const = 0;
    virtual std::string generateCustomBrightnessCommand(int brightness, int customNumber) const = 0;
    
    // Status data structure
    struct StatusData
    {
        int firmware = 0;
        double closePositionSet = 0.0;
        double openPositionSet = 0.0;
        double currentPosition = 0.0;
        double voltage = 0.0;
        int flatPanelBrightness = 0;
        bool asiairControlEnabled = false;
    };
};

// Legacy protocol implementation (pre-20250405)
class WandererCoverLegacyProtocol : public IWandererCoverProtocol
{
public:
    std::string getProtocolName() const override { return "WandererCover V4 Pro-EC (Legacy < 20250405)"; }
    int getProtocolVersion() const override { return 1; }
    int getMinFirmwareVersion() const override { return 0; }
    
    bool supportsFeature(const std::string& feature) const override;
    bool parseDeviceData(const char* data, WandererCoverV4ProEC* device) override;
    bool detectProtocol(const char* data) override;
    
    std::string generateOpenCommand() const override;
    std::string generateCloseCommand() const override;
    std::string generateSetBrightnessCommand(uint16_t value) const override;
    std::string generateTurnOffLightCommand() const override;
    std::string generateSetOpenPositionCommand(double value) const override;
    std::string generateSetClosePositionCommand(double value) const override;
    std::string generateASIAIRControlCommand(bool enable) const override;
    std::string generateCustomBrightnessCommand(int brightness, int customNumber) const override;
};

// Modern protocol implementation (20250405+)
class WandererCoverModernProtocol : public IWandererCoverProtocol
{
public:
    std::string getProtocolName() const override { return "WandererCover V4 Pro-EC (Modern >= 20250405)"; }
    int getProtocolVersion() const override { return 2; }
    int getMinFirmwareVersion() const override { return 20250405; }
    
    bool supportsFeature(const std::string& feature) const override;
    bool parseDeviceData(const char* data, WandererCoverV4ProEC* device) override;
    bool detectProtocol(const char* data) override;
    
    std::string generateOpenCommand() const override;
    std::string generateCloseCommand() const override;
    std::string generateSetBrightnessCommand(uint16_t value) const override;
    std::string generateTurnOffLightCommand() const override;
    std::string generateSetOpenPositionCommand(double value) const override;
    std::string generateSetClosePositionCommand(double value) const override;
    std::string generateASIAIRControlCommand(bool enable) const override;
    std::string generateCustomBrightnessCommand(int brightness, int customNumber) const override;
};

class WandererCoverV4ProEC : public INDI::DefaultDevice, public INDI::DustCapInterface, public INDI::LightBoxInterface
{
public:
    WandererCoverV4ProEC();
    virtual ~WandererCoverV4ProEC() = default;

    virtual bool initProperties() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool updateProperties() override;
    virtual bool ISSnoopDevice(XMLEle *root) override;

protected:

    // From Dust Cap
    virtual IPState ParkCap() override;
    virtual IPState UnParkCap() override;

    // From Light Box
    virtual bool SetLightBoxBrightness(uint16_t value) override;
    virtual bool EnableLightBox(bool enable) override;

    const char *getDefaultName() override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual void TimerHit() override;

    // Protocol handler access
    friend class IWandererCoverProtocol;
    friend class WandererCoverLegacyProtocol;
    friend class WandererCoverModernProtocol;

private:

    int firmware=0;
    bool toggleCover(bool open);
    bool sendCommand(std::string command);
    bool getData();
    bool parseDeviceData(const char *data);
    double closesetread=0;
    double opensetread=0;
    double positionread=0;
    double voltageread=0;
    double asiaircontrolenabledread=0;
    double flatpanelbrightnessread=0;
    bool setClose(double value);
    bool setOpen(double value);
    void updateData(double closesetread,double opensetread,double positionread,double voltageread,double flatpanelbrightnessread,double asiaircontrolenabledread);

    // Protocol detection and management
    bool detectProtocol();
    void setProtocol(std::unique_ptr<IWandererCoverProtocol> protocol);
    IWandererCoverProtocol* getCurrentProtocol() const { return currentProtocol.get(); }

    // Protocol handler
    std::unique_ptr<IWandererCoverProtocol> currentProtocol;

    // Status data
    IWandererCoverProtocol::StatusData statusData;

    // Properties for all protocol versions
    INDI::PropertyNumber DataNP{7}; // Extended to support new protocol fields
    enum
    {
        closeset_read,
        openset_read,
        position_read,
        voltage_read,
        flat_panel_brightness_read,
        null,
        asiair_control_enabled_read,
    };


    //Close Set///////////////////////////////////////////////////////////////
    INDI::PropertyNumber CloseSetNP{1};
    enum
    {
        CloseSet,
    };
    //Open Set///////////////////////////////////////////////////////////////
    INDI::PropertyNumber OpenSetNP{1};
    enum
    {
        OpenSet,
    };

    // Firmware information
    INDI::PropertyText FirmwareTP{1};
    enum
    {
        FIRMWARE_VERSION,
    };

    // New protocol features
    INDI::PropertySwitch ASIAIRControlSP{2};
    enum
    {
        ASIAIR_ENABLE,
        ASIAIR_DISABLE,
    };

    INDI::PropertyNumber CustomBrightnessNP{3};
    enum
    {
        CUSTOM_BRIGHTNESS_1,
        CUSTOM_BRIGHTNESS_2,
        CUSTOM_BRIGHTNESS_3,
    };



    int PortFD{ -1 };

    Connection::Serial *serialConnection{ nullptr };

    // Mutex for thread safety
    std::timed_mutex serialPortMutex;
};
