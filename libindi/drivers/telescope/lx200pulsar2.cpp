/*
    Pulsar 2 INDI driver

    Copyright (C) 2015 Jasem Mutlaq

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

 
 OPEN ISSUES:
 
1 - Park motion does not stop with click on "Abort"
- 2 - Park position goes wild (in fact, driver should take into account the implemented park coords)
3 - Add a button to define park position
- 4 - Read alpha, delta coordinates from device at initiatialization
- 5 - Sometimes the slew is immediately seen as finished
6 - Tube side of the pier should be readable
 */

#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
//#include <cstring.h>

#include "indicom.h"
#include "lx200pulsar2.h"
#include "lx200driver.h"

#define PULSAR2_BUF             32
#define PULSAR2_TIMEOUT         3

void LX200Pulsar2::cleanedOutput(char * outd)
{
char * pp1;
char * pp2;
char result[10];

    if(pp1 = strchr(outd,'+'))
        if(pp1-outd+1>1) {
            strncpy(result,outd+(pp1-outd),strlen(outd+(pp1-outd)));
        }
    if(pp2 = strchr(outd,'-'))
        if(pp2-outd+1>1) {
            strncpy(result,outd+(pp1-outd),strlen(outd+(pp1-outd)));
        }
    outd=result;
}

LX200Pulsar2::LX200Pulsar2()
{
    setVersion(1, 0);
    
    // currentPierSide=0;
    
    }

bool LX200Pulsar2::Connect()
{
    bool rc=false;
    
    rc = LX200Generic::Connect();
    
    if (rc) {
        if (isParked()) {
            DEBUGF(INDI::Logger::DBG_DEBUG, "%s", "Trying to wake up the mount.");
            UnPark();
        }
        else
        {
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s", "The mount is already tracking.");
        }
    }
    return rc;
}

bool LX200Pulsar2::updateProperties()
{
    
    LX200Generic::updateProperties();

    if (isConnected())
    {
//        defineText(&AzHomeTP);
//        defineText(&AltHomeTP);
        defineSwitch(&PierSideSP);

        // Delete unsupported properties
        deleteProperty(AlignmentSP.name);
        deleteProperty(FocusMotionSP.name);
        deleteProperty(FocusTimerNP.name);
        deleteProperty(FocusModeSP.name);
        deleteProperty(SiteSP.name);
        deleteProperty(SiteNameTP.name);
        deleteProperty(TrackingFreqNP.name);
        deleteProperty(ActiveDeviceTP.name);
        
        return true;
    }
    else
    {
//        deleteProperty(AzHomeTP.name);
//        deleteProperty(AltHomeTP.name);
        deleteProperty(PierSideSP.name);
        return true;
    }
}

const char * LX200Pulsar2::getDefaultName()
{
    return (char *)"Pulsar2";
}

bool LX200Pulsar2::initProperties()
{
    LX200Generic::initProperties();
    
//    IUFillText(&AltHomeT[0], "ParkAlt", "", "");
//    IUFillTextVector(&AltHomeTP, AltHomeT, 1, getDeviceName(), "ParkAlt", "", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

//    IUFillText(&AzHomeT[0], "ParkAz", "", "");
//    IUFillTextVector(&AzHomeTP, AzHomeT, 1, getDeviceName(), "ParkAz", "", OPTIONS_TAB, IP_RO, 0, IPS_IDLE);

    IUFillSwitch(&PierSideS[0], "PIER_EAST", "East (pointing west)", ISS_ON);
    IUFillSwitch(&PierSideS[1], "PIER_WEST", "West (pointing east)", ISS_OFF);
    IUFillSwitchVector(&PierSideSP, PierSideS, 2, getDeviceName(), "TELESCOPE_PIER_SIDE", "Pier side", MAIN_CONTROL_TAB, IP_RO, ISR_1OFMANY, 0, IPS_IDLE);
    
    return true;
}


void LX200Pulsar2::ISGetProperties (const char *dev)
{
    
    if(dev && strcmp(dev,getDeviceName()))
        return;
    
    LX200Generic::ISGetProperties(dev);
    
    if (isConnected())
    {
//        defineText(&AzHomeTP);
//        defineText(&AltHomeTP);
        defineSwitch(&PierSideSP);
    }
}



