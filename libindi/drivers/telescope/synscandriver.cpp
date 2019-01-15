/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.
  Copyright(c) 2018 Jasem Mutlaq. All rights reserved.

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

#include "synscandriver.h"
#include "connectionplugins/connectioninterface.h"
#include "connectionplugins/connectiontcp.h"
#include "indicom.h"

#include <libnova/transform.h>
#include <libnova/precession.h>
// libnova specifies round() on old systems and it collides with the new gcc 5.x/6.x headers
#define HAVE_ROUND
#include <libnova/utility.h>

#include <cmath>
#include <map>
#include <memory>
#include <termios.h>
#include <cstring>

constexpr uint16_t SynscanDriver::SLEW_RATE[];

SynscanDriver::SynscanDriver()
{
    strncpy(LastParkRead, "", 1);
}

const char * SynscanDriver::getDefaultName()
{
    return "SynScan";
}

bool SynscanDriver::initProperties()
{
    INDI::Telescope::initProperties();

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE,
                           NARRAY(SLEW_RATE));
    SetParkDataType(PARK_RA_DEC_ENCODER);

    // Slew Rates
    strncpy(SlewRateS[0].label, "1x", MAXINDILABEL);
    strncpy(SlewRateS[1].label, "8x", MAXINDILABEL);
    strncpy(SlewRateS[2].label, "16x", MAXINDILABEL);
    strncpy(SlewRateS[3].label, "32x", MAXINDILABEL);
    strncpy(SlewRateS[4].label, "64x", MAXINDILABEL);
    strncpy(SlewRateS[5].label, "128x", MAXINDILABEL);
    strncpy(SlewRateS[6].label, "400x", MAXINDILABEL);
    strncpy(SlewRateS[7].label, "600x", MAXINDILABEL);
    strncpy(SlewRateS[8].label, "MAX", MAXINDILABEL);
    IUResetSwitch(&SlewRateSP);
    // Max is the default
    SlewRateS[8].s = ISS_ON;

    //////////////////////////////////////////////////////////////////////////////////////////////////
    /// Mount Info Text Property
    //////////////////////////////////////////////////////////////////////////////////////////////////
    IUFillText(&BasicMountInfoT[MI_FW_VERSION], "MI_FW_VERSION", "Firmware", "-");
    IUFillText(&BasicMountInfoT[MI_MOUNT_MODEL], "MI_MOUNT_MODEL", "Model", "-");
    IUFillText(&BasicMountInfoT[MI_ALIGN_STATUS], "MI_ALIGN_STATUS", "Alignment", "-");
    IUFillText(&BasicMountInfoT[MI_GOTO_STATUS], "MI_GOTO_STATUS", "Goto", "-");
    IUFillText(&BasicMountInfoT[MI_POINT_STATUS], "MI_POINT_STATUS", "Pointing", "-");
    IUFillText(&BasicMountInfoT[MI_TRACK_MODE], "MI_TRACK_MODE", "Tracking Mode", "-");
    IUFillTextVector(&BasicMountInfoTP, BasicMountInfoT, 6, getDeviceName(), "BASIC_MOUNT_INFO",
                     "Mount Information", MOUNT_TAB, IP_RO, 60, IPS_IDLE);

    addAuxControls();

    return true;
}

bool SynscanDriver::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        sendLocation();
        sendTime();
        readFirmware();
        readModel();
        defineText(&BasicMountInfoTP);

        if (InitPark())
        {
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(90);
        }
        else
        {
            SetAxis1Park(0);
            SetAxis2Park(90);
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(90);
        }
    }
    else
    {
        deleteProperty(BasicMountInfoTP.name);
    }

    return true;
}

int SynscanDriver::hexStrToInteger(const std::string &res)
{
    int result=0;

    try
    {
        result = std::stoi(res, nullptr, 16);
    }
    catch (std::invalid_argument &)
    {
        LOGF_ERROR("Failed to parse %s to integer.", res.c_str());
    }

    return result;
}

bool SynscanDriver::Handshake()
{
    char res[SYN_RES]= {0};
    if (!echo())
        return false;

    // We can only proceed if the mount is aligned.
    if (!sendCommand("J", res))
        return false;

    if (res[0] == 0)
    {
        LOG_ERROR("Mount is not aligned. Please align the mount first and connect again.");
        return false;
    }

    return true;
}

bool SynscanDriver::echo()
{
    char res[SYN_RES]= {0};
    return sendCommand("Kx", res);
}

bool SynscanDriver::readFirmware()
{
    // Read the handset version
    char res[SYN_RES]= {0};
    if (sendCommand("V", res))
    {
        FirmwareVersion = static_cast<double>(hexStrToInteger(std::string(&res[0], 2)));
        FirmwareVersion += static_cast<double>(hexStrToInteger(std::string(&res[2], 2))) / 100;
        FirmwareVersion += static_cast<double>(hexStrToInteger(std::string(&res[4], 2))) / 10000;

        LOGF_INFO("Firmware version: %lf", FirmwareVersion);

        if (FirmwareVersion < 3.38 || (FirmwareVersion >= 4.0 && FirmwareVersion < 4.38))
        {
            LOG_WARN("Firmware version is too old. Update Synscan firmware to v4.38+");
        }
        else
        {
            NewFirmware = true;
        }

        m_MountInfo[MI_FW_VERSION] = std::to_string(FirmwareVersion);

        return true;
    }
    else
        LOG_WARN("Firmware version is too old. Update Synscan firmware to v4.38+");

    return false;
}

