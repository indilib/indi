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

#include "synscandriverlegacy.h"
#include "connectionplugins/connectioninterface.h"
#include "connectionplugins/connectiontcp.h"
#include "indicom.h"
#include "libastro.h"

#include <libnova/transform.h>
#include <libnova/precession.h>
// libnova specifies round() on old systems and it collides with the new gcc 5.x/6.x headers
#define HAVE_ROUND
#include <libnova/utility.h>

#include <cmath>
#include <memory>
#include <cstring>

#define SYNSCAN_SLEW_RATES 9

constexpr uint16_t SynscanLegacyDriver::SLEW_RATE[];

SynscanLegacyDriver::SynscanLegacyDriver()
{
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION,
                           SYNSCAN_SLEW_RATES);
    strncpy(LastParkRead, "", 1);
}

bool SynscanLegacyDriver::Connect()
{
    if (isConnected())
        return true;

    bool rc = INDI::Telescope::Connect();

    if (rc)
        return AnalyzeMount();

    return rc;
}

const char *SynscanLegacyDriver::getDefaultName()
{
    return "SynScan Legacy";
}

bool SynscanLegacyDriver::initProperties()
{
    INDI::Telescope::initProperties();

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION | TELESCOPE_HAS_PIER_SIDE,
                           SYNSCAN_SLEW_RATES);
    SetParkDataType(PARK_RA_DEC_ENCODER);

    // Slew Rates
    SlewRateSP[0].setLabel("1x");
    SlewRateSP[1].setLabel("8x");
    SlewRateSP[2].setLabel("16x");
    SlewRateSP[3].setLabel("32x");
    SlewRateSP[4].setLabel("64x");
    SlewRateSP[5].setLabel("128x");
    SlewRateSP[6].setLabel("400x");
    SlewRateSP[7].setLabel("600x");
    SlewRateSP[8].setLabel("MAX");
    SlewRateSP.reset();
    // Max is the default
    SlewRateSP[8].setState(ISS_ON);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    /// Mount Info Text Property
    //////////////////////////////////////////////////////////////////////////////////////////////////
    IUFillText(&BasicMountInfoT[MI_FW_VERSION], "FW_VERSION", "Firmware version", "-");
    IUFillText(&BasicMountInfoT[MI_MOUNT_CODE], "MOUNT_CODE", "Mount code", "-");
    IUFillText(&BasicMountInfoT[MI_ALIGN_STATUS], "ALIGNMENT_STATUS", "Alignment status", "-");
    IUFillText(&BasicMountInfoT[MI_GOTO_STATUS], "GOTO_STATUS", "Goto status", "-");
    IUFillText(&BasicMountInfoT[MI_POINT_STATUS], "MOUNT_POINTING_STATUS",
               "Mount pointing status", "-");
    IUFillText(&BasicMountInfoT[MI_TRACK_MODE], "TRACKING_MODE", "Tracking mode", "-");
    IUFillTextVector(&BasicMountInfoTP, BasicMountInfoT, 6, getDeviceName(), "BASIC_MOUNT_INFO",
                     "Mount information", MountInfoPage, IP_RO, 60, IPS_IDLE);

    //////////////////////////////////////////////////////////////////////////////////////////////////
    /// Use WiFi Switch Property
    //////////////////////////////////////////////////////////////////////////////////////////////////
    //    IUFillSwitch(&UseWiFiS[WIFI_ENABLED], "Enabled", "Enabled", ISS_OFF);
    //    IUFillSwitch(&UseWiFiS[WIFI_DISABLED], "Disabled", "Disabled", ISS_ON);
    //    IUFillSwitchVector(&UseWiFiSP, UseWiFiS, 2, getDeviceName(), "WIFI_SELECT", "Use WiFi?", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    MountTypeSP.reset();
    MountTypeSP[MOUNT_ALTAZ].setState(ISS_ON);

    addAuxControls();

    return true;
}

