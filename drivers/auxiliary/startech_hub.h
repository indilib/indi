/*******************************************************************************
  INDI StarTech Managed USB Hub Driver
*******************************************************************************/

#pragma once

#include "defaultdevice.h"
#include "indipropertyswitch.h"
#include "indipropertytext.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace Connection
{
class Serial;
}

class StarTechHub : public INDI::DefaultDevice
{
public:
    StarTechHub();
    virtual ~StarTechHub() = default;

    bool initProperties() override;
    bool updateProperties() override;

protected:
    const char *getDefaultName() override;
    bool saveConfigItems(FILE *fp) override;
    void TimerHit() override;

    // Hook needed to react immediately to CONFIG_LOAD / CONFIG_DEFAULT
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

private:
    // Connection / protocol
    bool Handshake();
    bool queryIdentity(std::string &out);
    bool getPortMask(uint32_t &mask);
    bool setPortMask(uint32_t mask);
    bool sendCommand(const std::string &cmd, std::string &response);

    bool setUSBPortEnabled(uint8_t port, bool enabled);
    bool setAllPortsEnabled(bool enabled);
    void updatePortsFromMask(uint32_t mask);

    static constexpr const char CMD_GET_IDENTITY[] = "?Q";
    static constexpr const char CMD_GET_MASK[]     = "GP";
    static constexpr const char CMD_SET_MASK[]     = "SP";

    // Protocol requires an 8-char password field. Factory default.
    static constexpr const char DEFAULT_PASS8[]    = "pass    ";

    static constexpr uint8_t USB_PORTS = 7;
    static constexpr size_t  ALIAS_MAX_LEN = 16;
    static constexpr uint8_t STARTECH_TIMEOUT_SEC = 3;

    // Switch item order: ON first, OFF second
    enum { SW_ON = 0, SW_OFF = 1 };

    // Properties
    enum { INFO_IDENTITY, INFO_MASK, INFO_N };
    INDI::PropertyText InfoTP {INFO_N};

    enum { ALIAS_P1, ALIAS_P2, ALIAS_P3, ALIAS_P4, ALIAS_P5, ALIAS_P6, ALIAS_P7, ALIAS_N };
    INDI::PropertyText PortAliasesTP {ALIAS_N};

    std::array<INDI::PropertySwitch, USB_PORTS> USBPortSP {
        INDI::PropertySwitch{2}, INDI::PropertySwitch{2}, INDI::PropertySwitch{2},
        INDI::PropertySwitch{2}, INDI::PropertySwitch{2}, INDI::PropertySwitch{2},
        INDI::PropertySwitch{2}
    };
    INDI::PropertySwitch USBAllSP {2};

    // Alias state
    std::array<std::string, USB_PORTS> portAliases {};
    std::array<std::string, USB_PORTS> committedAliases {};

    void syncAliasesFromProperty(bool rewriteTruncated = true);
    std::string sanitizeAlias(const std::string &in, bool &wasTruncated) const;
    std::string defaultPortLabel(uint8_t port) const;

    // Apply labels to USB properties and force clients to refresh labels
    void applyPortLabelsAndRedefineUSBProperties();

    // Serial
    int PortFD {-1};
    Connection::Serial *serialConnection {nullptr};

    uint32_t currentMask {0xFFFFFFFF};
};