bool SynscanDriver::readModel()
{
    // extended list of mounts
    std::map<int, std::string> models =
    {
        {0, "EQ6 GOTO Series"},
        {1, "HEQ5 GOTO Series"},
        {2, "EQ5 GOTO Series"},
        {3, "EQ3 GOTO Series"},
        {4, "EQ8 GOTO Series"},
        {5, "AZ-EQ6 GOTO Series"},
        {6, "AZ-EQ5 GOTO Series"},
        {160, "AllView GOTO Series"}
    };

    // Read the handset version
    char res[SYN_RES]= {0};

    if (!sendCommand("m", res))
        return false;

    m_MountModel = res[0];

    // 128 - 143 --> AZ Goto series
    if (m_MountModel >= 128 && m_MountModel <= 143)
        IUSaveText(&BasicMountInfoT[MI_MOUNT_MODEL], "AZ GOTO Series");
    // 144 - 159 --> DOB Goto series
    else if (m_MountModel >= 144 && m_MountModel <= 159)
        IUSaveText(&BasicMountInfoT[MI_MOUNT_MODEL], "Dob GOTO Series");
    else if (models.count(m_MountModel) > 0)
        IUSaveText(&BasicMountInfoT[MI_MOUNT_MODEL], models[m_MountModel].c_str());
    else
        IUSaveText(&BasicMountInfoT[MI_MOUNT_MODEL], "Unknown model");

    return true;
}

