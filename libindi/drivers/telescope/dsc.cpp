/*******************************************************************************
 Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 It just gets the encoder positoin and outputs current coordinates.
 Calibratoin and syncing not supported yet.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <regex>

#include "dsc.h"
#include "indicom.h"

#include <memory>

#define DSC_TIMEOUT    2

// We declare an auto pointer to DSC.
std::unique_ptr<DSC> dsc(new DSC());

void ISGetProperties(const char *dev)
{
        dsc->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        dsc->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        dsc->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        dsc->ISNewNumber(dev, name, values, names, num);
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
   dsc->ISSnoopDevice(root);
}

DSC::DSC()
{    
    SetTelescopeCapability(0,0);
}

DSC::~DSC()
{
}

const char * DSC::getDefaultName()
{
    return (char *)"Digital Setting Circle";
}

bool DSC::initProperties()
{
    INDI::Telescope::initProperties();

    IUFillNumber(&EncoderN[RA_ENCODER], "RA_ENCODER", "RA", "%0.f", 0, 1e6, 0, 0);
    IUFillNumber(&EncoderN[DE_ENCODER], "DE_ENCODER", "DE", "%0.f", 0, 1e6, 0, 0);
    IUFillNumberVector(&EncoderNP, EncoderN, 2, getDeviceName(), "DCS_ENCODER", "Encoders", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Offsets applied to raw encoder values to adjust them as necessary
    IUFillNumber(&OffsetN[OFFSET_RA_SCALE], "OFFSET_RA_SCALE", "RA Scale", "%g", 0, 1e6, 0, 0.0390625);
    IUFillNumber(&OffsetN[OFFSET_RA_OFFSET], "OFFSET_RA_OFFSET", "RA Offset", "%g", -1e6, 1e6, 0, 0);
    IUFillNumber(&OffsetN[OFFSET_DE_SCALE], "OFFSET_DE_SCALE", "DE Scale", "%g", 0, 1e6, 0, 0.0390625);
    IUFillNumber(&OffsetN[OFFSET_DE_OFFSET], "OFFSET_DE_OFFSET", "DE Offset", "%g", -1e6, 1e6, 0, 0);
    IUFillNumberVector(&OffsetNP, OffsetN, 4, getDeviceName(), "DCS_OFFSET", "Offsets", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    return true;
}

bool DSC::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineNumber(&EncoderNP);
        defineNumber(&OffsetNP);
    }
    else
    {
        deleteProperty(OffsetNP.name);
        deleteProperty(EncoderNP.name);
    }

    return true;
}

bool DSC::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &OffsetNP);

    return true;
}

bool DSC::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
         if(strcmp(name,OffsetNP.name)==0)
         {
             IUUpdateNumber(&OffsetNP, values, names, n);
             OffsetNP.s = IPS_OK;
             IDSetNumber(&OffsetNP, NULL);
             return true;
         }
    }

    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool DSC::Handshake()
{
    DEBUG(INDI::Logger::DBG_SESSION, "RA and DEC values are determined form raw encoder values. RA = (encoder * scale) + offset");
    return true;
}

bool DSC::ReadScopeStatus()
{
    // Send 'Q'
    char CR[1] = { 0x51 };
    int rc=0, nbytes_read=0, nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %#02X", CR[0]);

    tcflush(PortFD, TCIFLUSH);

    if ( (rc = tty_write(PortFD, CR, 1, &nbytes_written)) != TTY_OK)
    {
        char errmsg[256];
        tty_error_msg(rc, errmsg, 256);
        DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
        return false;
    }

    char coords[16] = {0};
    // Read until we encounter a CR
    if ( (rc = tty_read_section(PortFD, coords, 0x0D, DSC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        // if we read enough, let's try to process, otherwise throw error
        // 6 characters for each number
        if (nbytes_read < 12)
        {
            char errmsg[256];
            tty_error_msg(rc, errmsg, 256);
            DEBUGF(INDI::Logger::DBG_ERROR, "Error reading from device %s (%d)", errmsg, rc);
            return false;
        }
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", coords);

    double RAEncoder=0, DEEncoder=0;
    std::regex rgx("(\\+?\\-?\\d+)\\s(\\+?\\-?\\d+)");
    std::smatch match;
    std::string input(coords);
    if (std::regex_search(input, match, rgx))
    {
        RAEncoder = atof(match.str(1).c_str());
        DEEncoder = atof(match.str(2).c_str());
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error processing response: %s", coords );
        EncoderNP.s = IPS_ALERT;
        IDSetNumber(&EncoderNP, NULL);
        return false;
    }

    EncoderN[RA_ENCODER].value = RAEncoder;
    EncoderN[DE_ENCODER].value = DEEncoder;
    EncoderNP.s = IPS_OK;
    IDSetNumber(&EncoderNP, NULL);

    double RA = RAEncoder * OffsetN[OFFSET_RA_SCALE].value + OffsetN[OFFSET_RA_OFFSET].value;
    double DE = DEEncoder * OffsetN[OFFSET_DE_SCALE].value + OffsetN[OFFSET_DE_OFFSET].value;

    char RAStr[64], DecStr[64];
    fs_sexa(RAStr, RA, 2, 3600);
    fs_sexa(DecStr, DE, 2, 3600);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(RA, DE);
    return true;
}