bool LX200Pulsar2::checkConnection()
{
    char initCMD[] = "#:YV#";
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[PULSAR2_BUF];
    int nbytes_read=0;
    int nbytes_written=0;

    DEBUGF(INDI::Logger::DBG_DEBUG, "Initializing Pulsar2 using %s CMD...", initCMD);

    for (int i=0; i < 2; i++)
    {
        if ( (errcode = tty_write(PortFD, initCMD, strlen(initCMD), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            usleep(50000);
            continue;
        }

        if ( (errcode = tty_read_section(PortFD, response, '#', PULSAR2_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            usleep(50000);
            continue;
        }

        if (nbytes_read > 0)
        {
            response[nbytes_read] = '\0';
            DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

            DEBUGF(INDI::Logger::DBG_SESSION, "Detected %s", response);

            return true;
        }

        usleep(50000);
    }

    return false;
}

bool LX200Pulsar2::updateTime(ln_date * utc, double utc_offset)
{
    
    // For the Pulsar 2, for the time being, any external clock sync is disabled. It is assumed that the
    // PC hosting INDI is well set with its own clock, whatever the time zone is.
    // Otherwise, if sync must be forced, one should use UTC only with Pulsar, by this call:
    
    // return LX200Generic::upadeteTime(utc,0.0);
    return true;
    
}


bool LX200Pulsar2::Goto(double r,double d)
{
    targetRA=r;
    targetDEC=d;
    char RAStr[64], DecStr[64];

    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
         if (!isSimulation() && abortSlew(PortFD) < 0)
         {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
         }

         AbortSP.s = IPS_OK;
         EqNP.s       = IPS_IDLE;
         IDSetSwitch(&AbortSP, "Slew aborted.");
         IDSetNumber(&EqNP, NULL);

         if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
         {
                MovementNSSP.s  = MovementWESP.s =  IPS_IDLE;
                EqNP.s       = IPS_IDLE;
                IUResetSwitch(&MovementNSSP);
                IUResetSwitch(&MovementWESP);
                IDSetSwitch(&MovementNSSP, NULL);
                IDSetSwitch(&MovementWESP, NULL);
          }

       // sleep for 100 mseconds
       usleep(100000);
    }

    if (isSimulation() == false)
    {
        if (setObjectRA(PortFD, targetRA) < 0 || (setObjectDEC(PortFD, targetDEC)) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC.");
            return false;
        }

        int err=0;
        /* Slew reads the '0', that is not the end of the slew */
        if (err = Slew(PortFD))
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            slewError(err);
            return  false;
        }
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s    = IPS_BUSY;

    DEBUGF(INDI::Logger::DBG_SESSION, "Slewing to RA: %s - DEC: %s", RAStr, DecStr);

    // Sleep for 800ms so that YGi returns valid values.
    usleep(800000);

    return true;
}


bool LX200Pulsar2::isSlewComplete()
{
    char cmd[PULSAR2_BUF];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[2];
    int nbytes_read=0;
    int nbytes_written=0;

    strncpy(cmd, "#:YGi#", PULSAR2_BUF);

    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);

    if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if ( (errcode = tty_read(PortFD, response, 1, PULSAR2_TIMEOUT, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }

    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);

        tcflush(PortFD, TCIFLUSH);

        if (response[0] == '0'&&(!isParking()))
            return true;
        else
            return false;
    }

    DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
}

bool LX200Pulsar2::isParking()
{
    int x;
    getCommandInt(PortFD, &x, "#:YGj#");
    if(x==1) {
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s", "Pulsar2 is now parking the telescope.");
        return(true);
        }
    else
        {
        return(false);
        }
}


bool LX200Pulsar2::isHomeSet()
{
    char cmd[PULSAR2_BUF];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[2];
    int nbytes_read=0;
    int nbytes_written=0;
    
    strncpy(cmd, "#:YGh#", PULSAR2_BUF);
    
    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);
    
    if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }
    
    if ( (errcode = tty_read(PortFD, response, 1, PULSAR2_TIMEOUT, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }
    
    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
        
        tcflush(PortFD, TCIFLUSH);
        
        if (response[0] == '1')
            return true;
        else
            return false;
    }
    
    DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;

}

bool LX200Pulsar2::isParked()
{
    char cmd[PULSAR2_BUF];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[2];
    int nbytes_read=0;
    int nbytes_written=0;
    
    strncpy(cmd, "#:YGk#", PULSAR2_BUF);
    
    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);
    
    if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }
    
    if ( (errcode = tty_read(PortFD, response, 1, PULSAR2_TIMEOUT, &nbytes_read)))
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }
    
    if (nbytes_read > 0)
    {
        response[nbytes_read] = '\0';
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
        
        tcflush(PortFD, TCIFLUSH);
        
        if (response[0] == '1') {
            DEBUGF(INDI::Logger::DBG_SESSION, "%s", "Mount is currently parked.");
            TrackState = SCOPE_PARKED;
            ParkSP.s = IPS_OK;
            return true;
        }
        else {
            DEBUGF(INDI::Logger::DBG_SESSION, "%s", "Mount is not parked.");
            return false;
        }
    }
    
    DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
    return false;
    
}