bool SynscanDriver::ReadScopeStatus()
{
#if 0
    if (isSimulation())
    {
        mountSim();
        return true;
    }

    // Query mount information
    memset(res, 0, SYN_RES);
    LOG_DEBUG("CMD <J>");
    tty_write(PortFD, "J", 1, &bytesWritten);
    numread = tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (res[1] == '#')
    {
        m_MountInfo[MI_ALIGN_STATUS] = std::to_string(static_cast<int>(res[0]));
    }
    memset(res, 0, SYN_RES);
    LOG_DEBUG("CMD <L>");
    tty_write(PortFD, "L", 1, &bytesWritten);
    numread = tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (res[1] == '#')
    {
        m_MountInfo[MI_GOTO_STATUS] = res[0];
    }
    memset(res, 0, SYN_RES);
    LOG_DEBUG("CMD <p>");
    tty_write(PortFD, "p", 1, &bytesWritten);
    numread = tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (res[1] == '#')
    {
        m_MountInfo[MI_POINT_STATUS] = res[0];

        // INDI and mount pier sides are opposite to each other
        setPierSide(res[0] == 'W' ? PIER_EAST : PIER_WEST);
    }
    memset(res, 0, SYN_RES);
    LOG_DEBUG("CMD <t>");
    tty_write(PortFD, "t", 1, &bytesWritten);
    numread = tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (res[1] == '#')
    {
        TrackingStatus = res[0];
        switch(static_cast<int>(res[0]))
        {
            case 0:
                m_MountInfo[MI_TRACK_MODE] = "Tracking off";
                break;
            case 1:
                m_MountInfo[MI_TRACK_MODE] = "Alt/Az tracking";
                break;
            case 2:
                m_MountInfo[MI_TRACK_MODE] = "EQ tracking";
                break;
            case 3:
                m_MountInfo[MI_TRACK_MODE] = "PEC mode";
                break;
        }
    }

    sendMountStatus(true);

    if (TrackState == SCOPE_SLEWING)
    {
        //  We have a slew in progress
        //  lets see if it's complete
        //  This only works for ra/dec goto commands
        //  The goto complete flag doesn't trip for ALT/AZ commands
        if (m_MountInfo[MI_GOTO_STATUS] != "0")
        {
            //  Nothing to do here
        }
        else if (m_MountModel < 128)
        {
            if (TrackingStatus[0] != 0)
                TrackState = SCOPE_TRACKING;
            else
                TrackState = SCOPE_IDLE;
        }
    }
    if (TrackState == SCOPE_PARKING)
    {
        if (FirmwareVersion == 4.103500)
        {
            //  With this firmware the correct way
            //  is to check the slewing flat
            memset(res, 0, 3);
            LOG_DEBUG("CMD <L>");
            tty_write(PortFD, "L", 1, &bytesWritten);
            numread = tty_read(PortFD, res, 2, 3, &bytesRead);
            LOGF_DEBUG("RES <%s>", res);
            if (res[0] != 48)
            {
                //  Nothing to do here
            }
            else
            {
                if (NumPark++ < 2)
                {
                    Park();
                }
                else
                {
                    TrackState = SCOPE_PARKED;
                    SetParked(true);
                }
            }
        }
        else
        {
            //  ok, lets try read where we are
            //  and see if we have reached the park position
            //  newer firmware versions dont read it back the same way
            //  so we watch now to see if we get the same read twice in a row
            //  to confirm that it has stopped moving
            memset(res, 0, SYN_RES);
            LOG_DEBUG("CMD <z>");
            tty_write(PortFD, "z", 1, &bytesWritten);
            numread = tty_read(PortFD, res, 18, 2, &bytesRead);
            LOGF_DEBUG("RES <%s>", res);

            //IDMessage(getDeviceName(),"Park Read %s %d",res,StopCount);

            if (strncmp((char *)res, LastParkRead, 18) == 0)
            {
                //  We find that often after it stops from park
                //  it's off the park position by a small amount
                //  issuing another park command gets a small movement and then
                if (++StopCount > 2)
                {
                    if (NumPark++ < 2)
                    {
                        StopCount = 0;
                        //IDMessage(getDeviceName(),"Sending park again");
                        Park();
                    }
                    else
                    {
                        TrackState = SCOPE_PARKED;
                        //ParkSP.s=IPS_OK;
                        //IDSetSwitch(&ParkSP,nullptr);
                        //IDMessage(getDeviceName(),"Telescope is Parked.");
                        SetParked(true);
                    }
                }
                else
                {
                    //StopCount=0;
                }
            }
            else
            {
                StopCount = 0;
            }
            strncpy(LastParkRead, res, 20);
        }
    }

    memset(res, 0, SYN_RES);
    LOG_DEBUG("CMD <e>");
    tty_write(PortFD, "e", 1, &bytesWritten);
    numread = tty_read(PortFD, res, 18, 1, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (bytesRead != 18)
    {
        LOG_DEBUG("Read current position failed");
        return false;
    }

    sscanf(res, "%lx,%lx#", &n1, &n2);
    ra  = static_cast<double>(n1) / 0x100000000 * 24.0;
    dec = static_cast<double>(n2) / 0x100000000 * 360.0;

    ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };
    J2000Pos.ra  = range24(ra) * 15.0;
    J2000Pos.dec = rangeDec(dec);

    // Synscan reports J2000 coordinates so we need to convert from J2000 to JNow
    ln_get_equ_prec2(&J2000Pos, JD2000, ln_get_julian_from_sys(), &epochPos);

    CurrentRA  = epochPos.ra/15.0;
    CurrentDEC = epochPos.dec;

    //  Now feed the rest of the system with corrected data
    NewRaDec(CurrentRA, CurrentDEC);

    if (TrackState == SCOPE_SLEWING && m_MountModel >= 128 && (SlewTargetAz != -1 || SlewTargetAlt != -1))
    {
        ln_hrz_posn CurrentAltAz { 0, 0 };
        double DiffAlt { 0 };
        double DiffAz { 0 };

        CurrentAltAz = getAltAzPosition(ra, dec);
        DiffAlt = CurrentAltAz.alt-SlewTargetAlt;
        if (SlewTargetAlt != -1 && std::abs(DiffAlt) > 0.01)
        {
            int NewRate = 2;

            if (std::abs(DiffAlt) > 4)
            {
                NewRate = 9;
            }
            else if (std::abs(DiffAlt) > 1.2)
            {
                NewRate = 7;
            }
            else if (std::abs(DiffAlt) > 0.5)
            {
                NewRate = 5;
            }
            else if (std::abs(DiffAlt) > 0.2)
            {
                NewRate = 4;
            }
            else if (std::abs(DiffAlt) > 0.025)
            {
                NewRate = 3;
            }
            LOGF_DEBUG("Slewing Alt axis: %1.3f-%1.3f -> %1.3f (speed: %d)",
                       CurrentAltAz.alt, SlewTargetAlt, CurrentAltAz.alt-SlewTargetAlt, CustomNSSlewRate);
            if (NewRate != CustomNSSlewRate)
            {
                if (DiffAlt < 0)
                {
                    CustomNSSlewRate = NewRate;
                    MoveNS(DIRECTION_NORTH, MOTION_START);
                }
                else
                {
                    CustomNSSlewRate = NewRate;
                    MoveNS(DIRECTION_SOUTH, MOTION_START);
                }
            }
        }
        else if (SlewTargetAlt != -1 && std::abs(DiffAlt) < 0.01)
        {
            MoveNS(DIRECTION_NORTH, MOTION_STOP);
            SlewTargetAlt = -1;
            LOG_DEBUG("Slewing on Alt axis finished");
        }
        DiffAz = CurrentAltAz.az-SlewTargetAz;
        if (DiffAz < -180)
            DiffAz = (DiffAz+360)*2;
        else if (DiffAz > 180)
            DiffAz = (DiffAz-360)*2;
        if (SlewTargetAz != -1 && std::abs(DiffAz) > 0.01)
        {
            int NewRate = 2;

            if (std::abs(DiffAz) > 4)
            {
                NewRate = 9;
            }
            else if (std::abs(DiffAz) > 1.2)
            {
                NewRate = 7;
            }
            else if (std::abs(DiffAz) > 0.5)
            {
                NewRate = 5;
            }
            else if (std::abs(DiffAz) > 0.2)
            {
                NewRate = 4;
            }
            else if (std::abs(DiffAz) > 0.025)
            {
                NewRate = 3;
            }
            LOGF_DEBUG("Slewing Az axis: %1.3f-%1.3f -> %1.3f (speed: %d)",
                       CurrentAltAz.az, SlewTargetAz, CurrentAltAz.az-SlewTargetAz, CustomWESlewRate);
            if (NewRate != CustomWESlewRate)
            {
                if (DiffAz > 0)
                {
                    CustomWESlewRate = NewRate;
                    MoveWE(DIRECTION_WEST, MOTION_START);
                }
                else
                {
                    CustomWESlewRate = NewRate;
                    MoveWE(DIRECTION_EAST, MOTION_START);
                }
            }
        }
        else if (SlewTargetAz != -1 && std::abs(DiffAz) < 0.01)
        {
            MoveWE(DIRECTION_WEST, MOTION_STOP);
            SlewTargetAz = -1;
            LOG_DEBUG("Slewing on Az axis finished");
        }
        if (SlewTargetAz == -1 && SlewTargetAlt == -1)
        {
            startTrackMode();
        }
    }
    return true;
}