bool SynscanLegacyDriver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool SynscanLegacyDriver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
#if 0
    if (!strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, UseWiFiSP.name))
        {
            if (isConnected())
            {
                UseWiFiSP.s = IPS_ALERT;
                IDSetSwitch(&UseWiFiSP, nullptr);
                LOG_ERROR("Cannot select WiFi mode while already connected. It must be selected when when disconnected.");
                return true;
            }

            IUUpdateSwitch(&UseWiFiSP, states, names, n);

            if (UseWiFiS[WIFI_ENABLED].s == ISS_ON)
            {
                setTelescopeConnection(CONNECTION_TCP);
                tcpConnection->setDefaultHost("192.168.4.2");
                tcpConnection->setDefaultPort(11882);

                LOG_INFO("Driver is configured for WiFi connection to 192.168.4.2 at TCP port 11882");

                saveConfig(true, "WIFI_SELECT");
                UseWiFiSP.s = IPS_OK;
                IDSetSwitch(&UseWiFiSP, nullptr);

                ISState newStates[] = { ISS_OFF, ISS_ON };
                const char *newNames[] = { "CONNECTION_SERIAL", "CONNECTION_TCP" };
                ISNewSwitch(getDeviceName(), "CONNECTION_MODE", newStates, const_cast<char **>(newNames), 2);
            }
            else
            {
                setTelescopeConnection(CONNECTION_SERIAL);
                LOG_INFO("Driver is configured for serial connection to the hand controller.");

                UseWiFiSP.s = IPS_OK;
                IDSetSwitch(&UseWiFiSP, nullptr);

                ISState newStates[] = { ISS_ON, ISS_OFF };
                const char *newNames[] = { "CONNECTION_SERIAL", "CONNECTION_TCP" };
                ISNewSwitch(getDeviceName(), "CONNECTION_MODE", newStates, const_cast<char **>(newNames), 2);
            }

            return true;
        }
    }
