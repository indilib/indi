/*******************************************************************************
  INDI StarTech Managed USB Hub Driver
*******************************************************************************/

#include "startech_hub.h"

#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include <sys/ioctl.h>
#include <termios.h>

namespace
{
std::unique_ptr<StarTechHub> hub(new StarTechHub());

std::string trimCRLF(std::string s)
{
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    return s;
}
} // namespace

void ISGetProperties(const char *dev)
{
    hub->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Call through the public base-class interface to avoid access issues.
    // Virtual dispatch will still reach StarTechHub::ISNewSwitch().
    static_cast<INDI::DefaultDevice *>(hub.get())->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    static_cast<INDI::DefaultDevice *>(hub.get())->ISNewText(dev, name, texts, names, n);
}

void ISSnoopDevice(XMLEle *root)
{
    hub->ISSnoopDevice(root);
}

StarTechHub::StarTechHub()
{
    setVersion(1, 1);
}

const char *StarTechHub::getDefaultName()
{
    return "StarTech Managed USB Hub";
}

bool StarTechHub::initProperties()
{
    INDI::DefaultDevice::initProperties();
    setDriverInterface(AUX_INTERFACE);

    // -----------------------
    // Options: Port Aliases
    // -----------------------
    PortAliasesTP.fill(getDeviceName(), "USB_PORT_ALIASES", "Port Labels", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    for (uint8_t i = 0; i < USB_PORTS; i++)
    {
        char itemName[16];
        snprintf(itemName, sizeof(itemName), "ALIAS_P%u", i + 1);
        PortAliasesTP[i].fill(itemName, defaultPortLabel(i + 1).c_str(), "");
    }

    PortAliasesTP.onUpdate([this]()
    {
        syncAliasesFromProperty(true);
        applyPortLabelsAndRedefineUSBProperties();
        PortAliasesTP.setState(IPS_OK);
        PortAliasesTP.apply();
        return true;
    });

    // -----------------------
    // Main Control: Info
    // -----------------------
    InfoTP.fill(getDeviceName(), "HUB_INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);
    InfoTP[INFO_IDENTITY].fill("IDENTITY", "Model", "N/A");
    InfoTP[INFO_MASK].fill("MASK", "Current Mask", "N/A");

    // -----------------------
    // Main Control: USB Ports
    // -----------------------
    for (uint8_t i = 0; i < USB_PORTS; i++)
    {
        char propName[20];
        snprintf(propName, sizeof(propName), "USB_PORT_%u", i + 1);

        // ON must precede OFF
        USBPortSP[i][SW_ON].fill("ON", "On", ISS_OFF);
        USBPortSP[i][SW_OFF].fill("OFF", "Off", ISS_ON);

        USBPortSP[i].fill(getDeviceName(), propName, defaultPortLabel(i + 1).c_str(), MAIN_CONTROL_TAB,
                          IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

        USBPortSP[i].onUpdate([this, i]()
        {
            const bool enable = (USBPortSP[i][SW_ON].getState() == ISS_ON);
            if (setUSBPortEnabled(i + 1, enable))
                USBPortSP[i].setState(IPS_OK);
            else
                USBPortSP[i].setState(IPS_ALERT);

            USBPortSP[i].apply();
            return true;
        });
    }

    // All ports
    USBAllSP[SW_ON].fill("ON", "On", ISS_OFF);
    USBAllSP[SW_OFF].fill("OFF", "Off", ISS_ON);
    USBAllSP.fill(getDeviceName(), "USB_ALL_PORTS", "USB All Ports", MAIN_CONTROL_TAB,
                  IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    USBAllSP.onUpdate([this]()
    {
        const bool enable = (USBAllSP[SW_ON].getState() == ISS_ON);
        if (setAllPortsEnabled(enable))
            USBAllSP.setState(IPS_OK);
        else
            USBAllSP.setState(IPS_ALERT);

        USBAllSP.apply();
        return true;
    });

    // Serial connection
    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([this]() { return Handshake(); });
    registerConnection(serialConnection);

    return true;
}

bool StarTechHub::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Order requested: Info first, then port switches
        defineProperty(InfoTP);
        for (auto &p : USBPortSP)
            defineProperty(p);
        defineProperty(USBAllSP);

        // Options property should appear after Polling, so define after DefaultDevice::updateProperties()
        defineProperty(PortAliasesTP);

        // Make sure labels reflect current alias texts
        syncAliasesFromProperty(false);
        committedAliases = portAliases;
        applyPortLabelsAndRedefineUSBProperties();

        // Start polling loop
        SetTimer(getPollingPeriod());
    }
    else
    {
        deleteProperty(InfoTP.getName());
        for (auto &p : USBPortSP)
            deleteProperty(p.getName());
        deleteProperty(USBAllSP.getName());
        deleteProperty(PortAliasesTP.getName());

        // Lose unsaved edits on disconnect: revert to last committed state
        portAliases = committedAliases;
        for (uint8_t i = 0; i < USB_PORTS; i++)
            PortAliasesTP[i].setText(portAliases[i].c_str());
    }

    return true;
}

bool StarTechHub::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Let DefaultDevice process everything first (including CONFIG_PROCESS)
    const bool rc = INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);

    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return rc;

    if (name == nullptr)
        return rc;

    // React immediately to Load/Default so aliases apply without reconnect
    if (strcmp(name, "CONFIG_PROCESS") == 0)
    {
        bool doLoad = false;
        bool doDefault = false;

        for (int i = 0; i < n; i++)
        {
            if (states[i] != ISS_ON || names[i] == nullptr)
                continue;

            if (strcmp(names[i], "CONFIG_LOAD") == 0)
                doLoad = true;
            else if (strcmp(names[i], "CONFIG_DEFAULT") == 0)
                doDefault = true;
        }

        if (doLoad)
        {
            // Load saved aliases (if present) and commit them.
            loadConfig(PortAliasesTP);
            syncAliasesFromProperty(true);
            committedAliases = portAliases;
            applyPortLabelsAndRedefineUSBProperties();
            PortAliasesTP.apply();
        }
        else if (doDefault)
        {
            // Reset to defaults (blank aliases) WITHOUT committing.
            for (uint8_t i = 0; i < USB_PORTS; i++)
                PortAliasesTP[i].setText("");
            PortAliasesTP.setState(IPS_OK);
            PortAliasesTP.apply();

            syncAliasesFromProperty(false);
            applyPortLabelsAndRedefineUSBProperties();
        }
    }

    return rc;
}