bool SynscanDriver::startTrackMode()
{
    char res[SYN_RES]= {0};
    int numread, bytesWritten, bytesRead;

    TrackState = SCOPE_TRACKING;
    LOG_INFO("Tracking started.");

    if (isSimulation())
        return true;

    // Start tracking
    res[0] = 'T';
    // Check the mount type to choose tracking mode
    if (m_MountModel >= 128)
    {
        // Alt/Az tracking mode
        res[1] = 1;
    }
    else
    {
        // EQ tracking mode
        res[1] = 2;
    }
    tty_write(PortFD, res, 2, &bytesWritten);
    numread = tty_read(PortFD, res, 1, 2, &bytesRead);
    if (bytesRead != 1 || res[0] != '#')
    {
        LOG_DEBUG("Timeout waiting for scope to start tracking.");
        return false;
    }
#endif
    return true;

}

bool SynscanDriver::Goto(double ra, double dec)
{
    char res[SYN_RES]= {0};
    int bytesWritten, bytesRead;
    ln_hrz_posn TargetAltAz { 0, 0 };

    if (isSimulation() == false)
    {
        LOG_DEBUG("CMD <Ka>");
        tty_write(PortFD, "Ka", 2, &bytesWritten); //  test for an echo
        tty_read(PortFD, res, 2, 2, &bytesRead);   //  Read 2 bytes of response
        LOGF_DEBUG("RES <%s>", res);
        if (res[1] != '#')
        {
            LOG_WARN("Wrong answer from the mount");
            //  this is not a correct echo
            //  so we are not talking to a mount properly
            return false;
        }
    }

    TrackState = SCOPE_SLEWING;
    // EQ mount has a different Goto mode
    if (m_MountModel < 128 && isSimulation() == false)
    {
        ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };

        epochPos.ra  = ra * 15.0;
        epochPos.dec = dec;

        // Synscan accepts J2000 coordinates so we need to convert from JNow to J2000
        ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);

        // Mount deals in J2000 coords.
        int n1 = J2000Pos.ra/15.0 * 0x1000000 / 24;
        int n2 = J2000Pos.dec * 0x1000000 / 360;
        int numread;

        LOGF_DEBUG("Goto - JNow RA: %g JNow DE: %g J2000 RA: %g J2000 DE: %g", ra, dec, J2000Pos.ra/15.0, J2000Pos.dec);

        n1 = n1 << 8;
        n2 = n2 << 8;
        LOGF_DEBUG("CMD <%s>", res);
        snprintf(res, SYN_RES, "r%08X,%08X", n1, n2);
        tty_write(PortFD, res, 18, &bytesWritten);
        memset(&res[18], 0, 1);

        numread = tty_read(PortFD, res, 1, 60, &bytesRead);
        if (bytesRead != 1 || res[0] != '#')
        {
            LOG_DEBUG("Timeout waiting for scope to complete goto.");
            return false;
        }

        return true;
    }

    TargetAltAz = getAltAzPosition(ra, dec);
    LOGF_DEBUG("Goto - JNow RA: %g JNow DE: %g (az: %g alt: %g)", ra, dec, TargetAltAz.az, TargetAltAz.alt);
    char RAStr[SYN_RES]= {0}, DEStr[SYN_RES]= {0}, AZStr[SYN_RES]= {0}, ATStr[SYN_RES]= {0};
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DEStr, dec, 2, 3600);
    fs_sexa(AZStr, TargetAltAz.az, 2, 3600);
    fs_sexa(ATStr, TargetAltAz.alt, 2, 3600);

    LOGF_INFO("Goto RA: %s DE: %s AZ: %s ALT: %s", RAStr, DEStr, AZStr, ATStr);

    SlewTargetAz = TargetAltAz.az;
    SlewTargetAlt = TargetAltAz.alt;

    TargetRA = ra;
    TargetDEC = dec;

    return true;
}

bool SynscanDriver::Park()
{
    char res[SYN_RES]= {0};
    int numread, bytesWritten, bytesRead;

    if (isSimulation() == false)
    {
        strncpy(LastParkRead, "", 1);
        memset(res, 0, 3);
        tty_write(PortFD, "Ka", 2, &bytesWritten); //  test for an echo
        tty_read(PortFD, res, 2, 2, &bytesRead);   //  Read 2 bytes of response
        if (res[1] != '#')
        {
            //  this is not a correct echo
            //  so we are not talking to a mount properly
            return false;
        }
        //  Now we stop tracking
        res[0] = 'T';
        res[1] = 0;
        tty_write(PortFD, res, 2, &bytesWritten);
        numread = tty_read(PortFD, res, 1, 60, &bytesRead);
        if (bytesRead != 1 || res[0] != '#')
        {
            LOG_DEBUG("Timeout waiting for scope to stop tracking.");
            return false;
        }

        //sprintf((char *)res,"b%08X,%08X",0x0,0x40000000);
        tty_write(PortFD, "b00000000,40000000", 18, &bytesWritten);
        numread = tty_read(PortFD, res, 1, 60, &bytesRead);
        if (bytesRead != 1 || res[0] != '#')
        {
            LOG_DEBUG("Timeout waiting for scope to respond to park.");
            return false;
        }
    }

    TrackState = SCOPE_PARKING;
    if (NumPark == 0)
    {
        LOG_INFO("Parking Mount...");
    }
    StopCount = 0;
    return true;
}