bool LX200Pulsar2::UnPark()
{
    char cmd[PULSAR2_BUF];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[2];
    int nbytes_read=0;
    int nbytes_written=0;
    
    DEBUGF(INDI::Logger::DBG_SESSION, "%s", "Mount wake up.");

    strncpy(cmd, "#:YL#", PULSAR2_BUF);
    
    DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);
    
    if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(errcode, errmsg, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
        return false;
    }
    
    TrackState = SCOPE_TRACKING;
    return true;

}

void LX200Pulsar2::getBasicData()
{
    char cmd[PULSAR2_BUF];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[20];
    int nbytes_read=0;
    int nbytes_written=0;
    int sep;
    char *pp;
    char separ = ',';
    char alt[10], az[10];
    char sideofpier[2];
    
    if (isSimulation() == false)
    {
        
        // read alt/az home position
        /*
        strncpy(cmd, "#:YGX#", PULSAR2_BUF);
        
        DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);
        
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return;
        }
        
        if ( (errcode = tty_read_section(PortFD, response, ',', PULSAR2_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return;
        }
        
        response[nbytes_read-1] = '\0';
        cleanedOutput(response);
        strcpy(az,response);
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
        
        IDSetText(&AzHomeTP,NULL);

        if ( (errcode = tty_read_section(PortFD, response, '#', PULSAR2_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return;
        }

        response[nbytes_read-1] = '\0';
        strcpy(alt, response);
        DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
        
        tcflush(PortFD, TCIFLUSH);

        IUSaveText(&AzHomeTP.tp[1], response);
        IDSetText(&AzHomeTP,NULL);

        DEBUGF(INDI::Logger::DBG_DEBUG, "Az, Alt = %s , %s", az, alt);
            
        IDSetText(&AzHomeTP,NULL);
        IDSetText(&AltHomeTP,NULL);
        */
        
        // ---- read RA, Dec ---
        
        if ( getLX200RA(PortFD, &currentRA) < 0 || getLX200DEC(PortFD, &currentDEC) < 0)
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error reading RA/DEC.");
            return;
        }
        
        NewRaDec(currentRA, currentDEC);

        
        // ---- FLIP STATUS /SIDE of PIER ----
        
        char cmd[PULSAR2_BUF];
        
        strncpy(cmd, "#:YGW#", PULSAR2_BUF);
        
        DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);
        
        if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return;
        }
        
        if ( (errcode = tty_read(PortFD, response, 1, PULSAR2_TIMEOUT, &nbytes_read)))
        {
            tty_error_msg(errcode, errmsg, MAXRBUF);
            DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
            return;
        }
        
        if (nbytes_read > 0)
        {
            response[nbytes_read] = '\0';
            DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
            
            tcflush(PortFD, TCIFLUSH);
            
            // compare to hour angle to decide normal of flipped state
            bool isTargetDueToWest = ( ln_get_apparent_sidereal_time(ln_get_julian_from_sys())*15. - currentRA ) >= 0.0;
            
            if (isTargetDueToWest)
            {
                if (response[0]=='0')
                {
                    strcpy(sideofpier,"E");  //normal
                    currentPierSide=0;
                }
                else
                {
                    strcpy(sideofpier,"W");   // flipped
                    currentPierSide=1;
                }
            }
            else {
             if (response[0]=='0')
                {
                strcpy(sideofpier,"W");  //normal
                currentPierSide=1;
                }
             else
                {
                strcpy(sideofpier,"E");  // flipped
                currentPierSide=0;
                }
               }

            PierSideS[0].s = (currentPierSide == 0) ? ISS_ON : ISS_OFF;
            PierSideS[1].s = (currentPierSide == 1) ? ISS_ON : ISS_OFF;
            IDSetSwitch(&PierSideSP,NULL);
        }
        
    }
}