bool StarTechHub::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);

    // Only persisted when the user presses Save (or AutoSave is enabled).
    PortAliasesTP.save(fp);
    committedAliases = portAliases;

    return true;
}

void StarTechHub::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getPollingPeriod());
        return;
    }

    uint32_t mask = 0;
    if (getPortMask(mask))
    {
        if (mask != currentMask)
        {
            currentMask = mask;
            updatePortsFromMask(currentMask);
        }
    }

    SetTimer(getPollingPeriod());
}

void StarTechHub::syncAliasesFromProperty(bool rewriteTruncated)
{
    bool anyRewrite = false;

    for (uint8_t i = 0; i < USB_PORTS; i++)
    {
        bool wasTruncated = false;
        const std::string sanitized = sanitizeAlias(PortAliasesTP[i].getText(), wasTruncated);
        portAliases[i] = sanitized;

        if (rewriteTruncated && wasTruncated)
        {
            PortAliasesTP[i].setText(sanitized.c_str());
            anyRewrite = true;
        }
    }

    if (anyRewrite)
    {
        LOGF_WARN("StarTechHub: one or more aliases were truncated to %zu characters.", ALIAS_MAX_LEN);
    }
}

std::string StarTechHub::sanitizeAlias(const std::string &in, bool &wasTruncated) const
{
    wasTruncated = false;

    // Keep spaces, but trim leading/trailing whitespace.
    size_t start = in.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";

    size_t end = in.find_last_not_of(" \t");
    std::string s = in.substr(start, end - start + 1);

    if (s.size() > ALIAS_MAX_LEN)
    {
        s.resize(ALIAS_MAX_LEN);
        wasTruncated = true;
    }

    return s;
}

std::string StarTechHub::defaultPortLabel(uint8_t port) const
{
    std::ostringstream oss;
    oss << "USB Port " << static_cast<unsigned>(port);
    return oss.str();
}

void StarTechHub::applyPortLabelsAndRedefineUSBProperties()
{
    // Update in-memory labels
    for (uint8_t i = 0; i < USB_PORTS; i++)
    {
        const std::string lbl = portAliases[i].empty() ? defaultPortLabel(i + 1) : portAliases[i];
        USBPortSP[i].setLabel(lbl.c_str());
    }

    // Force clients to refresh labels by redefining properties
    if (!isConnected())
        return;

    for (auto &p : USBPortSP)
    {
        deleteProperty(p.getName());
        defineProperty(p);
        p.apply();
    }

    deleteProperty(USBAllSP.getName());
    defineProperty(USBAllSP);
    USBAllSP.apply();
}

bool StarTechHub::sendCommand(const std::string &cmd, std::string &response)
{
    if (PortFD < 0)
        return false;

    const std::string fullCmd = cmd + "\r";

    int nbytesWritten = 0;
    if (tty_write_string(PortFD, fullCmd.c_str(), &nbytesWritten) != TTY_OK)
        return false;

    char buf[128] = {0};
    int nbytesRead = 0;
    if (tty_read_section(PortFD, buf, '\n', STARTECH_TIMEOUT_SEC, &nbytesRead) != TTY_OK)
        return false;

    buf[std::min(nbytesRead, static_cast<int>(sizeof(buf) - 1))] = '\0';
    response = trimCRLF(buf);
    return true;
}