#endif
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool SynscanLegacyDriver::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                    char *formats[], char *names[], int n)
{
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool SynscanLegacyDriver::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool SynscanLegacyDriver::updateProperties()
{
    INDI::Telescope::updateProperties();

    if (isConnected())
    {
        UpdateMountInformation(false);
        defineProperty(&BasicMountInfoTP);
    }
    else
    {
        deleteProperty(BasicMountInfoTP.name);
    }

    return true;
}

int SynscanLegacyDriver::HexStrToInteger(const std::string &res)
{
    int result = 0;

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

bool SynscanLegacyDriver::AnalyzeMount()
{
    LOG_DEBUG("Analyzing Mount...");

    bool rc = true;
    int tmp = 0;
    int bytesWritten = 0;
    int bytesRead;
    char res[MAX_SYN_BUF] = {0};

    // JM 2018-08-15 Why are we reading caps here? Looks like it serves no purpose
    //caps = GetTelescopeCapability();

    rc = ReadLocation();
    if (rc)
    {
        CanSetLocation = true;
        ReadTime();
    }

    if (isSimulation() == false)
    {
        bytesRead = 0;
        memset(res, 0, MAX_SYN_BUF);
        LOG_DEBUG("CMD <J>");
        tty_write(PortFD, "J", 1, &bytesWritten);
        tty_read(PortFD, res, 2, 2, &bytesRead);
        LOGF_DEBUG("RES <%s>", res);

        if (res[0] == 0)
        {
            LOG_ERROR("Mount is not aligned. Please align the mount first and connection again.");
            return false;
        }

        if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
        {
            // Read the handset version
            bytesRead = 0;
            memset(res, 0, MAX_SYN_BUF);
            LOG_DEBUG("Getting Firmware version...");
            LOG_DEBUG("CMD <V>");
            tty_write(PortFD, "V", 1, &bytesWritten);

            tty_read(PortFD, res, 7, 2, &bytesRead);
            LOGF_DEBUG("RES <%s>", res);

            if (bytesRead == 3)
            {
                int tmp1 { 0 }, tmp2 { 0 };

                tmp  = res[0];
                tmp1 = res[1];
                tmp2 = res[2];
                FirmwareVersion = tmp2;
                FirmwareVersion /= 100;
                FirmwareVersion += tmp1;
                FirmwareVersion /= 100;
                FirmwareVersion += tmp;
            }
            else
            {
                FirmwareVersion = (double)HexStrToInteger(std::string(&res[0], 2));
                FirmwareVersion += (double)HexStrToInteger(std::string(&res[2], 2)) / 100;
                FirmwareVersion += (double)HexStrToInteger(std::string(&res[4], 2)) / 10000;
            }

            LOGF_INFO("Firmware version: %lf", FirmwareVersion);

            if (FirmwareVersion < 3.38 || (FirmwareVersion >= 4.0 && FirmwareVersion < 4.38))
            {
                LOG_WARN("Firmware version is too old. Update Synscan firmware to v4.38+");
            }
            else
            {
                NewFirmware = true;
            }

            HandsetFwVersion = std::to_string(FirmwareVersion);

            // Mount Model
            memset(res, 0, MAX_SYN_BUF);
            LOG_DEBUG("CMD <m>");
            tty_write(PortFD, "m", 1, &bytesWritten);
            tty_read(PortFD, res, 2, 2, &bytesRead);
            LOGF_DEBUG("RES <%s>", res);

            if (bytesRead == 2)
            {
                // This workaround is needed because the firmware 3.39 sends these bytes swapped.
                if (res[1] == '#')
                    MountCode = static_cast<int>(*reinterpret_cast<unsigned char*>(&res[0]));
                else
                    MountCode = static_cast<int>(*reinterpret_cast<unsigned char*>(&res[1]));
            }
        }

        // Check the tracking status
        LOG_DEBUG("Getting Tracking status...");
        memset(res, 0, MAX_SYN_BUF);
        LOG_DEBUG("CMD <t>");
        tty_write(PortFD, "t", 1, &bytesWritten);
        tty_read(PortFD, res, 2, 2, &bytesRead);
        LOGF_DEBUG("RES <%s>", res);

        if (res[1] == '#' && static_cast<int>(res[0]) != 0)
        {
            TrackState = SCOPE_TRACKING;
        }
    }

    initParking();

    LOG_DEBUG("Analyzing mount complete.");

    return true;
}

void SynscanLegacyDriver::initParking()
{
    LOG_DEBUG("Initializing parking...");
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

bool SynscanLegacyDriver::ReadScopeStatus()
{
    if (isSimulation())
    {
        MountSim();
        return true;
    }

    char res[MAX_SYN_BUF] = {0};
    int bytesWritten, bytesRead;
    double ra, dec;
    long unsigned int n1, n2;

    LOG_DEBUG("CMD <Ka>");
    tty_write(PortFD, "Ka", 2, &bytesWritten); //  test for an echo

    tty_read(PortFD, res, 2, 2, &bytesRead);   //  Read 2 bytes of response
    LOGF_DEBUG("RES <%s>", res);

    if (res[1] != '#')
    {
        LOG_WARN("Synscan Mount not responding");
        // Usually, Abort() recovers the communication
        RecoverTrials++;
        Abort();
        //        HasFailed = true;
        return false;
    }
    RecoverTrials = 0;

    /*
    //  With 3.37 firmware, on the older line of eq6 mounts
    //  The handset does not always initialize the communication with the motors correctly
    //  We can check for this condition by querying the motors for firmware version
    //  and if it returns zero, it means we need to power cycle the handset
    //  and try again after it restarts again

        if(HasFailed) {
            int v1,v2;
            v1=PassthruCommand(0xfe,0x11,1,0,2);
            v2=PassthruCommand(0xfe,0x10,1,0,2);
            fprintf(stderr,"Motor firmware versions %d %d\n",v1,v2);
            if((v1==0)||(v2==0)) {
                IDMessage(getDeviceName(),"Cannot proceed");
                IDMessage(getDeviceName(),"Handset is responding, but Motors are Not Responding");
                return false;
        }
            //  if we get here, both motors are responding again
            //  so the problem is solved
        HasFailed=false;
        }
    */

    //  on subsequent passes, we just need to read the time
    //    if (HasTime())
    //    {
    //        ReadTime();
    //    }
    if (HasLocation())
    {
        //  this flag is set when we get a new lat/long from the host
        //  so we should go thru the read routine once now, so things update
        //  correctly in the client displays
        if (ReadLatLong)
        {
            ReadLocation();
        }
    }

    // Query mount information
    memset(res, 0, MAX_SYN_BUF);
    LOG_DEBUG("CMD <J>");
    tty_write(PortFD, "J", 1, &bytesWritten);
    tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (res[1] == '#')
    {
        AlignmentStatus = std::to_string((int)res[0]);
    }
    memset(res, 0, MAX_SYN_BUF);
    LOG_DEBUG("CMD <L>");
    tty_write(PortFD, "L", 1, &bytesWritten);
    tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (res[1] == '#')
    {
        GotoStatus = res[0];
    }
    memset(res, 0, MAX_SYN_BUF);
    LOG_DEBUG("CMD <p>");
    tty_write(PortFD, "p", 1, &bytesWritten);
    tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (res[1] == '#')
    {
        PointingStatus = res[0];

        // INDI and mount pier sides are opposite to each other
        setPierSide(res[0] == 'W' ? PIER_EAST : PIER_WEST);
    }
    memset(res, 0, MAX_SYN_BUF);
    LOG_DEBUG("CMD <t>");
    tty_write(PortFD, "t", 1, &bytesWritten);
    tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (res[1] == '#')
    {
        TrackingStatus = res[0];
        switch((int)res[0])
        {
            case 0:
                TrackingMode = "Tracking off";
                break;
            case 1:
                TrackingMode = "Alt/Az tracking";
                break;
            case 2:
                TrackingMode = "EQ tracking";
                break;
            case 3:
                TrackingMode = "PEC mode";
                break;
        }
    }

    UpdateMountInformation(true);

    if (TrackState == SCOPE_SLEWING)
    {
        //  We have a slew in progress
        //  lets see if it's complete
        //  This only works for ra/dec goto commands
        //  The goto complete flag doesn't trip for ALT/AZ commands
        if (GotoStatus != "0")
        {
            //  Nothing to do here
        }
        else if (MountCode < 128)
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
            tty_read(PortFD, res, 2, 3, &bytesRead);
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
            memset(res, 0, MAX_SYN_BUF);
            LOG_DEBUG("CMD <z>");
            tty_write(PortFD, "z", 1, &bytesWritten);
            tty_read(PortFD, res, 18, 2, &bytesRead);
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

    memset(res, 0, MAX_SYN_BUF);
    LOG_DEBUG("CMD <e>");
    tty_write(PortFD, "e", 1, &bytesWritten);
    tty_read(PortFD, res, 18, 1, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);
    if (bytesRead != 18)
    {
        LOG_DEBUG("Read current position failed");
        return false;
    }

    sscanf(res, "%lx,%lx#", &n1, &n2);
    ra  = static_cast<double>(n1) / 0x100000000 * 24.0;
    dec = static_cast<double>(n2) / 0x100000000 * 360.0;

    INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };
    J2000Pos.rightascension  = range24(ra);
    J2000Pos.declination = rangeDec(dec);

    // Synscan reports J2000 coordinates so we need to convert from J2000 to JNow
    INDI::J2000toObserved(&J2000Pos, ln_get_julian_from_sys(), &epochPos);

    CurrentRA  = epochPos.rightascension;
    CurrentDEC = epochPos.declination;

    //  Now feed the rest of the system with corrected data
    NewRaDec(CurrentRA, CurrentDEC);

    if (TrackState == SCOPE_SLEWING && MountCode >= 128 && (SlewTargetAz != -1 || SlewTargetAlt != -1))
    {
        INDI::IHorizontalCoordinates CurrentAltAz { 0, 0 };
        double DiffAlt { 0 };
        double DiffAz { 0 };

        INDI::EquatorialToHorizontal(&epochPos, &m_Location, ln_get_julian_from_sys(), &CurrentAltAz);
        DiffAlt = CurrentAltAz.altitude - SlewTargetAlt;
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
                       CurrentAltAz.altitude, SlewTargetAlt, CurrentAltAz.altitude - SlewTargetAlt, CustomNSSlewRate);
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
        DiffAz = CurrentAltAz.azimuth - SlewTargetAz;
        if (DiffAz < -180)
            DiffAz = (DiffAz + 360) * 2;
        else if (DiffAz > 180)
            DiffAz = (DiffAz - 360) * 2;
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
                       CurrentAltAz.azimuth, SlewTargetAz, CurrentAltAz.azimuth - SlewTargetAz, CustomWESlewRate);
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
            StartTrackMode();
        }
    }
    return true;
}

