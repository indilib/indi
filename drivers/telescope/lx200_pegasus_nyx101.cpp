/*******************************************************************************
  Copyright(c) 2021 Chrysikos Efstathios. All rights reserved.

  Pegasus NYX

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

#include "lx200_pegasus_nyx101.h"
#include "lx200driver.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include <libnova/transform.h>

#include <cmath>
#include <cstring>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <regex>

const char *SETTINGS_TAB  = "Settings";
const char *STATUS_TAB = "Status";

LX200NYX101::LX200NYX101()
{
    setVersion(1, 0);

    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK |
                           TELESCOPE_CAN_SYNC |
                           TELESCOPE_CAN_GOTO |
                           TELESCOPE_CAN_ABORT |
                           TELESCOPE_CAN_CONTROL_TRACK |
                           TELESCOPE_HAS_TIME |
                           TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TRACK_MODE,
                                 SLEW_MODES);
}

bool LX200NYX101::initProperties()
{
    LX200Generic::initProperties();

    SetParkDataType(PARK_NONE);
    timeFormat = LX200_24;
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    // Mount Type
    int mountType = Equatorial;
    IUGetConfigOnSwitchIndex(getDeviceName(), "MOUNT_TYPE", &mountType);
    MountTypeSP[AltAz].fill("AltAz", "AltAz", mountType == AltAz ? ISS_ON : ISS_OFF);
    MountTypeSP[Equatorial].fill("Equatorial", "Equatorial", mountType == Equatorial ? ISS_ON : ISS_OFF);
    MountTypeSP.fill(getDeviceName(), "MOUNT_TYPE", "Mount Type", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    if (mountType == Equatorial)
        SetTelescopeCapability(GetTelescopeCapability() | TELESCOPE_HAS_PIER_SIDE, SLEW_MODES);

    // Guide Rate
    int guideRate = 1;
    IUGetConfigOnSwitchIndex(getDeviceName(), "GUIDE_RATE", &guideRate);
    GuideRateSP[0].fill("0.25","0.25", guideRate == 0 ? ISS_ON : ISS_OFF);
    GuideRateSP[1].fill("0.50","0.50", guideRate == 1 ? ISS_ON : ISS_OFF);
    GuideRateSP[2].fill("1.00","1.00", guideRate == 2 ? ISS_ON : ISS_OFF);
    GuideRateSP.fill(getDeviceName(), "GUIDE_RATE", "Guide Rate", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //Go Home
    HomeSP[0].fill("Home", "Go", ISS_OFF);
    HomeSP.fill(getDeviceName(), "HOME_GO", "Home go", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    //Reset Home
    ResetHomeSP[0].fill("Home", "Reset", ISS_OFF);
    ResetHomeSP.fill(getDeviceName(), "HOME_RESET", "Home Reset", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    verboseReport = false;
    VerboseReportSP[0].fill("On","On",  ISS_OFF);
    VerboseReportSP[1].fill("Off","Off", ISS_ON);
    VerboseReportSP.fill(getDeviceName(), "REPORT_VERBOSE", "Verbose", STATUS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    Report[0].fill("Report","GU","-");
    Report.fill(getDeviceName(), "Report", "Report", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    IsTracking[0].fill("IsTracking","n","-");
    IsTracking.fill(getDeviceName(),"IsTracking","IsTracking",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    IsSlewCompleted[0].fill("IsSlewCompleted","N","-");
    IsSlewCompleted.fill(getDeviceName(),"IsSlewCompleted","IsSlewCompleted",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    IsParked[0].fill("IsParked","p/P","-");
    IsParked.fill(getDeviceName(),"IsParked","IsParked",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    IsParkginInProgress[0].fill("IsParkginInProgress","I","-");
    IsParkginInProgress.fill(getDeviceName(),"IsParkginInProgress","IsParkginInProgress",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    IsAtHomePosition[0].fill("IsAtHomePosition","H","-");
    IsAtHomePosition.fill(getDeviceName(),"IsAtHomePosition","IsAtHomePosition",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    TrackSidereal[0].fill("TrackSidereal","","-");
    TrackSidereal.fill(getDeviceName(),"TrackSidereal","TrackSidereal",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    TrackLunar[0].fill("TrackLunar","(","-");
    TrackLunar.fill(getDeviceName(),"TrackLunar","TrackLunar",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    TrackSolar[0].fill("TrackSolar","O","-");
    TrackSolar.fill(getDeviceName(),"TrackSolar","TrackSolar",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    MountAltAz[0].fill("MountAltAz","A","-");
    MountAltAz.fill(getDeviceName(),"MountAltAz","MountAltAz",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    MountEquatorial[0].fill("MountEquatorial","E","-");
    MountEquatorial.fill(getDeviceName(),"MountEquatorial","MountEquatorial",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    PierNone[0].fill("PierNone","","-");
    PierNone.fill(getDeviceName(),"PierNone","PierNone",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    PierEast[0].fill("PierEast","T","-");
    PierEast.fill(getDeviceName(),"PierEast","PierEast",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    PierWest[0].fill("PierWest","W","-");
    PierWest.fill(getDeviceName(),"PierWest","PierWest",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    DoesRefractionComp[0].fill("DoesRefractionComp","r","-");
    DoesRefractionComp.fill(getDeviceName(),"DoesRefractionComp","DoesRefractionComp",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    WaitingAtHome[0].fill("WaitingAtHome","w","-");
    WaitingAtHome.fill(getDeviceName(),"WaitingAtHome","WaitingAtHome",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    IsHomePaused[0].fill("IsHomePaused","u","-");
    IsHomePaused.fill(getDeviceName(),"IsHomePaused","IsHomePaused",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    ParkFailed[0].fill("ParkFailed","F","-");
    ParkFailed.fill(getDeviceName(),"ParkFailed","ParkFailed",STATUS_TAB, IP_RO, 60, IPS_IDLE);

    SlewingHome[0].fill("SlewingHome","h","-");
    SlewingHome.fill(getDeviceName(),"SlewingHome","SlewingHome",STATUS_TAB, IP_RO, 60, IPS_IDLE);


    // Slew Rates
    strncpy(SlewRateS[0].label, "2x", MAXINDILABEL);
    strncpy(SlewRateS[1].label, "8x", MAXINDILABEL);
    strncpy(SlewRateS[2].label, "16x", MAXINDILABEL);
    strncpy(SlewRateS[3].label, "64x", MAXINDILABEL);
    strncpy(SlewRateS[4].label, "128x", MAXINDILABEL);
    strncpy(SlewRateS[5].label, "200x", MAXINDILABEL);
    strncpy(SlewRateS[6].label, "300x", MAXINDILABEL);
    strncpy(SlewRateS[7].label, "600x", MAXINDILABEL);
    strncpy(SlewRateS[8].label, "900x", MAXINDILABEL);
    strncpy(SlewRateS[9].label, "1200x", MAXINDILABEL);
    IUResetSwitch(&SlewRateSP);

    SlewRateS[9].s = ISS_ON;


    return true;
}

bool LX200NYX101::updateProperties()
{
    LX200Generic::updateProperties();

    if (isConnected())
    {
        char status[DRIVER_LEN] = {0};
        if (sendCommand(":GU#", status))
        {
            if(strchr(status,'P'))
                SetParked(true);
            else
                SetParked(false);

            MountTypeSP.reset();
            MountTypeSP[AltAz].setState(strchr(status, 'A') ? ISS_ON : ISS_OFF);
            MountTypeSP[Equatorial].setState(strchr(status, 'E') ? ISS_ON : ISS_OFF);
            MountTypeSP.setState(IPS_OK);
            MountTypeSP.apply();
        }

        if(sendCommand(":GX90#", status))
        {
            std::string c = status;

            GuideRateSP.reset();
            GuideRateSP[0].setState((c.find("0.25") != std::string::npos) ? ISS_ON : ISS_OFF);
            GuideRateSP[1].setState((c.find("0.50") != std::string::npos) ? ISS_ON : ISS_OFF);
            GuideRateSP[2].setState((c.find("1.00") != std::string::npos) ? ISS_ON : ISS_OFF);
            GuideRateSP.setState((IPS_OK));
            GuideRateSP.apply();
        }

        defineProperty(MountTypeSP);
        defineProperty(GuideRateSP);
        defineProperty(HomeSP);
        defineProperty(ResetHomeSP);
        defineProperty(Report);
        defineProperty(VerboseReportSP);
        defineProperty(IsTracking);
        defineProperty(IsSlewCompleted);
        defineProperty(IsParked);
        defineProperty(IsParkginInProgress);
        defineProperty(IsAtHomePosition);
        defineProperty(TrackSidereal);
        defineProperty(TrackLunar);
        defineProperty(TrackSolar);
        defineProperty(MountAltAz);
        defineProperty(MountEquatorial);
        defineProperty(PierNone);
        defineProperty(PierEast);
        defineProperty(PierWest);
        defineProperty(DoesRefractionComp);
        defineProperty(WaitingAtHome);
        defineProperty(IsHomePaused);
        defineProperty(ParkFailed);
        defineProperty(SlewingHome);
    }
    else
    {
        deleteProperty(MountTypeSP);
        deleteProperty(GuideRateSP);
        deleteProperty(HomeSP);
        deleteProperty(ResetHomeSP);
        deleteProperty(Report);
        deleteProperty(VerboseReportSP);
        deleteProperty(IsTracking);
        deleteProperty(IsSlewCompleted);
        deleteProperty(IsParked);
        deleteProperty(IsParkginInProgress);
        deleteProperty(IsAtHomePosition);
        deleteProperty(TrackSidereal);
        deleteProperty(TrackLunar);
        deleteProperty(TrackSolar);
        deleteProperty(MountAltAz);
        deleteProperty(MountEquatorial);
        deleteProperty(PierNone);
        deleteProperty(PierEast);
        deleteProperty(PierWest);
        deleteProperty(DoesRefractionComp);
        deleteProperty(WaitingAtHome);
        deleteProperty(IsHomePaused);
        deleteProperty(ParkFailed);
        deleteProperty(SlewingHome);

    }

    return true;
}


const char *LX200NYX101::getDefaultName()
{
    return "Pegasus NYX-101";
}

const char *ON = "ON";
const char *OFF = "OFF";

void LX200NYX101::SetPropertyText(INDI::PropertyText propertyTxt, IPState state)
{
    if(!verboseReport)
        return;

    if(state == IPS_OK)
    {
        propertyTxt[0].setText(ON);
    }
    else if(state == IPS_BUSY)
    {
        propertyTxt[0].setText(OFF);
    }
    else if(state == IPS_IDLE)
    {
        propertyTxt[0].setText("-");
    }
    propertyTxt.setState(state);
    propertyTxt.apply();
}

bool LX200NYX101::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    bool _IsTracking = true;
    SetPropertyText(IsTracking, IPS_OK);

    bool _IsSlewCompleted = false;
    SetPropertyText(IsSlewCompleted, IPS_BUSY);

    bool _IsParked = false;
    SetPropertyText(IsParked, IPS_BUSY);

    //bool _IsParkginInProgress = false;
    SetPropertyText(IsParkginInProgress, IPS_BUSY);

    //bool _IsAtHomePosition = false;
    SetPropertyText(IsAtHomePosition, IPS_BUSY);

    TelescopeTrackMode _TrackingMode = TRACK_SIDEREAL;

    //MountType _MountType = Equatorial;

    TelescopePierSide _PierSide = PIER_UNKNOWN;

    //bool _DoesRefractionComp = false;
    SetPropertyText(DoesRefractionComp, IPS_BUSY);

    //bool _WaitingAtHome = false;
    SetPropertyText(WaitingAtHome, IPS_BUSY);

    //bool _IsHomePaused = false;
    SetPropertyText(IsHomePaused, IPS_BUSY);

    //bool _ParkFailed = false;
    SetPropertyText(ParkFailed, IPS_BUSY);

    //bool _SlewingHome = false;
    SetPropertyText(SlewingHome, IPS_BUSY);




    char status[DRIVER_LEN] = {0};    
    if(sendCommand(":GU#", status))
    {
        Report[0].text = status;
        Report.apply();
        int index = 0;
        while(true)
        {
            switch (status[index++])
            {
            case 'n':
                _IsTracking = false;
                SetPropertyText(IsTracking, IPS_BUSY);
                continue;
            case 'N':
                _IsSlewCompleted = true;
                 SetPropertyText(IsSlewCompleted, IPS_OK);
                continue;
            case 'p':
                _IsParked = false;
                SetPropertyText(IsParked, IPS_BUSY);
                continue;
            case 'P':
                _IsParked = true;
                SetPropertyText(IsParked, IPS_OK);
                continue;
            case 'I':
                //_IsParkginInProgress = true;
                SetPropertyText(IsParkginInProgress, IPS_OK);
                continue;
            case 'H':
                //_IsAtHomePosition = true;
                SetPropertyText(IsAtHomePosition, IPS_OK);
                continue;
            case '(':
                _TrackingMode = TRACK_LUNAR;
                continue;
            case 'O':
                _TrackingMode = TRACK_SOLAR;
                continue;
            case 'k':
                //Not Supported by TelescopeTrackMode
                continue;
            case 'A':
                //_MountType = AltAz;
                SetPropertyText(MountAltAz, IPS_OK);
                SetPropertyText(MountEquatorial, IPS_BUSY);
                continue;
            case 'E':
                //_MountType = Equatorial;
                SetPropertyText(MountEquatorial, IPS_OK);
                SetPropertyText(MountAltAz, IPS_BUSY);
                continue;
            case 'T':
                _PierSide = PIER_EAST;
                continue;
            case 'W':
                _PierSide = PIER_WEST;
                continue;
            case 'r':
                //_DoesRefractionComp = true;
                SetPropertyText(DoesRefractionComp, IPS_OK);
                continue;
            case 'w':
                //_WaitingAtHome = true;
                SetPropertyText(WaitingAtHome, IPS_OK);
                continue;
            case 'u':
                //_IsHomePaused = true;
                SetPropertyText(IsHomePaused, IPS_OK);
                continue;
            case 'F':
                //_ParkFailed = true;
                SetPropertyText(ParkFailed, IPS_OK);
                continue;
            case 'h':
                //_SlewingHome = true;
                SetPropertyText(SlewingHome, IPS_OK);
                continue;
            case '#':
                break;
            default:
                continue;
            }
            break;
        }
    }

    switch(_TrackingMode)
    {
        case INDI::Telescope::TRACK_SIDEREAL:
            SetPropertyText(TrackSidereal, IPS_OK);
            SetPropertyText(TrackLunar, IPS_BUSY);
            SetPropertyText(TrackSolar, IPS_BUSY);
            break;
        case INDI::Telescope::TRACK_LUNAR:
            SetPropertyText(TrackLunar, IPS_OK);
            SetPropertyText(TrackSidereal, IPS_BUSY);
            SetPropertyText(TrackSolar, IPS_BUSY);
            break;
        case INDI::Telescope::TRACK_SOLAR:
            SetPropertyText(TrackSolar, IPS_OK);
            SetPropertyText(TrackSidereal, IPS_BUSY);
            SetPropertyText(TrackLunar, IPS_BUSY);
            break;
        case INDI::Telescope::TRACK_CUSTOM:
            break;
    }

    switch(_PierSide)
    {
        case INDI::Telescope::PIER_UNKNOWN:
            SetPropertyText(PierNone, IPS_OK);
            SetPropertyText(PierEast, IPS_BUSY);
            SetPropertyText(PierWest, IPS_BUSY);
            break;
        case INDI::Telescope::PIER_EAST:
            SetPropertyText(PierEast, IPS_OK);
            SetPropertyText(PierNone, IPS_BUSY);
            SetPropertyText(PierWest, IPS_BUSY);
            break;
        case INDI::Telescope::PIER_WEST:
            SetPropertyText(PierWest, IPS_OK);
            SetPropertyText(PierEast, IPS_BUSY);
            SetPropertyText(PierNone, IPS_BUSY);
            break;
    }


    if (TrackState == SCOPE_SLEWING)
    {
        if (_IsSlewCompleted)
        {
            TrackState = SCOPE_TRACKING;
            LOG_INFO("Slew is complete. Tracking...");
        }
    }
    else if (TrackState != SCOPE_PARKED && _IsParked)
    {
        SetParked(true);
    }
    else
    {
        auto wasTracking = TrackStateS[INDI_ENABLED].s == ISS_ON;
        if (wasTracking != _IsTracking)
            TrackState = _IsTracking ? SCOPE_TRACKING : SCOPE_IDLE;
    }


    if (getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading Ra - Dec");
        return false;
    }

    if (HasPierSide())
    {
        char response[DRIVER_LEN] = {0};
        if (sendCommand(":Gm#", response))
        {
            if (response[0] == 'W')
                setPierSide(PIER_WEST);
            else if (response[0] == 'E')
                setPierSide(PIER_EAST);
            else
                setPierSide(PIER_UNKNOWN);
        }
    }

    NewRaDec(currentRA, currentDEC);

    return true;
}

bool LX200NYX101::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}

bool LX200NYX101::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Mount Type
        if (MountTypeSP.isNameMatch(name))
        {
            int previousType = MountTypeSP.findOnSwitchIndex();
            MountTypeSP.update(states, names, n);
            IPState state = IPS_OK;
            if (isConnected())
            {
                auto targetType = MountTypeSP.findOnSwitchIndex();
                state = setMountType(targetType) ? IPS_OK : IPS_ALERT;
                if (state == IPS_OK && previousType != targetType)
                    LOG_WARN("Restart mount in order to apply changes to Mount Type.");
            }
            MountTypeSP.setState(state);
            MountTypeSP.apply();
            return true;
        }
        else if(GuideRateSP.isNameMatch(name))
        {
            int previousType = GuideRateSP.findOnSwitchIndex();
            GuideRateSP.update(states, names, n);
            IPState state = IPS_OK;
            if (isConnected())
            {
                auto targetType = GuideRateSP.findOnSwitchIndex();
                state = setGuideRate(targetType) ? IPS_OK : IPS_ALERT;
                if (state == IPS_OK && previousType != targetType)
                    LOG_WARN("RA and DEC guide rate changed.");
            }
            GuideRateSP.setState(state);
            GuideRateSP.apply();
            return true;
        }
        else if(HomeSP.isNameMatch(name))
        {
            HomeSP.update(states, names, n);
            IPState state = IPS_OK;
            if (isConnected())
            {
                HomeSP[0].setState(ISS_OFF);
                sendCommand(":hC#");
            }
            HomeSP.setState(state);
            HomeSP.apply();
            return true;
        }
        else if(ResetHomeSP.isNameMatch(name))
        {
            ResetHomeSP.update(states, names, n);
            IPState state = IPS_OK;
            if (isConnected())
            {
                ResetHomeSP[0].setState(ISS_OFF);
                sendCommand(":hF#");
            }
            ResetHomeSP.setState(state);
            ResetHomeSP.apply();
            return true;
        }
        else if(VerboseReportSP.isNameMatch(name))
        {
            VerboseReportSP.update(states, names, n);
            int index = VerboseReportSP.findOnSwitchIndex();
            VerboseReportSP[index].setState(ISS_OFF);

            if(index == 0)
                verboseReport = true;
            else
            {

                SetPropertyText(IsTracking, IPS_IDLE);
                SetPropertyText(IsSlewCompleted, IPS_IDLE);
                SetPropertyText(IsParked, IPS_IDLE);
                SetPropertyText(IsParkginInProgress, IPS_IDLE);
                SetPropertyText(IsAtHomePosition, IPS_IDLE);
                SetPropertyText(TrackSidereal, IPS_IDLE);
                SetPropertyText(TrackLunar, IPS_IDLE);
                SetPropertyText(TrackSolar, IPS_IDLE);
                SetPropertyText(MountAltAz, IPS_IDLE);
                SetPropertyText(MountEquatorial, IPS_IDLE);
                SetPropertyText(PierNone, IPS_IDLE);
                SetPropertyText(PierEast, IPS_IDLE);
                SetPropertyText(PierWest, IPS_IDLE);
                SetPropertyText(DoesRefractionComp, IPS_IDLE);
                SetPropertyText(WaitingAtHome, IPS_IDLE);
                SetPropertyText(IsHomePaused, IPS_IDLE);
                SetPropertyText(ParkFailed, IPS_IDLE);
                SetPropertyText(SlewingHome, IPS_IDLE);
                verboseReport = false;
            }


            VerboseReportSP.setState(index == 0 ? IPS_OK : IPS_IDLE);
            VerboseReportSP.apply();
            return true;
        }
    }
    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200NYX101::SetSlewRate(int index)
{
    double value = 0.0;
    switch(index)
    {
    case 0:
        value = 0.01;
        break;
    case 1:
        value = 0.03;
        break;
    case 2:
        value = 0.07;
        break;
    case 3:
        value = 0.27;
        break;
    case 4:
        value = 0.50;
        break;
    case 5:
        value = 0.65;
        break;
    case 6:
        value = 0.80;
        break;
    case 7:
        value = 1;
        break;
    case 8:
        value = 2.5;
        break;
    case 9:
        value = 5;
        break;
    };

    char decCommand[DRIVER_LEN] = {0};
    snprintf(decCommand, DRIVER_LEN, ":RE%f#", value);

    char raCommand[DRIVER_LEN] = {0};
    snprintf(raCommand, DRIVER_LEN, ":RA%f#", value);

    return sendCommand(decCommand) && sendCommand(raCommand);
}

bool LX200NYX101::setGuideRate(int rate)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":R%d#", rate);
    return sendCommand(command);
}

bool LX200NYX101::setMountType(int type)
{
    return sendCommand((type == Equatorial) ? ":SXEM,1#" : "::SXEM,3#");
}

bool LX200NYX101::goToPark()
{
    LOG_INFO("Park requested.");
    return sendCommand(":hP#");
}

bool LX200NYX101::goToUnPark()
{
    return sendCommand(":hR#");
}

bool LX200NYX101::Park()
{
    bool rc = goToPark();
    if (rc)
        TrackState = SCOPE_PARKING;

    return rc;
}

bool LX200NYX101::UnPark()
{
    bool rc = goToUnPark();
    if(rc)
        SetParked(false);

    return rc;
}

bool LX200NYX101::SetTrackEnabled(bool enabled)
{
    char response[DRIVER_LEN] = {0};
    bool rc = sendCommand(enabled ? ":Te#" : ":Td#", response, 4, 1);
    return rc && response[0] == '1';
}

bool LX200NYX101::setUTCOffset(double offset)
{
    int h {0}, m {0}, s {0};
    char command[DRIVER_LEN] = {0};
    offset *= -1;
    getSexComponents(offset, &h, &m, &s);

    snprintf(command, DRIVER_LEN, ":SG%c%02d:%02d#", offset >= 0 ? '+' : '-', std::abs(h), m);
    return setStandardProcedure(PortFD, command) == 0;
}

bool LX200NYX101::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    char command[DRIVER_LEN] = {0};
    snprintf(command, DRIVER_LEN, ":SC%02d/%02d/%02d#", months, days, years % 100);
    return setStandardProcedure(PortFD, command) == 0;
}

bool LX200NYX101::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    int d{0}, m{0}, s{0};

    // JM 2021-04-10: MUST convert from INDI longitude to standard longitude.
    // DO NOT REMOVE
    if (longitude > 180)
        longitude = longitude - 360;

    char command[DRIVER_LEN] = {0};
    // Reverse as per Meade way
    longitude *= -1;
    getSexComponents(longitude, &d, &m, &s);
    snprintf(command, DRIVER_LEN, ":Sg%c%03d*%02d:%02d#", longitude >= 0 ? '+' : '-', std::abs(d), m, s);
    if (setStandardProcedure(PortFD, command) < 0)
    {
        LOG_ERROR("Error setting site longitude coordinates");
        return false;
    }

    getSexComponents(latitude, &d, &m, &s);
    snprintf(command, DRIVER_LEN, ":St%c%02d*%02d:%02d#", latitude >= 0 ? '+' : '-', std::abs(d), m, s);
    if (setStandardProcedure(PortFD, command) < 0)
    {
        LOG_ERROR("Error setting site latitude coordinates");
        return false;
    }

    return true;
}


bool LX200NYX101::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        rc = tty_write_string(PortFD, cmd, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
    {
        tcdrain(PortFD);
        return true;
    }

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void LX200NYX101::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

std::vector<std::string> LX200NYX101::split(const std::string &input, const std::string &regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator
    first{input.begin(), input.end(), re, -1},
          last;
    return {first, last};
}
