/*
 Starlight Xpress Active Optics INDI Driver

 Copyright (c) 2012 Cloudmakers, s. r. o.
 All Rights Reserved.

 Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include "sxao.h"

#include "sxconfig.h"

#include <indicom.h>

#include <memory>
#include <string.h>
#include <connectionplugins/connectionserial.h>

static std::unique_ptr<SXAO> sxao(new SXAO);

void ISGetProperties(const char *dev)
{
    sxao->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    sxao->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    sxao->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    sxao->ISNewNumber(dev, name, values, names, num);
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
    INDI_UNUSED(root);
}

SXAO::SXAO()
{
    setVersion(VERSION_MAJOR, VERSION_MINOR);
}

const char *SXAO::getDefaultName()
{
    return "SX AO";
}

int SXAO::aoCommand(const char *request, char *response, int nbytes)
{
    if (isSimulation())
    {
        LOGF_DEBUG("simulation: command %s", request);

        if (!strcmp(request, "X"))
            strcpy(response, "Y");
        else
            strcpy(response, "*");
        return TTY_OK;
    }

    int actual;
    int rc = tty_write(PortFD, request, strlen(request), &actual);

    LOGF_DEBUG("CMD <%s>", request);

    if (rc == TTY_OK)
    {
        rc = tty_read(PortFD, response, nbytes, 10, &actual);
        response[actual] = 0;
        LOGF_DEBUG("RES <%s>", response);
    }
    else
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("aoCommand TTY error: %s", errstr);
    }

    return rc;
}

bool SXAO::initProperties()
{
    DefaultDevice::initProperties();
    initGuiderProperties(getDeviceName(), GUIDE_CONTROL_TAB);

    IUFillNumber(&AONS[0], "AO_N", "North (steps)", "%g", 0, 80, 1, 0);
    IUFillNumber(&AONS[1], "AO_S", "South (steps)", "%g", 0, 80, 1, 0);
    IUFillNumberVector(&AONSNP, AONS, 2, getDeviceName(), "AO_NS", "AO Tilt North/South", GUIDE_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);
    IUFillNumber(&AOWE[0], "AO_E", "East (steps)", "%g", 0, 80, 1, 0);
    IUFillNumber(&AOWE[1], "AO_W", "West (steps)", "%g", 0, 80, 1, 0);
    IUFillNumberVector(&AOWENP, AOWE, 2, getDeviceName(), "AO_WE", "AO Tilt East/West", GUIDE_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);
    IUFillSwitch(&Center[0], "CENTER", "Center", ISS_OFF);
    IUFillSwitch(&Center[1], "UNJAM", "Unjam", ISS_OFF);
    IUFillSwitchVector(&CenterP, Center, 2, getDeviceName(), "AO_CENTER", "AO Center", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);
    IUFillLight(&AtLimitL[0], "AT_LIMIT_N", "North", IPS_IDLE);
    IUFillLight(&AtLimitL[1], "AT_LIMIT_S", "South", IPS_IDLE);
    IUFillLight(&AtLimitL[2], "AT_LIMIT_E", "East", IPS_IDLE);
    IUFillLight(&AtLimitL[3], "AT_LIMIT_W", "West", IPS_IDLE);
    IUFillLightVector(&AtLimitLP, AtLimitL, 4, getDeviceName(), "AT_LIMIT", "At limit", MAIN_CONTROL_TAB, IPS_IDLE);
    IUFillText(&FWT[0], "FIRMWARE", "Firmware version", "V000");
    IUFillTextVector(&FWTP, FWT, 1, getDeviceName(), "INFO", "Info", OPTIONS_TAB, IP_RO, 60, IPS_IDLE);

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    addDebugControl();
    addSimulationControl();

    setDriverInterface(AO_INTERFACE | GUIDER_INTERFACE);

    return true;
}

bool SXAO::Handshake()
{
    PortFD = serialConnection->getPortFD();

    char buf[8] = {0};
    int rc = aoCommand("X", buf, 1);

    if (rc == TTY_OK)
    {
        if (!strcmp(buf, "Y"))
        {
            aoCommand("V", FWT[0].text, 4);
            if (!strcmp(FWT[0].text, "V000"))
            {
                LOG_ERROR("Firmware needs to be updated!");
                return false;
            }
            AOCenter();
            return true;
        }
        else
        {
            LOG_ERROR("Not SXAO was detected.");
            return false;
        }
    }

    return false;
}

bool SXAO::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);
        defineNumber(&AONSNP);
        defineNumber(&AOWENP);
        defineSwitch(&CenterP);
        defineText(&FWTP);
        defineLight(&AtLimitLP);
    }
    else
    {
        deleteProperty(GuideNSNP.name);
        deleteProperty(GuideWENP.name);
        deleteProperty(AONSNP.name);
        deleteProperty(AOWENP.name);
        deleteProperty(CenterP.name);
        deleteProperty(FWTP.name);
        deleteProperty(AtLimitLP.name);
    }
    return true;
}

bool SXAO::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (strcmp(name, AONSNP.name) == 0)
    {
        AONSNP.s = IPS_BUSY;
        IUUpdateNumber(&AONSNP, values, names, n);
        IDSetNumber(&AONSNP, nullptr);
        if (AONS[0].value != 0)
        {
            AONSNP.s      = AONorth(AONS[0].value) ? IPS_OK : IPS_ALERT;
            AONS[0].value = 0;
        }
        else if (AONS[1].value != 0)
        {
            AONSNP.s      = AOSouth(AONS[1].value) ? IPS_OK : IPS_ALERT;
            AONS[1].value = 0;
        }
        IDSetNumber(&AONSNP, nullptr);
        CheckLimit(false);
        return true;
    }
    else if (strcmp(name, AOWENP.name) == 0)
    {
        AOWENP.s = IPS_BUSY;
        IUUpdateNumber(&AOWENP, values, names, n);
        IDSetNumber(&AOWENP, nullptr);
        if (AOWE[0].value != 0)
        {
            AOWENP.s      = AOEast(AOWE[0].value) ? IPS_OK : IPS_ALERT;
            AOWE[0].value = 0;
        }
        else if (AOWE[1].value != 0)
        {
            AOWENP.s      = AOWest(AOWE[1].value) ? IPS_OK : IPS_ALERT;
            AOWE[1].value = 0;
        }
        IDSetNumber(&AOWENP, nullptr);
        CheckLimit(false);
        return true;
    }
    else if (!strcmp(name, GuideNSNP.name) || !strcmp(name, GuideWENP.name))
    {
        processGuiderProperties(name, values, names, n);
        return true;
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool SXAO::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(name, "AO_CENTER") == 0)
    {
        CenterP.s = IPS_BUSY;
        IDSetSwitch(&CenterP, nullptr);
        IUUpdateSwitch(&CenterP, states, names, n);
        if (Center[0].s == ISS_ON)
        {
            AOCenter();
            Center[0].s = ISS_OFF;
        }
        else if (Center[1].s == ISS_ON)
        {
            AOUnjam();
            Center[1].s = ISS_OFF;
        }
        CenterP.s = IPS_OK;
        IDSetSwitch(&CenterP, nullptr);
        CheckLimit(true);
        return true;
    }

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

IPState SXAO::GuideNorth(uint32_t ms)
{
    char buf[8];
    sprintf(buf, "MN%05d", (uint16_t)(ms / 10));
    int rc = aoCommand(buf, buf, 1);
    return (rc == TTY_OK ? IPS_OK : IPS_ALERT);
}

IPState SXAO::GuideSouth(uint32_t ms)
{
    char buf[8];
    sprintf(buf, "MS%05d", (uint16_t)(ms / 10));
    int rc = aoCommand(buf, buf, 1);
    return (rc == TTY_OK ? IPS_OK : IPS_ALERT);
}

IPState SXAO::GuideEast(uint32_t ms)
{
    char buf[8];
    sprintf(buf, "MT%05d", (uint16_t)(ms / 10));
    int rc = aoCommand(buf, buf, 1);
    return (rc == TTY_OK ? IPS_OK : IPS_ALERT);
}

IPState SXAO::GuideWest(uint32_t ms)
{
    char buf[8];
    sprintf(buf, "MW%05d", (uint16_t)(ms / 10));
    int rc = aoCommand(buf, buf, 1);
    return (rc == TTY_OK ? IPS_OK : IPS_ALERT);
}

bool SXAO::AONorth(int steps)
{
    char buf[8];
    sprintf(buf, "GN%05d", steps);
    int rc = aoCommand(buf, buf, 1);
    rc     = rc == TTY_OK && !strcmp(buf, "G");
    return rc;
}

bool SXAO::AOSouth(int steps)
{
    char buf[8];
    sprintf(buf, "GS%05d", steps);
    int rc = aoCommand(buf, buf, 1);
    rc     = rc == TTY_OK && !strcmp(buf, "G");
    return rc;
}

bool SXAO::AOEast(int steps)
{
    char buf[8];
    sprintf(buf, "GT%05d", steps);
    int rc = aoCommand(buf, buf, 1);
    rc     = rc == TTY_OK && !strcmp(buf, "G");
    return rc;
}

bool SXAO::AOWest(int steps)
{
    char buf[8];
    sprintf(buf, "GW%05d", steps);
    int rc = aoCommand(buf, buf, 1);
    rc     = rc == TTY_OK && !strcmp(buf, "G");
    return rc;
}

bool SXAO::AOCenter()
{
    char buf[8];
    int rc = aoCommand("K", buf, 1);
    return rc == TTY_OK;
}

bool SXAO::AOUnjam()
{
    char buf[8];
    int rc = aoCommand("R", buf, 1);
    return rc == TTY_OK;
}

void SXAO::CheckLimit(bool force)
{
    char buf[8];
    int rc = aoCommand("L", buf, 1);
    if (rc == TTY_OK)
    {
        char limit = buf[0];
        if (force || limit != lastLimit)
        {
            AtLimitL[0].s = (limit & 0x01) == 0x01 ? IPS_ALERT : IPS_IDLE;
            AtLimitL[1].s = (limit & 0x04) == 0x04 ? IPS_ALERT : IPS_IDLE;
            AtLimitL[2].s = (limit & 0x02) == 0x02 ? IPS_ALERT : IPS_IDLE;
            AtLimitL[3].s = (limit & 0x08) == 0x08 ? IPS_ALERT : IPS_IDLE;
            AtLimitLP.s   = (limit & 0x0F) ? IPS_ALERT : IPS_IDLE;
            IDSetLight(&AtLimitLP, nullptr);
            lastLimit = limit;
        }
    }
}