bool SynscanLegacyDriver::StartTrackMode()
{
    char res[MAX_SYN_BUF] = {0};
    int bytesWritten, bytesRead;

    TrackState = SCOPE_TRACKING;
    LOG_INFO("Tracking started.");

    if (isSimulation())
        return true;

    // Start tracking
    res[0] = 'T';
    // Check the mount type to choose tracking mode
    if (MountCode >= 128)
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
    tty_read(PortFD, res, 1, 2, &bytesRead);
    if (bytesRead != 1 || res[0] != '#')
    {
        LOG_DEBUG("Timeout waiting for scope to start tracking.");
        return false;
    }
    return true;
}

bool SynscanLegacyDriver::Goto(double ra, double dec)
{
    char res[MAX_SYN_BUF] = {0};
    int bytesWritten, bytesRead;
    INDI::IHorizontalCoordinates TargetAltAz { 0, 0 };

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
    if (MountCode < 128 && isSimulation() == false)
    {
        INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };
        epochPos.rightascension  = ra;
        epochPos.declination = dec;

        // Synscan accepts J2000 coordinates so we need to convert from JNow to J2000
        INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);

        // Mount deals in J2000 coords.
        int n1 = J2000Pos.rightascension * 0x1000000 / 24;
        int n2 = J2000Pos.declination * 0x1000000 / 360;

        LOGF_DEBUG("Goto - JNow RA: %g JNow DE: %g J2000 RA: %g J2000 DE: %g", ra, dec, J2000Pos.rightascension,
                   J2000Pos.declination);

        n1 = n1 << 8;
        n2 = n2 << 8;
        LOGF_DEBUG("CMD <%s>", res);
        snprintf(res, MAX_SYN_BUF, "r%08X,%08X", n1, n2);
        tty_write(PortFD, res, 18, &bytesWritten);
        memset(&res[18], 0, 1);

        tty_read(PortFD, res, 1, 60, &bytesRead);
        if (bytesRead != 1 || res[0] != '#')
        {
            LOG_DEBUG("Timeout waiting for scope to complete goto.");
            return false;
        }

        return true;
    }

    INDI::IEquatorialCoordinates epochPos { ra, dec };
    INDI::EquatorialToHorizontal(&epochPos, &m_Location, ln_get_julian_from_sys(), &TargetAltAz);
    LOGF_DEBUG("Goto - JNow RA: %g JNow DE: %g (az: %g alt: %g)", ra, dec, TargetAltAz.azimuth, TargetAltAz.altitude);
    char RAStr[MAX_SYN_BUF] = {0}, DEStr[MAX_SYN_BUF] = {0}, AZStr[MAX_SYN_BUF] = {0}, ATStr[MAX_SYN_BUF] = {0};
    fs_sexa(RAStr, ra, 2, 3600);
    fs_sexa(DEStr, dec, 2, 3600);
    fs_sexa(AZStr, TargetAltAz.azimuth, 2, 3600);
    fs_sexa(ATStr, TargetAltAz.altitude, 2, 3600);

    LOGF_INFO("Goto RA: %s DE: %s AZ: %s ALT: %s", RAStr, DEStr, AZStr, ATStr);

    SlewTargetAz = TargetAltAz.azimuth;
    SlewTargetAlt = TargetAltAz.altitude;

    TargetRA = ra;
    TargetDEC = dec;

    return true;
}

