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
#define AXIS_TAB       "Axis Settings"

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
    SetTelescopeCapability(TELESCOPE_CAN_SYNC | TELESCOPE_HAS_LOCATION,0);
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

    // Raw encoder values
    IUFillNumber(&EncoderN[AXIS1_ENCODER], "AXIS1_ENCODER", "Axis 1", "%0.f", 0, 1e6, 0, 0);
    IUFillNumber(&EncoderN[AXIS2_ENCODER], "AXIS2_ENCODER", "Axis 2", "%0.f", 0, 1e6, 0, 0);
    IUFillNumber(&EncoderN[AXIS1_RAW_ENCODER], "AXIS1_RAW_ENCODER", "RAW Axis 1", "%0.f", 0, 1e6, 0, 0);
    IUFillNumber(&EncoderN[AXIS2_RAW_ENCODER], "AXIS2_RAW_ENCODER", "RAW Axis 2", "%0.f", 0, 1e6, 0, 0);
    IUFillNumberVector(&EncoderNP, EncoderN, 4, getDeviceName(), "DCS_ENCODER", "Encoders", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Encoder Settings
    IUFillNumber(&AxisSettingsN[AXIS1_TICKS], "AXIS1_TICKS", "#1 Resolution", "%g", 256, 1e6, 0, 4096);
    IUFillNumber(&AxisSettingsN[AXIS1_DEGREE_OFFSET], "AXIS1_DEGREE_OFFSET", "#1 Degree Offset", "%g", -180, 180, 30, 0);
    IUFillNumber(&AxisSettingsN[AXIS2_TICKS], "AXIS2_TICKS", "#2 Resolution", "%g", 256, 1e6, 0, 4096);
    IUFillNumber(&AxisSettingsN[AXIS2_DEGREE_OFFSET], "AXIS2_DEGREE_OFFSET", "#2 Degree Offset", "%g", -180, 180, 30, 0);
    IUFillNumberVector(&AxisSettingsNP, AxisSettingsN, 4, getDeviceName(), "AXIS_SETTINGS", "Axis Settings", AXIS_TAB, IP_RW, 0, IPS_IDLE);

    // Offsets applied to raw encoder values to adjust them as necessary
    IUFillNumber(&EncoderOffsetN[OFFSET_AXIS1_SCALE], "OFFSET_AXIS1_SCALE", "#1 Scale", "%g", 0, 1e6, 0, 0.0390625);
    IUFillNumber(&EncoderOffsetN[OFFSET_AXIS1_OFFSET], "OFFSET_AXIS1_OFFSET", "#1 Offset", "%g", -1e6, 1e6, 0, 0);
    IUFillNumber(&EncoderOffsetN[OFFSET_AXIS2_SCALE], "OFFSET_AIXS2_SCALE", "#2 Scale", "%g", 0, 1e6, 0, 0.0390625);
    IUFillNumber(&EncoderOffsetN[OFFSET_AXIS2_OFFSET], "OFFSET_AXIS2_OFFSET", "#2 Offset", "%g", -1e6, 1e6, 0, 0);
    IUFillNumberVector(&EncoderOffsetNP, EncoderOffsetN, 4, getDeviceName(), "AXIS_OFFSET", "Offsets", AXIS_TAB, IP_RW, 0, IPS_IDLE);

    // Reverse Encoder Direction
    IUFillSwitch(&ReverseS[AXIS1_ENCODER], "AXIS1_REVERSE", "Axis 1", ISS_OFF);
    IUFillSwitch(&ReverseS[AXIS2_ENCODER], "AXIS2_REVERSE", "Axis 2", ISS_OFF);
    IUFillSwitchVector(&ReverseSP, ReverseS, 2, getDeviceName(), "AXIS_REVERSE", "Reverse", AXIS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    // Mount Type
    IUFillSwitch(&MountTypeS[0], "MOUNT_EQUATORIAL", "Equatorial", ISS_ON);
    IUFillSwitch(&MountTypeS[1], "MOUNT_ALTAZ", "AltAz", ISS_OFF);
    IUFillSwitchVector(&MountTypeSP, MountTypeS, 2, getDeviceName(), "MOUNT_TYPE", "Mount Type", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Simulation encoder values
    IUFillNumber(&SimEncoderN[AXIS1_ENCODER], "AXIS1_ENCODER", "Axis 1", "%0.f", 0, 1e6, 0, 0);
    IUFillNumber(&SimEncoderN[AXIS2_ENCODER], "AXIS2_ENCODER", "Axis 2", "%0.f", 0, 1e6, 0, 0);
    IUFillNumberVector(&SimEncoderNP, SimEncoderN, 2, getDeviceName(), "SIM_ENCODER", "Sim Encoders", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

    addAuxControls();

    return true;
}

bool DSC::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        defineNumber(&EncoderNP);
        defineNumber(&AxisSettingsNP);
        defineNumber(&EncoderOffsetNP);
        defineSwitch(&ReverseSP);
        defineSwitch(&MountTypeSP);

        if (isSimulation())
            defineNumber(&SimEncoderNP);
    }
    else
    {
        deleteProperty(EncoderNP.name);
        deleteProperty(AxisSettingsNP.name);
        deleteProperty(EncoderOffsetNP.name);
        deleteProperty(ReverseSP.name);
        deleteProperty(MountTypeSP.name);

        if (isSimulation())
            deleteProperty(SimEncoderNP.name);
    }

    return true;
}

bool DSC::saveConfigItems(FILE *fp)
{
    INDI::Telescope::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &AxisSettingsNP);
    IUSaveConfigNumber(fp, &EncoderOffsetNP);
    IUSaveConfigSwitch(fp, &ReverseSP);
    IUSaveConfigSwitch(fp, &MountTypeSP);

    return true;
}