bool SynscanDriver::UnPark()
{
    SetParked(false);
    NumPark = 0;
    return true;
}

bool SynscanDriver::SetCurrentPark()
{
    LOG_INFO("Setting arbitrary park positions is not supported yet.");
    return false;
}

bool SynscanDriver::SetDefaultPark()
{
    // By default az to north, and alt to pole
    LOG_DEBUG("Setting Park Data to Default.");
    SetAxis1Park(0);
    SetAxis2Park(90);

    return true;
}

bool SynscanDriver::Abort()
{
    if (TrackState == SCOPE_IDLE || RecoverTrials >= 3)
        return true;

    char res[SYN_RES]= {0};
    int numread, bytesWritten, bytesRead;

    LOG_DEBUG("Abort mount...");
    TrackState = SCOPE_IDLE;

    if (isSimulation())
        return true;

    SlewTargetAlt = -1;
    SlewTargetAz = -1;
    CustomNSSlewRate = -1;
    CustomWESlewRate = -1;
    // Stop tracking
    res[0] = 'T';
    res[1] = 0;

    LOGF_DEBUG("CMD <%s>", res);
    tty_write(PortFD, res, 2, &bytesWritten);

    numread = tty_read(PortFD, res, 1, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);

    if (bytesRead != 1 || res[0] != '#')
    {
        LOG_DEBUG("Timeout waiting for scope to stop tracking.");
        return false;
    }

    // Hmmm twice only stops it
    LOG_DEBUG("CMD <M>");
    tty_write(PortFD, "M", 1, &bytesWritten);
    tty_read(PortFD, res, 1, 1, &bytesRead);
    LOGF_DEBUG("RES <%c>", res[0]);

    LOG_DEBUG("CMD <M>");
    tty_write(PortFD, "M", 1, &bytesWritten);
    tty_read(PortFD, res, 1, 1, &bytesRead);
    LOGF_DEBUG("RES <%c>", res[0]);

    return true;
}

bool SynscanDriver::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (isSimulation())
        return true;

    if (command != MOTION_START)
    {
        passThruCommand(37, 17, 2, 0, 0);
    }
    else
    {
        int tt = (CustomNSSlewRate == -1 ? SlewRate : CustomNSSlewRate);

        tt = tt << 16;
        if (dir != DIRECTION_NORTH)
        {
            passThruCommand(37, 17, 2, tt, 0);
        }
        else
        {
            passThruCommand(36, 17, 2, tt, 0);
        }
    }

    return true;
}

bool SynscanDriver::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (isSimulation())
        return true;

    if (command != MOTION_START)
    {
        passThruCommand(37, 16, 2, 0, 0);
    }
    else
    {
        int tt = (CustomWESlewRate == -1 ? SlewRate : CustomWESlewRate);

        tt = tt << 16;
        if (dir != DIRECTION_WEST)
        {
            passThruCommand(36, 16, 2, tt, 0);
        }
        else
        {
            passThruCommand(37, 16, 2, tt, 0);
        }
    }

    return true;
}

bool SynscanDriver::SetSlewRate(int s)
{
    SlewRate = s + 1;
    return true;
}

