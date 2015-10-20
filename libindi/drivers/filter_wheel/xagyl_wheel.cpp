/*******************************************************************************
  Copyright(c) 2015 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include <memory>

#include "indicom.h"
#include "xagyl_wheel.h"

#define XAGYL_MAXBUF    32
#define SETTINGS_TAB    "Settings"

// We declare an auto pointer to XAGYLWheel.
std::auto_ptr<XAGYLWheel> xagylWheel(0);

void ISPoll(void *p);


void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(xagylWheel.get() == 0) xagylWheel.reset(new XAGYLWheel());

}

void ISGetProperties(const char *dev)
{
        ISInit();
        xagylWheel->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        xagylWheel->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        xagylWheel->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        xagylWheel->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
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
void ISSnoopDevice (XMLEle *root)
{
    xagylWheel->ISSnoopDevice(root);
}


XAGYLWheel::XAGYLWheel()
{
    PortFD=-1;
    sim = false;
}

XAGYLWheel::~XAGYLWheel()
{

}

const char *XAGYLWheel::getDefaultName()
{
    return (char *)"XAGYL Wheel";
}

bool XAGYLWheel::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Device port
    IUFillText(&PortT[0],"PORT","Port","/dev/ttyUSB0");
    IUFillTextVector(&PortTP,PortT,1,getDeviceName(),"DEVICE_PORT","Ports",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    // Firmware info
    IUFillText(&FirmwareInfoT[0],"Product","",NULL);
    IUFillText(&FirmwareInfoT[1],"Firmware","",NULL);
    IUFillText(&FirmwareInfoT[2],"Serial #","",NULL);
    IUFillTextVector(&FirmwareInfoTP,FirmwareInfoT,3,getDeviceName(),"Info","",MAIN_CONTROL_TAB,IP_RO,60,IPS_IDLE);

    // Settings
    IUFillNumber(&SettingsN[0], "Max Speed", "", "%.f", 0, 100, 1., 0.);
    IUFillNumber(&SettingsN[1], "Jitter", "", "%.f", 0, 10, 1., 0.);
    IUFillNumber(&SettingsN[2], "Threshold", "", "%.f", 0, 100, 1., 0.);
    IUFillNumber(&SettingsN[3], "Pulse Width", "", "%.f", 100, 10000, 1., 0.);
    IUFillNumberVector(&SettingsNP, SettingsN, 4, getDeviceName(), "Settings", "", SETTINGS_TAB, IP_RO, 0, IPS_IDLE);

    // Max Speed
    IUFillSwitch(&MaxSpeedS[0], "+", "", ISS_OFF);
    IUFillSwitch(&MaxSpeedS[1], "-", "", ISS_OFF);
    IUFillSwitchVector(&MaxSpeedSP, MaxSpeedS, 2, getDeviceName(), "Max Speed", "", SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Jitter
    IUFillSwitch(&JitterS[0], "+", "", ISS_OFF);
    IUFillSwitch(&JitterS[1], "-", "", ISS_OFF);
    IUFillSwitchVector(&JitterSP, JitterS, 2, getDeviceName(), "Jitter", "", SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Threshold
    IUFillSwitch(&ThresholdS[0], "+", "", ISS_OFF);
    IUFillSwitch(&ThresholdS[1], "-", "", ISS_OFF);
    IUFillSwitchVector(&ThresholdSP, ThresholdS, 2, getDeviceName(), "Threshold", "", SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Pulse Width
    IUFillSwitch(&PulseWidthS[0], "+", "", ISS_OFF);
    IUFillSwitch(&PulseWidthS[1], "-", "", ISS_OFF);
    IUFillSwitchVector(&PulseWidthSP, PulseWidthS, 2, getDeviceName(), "Pulse Width", "", SETTINGS_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    return true;
}

void XAGYLWheel::ISGetProperties (const char *dev)
{
    INDI::FilterWheel::ISGetProperties(dev);

    defineText(&PortTP);
    loadConfig(true, "DEVICE_PORT");
}

bool XAGYLWheel::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        bool rc = getBasicData();

        if (rc == false)
            return false;

    }
    else
    {

    }

    return true;
}

bool XAGYLWheel::Connect()
{
    int connectrc=0;
    char errorMsg[MAXRBUF];

    sim = isSimulation();

    if (!sim && (connectrc = tty_connect(PortT[0].text, 9600, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(INDI::Logger::DBG_SESSION, "Failed to connect to port %s. Error: %s", PortT[0].text, errorMsg);

        return false;
    }

    if (Ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "XAGYL is online. Getting filter parameters...");
        return true;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Error retreiving data from XAGYL Filter Wheel, please ensure filter wheel is powered and the port is correct.");
    return false;
}

bool XAGYLWheel::Disconnect()
{    
    tty_disconnect(PortFD);
    DEBUG(Logger::DBG_SESSION,"XAGYL is offline.");

    return true;
}

bool XAGYLWheel::getMaxFilterSlots()
{
    int nbytes_written=0, nbytes_read=0, rc=-1;
    char errstr[MAXRBUF];
    char resp[XAGYL_MAXBUF];

    tcflush(PortFD, TCIOFLUSH);

    DEBUG(INDI::Logger::DBG_DEBUG, "CMD (I8)");

    if (!sim && (rc = tty_write(PortFD, "I8", 2, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "I8 error: %s.", errstr);
        return false;
    }

    if (sim)
    {
        strncpy(resp, "FilterSlots 5", XAGYL_MAXBUF);
        nbytes_read=strlen(resp);
    }
    else if ( (rc = tty_read(PortFD, resp, 13, XAGYL_MAXBUF, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "I2: %s.", errstr);
        return false;
    }

    resp[nbytes_read] = '\0';

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", resp);

    int maxFilterSlots=0;
    rc = sscanf(resp, "FilterSlots#", &maxFilterSlots);

    if (rc > 0)
    {
        FilterSlotN[0].max = maxFilterSlots;
        return true;
    }
    else
        return false;
}

bool XAGYLWheel::SelectFilter(int f)
{
    CurrentFilter=f;
    SetTimer(500);
    return true;
}

void XAGYLWheel::TimerHit()
{
    SelectFilterDone(CurrentFilter);
}

bool XAGYLWheel::GetFilterNames(const char* groupName)
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];
    int MaxFilter = FilterSlotN[0].max;

    if (FilterNameT != NULL)
        delete FilterNameT;

    FilterNameT = new IText[MaxFilter];

    for (int i=0; i < MaxFilter; i++)
    {
        snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i+1);
        snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i+1);
        IUFillText(&FilterNameT[i], filterName, filterLabel, filterLabel);
    }

    IUFillTextVector(FilterNameTP, FilterNameT, MaxFilter, getDeviceName(), "FILTER_NAME", "Filter", groupName, IP_RW, 0, IPS_IDLE);

    return true;
}

bool XAGYLWheel::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);

    IUSaveConfigText(fp, &PortTP);
}