bool StarTechHub::queryIdentity(std::string &out)
{
    std::string rx;
    if (!sendCommand(CMD_GET_IDENTITY, rx))
        return false;

    out = rx;
    return !out.empty();
}

bool StarTechHub::getPortMask(uint32_t &mask)
{
    std::string rx;
    if (!sendCommand(CMD_GET_MASK, rx))
        return false;

    if (rx.size() != 8)
    {
        LOGF_ERROR("StarTechHub: unexpected mask length '%s'", rx.c_str());
        return false;
    }

    char *end = nullptr;
    const unsigned long v = strtoul(rx.c_str(), &end, 16);
    if (end == nullptr || *end != '\0')
    {
        LOGF_ERROR("StarTechHub: failed parsing mask '%s'", rx.c_str());
        return false;
    }

    mask = static_cast<uint32_t>(v);
    return true;
}

bool StarTechHub::setPortMask(uint32_t mask)
{
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << mask;
    const std::string maskStr = oss.str();

    const std::string cmd = std::string(CMD_SET_MASK) + DEFAULT_PASS8 + maskStr;

    std::string rx;
    if (!sendCommand(cmd, rx))
        return false;

    const std::string expected = std::string("G") + maskStr;
    if (rx != expected)
    {
        LOGF_ERROR("StarTechHub: unexpected ACK '%s' (expected '%s')", rx.c_str(), expected.c_str());
        return false;
    }

    currentMask = mask;
    updatePortsFromMask(currentMask);
    return true;
}

bool StarTechHub::setUSBPortEnabled(uint8_t port, bool enabled)
{
    if (port < 1 || port > USB_PORTS)
        return false;

    const uint8_t bit = 1u << (port - 1);
    uint8_t top = static_cast<uint8_t>((currentMask >> 24) & 0xFF);

    if (enabled)
        top |= bit;
    else
        top &= static_cast<uint8_t>(~bit);

    const uint32_t newMask = (currentMask & 0x00FFFFFFu) | (static_cast<uint32_t>(top) << 24);
    return setPortMask(newMask);
}

bool StarTechHub::setAllPortsEnabled(bool enabled)
{
    const uint32_t newMask = enabled ? (currentMask | 0xFF000000u)
                                     : (currentMask & 0x00FFFFFFu);
    return setPortMask(newMask);
}

void StarTechHub::updatePortsFromMask(uint32_t mask)
{
    // Update Info
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << mask;
    InfoTP[INFO_MASK].setText(oss.str().c_str());
    InfoTP.apply();

    const uint8_t top = static_cast<uint8_t>((mask >> 24) & 0xFF);

    // All ports state
    const uint8_t allBits = 0x7F; // ports 1..7
    if ((top & allBits) == allBits)
    {
        USBAllSP[SW_ON].setState(ISS_ON);
        USBAllSP[SW_OFF].setState(ISS_OFF);
    }
    else if ((top & allBits) == 0)
    {
        USBAllSP[SW_ON].setState(ISS_OFF);
        USBAllSP[SW_OFF].setState(ISS_ON);
    }
    else
    {
        USBAllSP[SW_ON].setState(ISS_OFF);
        USBAllSP[SW_OFF].setState(ISS_OFF);
    }

    USBAllSP.setState(IPS_OK);
    USBAllSP.apply();

    // Individual ports
    for (uint8_t i = 0; i < USB_PORTS; i++)
    {
        const bool enabled = (top & (1u << i)) != 0;
        USBPortSP[i][SW_ON].setState(enabled ? ISS_ON : ISS_OFF);
        USBPortSP[i][SW_OFF].setState(enabled ? ISS_OFF : ISS_ON);
        USBPortSP[i].setState(IPS_OK);
        USBPortSP[i].apply();
    }
}

bool StarTechHub::Handshake()
{
    PortFD = serialConnection->getPortFD();
    if (PortFD < 0)
        return false;

    // Assert DTR and RTS.
    int status = 0;
    if (ioctl(PortFD, TIOCMGET, &status) == 0)
    {
        status |= (TIOCM_DTR | TIOCM_RTS);
        ioctl(PortFD, TIOCMSET, &status);
    }

    // Identify
    std::string ident;
    if (queryIdentity(ident))
        InfoTP[INFO_IDENTITY].setText(ident.c_str());
    else
        InfoTP[INFO_IDENTITY].setText("NA");

    // Read initial mask
    uint32_t mask = 0;
    if (!getPortMask(mask))
        return false;

    currentMask = mask;
    updatePortsFromMask(currentMask);

    return true;
}