bool SynscanDriver::passThruCommand(int cmd, int target, int msgsize, int data, int numReturn)
{
    char test[20]= {0};
    int bytesRead, bytesWritten;
    char a, b, c;
    int tt = data;

    a  = tt % 256;
    tt = tt >> 8;
    b  = tt % 256;
    tt = tt >> 8;
    c  = tt % 256;

    //  format up a passthru command
    memset(test, 0, 20);
    test[0] = 80;      // passhtru
    test[1] = msgsize; // set message size
    test[2] = target;  // set the target
    test[3] = cmd;     // set the command
    test[4] = c;       // set data bytes
    test[5] = b;
    test[6] = a;
    test[7] = numReturn;

    LOGF_DEBUG("CMD <%s>", test);
    tty_write(PortFD, test, 8, &bytesWritten);
    memset(test, 0, 20);
    tty_read(PortFD, test, numReturn + 1, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", test);
    if (numReturn > 0)
    {
        int retval = 0;
        retval     = test[0];
        if (numReturn > 1)
        {
            retval = retval << 8;
            retval += test[1];
        }
        if (numReturn > 2)
        {
            retval = retval << 8;
            retval += test[2];
        }
        return retval;
    }

    return 0;
}

bool SynscanDriver::sendTime()
{
    LOG_DEBUG("Reading time...");

    if (isSimulation())
    {
        char timeString[MAXINDINAME] = {0};
        time_t now = time (nullptr);
        strftime(timeString, MAXINDINAME, "%T", gmtime(&now));
        IUSaveText(&TimeT[0], "3");
        IUSaveText(&TimeT[1], timeString);
        TimeTP.s = IPS_OK;
        IDSetText(&TimeTP, nullptr);
        return true;
    }

    char res[SYN_RES]= {0};
    int bytesWritten = 0, bytesRead = 0;

    //  lets see if this hand controller responds to a time request
    bytesRead = 0;
    LOG_DEBUG("CMD <h>");
    tty_write(PortFD, "h", 1, &bytesWritten);

    tty_read(PortFD, res, 9, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);

    if (res[8] == '#')
    {
        ln_zonedate localTime;
        ln_date utcTime;
        int offset, daylightflag;

        localTime.hours   = res[0];
        localTime.minutes = res[1];
        localTime.seconds = res[2];
        localTime.months  = res[3];
        localTime.days    = res[4];
        localTime.years   = res[5];
        offset            = static_cast<int>(res[6]);
        // Negative GMT offset is read. It needs special treatment
        if (offset > 200)
            offset -= 256;
        localTime.gmtoff = offset;
        //  this is the daylight savings flag in the hand controller, needed if we did not set the time
        daylightflag = res[7];
        localTime.years += 2000;
        localTime.gmtoff *= 3600;
        //  now convert to utc
        ln_zonedate_to_date(&localTime, &utcTime);

        //  now we have time from the hand controller, we need to set some variables
        int sec;
        char utc[100];
        char ofs[10];
        sec = static_cast<int>(utcTime.seconds);
        sprintf(utc, "%04d-%02d-%dT%d:%02d:%02d", utcTime.years, utcTime.months, utcTime.days, utcTime.hours,
                utcTime.minutes, sec);
        if (daylightflag == 1)
            offset = offset + 1;
        sprintf(ofs, "%d", offset);

        IUSaveText(&TimeT[0], utc);
        IUSaveText(&TimeT[1], ofs);
        TimeTP.s = IPS_OK;
        IDSetText(&TimeTP, nullptr);

        LOGF_INFO("Mount UTC Time %s Offset %d", utc, offset);

        return true;
    }
    return false;
}

bool SynscanDriver::sendLocation()
{
    char res[SYN_RES] = {0};

    if (isSimulation())
    {
        LocationN[LOCATION_LATITUDE].value  = 29.5;
        LocationN[LOCATION_LONGITUDE].value = 48;
        IDSetNumber(&LocationNP, nullptr);
        return true;
    }

    if (!sendCommand("w", res))
        return false;

    double lat, lon;
    //  lets parse this data now
    int a, b, c, d, e, f, g, h;
    a = res[0];
    b = res[1];
    c = res[2];
    d = res[3];
    e = res[4];
    f = res[5];
    g = res[6];
    h = res[7];

    LOGF_DEBUG("Pos %d:%d:%d  %d:%d:%d",a,b,c,e,f,g);

    double t1, t2, t3;

    t1  = c;
    t2  = b;
    t3  = a;
    t1  = t1 / 3600.0;
    t2  = t2 / 60.0;
    lat = t1 + t2 + t3;

    t1  = g;
    t2  = f;
    t3  = e;
    t1  = t1 / 3600.0;
    t2  = t2 / 60.0;
    lon = t1 + t2 + t3;

    if (d == 1)
        lat = lat * -1;
    if (h == 1)
        lon = 360 - lon;
    LocationN[LOCATION_LATITUDE].value  = lat;
    LocationN[LOCATION_LONGITUDE].value = lon;
    IDSetNumber(&LocationNP, nullptr);

    saveConfig(true, "GEOGRAPHIC_COORD");

    char LongitudeStr[32]= {0}, LatitudeStr[32]= {0};
    fs_sexa(LongitudeStr, lon, 2, 3600);
    fs_sexa(LatitudeStr, lat, 2, 3600);
    LOGF_INFO("Mount Longitude %s Latitude %s", LongitudeStr, LatitudeStr);

    return true;
}

bool SynscanDriver::updateTime(ln_date * utc, double utc_offset)
{
    char cmd[SYN_RES]= {0}, res[SYN_RES]= {0};

    //  start by formatting a time for the hand controller
    //  we are going to set controller to local time
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, utc_offset * 3600.0);

    int yr = ltm.years;

    yr = yr % 100;

    cmd[0] = 'H';
    cmd[1] = ltm.hours;
    cmd[2] = ltm.minutes;
    cmd[3] = static_cast<char>(ltm.seconds);
    cmd[4] = ltm.months;
    cmd[5] = ltm.days;
    cmd[6] = yr;
    //  offset from utc so hand controller is running in local time
    cmd[7] = static_cast<char>(utc_offset);
    //  and no daylight savings adjustments, it's already included in the offset
    cmd[8] = 0;

    LOGF_INFO("Setting mount date/time to %04d-%02d-%02d %d:%02d:%02d UTC Offset: %d",
              ltm.years, ltm.months, ltm.days, ltm.hours, ltm.minutes, ltm.seconds, utc_offset);

    if (isSimulation())
        return true;

    return sendCommand(cmd, res);
}