bool SynscanLegacyDriver::Park()
{
    char res[MAX_SYN_BUF] = {0};
    int bytesWritten, bytesRead;

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
        tty_read(PortFD, res, 1, 60, &bytesRead);
        if (bytesRead != 1 || res[0] != '#')
        {
            LOG_DEBUG("Timeout waiting for scope to stop tracking.");
            return false;
        }

        //sprintf((char *)res,"b%08X,%08X",0x0,0x40000000);
        tty_write(PortFD, "b00000000,40000000", 18, &bytesWritten);
        tty_read(PortFD, res, 1, 60, &bytesRead);
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

bool SynscanLegacyDriver::UnPark()
{
    SetParked(false);
    NumPark = 0;
    return true;
}

bool SynscanLegacyDriver::SetCurrentPark()
{
    LOG_INFO("Setting arbitrary park positions is not supported yet.");
    return false;
}

bool SynscanLegacyDriver::SetDefaultPark()
{
    // By default az to north, and alt to pole
    LOG_DEBUG("Setting Park Data to Default.");
    SetAxis1Park(0);
    SetAxis2Park(90);

    return true;
}

bool SynscanLegacyDriver::Abort()
{
    if (TrackState == SCOPE_IDLE || RecoverTrials >= 3)
        return true;

    char res[MAX_SYN_BUF] = {0};
    int bytesWritten, bytesRead;

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

    tty_read(PortFD, res, 1, 2, &bytesRead);
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

bool SynscanLegacyDriver::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    if (isSimulation())
        return true;

    if (command != MOTION_START)
    {
        PassthruCommand(37, 17, 2, 0, 0);
    }
    else
    {
        int tt = (CustomNSSlewRate == -1 ? SlewRate : CustomNSSlewRate);

        tt = tt << 16;
        if (dir != DIRECTION_NORTH)
        {
            PassthruCommand(37, 17, 2, tt, 0);
        }
        else
        {
            PassthruCommand(36, 17, 2, tt, 0);
        }
    }

    return true;
}

bool SynscanLegacyDriver::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    if (isSimulation())
        return true;

    if (command != MOTION_START)
    {
        PassthruCommand(37, 16, 2, 0, 0);
    }
    else
    {
        int tt = (CustomWESlewRate == -1 ? SlewRate : CustomWESlewRate);

        tt = tt << 16;
        if (dir != DIRECTION_WEST)
        {
            PassthruCommand(36, 16, 2, tt, 0);
        }
        else
        {
            PassthruCommand(37, 16, 2, tt, 0);
        }
    }

    return true;
}