bool DSC::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(!strcmp(name,AxisSettingsNP.name))
        {
            IUUpdateNumber(&AxisSettingsNP, values, names, n);
            AxisSettingsNP.s = IPS_OK;
            IDSetNumber(&AxisSettingsNP, NULL);
            return true;
        }

        if(!strcmp(name,EncoderOffsetNP.name))
        {
            IUUpdateNumber(&EncoderOffsetNP, values, names, n);
            EncoderOffsetNP.s = IPS_OK;
            IDSetNumber(&EncoderOffsetNP, NULL);
            return true;
        }

        if(!strcmp(name,SimEncoderNP.name))
        {
            IUUpdateNumber(&SimEncoderNP, values, names, n);
            SimEncoderNP.s = IPS_OK;
            IDSetNumber(&SimEncoderNP, NULL);
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev,name,values,names,n);
}

bool DSC::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if(!strcmp(name,ReverseSP.name))
        {
            IUUpdateSwitch(&ReverseSP, states, names, n);
            ReverseSP.s = IPS_OK;
            IDSetSwitch(&ReverseSP, NULL);
            return true;
        }

        if(!strcmp(name,MountTypeSP.name))
        {
            IUUpdateSwitch(&MountTypeSP, states, names, n);
            MountTypeSP.s = IPS_OK;
            IDSetSwitch(&MountTypeSP, NULL);
            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool DSC::Handshake()
{
    return true;
}

bool DSC::ReadScopeStatus()
{
    // Send 'Q'
    char CR[1] = { 0x51 };
    // Response
    char response[16] = {0};
    int rc=0, nbytes_read=0, nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD: %#02X", CR[0]);

    if (isSimulation())
    {
        snprintf(response, 16, "%06.f\t%06.f", SimEncoderN[AXIS1_ENCODER].value, SimEncoderN[AXIS2_ENCODER].value);
    }
    else
    {
        tcflush(PortFD, TCIFLUSH);

        if ( (rc = tty_write(PortFD, CR, 1, &nbytes_written)) != TTY_OK)
        {
            char errmsg[256];
            tty_error_msg(rc, errmsg, 256);
            DEBUGF(INDI::Logger::DBG_ERROR, "Error writing to device %s (%d)", errmsg, rc);
            return false;
        }


        // Read until we encounter a CR
        if ( (rc = tty_read_section(PortFD, response, 0x0D, DSC_TIMEOUT, &nbytes_read)) != TTY_OK)
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
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "RES: %s", response);

    double Axis1Encoder=0, Axis2Encoder=0;
    std::regex rgx("(\\+?\\-?\\d+)\\s(\\+?\\-?\\d+)");
    std::smatch match;
    std::string input(response);
    if (std::regex_search(input, match, rgx))
    {
        Axis1Encoder = atof(match.str(1).c_str());
        Axis2Encoder = atof(match.str(2).c_str());
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Error processing response: %s", response );
        EncoderNP.s = IPS_ALERT;
        IDSetNumber(&EncoderNP, NULL);
        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Raw Axis encoders. Axis1: %g Axis2: %g", Axis1Encoder, Axis2Encoder);

    // Calculate reverse values
    double Axis1 = Axis1Encoder;
    if (ReverseS[AXIS1_ENCODER].s == ISS_ON)
        Axis1 = AxisSettingsN[AXIS1_TICKS].value - Axis1;

    #if 0
    if (Axis1Diff < 0)
        Axis1Diff += AxisSettingsN[AXIS1_TICKS].value;
    else if (Axis1Diff > AxisSettingsN[AXIS1_TICKS].value)
        Axis1Diff -= AxisSettingsN[AXIS1_TICKS].value;
    #endif

    double Axis2 = Axis2Encoder;
    if (ReverseS[AXIS2_ENCODER].s == ISS_ON)
        Axis2 = AxisSettingsN[AXIS2_TICKS].value - Axis2;

    DEBUGF(INDI::Logger::DBG_DEBUG, "Axis encoders after reverse. Axis1: %g Axis2: %g", Axis1, Axis2);

    // Apply raw offsets
    Axis1 = (Axis1 * EncoderOffsetN[OFFSET_AXIS1_SCALE].value + EncoderOffsetN[OFFSET_AXIS1_OFFSET].value);
    Axis2 = (Axis2 * EncoderOffsetN[OFFSET_AXIS2_SCALE].value + EncoderOffsetN[OFFSET_AXIS2_OFFSET].value);

    DEBUGF(INDI::Logger::DBG_DEBUG, "Axis encoders after raw offsets. Axis1: %g Axis2: %g", Axis1, Axis2);

    EncoderN[AXIS1_ENCODER].value = Axis1;
    EncoderN[AXIS2_ENCODER].value = Axis2;
    EncoderN[AXIS1_RAW_ENCODER].value = Axis1Encoder;
    EncoderN[AXIS2_RAW_ENCODER].value = Axis2Encoder;
    EncoderNP.s = IPS_OK;
    IDSetNumber(&EncoderNP, NULL);

    double Axis1Degrees = (Axis1 / AxisSettingsN[AXIS1_TICKS].value * 360.0) + AxisSettingsN[AXIS1_DEGREE_OFFSET].value;
    double Axis2Degrees = (Axis2 / AxisSettingsN[AXIS2_TICKS].value * 360.0) + AxisSettingsN[AXIS2_DEGREE_OFFSET].value;

    Axis1Degrees = range360(Axis1Degrees);
    Axis2Degrees = range360(Axis2Degrees);

    double RA=0, DE=0;
    // Now we proceed depending on mount type
    if (MountTypeS[MOUNT_EQUATORIAL].s == ISS_ON)
    {
        RA = Axis1Degrees / 15.0;

        // Adjust for LST
        double LST = get_local_sideral_time(observer.lng);

        RA += LST;
        RA = range24(RA);

        DE = rangeDec(Axis2Degrees);
    }
    else
    {
        ln_hrz_posn horizontalPos;
        // Libnova south = 0, west = 90, north = 180, east = 270
        horizontalPos.az = Axis1Degrees + 180;
        if (horizontalPos.az >= 360)
            horizontalPos.az -= 360;
        horizontalPos.alt = Axis2Degrees;

        char AzStr[64], AltStr[64];
        fs_sexa(AzStr, horizontalPos.az, 2, 3600);
        fs_sexa(AltStr, DE, horizontalPos.alt, 3600);
        DEBUGF(INDI::Logger::DBG_DEBUG, "Current Az: %s Current Alt: %s", AzStr, AltStr);

        ln_equ_posn equatorialPos;
        ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);
        equatorialPos.ra /= 15.0;

        RA = range24(equatorialPos.ra);
        DE = rangeDec(equatorialPos.dec);
    }

    char RAStr[64], DecStr[64];
    fs_sexa(RAStr, RA, 2, 3600);
    fs_sexa(DecStr, DE, 2, 3600);
    DEBUGF(INDI::Logger::DBG_DEBUG, "Current RA: %s Current DEC: %s", RAStr, DecStr);

    NewRaDec(RA, DE);
    return true;
}

bool DSC::Sync(double ra, double dec)
{
    return false;
}

bool DSC::updateLocation(double latitude, double longitude, double elevation)
{
  INDI_UNUSED(elevation);
  // JM: INDI Longitude is 0 to 360 increasing EAST. libnova East is Positive, West is negative
  observer.lng =  longitude;

  if (observer.lng > 180)
      observer.lng -= 360;
  observer.lat =  latitude;

  DEBUGF(INDI::Logger::DBG_SESSION,"Location updated: Longitude (%g) Latitude (%g)", observer.lng, observer.lat);
  return true;
}

void DSC::simulationTriggered(bool enable)
{
    if (isConnected() == false)
        return;

    if (enable)
    {
        defineNumber(&SimEncoderNP);
    }
    else
    {
        deleteProperty(SimEncoderNP.name);
    }
}