bool SynscanDriver::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    char cmd[SYN_RES]= {0}, res[SYN_RES]= {0};
    bool IsWest = false;

    ln_lnlat_posn p1 { 0, 0 };
    lnh_lnlat_posn p2;

    LocationN[LOCATION_LATITUDE].value  = latitude;
    LocationN[LOCATION_LONGITUDE].value = longitude;
    IDSetNumber(&LocationNP, nullptr);

    if (isSimulation())
    {
        if (!CurrentDEC)
        {
            CurrentDEC = latitude > 0 ? 90 : -90;
            CurrentRA = get_local_sidereal_time(longitude);
        }
        return true;
    }

    if (longitude > 180)
    {
        p1.lng = 360.0 - longitude;
        IsWest = true;
    }
    else
    {
        p1.lng = longitude;
    }
    p1.lat = latitude;
    ln_lnlat_to_hlnlat(&p1, &p2);
    LOGF_INFO("Update location to latitude %d:%d:%1.2f longitude %d:%d:%1.2f",
              p2.lat.degrees, p2.lat.minutes, p2.lat.seconds, p2.lng.degrees, p2.lng.minutes, p2.lng.seconds);

    cmd[0] = 'W';
    cmd[1] = p2.lat.degrees;
    cmd[2] = p2.lat.minutes;
    cmd[3] = rint(p2.lat.seconds);
    cmd[4] = (p2.lat.neg == 0) ? 0 : 1;
    cmd[5] = p2.lng.degrees;
    cmd[6] = p2.lng.minutes;
    cmd[7] = rint(p2.lng.seconds);
    cmd[9] = IsWest ? 1 : 0;

    return sendCommand(cmd, res);
}

bool SynscanDriver::Sync(double ra, double dec)
{
#if 0
    /*
     * Frank Liu, R&D Engineer for Skywatcher, says to only issue a Sync
     * command, and not to use the Position Reset command, when syncing. I
     * removed the position reset code for EQ mounts, but left it in for
     * Alt/Az mounts, since it seems to be working, at least for the person
     * (@kecsap) who put it in there in the first place. :)
     *
     * The code prior to kecsap's recent fix would always send a position
     * reset command, but it would send Alt/Az coordinates, even to an EQ
     * mount. This would really screw up EQ mount alignment.
     *
     * The reason a lone Sync command appeared to not work before, is because
     * it will only accept a Sync command if the offset is relatively small,
     * within 6-7 degrees or so. So you must already have done an alignment
     * through the handset (a 1-star alignment would suffice), and only use
     * the Sync command to "touch-up" the alignment. You can't take a scope,
     * power it on, point it to a random place in the sky, do a plate-solve,
     * and sync. That won't work.
     */

    bool IsTrackingBeforeSync = (TrackState == SCOPE_TRACKING);

    // Abort any motion before syncing
    Abort();

    LOGF_INFO("Sync JNow %g %g -> %g %g", CurrentRA, CurrentDEC, ra, dec);
    char cmd[SYN_RES]= {0}, res[SYN_RES]= {0};

    if (isSimulation())
    {
        CurrentRA = ra;
        CurrentDEC = dec;
        return true;
    }

    // Alt/Az sync mode
    if (m_MountModel >= 128)
    {
        ln_hrz_posn TargetAltAz { 0, 0 };

        TargetAltAz = getAltAzPosition(ra, dec);
        LOGF_DEBUG("Sync - ra: %g de: %g to az: %g alt: %g", ra, dec, TargetAltAz.az, TargetAltAz.alt);
        // Assemble the Reset Position command for Az axis
        int Az = static_cast<int>((TargetAltAz.az*16777216 / 360));

        res[0] = 'P';
        res[1] = 4;
        res[2] = 16;
        res[3] = 4;
        *reinterpret_cast<uint8_t *>(&res[4]) = static_cast<uint8_t>((Az / 65536));
        Az -= (Az / 65536)*65536;
        *reinterpret_cast<uint8_t *>(&res[5]) = static_cast<uint8_t>((Az / 256));
        Az -= (Az / 256)*256;
        *reinterpret_cast<uint8_t *>(&res[6]) = static_cast<uint8_t>(Az);
        res[7] = 0;
        tty_write(PortFD, res, 8, &bytesWritten);
        numread = tty_read(PortFD, res, 1, 3, &bytesRead);
        // Assemble the Reset Position command for Alt axis
        int Alt = static_cast<int>((TargetAltAz.alt*16777216 / 360));

        res[0] = 'P';
        res[1] = 4;
        res[2] = 17;
        res[3] = 4;
        *reinterpret_cast<uint8_t *>(&res[4]) = static_cast<uint8_t>((Alt / 65536));
        Alt -= (Alt / 65536)*65536;
        *reinterpret_cast<uint8_t *>(&res[5]) = static_cast<uint8_t>((Alt / 256));
        Alt -= (Alt / 256)*256;
        *reinterpret_cast<uint8_t *>(&res[6]) = static_cast<uint8_t>(Alt);
        res[7] = 0;
        LOGF_DEBUG("CMD <%s>", res);
        tty_write(PortFD, res, 8, &bytesWritten);

        numread = tty_read(PortFD, res, 1, 2, &bytesRead);
        LOGF_DEBUG("CMD <%c>", res[0]);
    }

    ln_equ_posn epochPos { 0, 0 }, J2000Pos { 0, 0 };

    epochPos.ra  = ra * 15.0;
    epochPos.dec = dec;

    // Synscan accepts J2000 coordinates so we need to convert from JNow to J2000
    ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);

    // Pass the sync command to the handset
    int n1 = J2000Pos.ra/15.0 * 0x1000000 / 24;
    int n2 = J2000Pos.dec * 0x1000000 / 360;

    n1 = n1 << 8;
    n2 = n2 << 8;
    snprintf(res, SYN_RES, "s%08X,%08X", n1, n2);
    memset(&res[18], 0, 1);

    LOGF_DEBUG("CMD <%s>", res);
    tty_write(PortFD, res, 18, &bytesWritten);

    numread = tty_read(PortFD, res, 1, 60, &bytesRead);
    LOGF_DEBUG("RES <%c>", res[0]);

    if (bytesRead != 1 || res[0] != '#')
    {
        LOG_DEBUG("Timeout waiting for scope to complete syncing.");
        return false;
    }

    // Start tracking again
    if (IsTrackingBeforeSync)
        startTrackMode();

