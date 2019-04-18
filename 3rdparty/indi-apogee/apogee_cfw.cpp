/*******************************************************************************
 Apogee Filter Wheel Driver

 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <cstring>

#ifdef OSX_EMBEDED_MODE
#else
#include <libapogee/ApgLogger.h>
#endif

#include "config.h"
#include "apogee_cfw.h"

static std::unique_ptr<ApogeeCFW> apogeeCFW(new ApogeeCFW());

void ISGetProperties(const char *dev)
{
    apogeeCFW->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    apogeeCFW->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    apogeeCFW->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    apogeeCFW->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    apogeeCFW->ISSnoopDevice(root);
}

ApogeeCFW::ApogeeCFW()
{
    setVersion(APOGEE_VERSION_MAJOR, APOGEE_VERSION_MINOR);
    setFilterConnection(CONNECTION_NONE);

    ApgCFW.reset(new ApogeeFilterWheel());
}

const char *ApogeeCFW::getDefaultName()
{
    return "Apogee CFW";
}

bool ApogeeCFW::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Filter Type
    IUFillSwitch(&FilterTypeS[TYPE_UNKNOWN], "TYPE_UNKNOWN", "Unknown", ISS_ON);
    IUFillSwitch(&FilterTypeS[TYPE_FW50_9R], "TYPE_FW50_9R", "FW50 9R", ISS_OFF);
    IUFillSwitch(&FilterTypeS[TYPE_FW50_7S], "TYPE_FW50_7S", "FW50 7S", ISS_OFF);
    IUFillSwitch(&FilterTypeS[TYPE_AFW50_10S], "TYPE_AFW50_10S", "AFW50 10S", ISS_OFF);
    IUFillSwitch(&FilterTypeS[TYPE_AFW31_17R], "TYPE_AFW31_17R", "AFW31 17R", ISS_OFF);
    IUFillSwitchVector(&FilterTypeSP, FilterTypeS, 5, getDeviceName(), "FILTER_TYPE", "Type", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&PortTypeS[PORT_USB], "USB_PORT", "USB", ISS_ON);
    IUFillSwitch(&PortTypeS[PORT_NETWORK], "NETWORK_PORT", "Network", ISS_OFF);
    IUFillSwitchVector(&PortTypeSP, PortTypeS, 2, getDeviceName(), "PORT_TYPE", "Port", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    IUFillText(&NetworkInfoT[NETWORK_SUBNET], "SUBNET_ADDRESS", "Subnet", "192.168.0.255");
    IUFillText(&NetworkInfoT[NETWORK_ADDRESS], "IP_PORT_ADDRESS", "IP:Port", "");
    IUFillTextVector(&NetworkInfoTP, NetworkInfoT, 2, getDeviceName(), "NETWORK_INFO", "Network", MAIN_CONTROL_TAB,
                     IP_RW, 0, IPS_IDLE);

    IUFillText(&FilterInfoT[INFO_NAME], "CFW_NAME", "Name", "");
    IUFillText(&FilterInfoT[INFO_FIRMWARE], "CFW_FIRMWARE", "Firmware", "");
    IUFillTextVector(&FilterInfoTP, FilterInfoT, 2, getDeviceName(), "FILTER_INFO", "Info", MAIN_CONTROL_TAB, IP_RO, 0,
                     IPS_IDLE);

    addAuxControls();

    return true;
}

void ApogeeCFW::ISGetProperties(const char *dev)
{
    INDI::FilterWheel::ISGetProperties(dev);

    defineSwitch(&FilterTypeSP);
    defineSwitch(&PortTypeSP);
    defineText(&NetworkInfoTP);

    loadConfig(true, FilterTypeSP.name);
    loadConfig(true, PortTypeSP.name);
    loadConfig(true, NetworkInfoTP.name);
}

bool ApogeeCFW::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineText(&FilterInfoTP);
        SetTimer(POLLMS);
    }
    else
    {
        deleteProperty(FilterInfoTP.name);
    }

    return true;
}

bool ApogeeCFW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Filter Type
        if (!strcmp(name, FilterTypeSP.name))
        {
            IUUpdateSwitch(&FilterTypeSP, states, names, n);
            FilterTypeSP.s = IPS_OK;
            IDSetSwitch(&FilterTypeSP, nullptr);
            return true;
        }

        // Port Type
        if (!strcmp(name, PortTypeSP.name))
        {
            IUUpdateSwitch(&PortTypeSP, states, names, n);
            PortTypeSP.s = IPS_OK;
            IDSetSwitch(&PortTypeSP, nullptr);
            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool ApogeeCFW::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(NetworkInfoTP.name, name))
        {
            IUUpdateText(&NetworkInfoTP, texts, names, n);

            subnet = std::string(NetworkInfoT[NETWORK_SUBNET].text);

            if (strlen(NetworkInfoT[NETWORK_ADDRESS].text) > 0)
            {
                char ip[16];
                int port;
                int rc = sscanf(NetworkInfoT[NETWORK_ADDRESS].text, "%[^:]:%d", ip, &port);

                if (rc == 2)
                    NetworkInfoTP.s = IPS_OK;
                else
                {
                    LOG_ERROR("Invalid format. Format must be IP:Port (e.g. 192.168.1.1:80)");
                    NetworkInfoTP.s = IPS_ALERT;
                }
            }
            else
                NetworkInfoTP.s = IPS_OK;

            IDSetText(&NetworkInfoTP, nullptr);

            return true;
        }
    }

    return INDI::FilterWheel::ISNewText(dev, name, texts, names, n);
}

///////////////////////////
// MAKE	  TOKENS
std::vector<std::string> ApogeeCFW::MakeTokens(const std::string &str, const std::string &separator)
{
    std::vector<std::string> returnVector;
    std::string::size_type start = 0;
    std::string::size_type end   = 0;

    while ((end = str.find(separator, start)) != std::string::npos)
    {
        returnVector.push_back(str.substr(start, end - start));
        start = end + separator.size();
    }

    returnVector.push_back(str.substr(start));

    return returnVector;
}

///////////////////////////
//	GET    ITEM    FROM     FIND       STR
std::string ApogeeCFW::GetItemFromFindStr(const std::string &msg, const std::string &item)
{
    //search the single device input string for the requested item
    std::vector<std::string> params = MakeTokens(msg, ",");
    std::vector<std::string>::iterator iter;

    for (iter = params.begin(); iter != params.end(); ++iter)
    {
        if (std::string::npos != (*iter).find(item))
        {
            std::string result = MakeTokens((*iter), "=").at(1);

            return result;
        }
    } //for

    std::string noOp;
    return noOp;
}

////////////////////////////
//	GET		USB  ADDRESS
std::string ApogeeCFW::GetUsbAddress(const std::string &msg)
{
    return GetItemFromFindStr(msg, "address=");
}

////////////////////////////
//	GET		IP Address
std::string ApogeeCFW::GetIPAddress(const std::string &msg)
{
    std::string addr = GetItemFromFindStr(msg, "address=");
    return addr;
}

////////////////////////////
//	GET		ETHERNET  ADDRESS
std::string ApogeeCFW::GetEthernetAddress(const std::string &msg)
{
    std::string addr = GetItemFromFindStr(msg, "address=");
    addr.append(":");
    addr.append(GetItemFromFindStr(msg, "port="));
    return addr;
}

////////////////////////////
// Is CFW
bool ApogeeCFW::IsDeviceFilterWheel(const std::string &msg)
{
    std::string str = GetItemFromFindStr(msg, "deviceType=");

    return (0 == str.compare("filterWheel") ? true : false);
}

////////////////////////////
// Get Model
std::string ApogeeCFW::GetModel(const std::string &msg)
{
    return GetItemFromFindStr(msg, "model=");
}


bool ApogeeCFW::Connect()
{
    std::string msg;
    std::string addr;
    bool filterFound = false;

    LOG_INFO("Attempting to find Apogee CFW...");

    // USB
    if (PortTypeS[0].s == ISS_ON)
    {
        // Simulation
        if (isSimulation())
        {
            msg  = std::string("<d>address=1,interface=usb,model=Filter "
                               "Wheel,deviceType=filterWheel,id=0xFFFF,firmwareRev=0xFFEE</d>");
            addr = GetUsbAddress(msg);
        }
        else
        {
            ioInterface = std::string("usb");
            FindDeviceUsb lookUsb;
            try
            {
                msg  = lookUsb.Find();
                if (msg.size() > 0)
                {
                    // FIXME this can cause a crash
                    //LOGF_DEBUG("USB search result: %s", msg.c_str());
                    addr = GetUsbAddress(msg);
                }
                else
                    LOG_ERROR("USB lookup failed. Nothing detected.");

            }
            catch (std::runtime_error &err)
            {
                LOGF_ERROR("Error getting USB address: %s", err.what());
                return false;
            }
        }

        std::string delimiter = "</d>";
        size_t pos = 0;
        std::string token, token_ip;
        while ((pos = msg.find(delimiter)) != std::string::npos)
        {
            token    = msg.substr(0, pos);
            LOGF_DEBUG("Checking device: %s", token.c_str());

            filterFound = IsDeviceFilterWheel(token);
            if (filterFound)
                break;

            msg.erase(0, pos + delimiter.length());
        }
    }
    // Ethernet
    else
    {
        ioInterface = std::string("ethernet");
        FindDeviceEthernet look4Filter;
        char ip[32];
        int port;

        // Simulation
        if (isSimulation())
        {
            msg  = std::string("<d>address=1,interface=usb,model=Filter "
                               "Wheel,deviceType=filterWheel,id=0xFFFF,firmwareRev=0xFFEE</d>");
        }
        else
        {
            try
            {
                msg = look4Filter.Find(subnet);
                // FIXME this can cause a crash
                //LOGF_DEBUG("Network search result: %s", msg.c_str());
            }
            catch (std::runtime_error &err)
            {
                LOGF_ERROR("Error getting network address: %s", err.what());
                return false;
            }
        }

        int rc = 0;

        // Check if we have IP:Port format
        if (NetworkInfoT[NETWORK_ADDRESS].text != nullptr && strlen(NetworkInfoT[NETWORK_ADDRESS].text) > 0)
            rc = sscanf(NetworkInfoT[NETWORK_ADDRESS].text, "%[^:]:%d", ip, &port);

        // If we have IP:Port, then let's skip all entries that does not have our desired IP address.
        addr = NetworkInfoT[NETWORK_ADDRESS].text;

        std::string delimiter = "</d>";
        size_t pos = 0;
        std::string token, token_ip;
        while ((pos = msg.find(delimiter)) != std::string::npos)
        {
            token    = msg.substr(0, pos);

            if (IsDeviceFilterWheel(token))
            {
                if (rc == 2)
                {
                    addr = GetEthernetAddress(token);
                    IUSaveText(&NetworkInfoT[NETWORK_ADDRESS], addr.c_str());
                    LOGF_INFO("Detected filter at %s", addr.c_str());
                    IDSetText(&NetworkInfoTP, nullptr);
                    filterFound = true;
                    break;
                }
                else
                {
                    token_ip = GetIPAddress(token);
                    LOGF_DEBUG("Checking %s (%s) for IP %s", token.c_str(), token_ip.c_str(), ip);
                    if (token_ip == ip)
                    {
                        msg = token;
                        LOGF_DEBUG("IP matched (%s).", msg.c_str());
                        filterFound = true;
                        break;
                    }
                }
            }

            msg.erase(0, pos + delimiter.length());
        }
    }

    if (filterFound == false)
    {
        LOG_ERROR("Unable to find Apogee Filter Wheels attached. Please check connection and power and try again.");
        return false;
    }

    //    //uint16_t id       = GetID(msg);
    //    uint16_t frmwrRev = GetFrmwrRev(msg);
    //    char firmwareStr[16]={0};
    //    snprintf(firmwareStr, 16, "0x%X", frmwrRev);
    //    firmwareRev = std::string(firmwareStr);
    //model = GetModel(msg);

    try
    {
        if (isSimulation() == false)
        {
            ApgCFW->Init(static_cast<ApogeeFilterWheel::Type>(IUFindOnSwitchIndex(&FilterTypeSP)), addr);
        }
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Error opening CFW: %s", err.what());
        return false;
    }

    if (isSimulation())
        FilterSlotN[0].max = 5;
    else
    {
        try
        {
            FilterSlotN[0].max = ApgCFW->GetMaxPositions();
        }
        catch(std::runtime_error &err)
        {
            LOGF_ERROR("Failed to retrieve maximum filter position: %s", err.what());
            ApgCFW->Close();
            return false;
        }
    }

    if (isSimulation())
    {
        IUSaveText(&FilterInfoT[INFO_NAME], "Simulated Filter");
        IUSaveText(&FilterInfoT[INFO_FIRMWARE], "123456");
    }
    else
    {
        IUSaveText(&FilterInfoT[INFO_NAME], ApgCFW->GetName().c_str());
        IUSaveText(&FilterInfoT[INFO_FIRMWARE], ApgCFW->GetUsbFirmwareRev().c_str());
    }

    FilterInfoTP.s = IPS_OK;

    LOG_INFO("CFW is online.");
    return true;
}

bool ApogeeCFW::Disconnect()
{
    try
    {
        if (isSimulation() == false)
            ApgCFW->Close();
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Error: Close failed. %s.", err.what());
        return false;
    }

    LOG_INFO("CFW is offline.");
    return true;
}

int ApogeeCFW::QueryFilter()
{
    try
    {
        CurrentFilter = ApgCFW->GetPosition();
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Failed to query filter: %s", err.what());
        FilterSlotNP.s = IPS_ALERT;
        IDSetNumber(&FilterSlotNP, nullptr);
        return -1;
    }

    return CurrentFilter;
}

bool ApogeeCFW::SelectFilter(int position)
{
    try
    {
        ApgCFW->SetPosition(position);
    }
    catch (std::runtime_error &err)
    {
        LOGF_ERROR("Failed to set filter: %s", err.what());
        FilterSlotNP.s = IPS_ALERT;
        IDSetNumber(&FilterSlotNP, nullptr);
        return false;
    }

    TargetFilter = position;
    return true;
}

void ApogeeCFW::TimerHit()
{

    if (FilterSlotNP.s == IPS_BUSY)
    {
        try
        {
            ApogeeFilterWheel::Status status = ApgCFW->GetStatus();
            if (status == ApogeeFilterWheel::READY)
            {
                CurrentFilter = TargetFilter;
                SelectFilterDone(CurrentFilter);
            }
        }
        catch (std::runtime_error &err)
        {
            LOGF_ERROR("Failed to get CFW status: %s", err.what());
            FilterSlotNP.s = IPS_ALERT;
            IDSetNumber(&FilterSlotNP, nullptr);
        }
    }

    SetTimer(POLLMS);
}

void ApogeeCFW::debugTriggered(bool enabled)
{
    ApgLogger::Instance().SetLogLevel(enabled ? ApgLogger::LEVEL_DEBUG : ApgLogger::LEVEL_RELEASE);
}

bool ApogeeCFW::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &FilterTypeSP);
    IUSaveConfigSwitch(fp, &PortTypeSP);
    IUSaveConfigText(fp, &NetworkInfoTP);

    return true;
}