bool SynscanLegacyDriver::SetSlewRate(int s)
{
    SlewRate = s + 1;
    return true;
}

int SynscanLegacyDriver::PassthruCommand(int cmd, int target, int msgsize, int data, int numReturn)
{
    char test[20] = {0};
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

bool SynscanLegacyDriver::ReadTime()
{
    LOG_DEBUG("Reading time...");

    if (isSimulation())
    {
        char timeString[MAXINDINAME] = {0};
        time_t now = time (nullptr);
        strftime(timeString, MAXINDINAME, "%T", gmtime(&now));
        TimeTP[UTC].setText("3");
        TimeTP[OFFSET].setText(timeString);
        TimeTP.setState(IPS_OK);
        TimeTP.apply();
        return true;
    }

    char res[MAX_SYN_BUF] = {0};
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
        offset            = (int)res[6];
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
        char ofs[16] = {0};
        sec = (int)utcTime.seconds;
        sprintf(utc, "%04d-%02d-%dT%d:%02d:%02d", utcTime.years, utcTime.months, utcTime.days, utcTime.hours,
                utcTime.minutes, sec);
        if (daylightflag == 1)
            offset = offset + 1;
        snprintf(ofs, 16, "%d", offset);

        TimeTP[UTC].setText(utc);
        TimeTP[OFFSET].setText(ofs);
        TimeTP.setState(IPS_OK);
        TimeTP.apply(nullptr);

        LOGF_INFO("Mount UTC Time %s Offset %d", utc, offset);

        return true;
    }
    return false;
}

bool SynscanLegacyDriver::ReadLocation()
{
    LOG_DEBUG("Reading Location...");

    if (isSimulation())
    {
        LocationNP[LOCATION_LATITUDE].setValue(29.5);
        LocationNP[LOCATION_LONGITUDE].setValue(48);
        LocationNP.apply();
        ReadLatLong = false;
        return true;
    }

    char res[MAX_SYN_BUF] = {0};
    int bytesWritten = 0, bytesRead = 0;

    LOG_DEBUG("CMD <Ka>");
    //  test for an echo
    tty_write(PortFD, "Ka", 2, &bytesWritten);
    //  Read 2 bytes of response
    tty_read(PortFD, res, 2, 2, &bytesRead);
    LOGF_DEBUG("RES <%s>", res);

    if (res[1] != '#')
    {
        LOG_WARN("Bad echo in ReadLocation");
    }
    else
    {
        //  lets see if this hand controller responds to a location request
        bytesRead = 0;
        LOG_DEBUG("CMD <w>");
        tty_write(PortFD, "w", 1, &bytesWritten);

        tty_read(PortFD, res, 9, 2, &bytesRead);
        LOGF_DEBUG("RES <%s>", res);

        if (res[8] == '#')
        {
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

            LOGF_DEBUG("Pos %d:%d:%d  %d:%d:%d", a, b, c, e, f, g);

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
            LocationNP[LOCATION_LATITUDE].setValue(lat);
            LocationNP[LOCATION_LONGITUDE].setValue(lon);
            LocationNP.apply();

            saveConfig(true, "GEOGRAPHIC_COORD");

            char LongitudeStr[32] = {0}, LatitudeStr[32] = {0};
            fs_sexa(LongitudeStr, lon, 2, 3600);
            fs_sexa(LatitudeStr, lat, 2, 3600);
            LOGF_INFO("Mount Longitude %s Latitude %s", LongitudeStr, LatitudeStr);

            //  We dont need to keep reading this one on every cycle
            //  only need to read it when it's been changed
            ReadLatLong = false;
            return true;
        }
        else
        {
            LOG_INFO("Mount does not support setting location.");
        }
    }
    return false;
}

