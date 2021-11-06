/*
  Focus Lynx/Focus Boss II INDI driver
  Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
*/

#include "focuslynx.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <memory>
#include <cstring>
#include <termios.h>

#define FOCUSNAMEF1 "FocusLynx F1"
#define FOCUSNAMEF2 "FocusLynx F2"

static std::unique_ptr<FocusLynxF1> lynxDriveF1(new FocusLynxF1("F1"));
static std::unique_ptr<FocusLynxF2> lynxDriveF2(new FocusLynxF2("F2"));

/************************************************************************************
*
*               First Focuser (F1)
*
*************************************************************************************/

/************************************************************************************
 *
* ***********************************************************************************/
FocusLynxF1::FocusLynxF1(const char *target)
{
    /* Override the original constructor
     * and give the Focuser target
     * F1 or F2 to set the target of the created instance
     */
    setFocusTarget(target);

    // Both communication available, Serial and network (tcp/ip)
    setSupportedConnections(CONNECTION_SERIAL | CONNECTION_TCP);

    // explain in connect() function Only set on the F1 constructor, not on the F2 one
    PortFD = -1;
}

/************************************************************************************
 *
* ***********************************************************************************/
FocusLynxF1::~FocusLynxF1()
{
}

/**************************************************************************************
*
***************************************************************************************/
bool FocusLynxF1::initProperties()
/* New properties
 * Common properties for both focusers, Hub setting
 * Only display and managed by Focuser F1
 * TODO:
 * Make this properties writable to give possibility to set these via IndiDriver
 */
{
    FocusLynxBase::initProperties();

    // General info
    IUFillText(&HubT[0], "Firmware", "", "");
    IUFillText(&HubT[1], "Sleeping", "", "");
    IUFillTextVector(&HubTP, HubT, 2, getDeviceName(), "HUB-INFO", "Hub", HUB_SETTINGS_TAB, IP_RO, 0, IPS_IDLE);

    // Wired network
    IUFillText(&WiredT[0], "IP address", "", "");
    IUFillText(&WiredT[1], "DHCP active", "", "");
    IUFillTextVector(&WiredTP, WiredT, 2, getDeviceName(), "WIRED-INFO", "Wired", HUB_SETTINGS_TAB, IP_RO, 0, IPS_IDLE);

    // Wifi network
    IUFillText(&WifiT[0], "Installed", "", "");
    IUFillText(&WifiT[1], "Connected", "", "");
    IUFillText(&WifiT[2], "Firmware", "", "");
    IUFillText(&WifiT[3], "Firm. Version OK", "", "");
    IUFillText(&WifiT[4], "SSID", "", "");
    IUFillText(&WifiT[5], "Ip address", "", "");
    IUFillText(&WifiT[6], "Security mode", "", "");
    IUFillText(&WifiT[7], "Security key", "", "");
    IUFillText(&WifiT[8], "Wep key", "", "");
    IUFillTextVector(&WifiTP, WifiT, 9, getDeviceName(), "WIFI-INFO", "Wifi", HUB_SETTINGS_TAB, IP_RO, 0, IPS_IDLE);

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    tcpConnection->setDefaultPort(9760);
    // To avoid confusion has Debug levels only visible on F2 remove it from F1
    // Simultation option and Debug option present only on F2
    // deleteProperty("DEBUG");
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *FocusLynxF1::getDefaultName()
{
    return FOCUSNAMEF1;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxF1::Connect()
/* Overide of connect() function
 * different for F1 or F2 focuser
 * F1 connect only himself to the driver and
 * it is the only one who's connect to the communication port to establish the physical communication
 */
{
    configurationComplete = false;
    if (isSimulation())
    {
        /* PortFD value used to give the /dev/ttyUSBx or TCP descriptor
         * if -1 = no physical port selected or simulation mode
         * if 0 = no descriptor created, F1 not connected (error)
         * other value = descriptor number
         */
        PortFD = -1;
        SetTimer(getCurrentPollingPeriod());
    }
    else if (!INDI::Focuser::Connect())
        return false;
    return Handshake();
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxF1::Disconnect()
{
    // If we disconnect F1, the socket would be close.
    INDI::Focuser::Disconnect();

    // Get value of PortFD, should be -1
    if (getActiveConnection() == serialConnection)
        PortFD = serialConnection->getPortFD();
    else if (getActiveConnection() == tcpConnection)
        PortFD = tcpConnection->getPortFD();
    // Then we have to disconnect the second focuser F2
    lynxDriveF2->RemoteDisconnect();

    LOGF_INFO("Value of PortFD = %d", PortFD);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
int FocusLynxF1::getPortFD()
// Would be used by F2 instance to communicate with the HUB
{
    LOGF_INFO("F1 PortFD : %d", PortFD);
    return PortFD;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxF1::updateProperties()
/* Add the HUB properties on the driver
 * Only displayed and used by the first focuser F1
 */
{
    FocusLynxBase::updateProperties();

    if (isConnected())
    {
        defineProperty(&HubTP);
        defineProperty(&WiredTP);
        defineProperty(&WifiTP);
        defineProperty(&LedNP);

        if (getHubConfig())
            LOG_INFO("HUB parameters updated.");
        else
        {
            LOG_ERROR("Failed to retrieve HUB configuration settings...");
            return false;
        }
    }
    else
    {
        deleteProperty(HubTP.name);
        deleteProperty(WiredTP.name);
        deleteProperty(WifiTP.name);
        deleteProperty(LedNP.name);
    }
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxF1::getHubConfig()
{
    char cmd[32] = {0};
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[32] = {0};
    int nbytes_read    = 0;
    int nbytes_written = 0;
    char key[16] = {0};
    char text[32] = {0};

    /* Answer from the HUB
     <FHGETHUBINFO>!
    HUB INFO
    Hub FVer = 2.0.4
    Sleeping = 0
    Wired IP = 169.254.190.196
    DHCPisOn = 1
    WF Atchd = 0
    WF Conn  = 0
    WF FVer  = 0.0.0
    WF FV OK = 0
    WF SSID  =
    WF IP    = 0.0.0.0
    WF SecMd = A
    WF SecKy =
    WF WepKI = 0
    END

     * */

    memset(response, 0, sizeof(response));

    strncpy(cmd, "<FHGETHUBINFO>", 16);
    LOGF_DEBUG("CMD <%s>", cmd);

    if (isSimulation())
    {
        strncpy(response, "HUB INFO\n", 16);
        nbytes_read = strlen(response);
    }
    else
    {
        if ((errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }

        if (!isResponseOK())
            return false;

        if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            LOGF_ERROR("%s", errmsg);
            return false;
        }
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';
        LOGF_DEBUG("RES <%s>", response);

        if (strcmp(response, "HUB INFO"))
            return false;
    }

    memset(response, 0, sizeof(response));

    // Hub Version
    if (isSimulation())
    {
        strncpy(response, "Hub FVer = 2.0.4\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    int rc = sscanf(response, "%16[^=]=%16[^\n]s", key, text);
    if (rc == 2)
    {
        HubTP.s = IPS_OK;
        IUSaveText(&HubT[0], text);
        IDSetText(&HubTP, nullptr);

        //Save localy the Version of the firmware's Hub
        strncpy(version, text, sizeof(version));

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // Sleeping status
    if (isSimulation())
    {
        strncpy(response, "Sleeping = 0\n", 16);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        HubTP.s = IPS_OK;
        IUSaveText(&HubT[1], text);
        IDSetText(&HubTP, nullptr);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // Wired IP address
    if (isSimulation())
    {
        strncpy(response, "Wired IP = 169.168.1.10\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        WiredTP.s = IPS_OK;
        IUSaveText(&WiredT[0], text);
        IDSetText(&WiredTP, nullptr);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // DHCP on/off
    if (isSimulation())
    {
        strncpy(response, "DHCPisOn = 1\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%16[^\n]s", key, text);
    if (rc == 2)
    {
        WiredTP.s = IPS_OK;
        IUSaveText(&WiredT[1], text);
        IDSetText(&WiredTP, nullptr);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // Is WIFI module present
    if (isSimulation())
    {
        strncpy(response, "WF Atchd = 1\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[0], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // Is WIFI connected
    if (isSimulation())
    {
        strncpy(response, "WF Conn  = 1\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[1], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // WIFI Version firmware
    if (isSimulation())
    {
        strncpy(response, "WF FVer  = 1.0.0\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[2], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // WIFI OK
    if (isSimulation())
    {
        strncpy(response, "WF FV OK = 1\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[3], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // WIFI SSID
    if (isSimulation())
    {
        strncpy(response, "WF SSID = FocusLynxConfig\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%32[^=]=%s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[4], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // WIFI IP adress
    if (isSimulation())
    {
        strncpy(response, "WF IP = 192.168.1.11\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[5], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // WIFI Security mode
    if (isSimulation())
    {
        strncpy(response, "WF SecMd = A\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]= %s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[6], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // WF Security key
    if (isSimulation())
    {
        strncpy(response, "WF SecKy =\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[7], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // WIFI Wep
    if (isSimulation())
    {
        strncpy(response, "WF WepKI = 0\n", 32);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }
    response[nbytes_read - 1] = '\0';
    LOGF_DEBUG("RES <%s>", response);

    rc = sscanf(response, "%16[^=]=%s", key, text);
    if (rc == 2)
    {
        WifiTP.s = IPS_OK;
        IUSaveText(&WifiT[8], text);

        LOGF_DEBUG("Text =  %s,  Key = %s", text, key);
    }
    else if (rc != 1)
        return false;

    memset(response, 0, sizeof(response));
    memset(text, 0, sizeof(text));

    // Set the light to ILDE if no module WIFI detected
    if (!strcmp(WifiT[0].text, "0"))
    {
        LOGF_INFO("Wifi module = %s", WifiT[0].text);
        WifiTP.s = IPS_IDLE;
    }
    IDSetText(&WifiTP, nullptr);

    // END is reached
    if (isSimulation())
    {
        strncpy(response, "END\n", 16);
        nbytes_read = strlen(response);
    }
    else if ( (errcode = tty_read_section(PortFD, response, 0xA, LYNXFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        LOGF_ERROR("%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read - 1] = '\0';

        // Display the response to be sure to have read the complet TTY Buffer.
        LOGF_DEBUG("RES <%s>", response);

        if (strcmp(response, "END"))
            return false;
    }

    tcflush(PortFD, TCIFLUSH);

    configurationComplete = true;

    int a, b, c, temp;
    temp = getVersion(&a, &b, &c);
    if (temp != 0)
        LOGF_INFO("Version major: %d, minor: %d, subversion: %d", a, b, c);
    else
        LOG_INFO("Couldn't get version information");

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxF1::simulationTriggered(bool enable)
{
    INDI::Focuser::simulationTriggered(enable);
    // Set the simultation mode on F1 as selected by the user
    lynxDriveF2->setSimulation(enable);
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxF1::debugTriggered(bool enable)
{
    INDI::Focuser::debugTriggered(enable);
    // Set the Debug mode on F2 as selected by the user in EKOS
    lynxDriveF2->setDebug(enable);
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxF1::setDebug(bool enable)
{
    // Call by F2 to set the Debug option
    //    INDI::DefaultDevice::setDebug(enable);
    INDI_UNUSED(enable);
}

/************************************************************************************
*
*               Second Focuser (F2)
*
*************************************************************************************/

/************************************************************************************
 *
* ***********************************************************************************/
FocusLynxF2::FocusLynxF2(const char *target)
{
    setFocusTarget(target);

    // The second focuser has no direct communication with the hub
    setSupportedConnections(CONNECTION_NONE);
}
/************************************************************************************
 *
* ***********************************************************************************/
FocusLynxF2::~FocusLynxF2()
{
}

/**************************************************************************************
*
***************************************************************************************/
bool FocusLynxF2::initProperties()
{
    FocusLynxBase::initProperties();
    // Remove from F2 to avoid confusion, already present on F1
    deleteProperty("DRIVER_INFO");
    deleteProperty("SIMULATION");
    // deleteProperty("POLLING_PERIOD");
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
const char *FocusLynxF2::getDefaultName()
{
    return FOCUSNAMEF2;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxF2::Connect()
/* Overide of connect() function
 * different for F2 or F1 focuser
 * F2 don't connect himself to the hub
 */
{
    configurationComplete = false;

    if (!lynxDriveF1->isConnected())
    {
        if (!lynxDriveF1->Connect())
        {
            LOG_INFO("Focus F1 should be connected before try to connect F2");
            return false;
        }
        lynxDriveF1->setConnected(true, IPS_OK);
        lynxDriveF1->updateProperties();
    }
    PortFD = lynxDriveF1->getPortFD(); //Get the socket descriptor open by focuser F1 connect()
    LOGF_INFO("F2 PortFD : %d", PortFD);

    if (ack())
    {
        LOG_INFO("FocusLynx is online. Getting focus parameters...");
        // as DefaultDevice::Connect() is not involved, initiate the timer.
        SetTimer(getCurrentPollingPeriod());
        return true;
    }

    LOG_INFO("Error retrieving data from FocusLynx, please ensure FocusLynx controller is powered and the port is correct.");
    return false;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxF2::Disconnect()
{
    // If we disconnect F2, No socket to close, set local PortFD to -1
    PortFD = -1;
    LOGF_INFO("%s is offline.", getDeviceName());
    LOGF_INFO("Value of F2 PortFD = %d", PortFD);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
bool FocusLynxF2::RemoteDisconnect()
{
    if (isConnected())
    {
        setConnected(false, IPS_IDLE);
        updateProperties();
    }

    // When called by F1, the PortFD should be -1; For debbug purpose
    PortFD = lynxDriveF1->getPortFD();
    LOGF_INFO("Remote disconnection: %s is offline.", getDeviceName());
    LOGF_INFO("Value of F2 PortFD = %d", PortFD);
    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxF2::setSimulation(bool enable)
{
    // call by F2 to set the Simulation option
    INDI::DefaultDevice::setSimulation(enable);
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxF2::debugTriggered(bool enable)
{
    INDI::Focuser::debugTriggered(enable);
    // Set the Debug mode on F1 as selected by the user
    //lynxDriveF1->setDebug(enable);
}

/************************************************************************************
 *
* ***********************************************************************************/
void FocusLynxF2::setDebug(bool enable)
{
    // Call by F1 to set the Debug option via EKOS
    INDI::DefaultDevice::setDebug(enable);
}