bool LX200Pulsar2::Park()
{
    char cmd[PULSAR2_BUF];
    int errcode = 0;
    char errmsg[MAXRBUF];
    char response[2];
    int nbytes_read=0;
    int nbytes_written=0;

    if (!isHomeSet()) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", "HOME position is not set -- cannot park");
        return false;
    }
    
    if (isParked()||isParking()) {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s", "No further park action required.");
        return false;
    }
    
    strncpy(cmd, "#:YH#", PULSAR2_BUF);   // goto HOME
    
    if (isSimulation() == false)
    {
        // If scope is moving, let's stop it first.
        if (EqNP.s == IPS_BUSY)
        {
            if (isSimulation() == false && abortSlew(PortFD) < 0)
            {
                AbortSP.s = IPS_ALERT;
                IDSetSwitch(&AbortSP, "Abort slew failed.");
                return false;
            }
            AbortSP.s    = IPS_OK;
            EqNP.s       = IPS_IDLE;
            IDSetSwitch(&AbortSP, "Slew aborted.");
            IDSetNumber(&EqNP, NULL);
            
            if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
            {
                MovementNSSP.s  = MovementWESP.s =  IPS_IDLE;
                EqNP.s       = IPS_IDLE;
                IUResetSwitch(&MovementNSSP);
                IUResetSwitch(&MovementWESP);
                
                IDSetSwitch(&MovementNSSP, NULL);
                IDSetSwitch(&MovementWESP, NULL);
            }
            // sleep (ms ?)
            usleep(10000);
        }
        
        
        DEBUGF(INDI::Logger::DBG_DEBUG, "CMD (%s)", cmd);
            
            if ( (errcode = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
            {
                tty_error_msg(errcode, errmsg, MAXRBUF);
                DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
                return false;
            }
            if ( (errcode = tty_read(PortFD, response, 1, PULSAR2_TIMEOUT, &nbytes_read)))
            {
                tty_error_msg(errcode, errmsg, MAXRBUF);
                DEBUGF(INDI::Logger::DBG_ERROR, "%s", errmsg);
                return false;
            }
            if (nbytes_read > 0)
            {
                response[nbytes_read] = '\0';
                DEBUGF(INDI::Logger::DBG_DEBUG, "RES (%s)", response);
                
                tcflush(PortFD, TCIFLUSH);
                
                if (response[0] == '1')
                    return true;
                else
                    return false;
            }
            
            DEBUGF(INDI::Logger::DBG_ERROR, "Only received #%d bytes, expected 1.", nbytes_read);
            return false;
    }
    
    ParkSP.s = IPS_BUSY;
    TrackState = SCOPE_PARKING;
    IDMessage(getDeviceName(), "Parking telescope in progress...");

    return true;
}

//int LX200Pulsar2::setAltAzHome(int portfd, char * text, const char * name)
//{
//   DEBUGF(INDI::Logger::DBG_DEBUG, "Setting home altitude/azimuth %s - %s", text, name);
//
//}

bool LX200Pulsar2::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
/*        if (!strcmp (name, AltHomeTP.name) || !strcmp (name, AzHomeTP.name))
        {
            if (isSimulation() == false && setAltAzHome(PortFD, texts[0], name) < 0)
            {
                AltHomeTP.s = IPS_ALERT;
                AzHomeTP.s = IPS_ALERT;
                IDSetText(&AltHomeTP, "Setting home altitude");
                IDSetText(&AzHomeTP, "Setting home azimuth");
                return false;
            }
            
            AltHomeTP.s = IPS_OK;
            AzHomeTP.s = IPS_OK;
            IText *tp1 = IUFindText(&AltHomeTP, names[0]);
            IUSaveText(tp1, texts[0]);
            IDSetText(&AltHomeTP , "Home altitude updated");
            IText *tp2 = IUFindText(&AzHomeTP, names[0]);
            IUSaveText(tp2, texts[0]);
            IDSetText(&AzHomeTP , "Home azimuth updated");
            return true;
        }*/
        
    }
    
    return LX200Generic::ISNewText(dev, name, texts, names, n);
}