bool SynscanLegacyDriver::updateTime(ln_date *utc, double utc_offset)
{
    char res[MAX_SYN_BUF] = {0};
    int bytesWritten = 0, bytesRead = 0;

    //  start by formatting a time for the hand controller
    //  we are going to set controller to local time
    struct ln_zonedate ltm;

    ln_date_to_zonedate(utc, &ltm, (long)utc_offset * 3600.0);

    int yr = ltm.years;

    yr = yr % 100;

    res[0] = 'H';
    res[1] = ltm.hours;
    res[2] = ltm.minutes;
    res[3] = (char)(int)ltm.seconds;
    res[4] = ltm.months;
    res[5] = ltm.days;
    res[6] = yr;
    // Strangely enough static_cast<int>(double) results 0 for negative values on arm
    // We need to use old C-like casts in this case.
    res[7] = (char)(int)utc_offset; //  offset from utc so hand controller is running in local time
    res[8] = 0;          //  and no daylight savings adjustments, it's already included in the offset
    //  lets write a time to the hand controller
    bytesRead = 0;

    LOGF_INFO("Setting mount date/time to %04d-%02d-%02d %d:%02d:%02d UTC Offset: %d",
              ltm.years, ltm.months, ltm.days, ltm.hours, ltm.minutes, ltm.seconds, utc_offset);

    if (isSimulation())
        return true;

    LOGF_DEBUG("CMD <%s>", res);
    tty_write(PortFD, res, 9, &bytesWritten);

    tty_read(PortFD, res, 1, 2, &bytesRead);
    LOGF_DEBUG("RES <%c>", res[0]);

    if (res[0] != '#')
    {
        LOG_INFO("Invalid return from set time");
    }
    return true;
}

bool SynscanLegacyDriver::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);

    char res[MAX_SYN_BUF] = {0};
    int bytesWritten = 0, bytesRead = 0;
    int s = 0;
    bool IsWest = false;
    double tmp = 0;

    ln_lnlat_posn p1 { 0, 0 };
    lnh_lnlat_posn p2;

    if (isSimulation())
    {
        if (CurrentDEC == 0)
        {
            CurrentDEC = latitude > 0 ? 90 : -90;
            CurrentRA = get_local_sidereal_time(longitude);
        }
        return true;
    }

    if (!CanSetLocation)
    {
        return true;
    }
    else
    {
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

        res[0] = 'W';
        res[1] = p2.lat.degrees;
        res[2] = p2.lat.minutes;
        tmp    = p2.lat.seconds + 0.5;
        s      = (int)tmp; //  put in an int that's rounded
        res[3] = s;
        if (p2.lat.neg == 0)
        {
            res[4] = 0;
        }
        else
        {
            res[4] = 1;
        }

        res[5] = p2.lng.degrees;
        res[6] = p2.lng.minutes;
        s      = (int)(p2.lng.seconds + 0.5); //  make an int, that's rounded
        res[7] = s;
        if (IsWest)
            res[8] = 1;
        else
            res[8] = 0;
        //  All formatted, now send to the hand controller;
        bytesRead = 0;

        LOGF_DEBUG("CMD <%s>", res);
        tty_write(PortFD, res, 9, &bytesWritten);

        tty_read(PortFD, res, 1, 2, &bytesRead);
        LOGF_DEBUG("RES <%c>", res[0]);

        if (res[0] != '#')
        {
            LOG_INFO("Invalid response for location setting");
        }
        //  want to read it on the next cycle, so we update the fields in the client
        ReadLatLong = true;

        return true;
    }
}