#endif
    return true;
}

ln_hrz_posn SynscanDriver::getAltAzPosition(double ra, double dec)
{
    ln_lnlat_posn Location { 0, 0 };
    ln_equ_posn Eq { 0, 0 };
    ln_hrz_posn AltAz { 0, 0 };

    // Set the current location
    Location.lat = LocationN[LOCATION_LATITUDE].value;
    Location.lng = LocationN[LOCATION_LONGITUDE].value;

    Eq.ra  = ra*360.0 / 24.0;
    Eq.dec = dec;
    ln_get_hrz_from_equ(&Eq, &Location, ln_get_julian_from_sys(), &AltAz);
    AltAz.az -= 180;
    if (AltAz.az < 0)
        AltAz.az += 360;

    return AltAz;
}

void SynscanDriver::sendMountStatus()
{
    bool BasicMountInfoHasChanged = false;

    if (std::string(BasicMountInfoT[MI_ALIGN_STATUS].text) != m_MountInfo[MI_ALIGN_STATUS])
    {
        IUSaveText(&BasicMountInfoT[MI_ALIGN_STATUS], m_MountInfo[MI_ALIGN_STATUS].c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[MI_GOTO_STATUS].text) != m_MountInfo[MI_GOTO_STATUS])
    {
        IUSaveText(&BasicMountInfoT[MI_GOTO_STATUS], m_MountInfo[MI_GOTO_STATUS].c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[MI_POINT_STATUS].text) != m_MountInfo[MI_POINT_STATUS])
    {
        IUSaveText(&BasicMountInfoT[MI_POINT_STATUS], m_MountInfo[MI_POINT_STATUS].c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[MI_TRACK_MODE].text) != m_MountInfo[MI_TRACK_MODE])
    {
        IUSaveText(&BasicMountInfoT[MI_TRACK_MODE], m_MountInfo[MI_TRACK_MODE].c_str());
        BasicMountInfoHasChanged = true;
    }

    if (BasicMountInfoHasChanged)
        IDSetText(&BasicMountInfoTP, nullptr);
}

bool SynscanDriver::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF]= {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if ( (rc = tty_nread_section(PortFD, res, SYN_RES, SYN_DEL, SYN_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF]= {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

void SynscanDriver::mountSim()
{
    static struct timeval ltv;
    struct timeval tv;
    double dt, da, dx;
    int nlocked;

    /* update elapsed time since last poll, don't presume exactly POLLMS */
    gettimeofday(&tv, nullptr);

    if (ltv.tv_sec == 0 && ltv.tv_usec == 0)
        ltv = tv;

    dt  = tv.tv_sec - ltv.tv_sec + (tv.tv_usec - ltv.tv_usec) / 1e6;
    ltv = tv;
    double currentSlewRate = SLEW_RATE[IUFindOnSwitchIndex(&SlewRateSP)] * TRACKRATE_SIDEREAL/3600.0;
    da  = currentSlewRate * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act acoordingly */
    switch (TrackState)
    {
        case SCOPE_IDLE:
            CurrentRA += (TrackRateN[AXIS_RA].value/3600.0 * dt) / 15.0;
            CurrentRA = range24(CurrentRA);
            break;

        case SCOPE_TRACKING:
            break;

        case SCOPE_SLEWING:
        case SCOPE_PARKING:
            /* slewing - nail it when both within one pulse @ SLEWRATE */
            nlocked = 0;

            dx = TargetRA - CurrentRA;

            // Take shortest path
            if (fabs(dx) > 12)
                dx *= -1;

            if (fabs(dx) <= da)
            {
                CurrentRA = TargetRA;
                nlocked++;
            }
            else if (dx > 0)
                CurrentRA += da / 15.;
            else
                CurrentRA -= da / 15.;

            if (CurrentRA < 0)
                CurrentRA += 24;
            else if (CurrentRA > 24)
                CurrentRA -= 24;

            dx = TargetDEC - CurrentDEC;
            if (fabs(dx) <= da)
            {
                CurrentDEC = TargetDEC;
                nlocked++;
            }
            else if (dx > 0)
                CurrentDEC += da;
            else
                CurrentDEC -= da;

            if (nlocked == 2)
            {
                if (TrackState == SCOPE_SLEWING)
                    TrackState = SCOPE_TRACKING;
                else
                    TrackState = SCOPE_PARKED;
            }

            break;

        default:
            break;
    }

    NewRaDec(CurrentRA, CurrentDEC);
}