bool SynscanLegacyDriver::Sync(double ra, double dec)
{
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
    char res[MAX_SYN_BUF] = {0};
    int bytesWritten, bytesRead;

    if (isSimulation())
    {
        CurrentRA = ra;
        CurrentDEC = dec;
        return true;
    }

    // Alt/Az sync mode
    if (MountCode >= 128)
    {
        INDI::IHorizontalCoordinates TargetAltAz { 0, 0 };
        INDI::IEquatorialCoordinates epochPos {ra, dec};
        INDI::EquatorialToHorizontal(&epochPos, &m_Location, ln_get_julian_from_sys(), &TargetAltAz);
        LOGF_DEBUG("Sync - ra: %g de: %g to az: %g alt: %g", ra, dec, TargetAltAz.azimuth, TargetAltAz.altitude);
        // Assemble the Reset Position command for Az axis
        int Az = (int)(TargetAltAz.azimuth * 16777216 / 360);

        res[0] = 'P';
        res[1] = 4;
        res[2] = 16;
        res[3] = 4;
        *reinterpret_cast<unsigned char*>(&res[4]) = (unsigned char)(Az / 65536);
        Az -= (Az / 65536) * 65536;
        *reinterpret_cast<unsigned char*>(&res[5]) = (unsigned char)(Az / 256);
        Az -= (Az / 256) * 256;
        *reinterpret_cast<unsigned char*>(&res[6]) = (unsigned char)Az;
        res[7] = 0;
        tty_write(PortFD, res, 8, &bytesWritten);
        tty_read(PortFD, res, 1, 3, &bytesRead);
        // Assemble the Reset Position command for Alt axis
        int Alt = (int)(TargetAltAz.altitude * 16777216 / 360);

        res[0] = 'P';
        res[1] = 4;
        res[2] = 17;
        res[3] = 4;
        *reinterpret_cast<unsigned char*>(&res[4]) = (unsigned char)(Alt / 65536);
        Alt -= (Alt / 65536) * 65536;
        *reinterpret_cast<unsigned char*>(&res[5]) = (unsigned char)(Alt / 256);
        Alt -= (Alt / 256) * 256;
        *reinterpret_cast<unsigned char*>(&res[6]) = (unsigned char)Alt;
        res[7] = 0;
        LOGF_DEBUG("CMD <%s>", res);
        tty_write(PortFD, res, 8, &bytesWritten);

        tty_read(PortFD, res, 1, 2, &bytesRead);
        LOGF_DEBUG("CMD <%c>", res[0]);
    }

    INDI::IEquatorialCoordinates epochPos { 0, 0 }, J2000Pos { 0, 0 };

    epochPos.rightascension  = ra;
    epochPos.declination = dec;

    // Synscan accepts J2000 coordinates so we need to convert from JNow to J2000
    //ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);
    INDI::ObservedToJ2000(&epochPos, ln_get_julian_from_sys(), &J2000Pos);

    // Pass the sync command to the handset
    int n1 = J2000Pos.rightascension * 0x1000000 / 24;
    int n2 = J2000Pos.declination * 0x1000000 / 360;

    n1 = n1 << 8;
    n2 = n2 << 8;
    snprintf(res, MAX_SYN_BUF, "s%08X,%08X", n1, n2);
    memset(&res[18], 0, 1);

    LOGF_DEBUG("CMD <%s>", res);
    tty_write(PortFD, res, 18, &bytesWritten);

    tty_read(PortFD, res, 1, 60, &bytesRead);
    LOGF_DEBUG("RES <%c>", res[0]);

    if (bytesRead != 1 || res[0] != '#')
    {
        LOG_DEBUG("Timeout waiting for scope to complete syncing.");
        return false;
    }

    // Start tracking again
    if (IsTrackingBeforeSync)
        StartTrackMode();

    return true;
}

void SynscanLegacyDriver::UpdateMountInformation(bool inform_client)
{
    bool BasicMountInfoHasChanged = false;
    std::string MountCodeStr = std::to_string(MountCode);

    if (std::string(BasicMountInfoT[MI_FW_VERSION].text) != HandsetFwVersion)
    {
        IUSaveText(&BasicMountInfoT[MI_FW_VERSION], HandsetFwVersion.c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[MI_MOUNT_CODE].text) != MountCodeStr)
    {
        IUSaveText(&BasicMountInfoT[MI_MOUNT_CODE], MountCodeStr.c_str());
        BasicMountInfoHasChanged = true;
    }

    if (std::string(BasicMountInfoT[MI_ALIGN_STATUS].text) != AlignmentStatus)
    {
        IUSaveText(&BasicMountInfoT[MI_ALIGN_STATUS], AlignmentStatus.c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[MI_GOTO_STATUS].text) != GotoStatus)
    {
        IUSaveText(&BasicMountInfoT[MI_GOTO_STATUS], GotoStatus.c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[MI_POINT_STATUS].text) != PointingStatus)
    {
        IUSaveText(&BasicMountInfoT[MI_POINT_STATUS], PointingStatus.c_str());
        BasicMountInfoHasChanged = true;
    }
    if (std::string(BasicMountInfoT[MI_TRACK_MODE].text) != TrackingMode)
    {
        IUSaveText(&BasicMountInfoT[MI_TRACK_MODE], TrackingMode.c_str());
        BasicMountInfoHasChanged = true;
    }

    if (BasicMountInfoHasChanged && inform_client)
        IDSetText(&BasicMountInfoTP, nullptr);
}

void SynscanLegacyDriver::MountSim()
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
    double currentSlewRate = SLEW_RATE[SlewRateSP.findOnSwitchIndex()] * TRACKRATE_SIDEREAL / 3600.0;
    da  = currentSlewRate * dt;

    /* Process per current state. We check the state of EQUATORIAL_COORDS and act accordingly */
    switch (TrackState)
    {
        case SCOPE_IDLE:
            CurrentRA += (TrackRateNP[AXIS_RA].getValue() / 3600.0 * dt) / 15.0;
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
