/*
    Pulsar2 INDI driver

    Copyright (C) 2016, 2017 Jasem Mutlaq and Camiel Severijns

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

#include "lx200pulsar2.h"

#include "indicom.h"
#include "lx200driver.h"

#include <cmath>
#include <cerrno>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <mutex>

extern char lx200Name[MAXINDIDEVICE];
extern unsigned int DBG_SCOPE;


namespace PulsarTX
{

namespace
{
	// We re-implement some low-level tty commands to solve intermittent
	// problems with tcflush() calls on the input stream. The following
	// methods send to and parse input from the Pulsar controller.

	static constexpr char Termination = '#';
	static constexpr int TimeOut     = 1; // tenths of a second
	static constexpr int MaxAttempts = 5;
	
	// The following indicates whether the input and output on the port
	// needs to be resynchronized due to a timeout error.
	static bool resynchronize_needed = false;
	
	static std::mutex dev_mtx;
	
	static char lastCmd[40]; // used only for verbose logging
		
	// --- --- --- --- --- --- --- ---
	// "private" namespace methods
	// --- --- --- --- --- --- --- ---
	const char * getDeviceName() // a local implementation meant only to satisfy logging macros such as LOG_INFO
	{
	  return static_cast<const char *>("Pulsar2");
	}

	// The following was a re-work of two previous elegantly-constructed functions,
	// in order to allow the insertion of some debug commands to try to figure out
	// what was going on with the controller.  We just leave it this way for now.
	bool _sendReceiveACK_(const int fd, char *received_char)
	{
		const char ackbuf[1] = { '\006' };
	    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "ACK CMD: <%02X>", ackbuf[0]);
	
		char response[8]; response[0] = LX200Pulsar2::Null; // oversized, just in case
		int nbytes_read = 0;
		bool success = (write(fd, ackbuf, sizeof(ackbuf)) > 0);
		if (success)
		{
			int error_type = tty_read(fd, response, 1, TimeOut, &nbytes_read);
			success = (error_type == TTY_OK && nbytes_read == 1);
			if (success)
			{
				*received_char = response[0];
				DEBUGFDEVICE(lx200Name, DBG_SCOPE, "ACK RESPONSE: <%c>", *received_char);
			}
			else
			{
				DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Error reading ACK: %s", strerror(errno));
			}
		}
		else
		{
			DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Error sending ACK: %s", strerror(errno));
		}
	
		return success;
	}
	
	inline bool _isValidACKResponse_(char test_char)
	{
		bool success;
		switch (test_char)
		{
			case 'P':
			case 'A':
			case 'L':
				success = true;
				break;
			default:
				success = false;
				break;
		}
		return success;
	}
	
	void _resynchronize_(const int fd)
	{
	    DEBUGDEVICE(lx200Name, DBG_SCOPE, "RESYNC");
	    const int ack_maxtries = 10;
	    int ack_try_cntr = 0;
	
		char lead_ACK = LX200Pulsar2::Null;
		char follow_ACK = LX200Pulsar2::Null;
		tcflush(fd, TCIOFLUSH);
		while (resynchronize_needed && ack_try_cntr++ < ack_maxtries)
		{
			if (_isValidACKResponse_(lead_ACK) || (_sendReceiveACK_(fd, &lead_ACK) && _isValidACKResponse_(lead_ACK)))
			{
				if (_isValidACKResponse_(follow_ACK) || (_sendReceiveACK_(fd, &follow_ACK) && _isValidACKResponse_(follow_ACK)))
				{
					if (follow_ACK == lead_ACK)
					{
						resynchronize_needed = false;
					}
					else
					{
						lead_ACK = follow_ACK;
						follow_ACK = LX200Pulsar2::Null;
					}
				}
				else
				{
					lead_ACK = LX200Pulsar2::Null;
					follow_ACK = LX200Pulsar2::Null;
					tcflush(fd, TCIFLUSH);
				}
			}
			else
			{
				lead_ACK = LX200Pulsar2::Null;
				follow_ACK = LX200Pulsar2::Null;
				tcflush(fd, TCIFLUSH);
			}
		}
	
	
		if (resynchronize_needed)
		{
			resynchronize_needed = false; // whether we succeeded or failed
	        DEBUGDEVICE(lx200Name, DBG_SCOPE, "RESYNC error");
			if (LX200Pulsar2::verboseLogging) LOG_INFO("tty resynchronize failed");
		}
		else
		{
	        DEBUGDEVICE(lx200Name, DBG_SCOPE, "RESYNC complete");
			if (LX200Pulsar2::verboseLogging) LOG_INFO("tty resynchronize complete");
		}
	
	}
	
	// Send a command string without waiting for any response from the Pulsar controller
	bool _send_(const int fd, const char *cmd)
	{
	    if (resynchronize_needed) _resynchronize_(fd);
	    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "CMD <%s>", cmd);
	    const int nbytes   = strlen(cmd);
	    int nbytes_written = 0;
	    do
	    {
	        const int errcode = tty_write(fd, &cmd[nbytes_written], nbytes - nbytes_written, &nbytes_written);
	        if (errcode != TTY_OK)
	        {
	            char errmsg[MAXRBUF];
	            tty_error_msg(errcode, errmsg, MAXRBUF);
	            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Error: %s (%s)", errmsg, strerror(errno));
	            return false;
	        }
	    }
	    while (nbytes_written < nbytes);   // Ensure that all characters have been sent
	
	    return true;
	}
	
	// Receive a terminated response string
	bool _receive_(const int fd, char response[], const char *cmd)
	{
	    const struct timespec nanosleeptime = {0, 100000000L}; // 1/10th second
	    response[0]           = LX200Pulsar2::Null;
	    bool done             = false;
	    int nbytes_read_total = 0;
	    int attempt;
	    for (attempt = 0; !done; ++attempt)
	    {
	        int nbytes_read   = 0;
	        const int errcode = tty_read_section(fd, response + nbytes_read_total, Termination, TimeOut, &nbytes_read);
	        if (errcode != TTY_OK)
	        {
	            char errmsg[MAXRBUF];
	            tty_error_msg(errcode, errmsg, MAXRBUF);
	            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Error: %s (%s, attempt %d)", errmsg, strerror(errno), attempt);
	            nbytes_read_total +=
	                nbytes_read; // Keep track of how many characters have been read successfully despite the error
	            if (attempt == MaxAttempts - 1)
	            {
	                resynchronize_needed = (errcode == TTY_TIME_OUT);
	                response[nbytes_read_total] = LX200Pulsar2::Null;
	                if (LX200Pulsar2::verboseLogging) LOGF_INFO("receive: resynchronize_needed flag set for cmd: %s, previous cmd was: %s", cmd, lastCmd);
	                return false;
	            }
				else
				{
					nanosleep(&nanosleeptime, nullptr);
				}
	        }
	        else
	        {
	            // Skip response strings consisting of a single termination character
	            if (nbytes_read_total == 0 && response[0] == Termination)
	                response[0] = LX200Pulsar2::Null;
	            else
	            {
					nbytes_read_total += nbytes_read;
	                done = true;
				}
	        }
	    }
	    response[nbytes_read_total - 1] = LX200Pulsar2::Null; // Remove the termination character
	    DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%s> (attempt %d)", response, attempt);
	    
		if (LX200Pulsar2::verboseLogging) strncpy(lastCmd, cmd, 39);
	
	    return true;
	}

} // end anonymous namespace



// --- --- --- --- --- --- --- ---
// "public" namespace methods
// --- --- --- --- --- --- --- ---

// send a command to the controller, without expectation of a return value.
bool sendOnly(const int fd, const char *cmd)
{
    const std::lock_guard<std::mutex> lock(dev_mtx);
	return _send_(fd, cmd);
}


// Send a command string and wait for a single character response indicating
// success or failure.  Ignore leading # characters.
bool confirmed(const int fd, const char *cmd, char &response)
{
    response = Termination;
    const std::lock_guard<std::mutex> lock(dev_mtx);
    if (_send_(fd, cmd))
    {
        for (int attempt = 0; response == Termination; ++attempt)
        {
            int nbytes_read   = 0;
            const int errcode = tty_read(fd, &response, sizeof(response), TimeOut, &nbytes_read);
            if (errcode != TTY_OK)
            {
                char errmsg[MAXRBUF];
                tty_error_msg(errcode, errmsg, MAXRBUF);
                DEBUGFDEVICE(lx200Name, DBG_SCOPE, "Error: %s (%s, attempt %d)", errmsg, strerror(errno), attempt);
                if (attempt == MaxAttempts - 1)
                {
                    resynchronize_needed = true;
                    if (LX200Pulsar2::verboseLogging) LOGF_INFO("confirmed: resynchronize_needed flag set for cmd: %s", cmd);
                    return false; // early exit
                }
            }
            else // tty_read was successful and nbytes_read should be 1
            {
                DEBUGFDEVICE(lx200Name, DBG_SCOPE, "RES <%c> (attempt %d)", response, attempt);
			}
        }
    }
    return true;
}


// send a command to the controller, expect a terminated response
bool sendReceive(const int fd, const char *cmd, char response[])
{
    const std::lock_guard<std::mutex> lock(dev_mtx);
    bool success = _send_(fd, cmd);
    if (success)
    {
 		success = _receive_(fd, response, cmd);
	}
    return success;
}

// send a command to the controller, expect (up to) two terminated responses
bool sendReceive2(const int fd, const char *cmd, char response1[], char response2[])
{
    const std::lock_guard<std::mutex> lock(dev_mtx);
    bool success = _send_(fd, cmd);
    if (success)
    {
 		success = _receive_(fd, response1, cmd);
 		if (success && response1[1] != Termination) // questionable
 		{
			success = _receive_(fd, response2, cmd);
		}
		else
		{
			*response2 = LX200Pulsar2::Null;
		}
	}
    return success;
}

// send a command to the controller, expect an integral response
bool sendReceiveInt(const int fd, const char *cmd, int *value)
{
    char response[16]; response[15] = LX200Pulsar2::Null;
    bool success = sendReceive(fd, cmd, response);

    if (!success)
    {
        unsigned long rlen = strlen(response);
        if (LX200Pulsar2::verboseLogging) LOGF_INFO("sendReceiveInt() Failed cmd is: %s, response len is: %lu ", cmd, rlen);
    }

    if (success)
    {
        success = (sscanf(response, "%d", value) == 1);
        if (success)
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%d]", *value);
        else
            DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response for integer value");
    }
    return success;
}

// go through the tty "resynchronize" protocol
void resyncTTY(const int fd)
{
    const std::lock_guard<std::mutex> lock(dev_mtx);
	resynchronize_needed = true;
	_resynchronize_(fd);
}

}; // end namespace PulsarTX



namespace Pulsar2Commands
{
// --- --- --- --- --- --- --- ---
// enums and static data members
// --- --- --- --- --- --- --- ---
enum PECorrection
{
    PECorrectionOff = 0,
    PECorrectionOn  = 1
};

enum RCorrection
{
    RCorrectionOff = 0,
    RCorrectionOn  = 1
};

enum TrackingRateInd
{
    RateSidereal = 0,
    RateLunar = 1,
    RateSolar = 2,
    RateUser1 = 3,
    RateUser2 = 4,
    RateUser3 = 5,
    RateStill = 6,
    RateNone = 99
};

enum MountType
{
	German = 0,
	Fork = 1,
	AltAz = 2,
	NumMountTypes
};

enum OTASideOfPier
{
    EastOfPier = 0,
    WestOfPier = 1,
    InvalidSideOfPier
};

enum PoleCrossing
{
    PoleCrossingOff = 0,
    PoleCrossingOn  = 1
};

enum Rotation
{
    RotationZero = 0,
    RotationOne  = 1
};

enum SlewMode
{
    SlewMax = 0,
    SlewFind,
    SlewCenter,
    SlewGuide,
    NumSlewRates
};

enum Direction
{
    North = 0,
    East,
    South,
    West,
    NumDirections
};


// state flags
static OTASideOfPier currentOTASideOfPier = OTASideOfPier::InvalidSideOfPier; // polling will handle this correctly
static int site_location_initialized = 0;
static bool check_ota_side_of_pier = false; // flip-flop
static bool speedsExtended = false; // may change according to firware version

// static codes and labels
static const char *DirectionName[Pulsar2Commands::NumDirections] = { "North", "East", "South", "West" };
static const char DirectionCode[Pulsar2Commands::NumDirections] = {'n', 'e', 's', 'w' };
static char nonGuideSpeedUnit[] = "1x Sidereal";
static char nonGuideSpeedExtendedUnit[] = "1/6x Sidereal";


// --- --- --- --- --- --- --- ---
// local namespace methods
// --- --- --- --- --- --- --- ---

const char * getDeviceName() // a local implementation meant only to satisfy logging macros such as LOG_INFO
{
  return static_cast<const char *>("Pulsar2");
}


// (was inline)
bool getVersion(const int fd, char response[])
{
    return PulsarTX::sendReceive(fd, ":YV#", response);
}

bool getPECorrection(const int fd, PECorrection *PECra, PECorrection *PECdec)
{
    char response[8];
    bool success = PulsarTX::sendReceive(fd, "#:YGP#", response);
    if (success)
    {
        success = (sscanf(response, "%1d,%1d", reinterpret_cast<int *>(PECra), reinterpret_cast<int *>(PECdec)) == 2);
    }
    return success;
}

bool getRCorrection(const int fd, RCorrection *Rra, RCorrection *Rdec)
{
    char response[8];
    bool success = PulsarTX::sendReceive(fd, "#:YGR#", response);
    if (success)
    {
        success = (sscanf(response, "%1d,%1d", reinterpret_cast<int *>(Rra), reinterpret_cast<int *>(Rdec)) == 2);
    }
    return success;
}

TrackingRateInd getTrackingRateInd(const int fd)
{
    TrackingRateInd result = Pulsar2Commands::RateNone; // start off pessimistic
    char response[16]; response[15] = LX200Pulsar2::Null;
    if (PulsarTX::sendReceive(fd, "#:YGS#", response))
    {
        int ra_tri, dec_tri;
        if (sscanf(response, "%1d,%1d", &ra_tri, &dec_tri) == 2)
		{
			result = static_cast<TrackingRateInd>(ra_tri == 0 ? (LX200Pulsar2::numPulsarTrackingRates - 1) : --ra_tri);
		}
    }
    return result;
}


MountType getMountType(const int fd)
{
	MountType result = Pulsar2Commands::German; // the overwhelming default
    char response[16]; response[15] = LX200Pulsar2::Null;
	if (PulsarTX::sendReceive(fd, "#:YGM#", response))
	{
		int itype;
		if (sscanf(response, "%d", &itype) == 1)
		{
			switch (itype)
			{
				case 1: result = Pulsar2Commands::German; break;
				case 2: result = Pulsar2Commands::Fork; break;
				case 3: result = Pulsar2Commands::AltAz; break;
				default: break; // paranoid
			}
		}
	}
	return result;
}

int getSpeedInd(const int fd, const char *cmd)
{
	int result = 0; // start off pessimistic (zero is a non-valid value)
    char response[16]; response[15] = LX200Pulsar2::Null;
	if (PulsarTX::sendReceive(fd, cmd, response))
	{
		int dec_dummy;
        if (sscanf(response, "%d,%d", &result, &dec_dummy) != 2) result = 0;
	}
	return result;
}

int getGuideSpeedInd(const int fd)
{
	return getSpeedInd(fd, "#:YGA#");
}

int getCenterSpeedInd(const int fd)
{
	return getSpeedInd(fd, "#:YGB#");
}

int getFindSpeedInd(const int fd)
{
	return getSpeedInd(fd, "#:YGC#");
}

int getSlewSpeedInd(const int fd)
{
	return getSpeedInd(fd, "#:YGD#");
}

int getGoToSpeedInd(const int fd)
{
	return getSpeedInd(fd, "#:YGE#");
}


bool getSwapTubeDelay(const int fd, int *delay_value) // unknown so far
{
	bool success = PulsarTX::sendReceiveInt(fd, "", delay_value);
	return success;
}

bool getPoleCrossingDirection(const int fd, int *direction) // unknown so far
{
	bool success = PulsarTX::sendReceiveInt(fd, "", direction);
	return success;
}


bool getRamp(const int fd, int *ra_ramp, int *dec_ramp)
{
    char response[16]; response[15] = LX200Pulsar2::Null;
	bool success = PulsarTX::sendReceive(fd, "#:YGp#", response);
	if (success)
	{
        success = (sscanf(response, "%u,%u", ra_ramp, dec_ramp) == 2);
	}
	return success;
}

bool setRamp(const int fd, int ra_ramp, int dec_ramp)
{
    char cmd[16]; cmd[15] = LX200Pulsar2::Null;
	int safe_ra_ramp = std::min(std::max(1, ra_ramp), 10);
	int safe_dec_ramp = std::min(std::max(1, dec_ramp), 10);
	sprintf(cmd, "#:YSp%d,%d#", safe_ra_ramp, safe_dec_ramp);
	char response = LX200Pulsar2::Null;
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool getReduction(const int fd, int *red_ra, int *red_dec)
{
    char response[20]; response[19] = LX200Pulsar2::Null;
	bool success = PulsarTX::sendReceive(fd, "#:YGr#", response);
	if (success)
	{
        success = (sscanf(response, "%u,%u", red_ra, red_dec) == 2);
	}
	return success;	
}

bool setReduction(const int fd, int red_ra, int red_dec)
{
    char cmd[20]; cmd[19] = LX200Pulsar2::Null;
	int safe_red_ra = std::min(std::max(100, red_ra), 6000);
	int safe_red_dec = std::min(std::max(100, red_dec), 6000);
	sprintf(cmd, "#:YSr%d,%d#", safe_red_ra, safe_red_dec);
	char response = LX200Pulsar2::Null;
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}


bool getMaingear(const int fd, int *mg_ra, int *mg_dec)
{
    char response[20]; response[19] = LX200Pulsar2::Null;
	bool success = PulsarTX::sendReceive(fd, "#:YGm#", response);
	if (success)
	{
        success = (sscanf(response, "%u,%u", mg_ra, mg_dec) == 2);
	}
	return success;	
}

bool setMaingear(const int fd, int mg_ra, int mg_dec)
{
    char cmd[20]; cmd[19] = LX200Pulsar2::Null;
	int safe_mg_ra = std::min(std::max(100, mg_ra), 6000);
	int safe_mg_dec = std::min(std::max(100, mg_dec), 6000);
	sprintf(cmd, "#:YSm%d,%d#", safe_mg_ra, safe_mg_dec);
	char response = LX200Pulsar2::Null;
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool getBacklash(const int fd, int *bl_min, int *bl_sec)
{
    char response[20]; response[19] = LX200Pulsar2::Null;
	bool success = PulsarTX::sendReceive(fd, "#:YGb#", response);
	if (success)
	{
		success = (sscanf(response, "%u:%u", bl_min, bl_sec) == 2);
	}
	return success;
}

bool setBacklash(const int fd, int bl_min, int bl_sec)
{
    char cmd[20]; cmd[19] = LX200Pulsar2::Null;
	int safe_bl_min = std::min(std::max(0, bl_min), 9);
	int safe_bl_sec = std::min(std::max(0, bl_sec), 59);
	sprintf(cmd, "#:YSb%d,%02d#", safe_bl_min, safe_bl_sec);
	char response = LX200Pulsar2::Null;
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool getHomePosition(const int fd, double *hp_alt, double *hp_az)
{
    char response[30]; response[18] = LX200Pulsar2::Null; response[29] = LX200Pulsar2::Null;
	bool success = PulsarTX::sendReceive(fd, "#:YGX#", response);
	if (success)
	{
        success = (sscanf(response, "%lf,%lf", hp_alt, hp_az) == 2);
	}
	return success;		
}

bool setHomePosition(const int fd, double hp_alt, double hp_az)
{
    char cmd[30]; cmd[29] = LX200Pulsar2::Null;
    double safe_hp_alt = std::min(std::max(0.0, hp_alt), 90.0);
     // There are odd limits for azimuth because the controller rounds
     // strangely, and defaults to a 180-degree value if it sees a number
     // as out-of-bounds. The min value here (0.0004) will be intepreted
     // as zero, max (359.9994) as 360.
    double safe_hp_az = std::min(std::max(0.0004, hp_az), 359.9994);
	sprintf(cmd, "#:YSX%+08.4lf,%08.4lf#", safe_hp_alt, safe_hp_az);
    char response = LX200Pulsar2::Null;
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

// note that the following has not been verified to work correctly
bool getUserRate(const int fd, int usr_ind, double *ur_ra, double *ur_dec)
{
    char response[30]; response[22] = LX200Pulsar2::Null; response[29] = LX200Pulsar2::Null;
    if (usr_ind < 1 || usr_ind > 3) return false; // paranoid, early exit
    char cmd[] = "#:YGZ_#";
    cmd[5] = usr_ind + '0';
	bool success = PulsarTX::sendReceive(fd, cmd, response);
	if (success)
	{
		// debug
		//LOGF_INFO("getUserRate(%d) for cmd: %s returns: %s", usr_ind, cmd, response);
		// end debug
		success = (sscanf(response, "%lf,%lf", ur_ra, ur_dec) == 2);
	}
	return success;
}


bool getUserRate1(const int fd, double *u1_ra, double *u1_dec)
{
	return getUserRate(fd, 1, u1_ra, u1_dec);
}

bool getUserRate2(const int fd, double *u2_ra, double *u2_dec)
{
	return getUserRate(fd, 2, u2_ra, u2_dec);
}

bool getUserRate3(const int fd, double *u3_ra, double *u3_dec)
{
	return getUserRate(fd, 3, u3_ra, u3_dec);
}


// note that the following has not been verified to work correctly
bool setUserRate(const int fd, int usr_ind, double ur_ra, double ur_dec)
{
    if (usr_ind < 1 || usr_ind > 3) return false; // paranoid, early exit
    char cmd[36]; cmd[35] = LX200Pulsar2::Null;
    double safe_ur_ra = std::min(std::max(-4.1887902, ur_ra), 4.1887902);
    double safe_ur_dec = std::min(std::max(-4.1887902, ur_dec), 4.1887902);
	sprintf(cmd, "#:YSZ%1c%+09.7lf,%+09.7lf#", usr_ind + '0', safe_ur_ra, safe_ur_dec);
    char response = LX200Pulsar2::Null;
    // debug
    //LOGF_INFO("setUserRate(%d) sending command: %s", usr_ind, cmd);
	// end debug
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool setUserRate1(const int fd, double ur_ra, double ur_dec)
{
	return setUserRate(fd, 1, ur_ra, ur_dec);
}

bool setUserRate2(const int fd, double ur_ra, double ur_dec)
{
	return setUserRate(fd, 2, ur_ra, ur_dec);
}

bool setUserRate3(const int fd, double ur_ra, double ur_dec)
{
	return setUserRate(fd, 3, ur_ra, ur_dec);
}



int getCurrentValue(const int fd, const char *cmd)
{
	int result = 0; // start off pessimistic (zero is a non-valid value)
    char response[16]; response[15] = LX200Pulsar2::Null;
	if (PulsarTX::sendReceive(fd, cmd, response))
	{
		int dec_dummy;
        if (sscanf(response, "%d,%d", &result, &dec_dummy) != 2) result = 0;
	}
	return result;	
}

int getTrackingCurrent(const int fd) // return is mA
{
	return getCurrentValue(fd, "#:YGt#");
}

int getStopCurrent(const int fd) // return is mA
{
	return getCurrentValue(fd, "#:YGs#");
}

int getGoToCurrent(const int fd) // return is mA
{
	return getCurrentValue(fd, "#:YGg#");
}

bool setMountType(const int fd, Pulsar2Commands::MountType mtype)
{
	char cmd[] = "#:YSM_#";
	cmd[5] = '0' + (char)((int)mtype + 1);
	char response = LX200Pulsar2::Null;
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool setCurrentValue(const int fd, const char *partialCmd, const int mA, const int maxmA) // input is mA
{
    char cmd[20]; cmd[19] = LX200Pulsar2::Null;
    char response;
    int actualCur = std::min(std::max(mA, 100), maxmA); // reasonable limits
	sprintf(cmd, "%s%04d,%04d#", partialCmd, actualCur, actualCur);
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool setTrackingCurrent(const int fd, const int mA)
{
	return setCurrentValue(fd, "#:YSt", mA, 2000);
}

bool setStopCurrent(const int fd, const int mA)
{
	return setCurrentValue(fd, "#:YSs", mA, 2000);
}

bool setGoToCurrent(const int fd, const int mA)
{
	return setCurrentValue(fd, "#:YSg", mA, 2000);
}

bool setSpeedInd(const int fd, const char *partialCmd, const int speedInd, const int maxInd)
{
    char cmd[20]; cmd[19] = LX200Pulsar2::Null;
    char response;
    int actualInd = std::min(std::max(speedInd, 1), maxInd);
	sprintf(cmd, "%s%04d,%04d#", partialCmd, actualInd, actualInd);
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool setGuideSpeedInd(const int fd, const int speedInd)
{
	return setSpeedInd(fd, "#:YSA", speedInd, 9);
}

bool setCenterSpeedInd(const int fd, const int speedInd)
{
	int maxVal = speedsExtended ? 9999 : 999;
	return setSpeedInd(fd, "#:YSB", speedInd, maxVal);
}

bool setFindSpeedInd(const int fd, const int speedInd)
{
	int maxVal = speedsExtended ? 9999 : 999;
	return setSpeedInd(fd, "#:YSC", speedInd, maxVal);
}

bool setSlewSpeedInd(const int fd, const int speedInd)
{
	int maxVal = speedsExtended ? 9999 : 999;
	return setSpeedInd(fd, "#:YSD", speedInd, maxVal);
}

bool setGoToSpeedInd(const int fd, const int speedInd)
{
	int maxVal = speedsExtended ? 9999 : 999;
	return setSpeedInd(fd, "#:YSE", speedInd, maxVal);
}


bool getSideOfPier(const int fd, OTASideOfPier *ota_side_of_pier)
{
	*ota_side_of_pier = Pulsar2Commands::OTASideOfPier::EastOfPier; // effectively a fail-safe default
	int ival;
	if (!PulsarTX::sendReceiveInt(fd, "#:YGN#", &ival)) return false;
	if (ival == 1) *ota_side_of_pier = Pulsar2Commands::OTASideOfPier::WestOfPier;
	return true;
}


bool getPoleCrossing(const int fd, PoleCrossing *pole_crossing)
{
    return PulsarTX::sendReceiveInt(fd, "#:YGQ#", reinterpret_cast<int *>(pole_crossing));
}


bool getRotation(const int fd, Rotation *rot_ra, Rotation *rot_dec)
{
    char response[8]; response[7] = LX200Pulsar2::Null;
    bool success = PulsarTX::sendReceive(fd, "#:YGn#", response);
    if (success)
    {
        success = (sscanf(response, "%1d,%1d", reinterpret_cast<int *>(rot_ra), reinterpret_cast<int *>(rot_dec)) == 2);
    }
    return success;
}


bool getSexa(const int fd, const char *cmd, double *value)
{
    char response[16]; response[15] = LX200Pulsar2::Null;
    bool success = PulsarTX::sendReceive(fd, cmd, response);
    if (success)
    {
        success = (f_scansexa(response, value) == 0);
        if (success)
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%g]", *value);
        else
            DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response");
    }
    return success;
}


bool getObjectRADec(const int fd, double *ra, double *dec)
{
    return (getSexa(fd, "#:GR#", ra) && getSexa(fd, "#:GD#", dec));
}

// --- --- --- --- --- --- --- --- --- --- ---
// Older-style geographic coordinate handling
//bool getDegreesMinutes(const int fd, const char *cmd, int *d, int *m)
//{
//    *d = *m = 0;
//    char response[16];
//    bool success = sendReceive(fd, cmd, response);
//    if (success)
//    {
//        success = (sscanf(response, "%d%*c%d", d, m) == 2);
//        if (success)
//            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%+03d:%02d]", *d, *m);
//        else
//            DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse response");
//    }
//    return success;
//}

//inline bool getSiteLatitude(const int fd, int *d, int *m)
//{
//    return getDegreesMinutes(fd, "#:Gt#", d, m);
//}

//inline bool getSiteLongitude(const int fd, int *d, int *m)
//{
//    return getDegreesMinutes(fd, "#:Gg#", d, m);
//}
// --- --- --- --- --- --- --- --- --- --- ---

// Newer-style latitude-longitude in a single call, with correction to
// make west negative, rather than east (as the controller returns)
// (was inline)
bool getSiteLatitudeLongitude(const int fd, double *lat, double *lon)
{
    *lat = 0.0;
    *lon = 0.0;
    char response[32]; response[31] = LX200Pulsar2::Null;

    bool success = PulsarTX::sendReceive(fd, "#:YGl#", response);
    if (success)
    {
        success = (sscanf(response, "%lf,%lf", lat, lon) == 2);
        if (success)
		{
			*lon = (*lon) * (-1.0);
		}
		else
		{
            DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse latitude-longitude response");
		}
    }
    return success;
}

bool getUTCDate(const int fd, int *m, int *d, int *y)
{
    char response[12];
    bool success = PulsarTX::sendReceive(fd, "#:GC#", response);
    if (success)
    {
        success = (sscanf(response, "%2d%*c%2d%*c%2d", m, d, y) == 3);
        if (success)
        {
            *y += (*y < 50 ? 2000 : 1900);
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%02d/%02d/%04d]", *m, *d, *y);
        }
        else
            DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse date string");
    }
    return success;
}

bool getUTCTime(const int fd, int *h, int *m, int *s)
{
    char response[12];
    bool success = PulsarTX::sendReceive(fd, "#:GL#", response);
    if (success)
    {
        success = (sscanf(response, "%2d%*c%2d%*c%2d", h, m, s) == 3);
        if (success)
            DEBUGFDEVICE(lx200Name, DBG_SCOPE, "VAL [%02d:%02d:%02d]", *h, *m, *s);
        else
            DEBUGDEVICE(lx200Name, DBG_SCOPE, "Unable to parse time string");
    }
    return success;
}

bool setDegreesMinutes(const int fd, const char *partialCmd, const double value)
{
    int degrees, minutes, seconds;
    getSexComponents(value, &degrees, &minutes, &seconds);
    char full_cmd[32];
    snprintf(full_cmd, sizeof(full_cmd), "#:%s %03d:%02d#", partialCmd, degrees, minutes);
    char response;
    return (PulsarTX::confirmed(fd, full_cmd, response) && response == '1');
}


bool setSite(const int fd, const double longitude, const double latitude)
{
    return (setDegreesMinutes(fd, "Sl", 360.0 - longitude) && setDegreesMinutes(fd, "St", latitude));
}

bool setSlewMode(const int fd, const SlewMode slewMode)
{
    static const char *commands[NumSlewRates] { "#:RS#", "#:RM#", "#:RC#", "#:RG#" };
    return PulsarTX::sendOnly(fd, commands[slewMode]);
}

bool moveTo(const int fd, const Direction direction)
{
    static const char *commands[NumDirections] = { "#:Mn#", "#:Me#", "#:Ms#", "#:Mw#" };
    return PulsarTX::sendOnly(fd, commands[direction]);
}

bool haltMovement(const int fd, const Direction direction)
{
    static const char *commands[NumDirections] = { "#:Qn#", "#:Qe#", "#:Qs#", "#:Qw#" };
    return PulsarTX::sendOnly(fd, commands[direction]);
}


bool startSlew(const int fd)
{
    char response[4];
    const bool success = (PulsarTX::sendReceive(fd, "#:MS#", response) && response[0] == '0');
    return success;
}


bool abortSlew(const int fd)
{
	return PulsarTX::sendOnly(fd, "#:Q#");
}

// Pulse guide commands are only supported by the Pulsar2 controller, and NOT the older Pulsar controller
bool pulseGuide(const int fd, const Direction direction, uint32_t ms)
{
	// make sure our pulse length is in a reasonable range
    int safePulseLen = std::min(std::max(1, static_cast<int>(ms)), 9990);

    bool success = true;
    if (safePulseLen > 4) // otherwise send no guide pulse -- 10ms is our minimum (5+ will round upward)
    {
		// our own little rounding method, so as not to call slower library rounding routines
		int splm10 = safePulseLen % 10;
		if (splm10 != 0) // worth the test, since it happens frequently
		{
			safePulseLen = (splm10 > 4) ? (safePulseLen - splm10) + 10 : safePulseLen - splm10;
		}

		char cmd[16];
		snprintf(cmd, sizeof(cmd), "#:Mg%c%04d#", Pulsar2Commands::DirectionCode[direction], safePulseLen);
		success = PulsarTX::sendOnly(fd, cmd);
		if (LX200Pulsar2::verboseLogging)
		{		
			if (success)
				LOGF_INFO("Pulse guide sent, direction %c, len: %d ms, cmd: %s", Pulsar2Commands::DirectionCode[direction], safePulseLen, cmd);
			else
				LOGF_INFO("Pulse guide FAILED direction %c, len: %d ms, cmd: %s", Pulsar2Commands::DirectionCode[direction], safePulseLen, cmd);
		}
	}
    return success;
}

bool setTime(const int fd, const int h, const int m, const int s)
{
    char full_cmd[32];
    snprintf(full_cmd, sizeof(full_cmd), "#:SL %02d:%02d:%02d#", h, m, s);
    char response;
    return (PulsarTX::confirmed(fd, full_cmd, response) && response == '1');
}

bool setDate(const int fd, const int dd, const int mm, const int yy)
{
	char response1[64]; // only first character consulted
	char response2[64]; // not used
    char cmd[32];
    snprintf(cmd, sizeof(cmd), ":SC %02d/%02d/%02d#", mm, dd, (yy % 100));
	bool success = (PulsarTX::sendReceive2(fd, cmd, response1, response2) && (*response1 == '1'));
    return success;
}

bool ensureLongFormat(const int fd)
{
    char response[16] = { 0 };
    bool success = PulsarTX::sendReceive(fd, "#:GR#", response);
    if (success)
    {
		if (response[5] == '.')
		{
			// In case of short format, set long format
			success = (PulsarTX::confirmed(fd, "#:U#", response[0]) && response[0] == '1');
		}
	}
    return success;
}

bool setObjectRA(const int fd, const double ra)
{
    int h, m, s;
    getSexComponents(ra, &h, &m, &s);
    char full_cmd[32];
    snprintf(full_cmd, sizeof(full_cmd), "#:Sr %02d:%02d:%02d#", h, m, s);
    char response;
    return (PulsarTX::confirmed(fd, full_cmd, response) && response == '1');
}

bool setObjectDEC(const int fd, const double dec)
{
    int d, m, s;
    getSexComponents(dec, &d, &m, &s);
    char full_cmd[32];
    snprintf(full_cmd, sizeof(full_cmd), "#:Sd %c%02d:%02d:%02d#",( dec < 0.0 ? '-' : '+' ), abs(d), m, s);
    char response;
    return (PulsarTX::confirmed(fd, full_cmd, response) && response == '1');
}


bool setObjectRADec(const int fd, const double ra, const double dec)
{
    return (setObjectRA(fd, ra) && setObjectDEC(fd, dec));
}


bool park(const int fd)
{
    int success = 0;
    return (PulsarTX::sendReceiveInt(fd, "#:YH#", &success) && success == 1);
}


bool unpark(const int fd)
{
    int result = 0;
    if (!PulsarTX::sendReceiveInt(fd, "#:YL#", &result))
    {
		// retry
		if (LX200Pulsar2::verboseLogging) LOG_INFO("Unpark retry compensating for failed unpark return value...");
		if (!PulsarTX::sendReceiveInt(fd, "#:YL#", &result))
			result = 0;
	}
    return (result == 1);
}


bool sync(const int fd)
{
	return PulsarTX::sendOnly(fd, "#:CM#");
}

static const char ZeroOneChar[2] = { '0', '1' };

bool setSideOfPier(const int fd, const OTASideOfPier ota_side_of_pier)
{
    char cmd[] = "#:YSN_#";
    cmd[5] = ZeroOneChar[(int)ota_side_of_pier];
    char response;
    return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool setTrackingRateInd(const int fd, const TrackingRateInd tri)
{
    char cmd[16]; cmd[15] = LX200Pulsar2::Null;
    char response;
    unsigned int trii = static_cast<unsigned int>(tri);
    trii = (trii == (LX200Pulsar2::numPulsarTrackingRates - 1)) ? 0 : (trii + 1);
    sprintf(cmd, "#:YSS%u,%u#", trii, 0);
    return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool setPECorrection(const int fd, const PECorrection pec_ra, const PECorrection pec_dec)
{
    char cmd[] = "#:YSP_,_#";
    cmd[5]            = ZeroOneChar[(int)pec_ra];
    cmd[7]            = ZeroOneChar[(int)pec_dec];
    char response;
    return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool setPoleCrossing(const int fd, const PoleCrossing pole_crossing)
{
    char cmd[] = "#:YSQ_#";
    cmd[5]            = ZeroOneChar[(int)pole_crossing];
    char response;
    return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

bool setRCorrection(const int fd, const RCorrection rc_ra, const RCorrection rc_dec)
{
    char cmd[] = "#:YSR_,_#";
    cmd[5]            = ZeroOneChar[(int)rc_ra];
    cmd[7]            = ZeroOneChar[(int)rc_dec];
    char response;
    return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}


bool setRotation(const int fd, const Rotation rot_ra, const Rotation rot_dec)
{
	char cmd[] = "#:YSn_,_#";
	cmd[5] = ZeroOneChar[(int)rot_ra];
	cmd[7] = ZeroOneChar[(int)rot_dec];
	char response;
	return (PulsarTX::confirmed(fd, cmd, response) && response == '1');
}

// - - - - - - - - - - - - - - - - - - -
// Predicates
// - - - - - - - - - - - - - - - - - - -

bool isHomeSet(const int fd)
{
    int is_home_set = -1;
    return (PulsarTX::sendReceiveInt(fd, "#:YGh#", &is_home_set) && is_home_set == 1);
}


bool isParked(const int fd)
{
    int is_parked = -1;
    return (PulsarTX::sendReceiveInt(fd, "#:YGk#", &is_parked) && is_parked == 1);
}


bool isParking(const int fd)
{
    int is_parking = -1;
    return (PulsarTX::sendReceiveInt(fd, "#:YGj#", &is_parking) && is_parking == 1);
}

}; // end Pulsar2Commands namespace


// ----------------------------------------------------
// LX200Pulsar2 implementation
// ----------------------------------------------------

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// constructor
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
LX200Pulsar2::LX200Pulsar2() : LX200Generic(), just_started_slewing(false)
{
    setVersion(1, 2);
    //setLX200Capability(0);
    setLX200Capability(LX200_HAS_PULSE_GUIDING);

	// Note that we do not have TELESCOPE_PIER_SIDE indicated here, since we re-implement it --
	// there is just too much confusion surrounding that value, so we preempt it.
    SetTelescopeCapability(TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION, 4);
                           
    PulsarTX::lastCmd[0] = LX200Pulsar2::Null; // paranoid
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
// Overrides
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

const char *LX200Pulsar2::getDefaultName()
{
    return static_cast<const char *>("Pulsar2");
}

bool LX200Pulsar2::Connect()
{
    const bool success = INDI::Telescope::Connect(); // takes care of hardware connection
    if (success)
    {
		if (Pulsar2Commands::isParked(PortFD))
        {
            LOGF_DEBUG("%s", "Trying to wake up the mount.");
            UnPark();
        }
		else
		{
			LOGF_DEBUG("%s", "The mount was awake on connection.");
			// the following assumes we are tracking, since there is no "idle" state for Pulsar2
			TrackState = SCOPE_TRACKING;
			ParkS[1].s = ISS_ON; // Unparked
			IDSetSwitch(&ParkSP, nullptr);
		}
    }

    return success;
}

bool LX200Pulsar2::Disconnect()
{
	// TODO: set tracking state (?)
	LX200Generic::Disconnect();

	return true;
}

bool LX200Pulsar2::Handshake()
{
    // Anything needs to be done besides this? INDI::Telescope would call ReadScopeStatus but
    // maybe we need to UnPark() before ReadScopeStatus() can return valid results?
    return true;
}

// The following function is called at the configured polling interval
bool LX200Pulsar2::ReadScopeStatus()
{
    bool success = isConnected();

    if (success)
    {
        success = isSimulation();
        if (success)
            mountSim();
        else
        {
			if (this->initialization_complete)
			{
				// set track state for slewing and parking
	            switch (TrackState)
	            {
	                case SCOPE_SLEWING:
	                    // Check if LX200 is done slewing
	                    if (isSlewComplete())
	                    {
	                        // Set slew mode to "Centering"
	                        IUResetSwitch(&SlewRateSP);
	                        SlewRateS[SLEW_CENTERING].s = ISS_ON;
	                        IDSetSwitch(&SlewRateSP, nullptr);
	                        TrackState = SCOPE_TRACKING;
	                        IDMessage(getDeviceName(), "Slew is complete. Tracking...");
	                    }
	                    break;
	
	                case SCOPE_PARKING:
	                    if (isSlewComplete() && !Pulsar2Commands::isParking(PortFD)) // !isParking() is experimental
	                        SetParked(true);
	                    break;

	                default:
	                    break;
	            }

				// read RA/Dec
	            success = Pulsar2Commands::getObjectRADec(PortFD, &currentRA, &currentDEC);
	            if (success)
	                NewRaDec(currentRA, currentDEC);
	            else
	            {
	                EqNP.s = IPS_ALERT;
	                IDSetNumber(&EqNP, "Error reading RA/DEC.");
	            }

				// check side of pier -- note that this is done only every other polling cycle
				Pulsar2Commands::check_ota_side_of_pier = !Pulsar2Commands::check_ota_side_of_pier; // set flip-flop
				if (Pulsar2Commands::check_ota_side_of_pier)
				{
					Pulsar2Commands::OTASideOfPier ota_side_of_pier;
					if (Pulsar2Commands::getSideOfPier(PortFD, &ota_side_of_pier))
					{
						if (ota_side_of_pier != Pulsar2Commands::currentOTASideOfPier) // init, or something changed
						{
							PierSideS[(int)Pulsar2Commands::EastOfPier].s = 
								ota_side_of_pier == Pulsar2Commands::EastOfPier ? ISS_ON : ISS_OFF;
							PierSideS[(int)Pulsar2Commands::WestOfPier].s = 
								ota_side_of_pier == Pulsar2Commands::WestOfPier ? ISS_ON : ISS_OFF;
							IDSetSwitch(&PierSideSP, nullptr);
							Pulsar2Commands::currentOTASideOfPier = ota_side_of_pier; // not thread-safe
						}
					}
					else
					{
						PierSideSP.s = IPS_ALERT;
						IDSetSwitch(&PierSideSP, "Could not read OTA side of pier from controller");
						if (LX200Pulsar2::verboseLogging) LOG_INFO("Could not read OTA side of pier from controller");
					}
				} // side of pier check
			} // init complete
        } // not a simulation
    }

    return success;
}

void LX200Pulsar2::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;
    // just pass this to the parent -- it will eventually call the grandparent,
    // which will (nearly) first thing call initProperties()
	LX200Generic::ISGetProperties(dev);
}

// this function is called only once by DefaultDevice::ISGetProperties()
bool LX200Pulsar2::initProperties()
{
    const bool result = LX200Generic::initProperties();
    if (result) // pretty much always true
    {
		IUFillSwitch(&TrackingRateIndS[0], "RATE_SIDEREAL", "Sidereal", ISS_ON);
		IUFillSwitch(&TrackingRateIndS[1], "RATE_LUNAR", "Lunar", ISS_OFF);
		IUFillSwitch(&TrackingRateIndS[2], "RATE_SOLAR", "Solar", ISS_OFF);
		IUFillSwitch(&TrackingRateIndS[3], "RATE_USER1", "User1", ISS_OFF);
		IUFillSwitch(&TrackingRateIndS[4], "RATE_USER2", "User2", ISS_OFF);
		IUFillSwitch(&TrackingRateIndS[5], "RATE_USER3", "User3", ISS_OFF);
		IUFillSwitch(&TrackingRateIndS[6], "RATE_STILL", "Still", ISS_OFF);
		IUFillSwitchVector(&TrackingRateIndSP, TrackingRateIndS, LX200Pulsar2::numPulsarTrackingRates, getDeviceName(),
			   "TRACKING_RATE_IND", "Tracking  Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);


		IUFillNumber(&GuideSpeedIndN[0], "GUIDE_SPEED_IND", "0.1x Sidereal", "%.0f", 1, 9, 1, 0.0);
		IUFillNumberVector(&GuideSpeedIndNP, GuideSpeedIndN, 1, getDeviceName(), "GUIDE_SPEED_IND", "Guide Speed",
				MOTION_TAB, IP_RW, 0, IPS_IDLE);

		// Note that the following three values may be modified dynamically in getBasicData
		int nonGuideSpeedMax = Pulsar2Commands::speedsExtended ? 9999 : 999;
		int nonGuideSpeedStep = Pulsar2Commands::speedsExtended ? 100 : 10;
		const char *nonGuideSpeedLabel = Pulsar2Commands::speedsExtended ? "1/6x Sidereal" : "1x Sidereal";

		IUFillNumber(&CenterSpeedIndN[0], "CENTER_SPEED_IND", nonGuideSpeedLabel, "%.0f", 1, nonGuideSpeedMax, nonGuideSpeedStep, 0.0);
		IUFillNumberVector(&CenterSpeedIndNP, CenterSpeedIndN, 1, getDeviceName(), "CENTER_SPEED_IND", "Center Speed",
				MOTION_TAB, IP_RW, 0, IPS_IDLE);

		IUFillNumber(&FindSpeedIndN[0], "FIND_SPEED_IND", nonGuideSpeedLabel, "%.0f", 1, nonGuideSpeedMax, nonGuideSpeedStep, 0.0);
		IUFillNumberVector(&FindSpeedIndNP, FindSpeedIndN, 1, getDeviceName(), "FIND_SPEED_IND", "Find Speed",
				MOTION_TAB, IP_RW, 0, IPS_IDLE);

		IUFillNumber(&SlewSpeedIndN[0], "SLEW_SPEED_IND", nonGuideSpeedLabel, "%.0f", 1, nonGuideSpeedMax, nonGuideSpeedStep, 0.0);
		IUFillNumberVector(&SlewSpeedIndNP, SlewSpeedIndN, 1, getDeviceName(), "SLEW_SPEED_IND", "Slew Speed",
				MOTION_TAB, IP_RW, 0, IPS_IDLE);

		IUFillNumber(&GoToSpeedIndN[0], "GOTO_SPEED_IND", nonGuideSpeedLabel, "%.0f", 1, nonGuideSpeedMax, nonGuideSpeedStep, 0.0);
		IUFillNumberVector(&GoToSpeedIndNP, GoToSpeedIndN, 1, getDeviceName(), "GOTO_SPEED_IND", "GoTo Speed",
				MOTION_TAB, IP_RW, 0, IPS_IDLE);

		// ramp
		IUFillNumber(&RampN[0], "RAMP_RA", "RA Ramp", "%.0f", 1, 10, 1, 0.0);
		IUFillNumber(&RampN[1], "RAMP_DEC", "Dec Ramp", "%.0f", 1, 10, 1, 0.0);
		IUFillNumberVector(&RampNP, RampN, 2, getDeviceName(), "RAMP", "Ramp",
				LX200Pulsar2::ADVANCED_TAB, IP_RW, 0, IPS_IDLE);

		// reduction
		IUFillNumber(&ReductionN[0], "REDUCTION_RA", "RA Reduction", "%.2f", 100, 6000, 100, 0.0);
		IUFillNumber(&ReductionN[1], "REDUCTION_DEC", "Dec Reduction", "%.2f", 100, 6000, 100, 0.0);
		IUFillNumberVector(&ReductionNP, ReductionN, 2, getDeviceName(), "REDUCTION", "Reduction",
				LX200Pulsar2::ADVANCED_TAB, IP_RW, 0, IPS_IDLE);

		// maingear
		IUFillNumber(&MaingearN[0], "MAINGEAR_RA", "RA Maingear", "%.2f", 100, 6000, 100, 0.0);
		IUFillNumber(&MaingearN[1], "MAINGEAR_DEC", "Dec Maingear", "%.2f", 100, 6000, 100, 0.0);
		IUFillNumberVector(&MaingearNP, MaingearN, 2, getDeviceName(), "MAINGEAR", "Maingear",
				LX200Pulsar2::ADVANCED_TAB, IP_RW, 0, IPS_IDLE);

		// backlash
		IUFillNumber(&BacklashN[0], "BACKLASH_MIN", "Dec Backlash Minutes", "%.0f", 0, 9, 1, 0.0);
		IUFillNumber(&BacklashN[1], "BACKLASH_SEC", "Dec Backlash Seconds", "%.0f", 0, 59, 1, 0.0);
		IUFillNumberVector(&BacklashNP, BacklashN, 2, getDeviceName(), "BACKLASH", "Backlash",
				LX200Pulsar2::ADVANCED_TAB, IP_RW, 0, IPS_IDLE);

		// user rate 1
		IUFillNumber(&UserRate1N[0], "USERRATE1_RA", "RA (radians/min)", "%.7f", -4.1887902, 4.1887902, 0, 0.0);
		IUFillNumber(&UserRate1N[1], "USERRATE1_DEC", "Dec (radians/min)", "%.7f", -4.1887902, 4.1887902, 0, 0.0);
		IUFillNumberVector(&UserRate1NP, UserRate1N, 2, getDeviceName(), "USERRATE1", "UserRate1",
				LX200Pulsar2::ADVANCED_TAB, IP_RW, 0, IPS_IDLE);

		// home position
		IUFillNumber(&HomePositionN[0], "HOME_POSITION_ALT", "Altitude (0 to +90 deg.)", "%.4f", 0, 90, 0, 0.0);
		IUFillNumber(&HomePositionN[1], "HOME_POSITION_AZ", "Azimuth (0 to 360 deg.)", "%.4f", 0, 360, 0, 0.0);
		IUFillNumberVector(&HomePositionNP, HomePositionN, 2, getDeviceName(), "HOME_POSITION", "Home Pos.",
				SITE_TAB, IP_RW, 0, IPS_IDLE);		

		// mount type
		IUFillSwitch(&MountTypeS[(int)Pulsar2Commands::German], "MOUNT_TYPE_GERMAN", "German", ISS_OFF); // no default
		IUFillSwitch(&MountTypeS[(int)Pulsar2Commands::Fork], "MOUNT_TYPE_FORK", "Fork", ISS_OFF); // no default
		IUFillSwitch(&MountTypeS[(int)Pulsar2Commands::AltAz], "MOUNT_TYPE_ALTAZ", "AltAz", ISS_OFF); // no default
		IUFillSwitchVector(&MountTypeSP, MountTypeS, 3, getDeviceName(), "MOUNT_TYPE", "Mount Type",
				MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

		// pier side (indicator)
		IUFillSwitch(&PierSideS[Pulsar2Commands::EastOfPier], "PIER_EAST", "OTA on East side (-> west)", ISS_OFF); // no default
		IUFillSwitch(&PierSideS[Pulsar2Commands::WestOfPier], "PIER_WEST", "OTA on West side (-> east)", ISS_OFF); // no default
		IUFillSwitchVector(&PierSideSP, PierSideS, 2, getDeviceName(), "TELESCOPE_PIER_SIDE", "Pier Side Ind",
				MAIN_CONTROL_TAB, IP_RO, ISR_ATMOST1, 60, IPS_IDLE);
		// pier side (toggle)
		IUFillSwitch(&PierSideToggleS[0], "PIER_SIDE_TOGGLE", "Toggle OTA Pier Side (init only)", ISS_OFF);
		IUFillSwitchVector(&PierSideToggleSP, PierSideToggleS, 1, getDeviceName(), "PIER_SIDE_TOGGLE", "Pier Side Switch",
				MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

		// PEC on/off
        IUFillSwitch(&PeriodicErrorCorrectionS[0], "PEC_OFF", "Off", ISS_OFF);
        IUFillSwitch(&PeriodicErrorCorrectionS[1], "PEC_ON", "On", ISS_ON); // default
        IUFillSwitchVector(&PeriodicErrorCorrectionSP, PeriodicErrorCorrectionS, 2, getDeviceName(), "PE_CORRECTION",
                "P.E. Correction", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

		// pole crossing on/off
        IUFillSwitch(&PoleCrossingS[0], "POLE_CROSS_OFF", "Off", ISS_OFF);
        IUFillSwitch(&PoleCrossingS[1], "POLE_CROSS_ON", "On", ISS_ON); // default
        IUFillSwitchVector(&PoleCrossingSP, PoleCrossingS, 2, getDeviceName(), "POLE_CROSSING", "Pole Crossing",
                MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

		// refraction correction
        IUFillSwitch(&RefractionCorrectionS[0], "REFR_CORR_OFF", "Off", ISS_OFF);
        IUFillSwitch(&RefractionCorrectionS[1], "REFR_CORR_ON", "On", ISS_ON); // default
        IUFillSwitchVector(&RefractionCorrectionSP, RefractionCorrectionS, 2, getDeviceName(), "REFR_CORRECTION",
                "Refraction Corr.", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

		// rotation (RA)
        IUFillSwitch(&RotationRAS[0], "ROT_RA_ZERO", "CW (Right)", ISS_OFF);
        IUFillSwitch(&RotationRAS[1], "ROT_RA_ONE", "CCW (Left)", ISS_OFF);
        IUFillSwitchVector(&RotationRASP, RotationRAS, 2, getDeviceName(), "ROT_RA",
                "RA Rotation", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
		// rotation (Dec)
        IUFillSwitch(&RotationDecS[0], "ROT_DEC_ZERO", "CW", ISS_OFF);
        IUFillSwitch(&RotationDecS[1], "ROT_DEC_ONE", "CCW", ISS_OFF);
        IUFillSwitchVector(&RotationDecSP, RotationDecS, 2, getDeviceName(), "ROT_DEC",
                "Dec Rotation", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

		// tracking current
		IUFillNumber(&TrackingCurrentN[0], "TRACKING_CURRENT", "mA", "%.0f", 200, 2000, 200, 0.0); // min, max, step, value
		IUFillNumberVector(&TrackingCurrentNP, TrackingCurrentN, 1, getDeviceName(), "TRACKING_CURRENT", "Tracking Current",
				LX200Pulsar2::ADVANCED_TAB, IP_RW, 0, IPS_IDLE);
		// stop current
		IUFillNumber(&StopCurrentN[0], "STOP_CURRENT", "mA", "%.0f", 200, 2000, 200, 0.0); // min, max, step, value
		IUFillNumberVector(&StopCurrentNP, StopCurrentN, 1, getDeviceName(), "STOP_CURRENT", "Stop Current",
				LX200Pulsar2::ADVANCED_TAB, IP_RW, 0, IPS_IDLE);
		// goto current
		IUFillNumber(&GoToCurrentN[0], "GOTO_CURRENT", "mA", "%.0f", 200, 2000, 200, 0.0); // min, max, step, value
		IUFillNumberVector(&GoToCurrentNP, GoToCurrentN, 1, getDeviceName(), "GOTO_CURRENT", "GoTo Current",
				LX200Pulsar2::ADVANCED_TAB, IP_RW, 0, IPS_IDLE);
    }
    return result;
}

bool LX200Pulsar2::updateProperties()
{
    if (isConnected())
    {
		if (!this->local_properties_updated)
		{
			// note that there are several other "defines" embedded within getBasicData()
			defineProperty(&MountTypeSP);
	        defineProperty(&RotationRASP);

			defineProperty(&PierSideSP);
			defineProperty(&PierSideToggleSP);
	        defineProperty(&RotationDecSP);

	        defineProperty(&PeriodicErrorCorrectionSP);
	        defineProperty(&PoleCrossingSP);
	        defineProperty(&RefractionCorrectionSP);

	        this->local_properties_updated = true;
		}
    }
    else
    {
		deleteProperty(TrackingRateIndSP.name);
		deleteProperty(MountTypeSP.name);
		deleteProperty(PierSideSP.name);
		deleteProperty(PierSideToggleSP.name);
        deleteProperty(PeriodicErrorCorrectionSP.name);
        deleteProperty(PoleCrossingSP.name);
        deleteProperty(RefractionCorrectionSP.name);
        deleteProperty(RotationRASP.name);
        deleteProperty(RotationDecSP.name);
        deleteProperty(TrackingCurrentNP.name);
        deleteProperty(StopCurrentNP.name);
        deleteProperty(GoToCurrentNP.name);
        deleteProperty(GuideSpeedIndNP.name);
        deleteProperty(CenterSpeedIndNP.name);
        deleteProperty(FindSpeedIndNP.name);
        deleteProperty(SlewSpeedIndNP.name);
        deleteProperty(GoToSpeedIndNP.name);
        deleteProperty(RampNP.name);
        deleteProperty(ReductionNP.name);
        deleteProperty(MaingearNP.name);
        deleteProperty(BacklashNP.name);
        deleteProperty(HomePositionNP.name);
        //deleteProperty(UserRate1NP.name); // user rates are not working correctly in the controller
        local_properties_updated = false;
    }

	LX200Generic::updateProperties(); // calls great-grandparent updateProperties() (which for connections calls getBasicData())

	if (isConnected())
	{
		storeScopeLocation();
		sendScopeTime();
		// for good measure, resynchronize the tty
		PulsarTX::resyncTTY(PortFD);
		LOG_INFO("Initial tty resync complete.");
	}

	// allow polling to proceed (or not) for this instance of the driver
	this->initialization_complete = isConnected();

    return true;
}


bool LX200Pulsar2::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first, make sure that the incoming message is for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
		///////////////////////////////////
		// Guide Speed
		///////////////////////////////////
		if (!strcmp(name, GuideSpeedIndNP.name))
		{
			int ival = static_cast<int>(round(values[0]));
			if (ival > 0 && ival < 10) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setGuideSpeedInd(PortFD, ival))
					{
						GuideSpeedIndNP.s = IPS_ALERT;
						IDSetNumber(&GuideSpeedIndNP, "Unable to set guide speed indicator to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&GuideSpeedIndNP, values, names, n);
				GuideSpeedIndNP.s = IPS_OK;
				IDSetNumber(&GuideSpeedIndNP, nullptr);
			}
            else
            {
                GuideSpeedIndNP.s = IPS_ALERT;
                IDSetNumber(&GuideSpeedIndNP, "Value out of bounds for guide speed indicator");
				return false; // early exit
            }
			return true; // early exit
		}
		///////////////////////////////////
		// Center Speed
		///////////////////////////////////
		if (!strcmp(name, CenterSpeedIndNP.name))
		{
			int ival = static_cast<int>(round(values[0]));
			if (ival > 0 && ival < (Pulsar2Commands::speedsExtended ? 10000 : 1000)) // paranoid at this point
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setCenterSpeedInd(PortFD, ival))
					{
						CenterSpeedIndNP.s = IPS_ALERT;
						IDSetNumber(&CenterSpeedIndNP, "Unable to set center speed indicator to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&CenterSpeedIndNP, values, names, n);
				CenterSpeedIndNP.s = IPS_OK;
				IDSetNumber(&CenterSpeedIndNP, nullptr);
			}
            else
            {
                CenterSpeedIndNP.s = IPS_ALERT;
                IDSetNumber(&CenterSpeedIndNP, "Value out of bounds for center speed indicator");
				return false; // early exit
            }
			return true; // early exit
		}
		///////////////////////////////////
		// Find Speed
		///////////////////////////////////
		if (!strcmp(name, FindSpeedIndNP.name))
		{
			int ival = static_cast<int>(round(values[0]));
			if (ival > 0 && ival < (Pulsar2Commands::speedsExtended ? 10000 : 1000)) // paranoid at this point
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setFindSpeedInd(PortFD, ival))
					{
						FindSpeedIndNP.s = IPS_ALERT;
						IDSetNumber(&FindSpeedIndNP, "Unable to set find speed indicator to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&FindSpeedIndNP, values, names, n);
				FindSpeedIndNP.s = IPS_OK;
				IDSetNumber(&FindSpeedIndNP, nullptr);
			}
            else
            {
                FindSpeedIndNP.s = IPS_ALERT;
                IDSetNumber(&FindSpeedIndNP, "Value out of bounds for find speed indicator");
				return false; // early exit
            }
			return true; // early exit
		}
		///////////////////////////////////
		// Slew Speed
		///////////////////////////////////
		if (!strcmp(name, SlewSpeedIndNP.name))
		{
			int ival = static_cast<int>(round(values[0]));
			if (ival > 0 && ival < (Pulsar2Commands::speedsExtended ? 10000 : 1000)) // paranoid at this point
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setSlewSpeedInd(PortFD, ival))
					{
						SlewSpeedIndNP.s = IPS_ALERT;
						IDSetNumber(&SlewSpeedIndNP, "Unable to set slew speed indicator to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&SlewSpeedIndNP, values, names, n);
				SlewSpeedIndNP.s = IPS_OK;
				IDSetNumber(&SlewSpeedIndNP, nullptr);
			}
            else
            {
                SlewSpeedIndNP.s = IPS_ALERT;
                IDSetNumber(&SlewSpeedIndNP, "Value out of bounds for slew speed indicator");
				return false; // early exit
            }
			return true; // early exit
		}
		///////////////////////////////////
		// GoTo Speed
		///////////////////////////////////
		if (!strcmp(name, GoToSpeedIndNP.name))
		{
			int ival = static_cast<int>(round(values[0]));
			if (ival > 0 && ival < (Pulsar2Commands::speedsExtended ? 10000 : 1000)) // paranoid at this point
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setGoToSpeedInd(PortFD, ival))
					{
						GoToSpeedIndNP.s = IPS_ALERT;
						IDSetNumber(&GoToSpeedIndNP, "Unable to set goto speed indicator to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&GoToSpeedIndNP, values, names, n);
				GoToSpeedIndNP.s = IPS_OK;
				IDSetNumber(&GoToSpeedIndNP, nullptr);
			}
            else
            {
                GoToSpeedIndNP.s = IPS_ALERT;
                IDSetNumber(&GoToSpeedIndNP, "Value out of bounds for goto speed indicator");
				return false; // early exit
            }
			return true; // early exit
		}

		///////////////////////////////////
		// Ramp
		///////////////////////////////////
		if (!strcmp(name, RampNP.name))
		{
			int ra_ramp_val = static_cast<int>(round(values[0]));
			int dec_ramp_val = static_cast<int>(round(values[1]));
			if (ra_ramp_val >= 1 && ra_ramp_val <= 10 && dec_ramp_val >=1 && dec_ramp_val <= 10) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setRamp(PortFD, ra_ramp_val, dec_ramp_val))
					{
						RampNP.s = IPS_ALERT;
						IDSetNumber(&RampNP, "Unable to set ramp to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&RampNP, values, names, n);
				RampNP.s = IPS_OK;
				IDSetNumber(&RampNP, nullptr);
			}
            else
            {
                RampNP.s = IPS_ALERT;
                IDSetNumber(&RampNP, "Value(s) out of bounds for ramp");
				return false; // early exit
            }
			return true; // early exit
		}

		///////////////////////////////////
		// Reduction
		///////////////////////////////////
		if (!strcmp(name, ReductionNP.name))
		{
			int red_ra_val = static_cast<int>(round(values[0]));
			int red_dec_val = static_cast<int>(round(values[1]));
			if (red_ra_val >= 100 && red_ra_val <= 6000 && red_dec_val >=100 && red_dec_val <= 6000) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setReduction(PortFD, red_ra_val, red_dec_val))
					{
						ReductionNP.s = IPS_ALERT;
						IDSetNumber(&ReductionNP, "Unable to set reduction values in mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&ReductionNP, values, names, n);
				ReductionNP.s = IPS_OK;
				IDSetNumber(&ReductionNP, nullptr);
			}
            else
            {
                ReductionNP.s = IPS_ALERT;
                IDSetNumber(&ReductionNP, "Value(s) out of bounds for reduction");
				return false; // early exit
            }
			return true; // early exit
		}

		///////////////////////////////////
		// Maingear
		///////////////////////////////////
		if (!strcmp(name, MaingearNP.name))
		{
			int mg_ra_val = static_cast<int>(round(values[0]));
			int mg_dec_val = static_cast<int>(round(values[1]));
			if (mg_ra_val >= 100 && mg_ra_val <= 6000 && mg_dec_val >=100 && mg_dec_val <= 6000) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setMaingear(PortFD, mg_ra_val, mg_dec_val))
					{
						MaingearNP.s = IPS_ALERT;
						IDSetNumber(&MaingearNP, "Unable to set maingear values in mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&MaingearNP, values, names, n);
				MaingearNP.s = IPS_OK;
				IDSetNumber(&MaingearNP, nullptr);
			}
            else
            {
                MaingearNP.s = IPS_ALERT;
                IDSetNumber(&MaingearNP, "Value(s) out of bounds for maingear");
				return false; // early exit
            }
			return true; // early exit
		}

		///////////////////////////////////
		// Backlash
		///////////////////////////////////
		if (!strcmp(name, BacklashNP.name))
		{
			int bl_min_val = static_cast<int>(round(values[0]));
			int bl_sec_val = static_cast<int>(round(values[1]));
			if (bl_min_val >= 0 && bl_min_val <= 9 && bl_sec_val >=0 && bl_sec_val <= 59) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setBacklash(PortFD, bl_min_val, bl_sec_val))
					{
						BacklashNP.s = IPS_ALERT;
						IDSetNumber(&BacklashNP, "Unable to set backlash values in mount controller");
						return false; // early exit
					}
					else
					{
						// we have to re-get the values from the controller, because
						// it sets this value according to some unknown rounding algorithm
						if (Pulsar2Commands::getBacklash(PortFD, &bl_min_val, &bl_sec_val))
						{
							values[0] = bl_min_val;
							values[1] = bl_sec_val;
						}
					}
				}
				IUUpdateNumber(&BacklashNP, values, names, n);
				BacklashNP.s = IPS_OK;
				IDSetNumber(&BacklashNP, nullptr);
			}
            else
            {
                BacklashNP.s = IPS_ALERT;
                IDSetNumber(&BacklashNP, "Value(s) out of bounds for backlash");
				return false; // early exit
            }
			return true; // early exit
		}


		///////////////////////////////////
		// Home Position
		///////////////////////////////////
		if (!strcmp(name, HomePositionNP.name))
		{
			double hp_alt = values[0];
			double hp_az = values[1];
			if (hp_alt >= -90.0 && hp_alt <= 90.0 && hp_az >= 0.0 && hp_az <= 360.0) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setHomePosition(PortFD, hp_alt, hp_az))
					{
						HomePositionNP.s = IPS_ALERT;
						IDSetNumber(&HomePositionNP, "Unable to set home position values in mount controller");
						return false; // early exit
					}
					else
					{
						// we have to re-get the values from the controller, because
						// it does flaky things with floating point rounding and
						// 180/360 degree calculations
						if (Pulsar2Commands::getHomePosition(PortFD, &hp_alt, &hp_az))
						{
							values[0] = hp_alt;
							values[1] = hp_az;
						}
					}
				}
				IUUpdateNumber(&HomePositionNP, values, names, n);
				HomePositionNP.s = IPS_OK;
				IDSetNumber(&HomePositionNP, nullptr);
			}
			else
			{
                HomePositionNP.s = IPS_ALERT;
                IDSetNumber(&HomePositionNP, "Value(s) out of bounds for home position");
				return false; // early exit
			}
			return true; // early exit
		}

		///////////////////////////////////
		// User Rate 1
		///////////////////////////////////
		// note that the following has not been verified to work correctly
		if (!strcmp(name, UserRate1NP.name))
		{
			if (!Pulsar2Commands::speedsExtended) // a way to check the firmware version
			{
				double ur1_ra = values[0];
				double ur1_dec = values[1];
				if (ur1_ra >= -4.1887902 && ur1_ra <= 4.1887902 && ur1_dec >= -4.1887902 && ur1_dec <= 4.1887902) // paranoid
				{
					if (!isSimulation())
					{
						if (!Pulsar2Commands::setUserRate1(PortFD, ur1_ra, ur1_dec))
						{
							UserRate1NP.s = IPS_ALERT;
							IDSetNumber(&UserRate1NP, "Unable to set user rate 1 values in mount controller");
							return false; // early exit
						}
						else
						{
							// we have to re-get the values from the controller, because
							// it does flaky things with floating point rounding
							if (Pulsar2Commands::getUserRate1(PortFD, &ur1_ra, &ur1_dec))
							{
								values[0] = ur1_ra;
								values[1] = ur1_dec;
							}
						}
					}
					IUUpdateNumber(&UserRate1NP, values, names, n);
					UserRate1NP.s = IPS_OK;
					IDSetNumber(&UserRate1NP, nullptr);
				}
			}
			return true; // early exit
		}

		///////////////////////////////////
		// Tracking Current
		///////////////////////////////////
		if (!strcmp(name, TrackingCurrentNP.name))
		{
			int ival = static_cast<int>(round(values[0]));
			if (ival >= 200 && ival <= 2000) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setTrackingCurrent(PortFD, ival))
					{
						TrackingCurrentNP.s = IPS_ALERT;
						IDSetNumber(&TrackingCurrentNP, "Unable to set tracking current to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&TrackingCurrentNP, values, names, n);
				TrackingCurrentNP.s = IPS_OK;
				IDSetNumber(&TrackingCurrentNP, nullptr);
			}
            else
            {
                TrackingCurrentNP.s = IPS_ALERT;
                IDSetNumber(&TrackingCurrentNP, "Value out of bounds for tracking current");
				return false; // early exit
            }
			return true; // early exit
		}

		///////////////////////////////////
		// Stop Current
		///////////////////////////////////
		if (!strcmp(name, StopCurrentNP.name))
		{
			int ival = static_cast<int>(round(values[0]));
			if (ival >= 200 && ival <= 2000) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setStopCurrent(PortFD, ival))
					{
						StopCurrentNP.s = IPS_ALERT;
						IDSetNumber(&StopCurrentNP, "Unable to set stop current to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&StopCurrentNP, values, names, n);
				StopCurrentNP.s = IPS_OK;
				IDSetNumber(&StopCurrentNP, nullptr);
			}
            else
            {
                StopCurrentNP.s = IPS_ALERT;
                IDSetNumber(&StopCurrentNP, "Value out of bounds for stop current");
				return false; // early exit
            }
			return true; // early exit
		}

		///////////////////////////////////
		// GoTo Current
		///////////////////////////////////
		if (!strcmp(name, GoToCurrentNP.name))
		{
			int ival = static_cast<int>(round(values[0]));
			if (ival >= 200 && ival <= 2000) // paranoid
			{
				if (!isSimulation())
				{
					if (!Pulsar2Commands::setGoToCurrent(PortFD, ival))
					{
						GoToCurrentNP.s = IPS_ALERT;
						IDSetNumber(&GoToCurrentNP, "Unable to set goto current to mount controller");
						return false; // early exit
					}
				}
				IUUpdateNumber(&GoToCurrentNP, values, names, n);
				GoToCurrentNP.s = IPS_OK;
				IDSetNumber(&GoToCurrentNP, nullptr);
			}
            else
            {
                GoToCurrentNP.s = IPS_ALERT;
                IDSetNumber(&GoToCurrentNP, "Value out of bounds for goto current");
				return false; // early exit
            }
			return true; // early exit
		}

        ///////////////////////////////////
        // Geographic Coords
        ///////////////////////////////////
        if (strcmp(name, "GEOGRAPHIC_COORD") == 0)
        {
			if (!isSimulation())
			{
				// first two rounds are local, so are trapped here -- after that,
				// pass it on to the parent.  This ugly hack is due to the fact
				// that sendScopeLocation() (renamed in this file to storeScopeLocation)
				// is not virtual/overridable
				if (Pulsar2Commands::site_location_initialized < 2)
				{
					Pulsar2Commands::site_location_initialized++;
					return true; // early exit
				}
			}
        }

    } // check for our device

    //  If we got here, the input name has not been processed, so pass it to the parent
    return LX200Generic::ISNewNumber(dev, name, values, names, n);
}


bool LX200Pulsar2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Sites (copied from lx200telescope.cpp, due to call to sendScopeLocation() which is not virtual)
        if (!strcmp(name, SiteSP.name))
        {
            if (IUUpdateSwitch(&SiteSP, states, names, n) < 0)
                return false;

            currentSiteNum = IUFindOnSwitchIndex(&SiteSP) + 1;

            if (!isSimulation() && selectSite(PortFD, currentSiteNum) < 0)
            {
                SiteSP.s = IPS_ALERT;
                IDSetSwitch(&SiteSP, "Error selecting sites.");
                return false;
            }

            if (isSimulation())
                IUSaveText(&SiteNameTP.tp[0], "Sample Site");
            else
                getSiteName(PortFD, SiteNameTP.tp[0].text, currentSiteNum);

            if (GetTelescopeCapability() & TELESCOPE_HAS_LOCATION)
                storeScopeLocation();

            SiteNameTP.s = IPS_OK;
            SiteSP.s = IPS_OK;

            IDSetText(&SiteNameTP, nullptr);
            IDSetSwitch(&SiteSP, nullptr);

            return false;
        }
        // end Sites copy

        // mount type
        if (strcmp(name, MountTypeSP.name) == 0)
        {
			if (IUUpdateSwitch(&MountTypeSP, states, names, n) < 0)
				return false;

			if (!isSimulation())
			{
				bool success = false; // start out pessimistic
				for (int idx = 0; idx < MountTypeSP.nsp; idx++)
				{
					if (MountTypeS[idx].s == ISS_ON)
					{
						success = setMountType(PortFD, (Pulsar2Commands::MountType)(idx));
						break;
					}
				}
				if (success)
				{
					MountTypeSP.s = IPS_OK;
					IDSetSwitch(&MountTypeSP, nullptr);
				}
				else
				{
					MountTypeSP.s = IPS_ALERT;
					IDSetSwitch(&MountTypeSP, "Could not determine or change the mount type");
				}
			}
		}

		// pier side toggle -- the sync command requires that the pier side be known.
		// This is *not* related to a meridian flip, but rather, to the OTA orientation.
		if (strcmp(name, PierSideToggleSP.name) == 0)
		{
            if (IUUpdateSwitch(&PierSideToggleSP, states, names, n) < 0)
                return false;

            if (!isSimulation())
            {
				if (Pulsar2Commands::currentOTASideOfPier != Pulsar2Commands::InvalidSideOfPier) // paranoid
				{
					Pulsar2Commands::OTASideOfPier requested_side_of_pier = 
						Pulsar2Commands::currentOTASideOfPier == Pulsar2Commands::EastOfPier ? Pulsar2Commands::WestOfPier: Pulsar2Commands::EastOfPier;
					bool success = Pulsar2Commands::setSideOfPier(PortFD, requested_side_of_pier);
					// always turn it off
					PierSideToggleS[0].s = ISS_OFF;
					if (success)
					{
						PierSideToggleSP.s = IPS_OK;
						IDSetSwitch(&PierSideToggleSP, nullptr);
					}
					else
					{
						PierSideToggleSP.s = IPS_ALERT;
						IDSetSwitch(&PierSideToggleSP, "Could not change the OTA side of pier");
					}
				}
				return true; // always signal success
			}
		}
	
		// periodic error correction
        if (strcmp(name, PeriodicErrorCorrectionSP.name) == 0)
        {
            if (IUUpdateSwitch(&PeriodicErrorCorrectionSP, states, names, n) < 0)
                return false;

            if (!isSimulation())
            {
                // Only control PEC in RA; PEC in Declination doesn't seem useful
                const bool success = Pulsar2Commands::setPECorrection(PortFD,
                                     (PeriodicErrorCorrectionS[1].s == ISS_ON ?
                                      Pulsar2Commands::PECorrectionOn :
                                      Pulsar2Commands::PECorrectionOff),
                                     Pulsar2Commands::PECorrectionOff);
                if (success)
                {
                    PeriodicErrorCorrectionSP.s = IPS_OK;
                    IDSetSwitch(&PeriodicErrorCorrectionSP, nullptr);
                }
                else
                {
                    PeriodicErrorCorrectionSP.s = IPS_ALERT;
                    IDSetSwitch(&PeriodicErrorCorrectionSP, "Could not change the periodic error correction");
                }
                return success;
            }
        }

		// pole crossing
        if (strcmp(name, PoleCrossingSP.name) == 0)
        {
            if (IUUpdateSwitch(&PoleCrossingSP, states, names, n) < 0)
                return false;

            if (!isSimulation())
            {
                const bool success = Pulsar2Commands::setPoleCrossing(PortFD, (PoleCrossingS[1].s == ISS_ON ?
                                     Pulsar2Commands::PoleCrossingOn :
                                     Pulsar2Commands::PoleCrossingOff));
                if (success)
                {
                    PoleCrossingSP.s = IPS_OK;
                    IDSetSwitch(&PoleCrossingSP, nullptr);
                }
                else
                {
                    PoleCrossingSP.s = IPS_ALERT;
                    IDSetSwitch(&PoleCrossingSP, "Could not change the pole crossing");
                }
                return success;
            }
        }

		// refraction correction
        if (strcmp(name, RefractionCorrectionSP.name) == 0)
        {
            if (IUUpdateSwitch(&RefractionCorrectionSP, states, names, n) < 0)
                return false;

            if (!isSimulation())
            {
                // Control refraction correction in both RA and decl.
                const Pulsar2Commands::RCorrection rc =
                    (RefractionCorrectionS[1].s == ISS_ON ? Pulsar2Commands::RCorrectionOn :
                     Pulsar2Commands::RCorrectionOff);
                const bool success = Pulsar2Commands::setRCorrection(PortFD, rc, rc);
                if (success)
                {
                    RefractionCorrectionSP.s = IPS_OK;
                    IDSetSwitch(&RefractionCorrectionSP, nullptr);
                }
                else
                {
                    RefractionCorrectionSP.s = IPS_ALERT;
                    IDSetSwitch(&RefractionCorrectionSP, "Could not change the refraction correction");
                }
                return success;
            }
        }

		// rotation RA
		if (strcmp(name, RotationRASP.name) == 0)
		{
            if (IUUpdateSwitch(&RotationRASP, states, names, n) < 0)
                return false;

            if (!isSimulation())
            {
                // Control rotation of RA
                Pulsar2Commands::Rotation rot_ra, rot_dec;
                bool success = Pulsar2Commands::getRotation(PortFD, &rot_ra, &rot_dec);
                if (success)
                {
					rot_ra = (RotationRAS[0].s == ISS_ON ? Pulsar2Commands::RotationZero : Pulsar2Commands::RotationOne);
					success = Pulsar2Commands::setRotation(PortFD, rot_ra, rot_dec);
					if (success)
					{
						RotationRASP.s = IPS_OK;
						IDSetSwitch(&RotationRASP, nullptr);
					}
					else
					{
						RotationRASP.s = IPS_ALERT;
						IDSetSwitch(&RotationRASP, "Could not change RA rotation direction");
					}
				}
                return success;
            }		
		}

		// rotation Dec
		if (strcmp(name, RotationDecSP.name) == 0)
		{
            if (IUUpdateSwitch(&RotationDecSP, states, names, n) < 0)
                return false;

            if (!isSimulation())
            {
                // Control rotation of Dec
                Pulsar2Commands::Rotation rot_ra, rot_dec;
                bool success = Pulsar2Commands::getRotation(PortFD, &rot_ra, &rot_dec);
                if (success)
                {
					rot_dec = (RotationDecS[0].s == ISS_ON ? Pulsar2Commands::RotationZero : Pulsar2Commands::RotationOne);
					success = Pulsar2Commands::setRotation(PortFD, rot_ra, rot_dec);
					if (success)
					{
						RotationDecSP.s = IPS_OK;
						IDSetSwitch(&RotationDecSP, nullptr);
					}
					else
					{
						RotationDecSP.s = IPS_ALERT;
						IDSetSwitch(&RotationDecSP, "Could not change Dec rotation direction");
					}
				}
                return success;
            }	
		}


		// tracking rate indicator
		if (strcmp(name, TrackingRateIndSP.name) == 0)
		{
			if (IUUpdateSwitch(&TrackingRateIndSP, states, names, n) < 0)
				return false;

			if (!isSimulation())
			{
				int idx = 0;
				for (; idx < static_cast<int>(LX200Pulsar2::numPulsarTrackingRates); idx++)
				{
					if (TrackingRateIndS[idx].s == ISS_ON) break;
				}

				bool success = Pulsar2Commands::setTrackingRateInd(PortFD, static_cast<Pulsar2Commands::TrackingRateInd>(idx));
				if (success)
				{
                    TrackingRateIndSP.s = IPS_OK;
                    IDSetSwitch(&TrackingRateIndSP, nullptr);
				}
				else
				{
                    TrackingRateIndSP.s = IPS_ALERT;
                    IDSetSwitch(&TrackingRateIndSP, "Could not change the tracking rate");
				}
				return success;
			}
		}

    } // dev is ok

    //  Nobody has claimed this, so pass it to the parent
    return LX200Generic::ISNewSwitch(dev, name, states, names, n);
}

bool LX200Pulsar2::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Nothing to do yet
    }
    return LX200Generic::ISNewText(dev, name, texts, names, n);
}

bool LX200Pulsar2::SetSlewRate(int index)
{
    // Convert index from Meade format
    index = 3 - index;
    const bool success =
        (isSimulation() || Pulsar2Commands::setSlewMode(PortFD, static_cast<Pulsar2Commands::SlewMode>(index)));
    if (success)
    {
        SlewRateSP.s = IPS_OK;
        IDSetSwitch(&SlewRateSP, nullptr);
    }
    else
    {
        SlewRateSP.s = IPS_ALERT;
        IDSetSwitch(&SlewRateSP, "Error setting slew rate");
    }
    return success;
}

bool LX200Pulsar2::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand motionCommand)
{
    Pulsar2Commands::Direction motionDirection;
    switch (dir) // map INDI directions to Pulsar2 directions
    {
		case DIRECTION_NORTH:
			motionDirection = Pulsar2Commands::North;
			break;
		case DIRECTION_SOUTH:
			motionDirection = Pulsar2Commands::South;
			break;
		default:
			LOG_INFO("Attempt to move neither North nor South using MoveNS()");
			return false;
	}

    bool success = true;
    switch (motionCommand)
    {
        case MOTION_START:
			last_ns_motion = dir; // globals such as this are not advisable
            success = (isSimulation() || Pulsar2Commands::moveTo(PortFD, motionDirection));
            if (success)
                LOGF_INFO("Moving toward %s.", Pulsar2Commands::DirectionName[motionDirection]);
            else
                LOG_ERROR("Error starting N/S motion.");
            break;
        case MOTION_STOP:
            success = (isSimulation() || Pulsar2Commands::haltMovement(PortFD, motionDirection));
            if (success)
                LOGF_INFO("Movement toward %s halted.",
                          Pulsar2Commands::DirectionName[motionDirection]);
            else
                LOG_ERROR("Error stopping N/S motion.");
            break;
    }
    return success;
}

bool LX200Pulsar2::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
   Pulsar2Commands::Direction motionDirection;
    switch (dir) // map INDI directions to Pulsar2 directions
    {
		case DIRECTION_WEST:
			motionDirection = Pulsar2Commands::West;
			break;
		case DIRECTION_EAST:
			motionDirection = Pulsar2Commands::East;
			break;
		default:
			LOG_INFO("Attempt to move neither West nor East using MoveWE()");
			return false;
	}

    bool success = true;
    switch (command)
    {
        case MOTION_START:
			last_we_motion = dir; // globals such as this are not advisable
            success = (isSimulation() || Pulsar2Commands::moveTo(PortFD, motionDirection));
            if (success)
                LOGF_INFO("Moving toward %s.", Pulsar2Commands::DirectionName[motionDirection]);
            else
                LOG_ERROR("Error starting W/E motion.");
            break;
        case MOTION_STOP:
            success = (isSimulation() || Pulsar2Commands::haltMovement(PortFD, motionDirection));
            if (success)
                LOGF_INFO("Movement toward %s halted.",
                          Pulsar2Commands::DirectionName[motionDirection]);
            else
                LOG_ERROR("Error stopping W/E motion.");
            break;
    }
    return success;
}

bool LX200Pulsar2::Abort()
{
    const bool success = (isSimulation() || Pulsar2Commands::abortSlew(PortFD));
    if (success)
    {
        if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
        {
            GuideNSNP.s = GuideWENP.s = IPS_IDLE;
            GuideNSN[0].value = GuideNSN[1].value = 0.0;
            GuideWEN[0].value = GuideWEN[1].value = 0.0;
            if (GuideNSTID)
            {
                IERmTimer(GuideNSTID);
                GuideNSTID = 0;
            }
            if (GuideWETID)
            {
                IERmTimer(GuideWETID);
                GuideNSTID = 0;
            }
            IDMessage(getDeviceName(), "Guide aborted.");
            IDSetNumber(&GuideNSNP, nullptr);
            IDSetNumber(&GuideWENP, nullptr);
        }
    }
    else
        LOG_ERROR("Failed to abort slew!");
    return success;
}


IPState LX200Pulsar2::GuideNorth(uint32_t ms)
{
    if (!usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }
    // If already moving (no pulse command), then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        const int dir = IUFindOnSwitchIndex(&MovementNSSP);
        MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
    }
    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }
    if (usePulseCommand)
    {
        (void)Pulsar2Commands::pulseGuide(PortFD, Pulsar2Commands::North, ms);
	}
    else
    {
        if (!Pulsar2Commands::setSlewMode(PortFD, Pulsar2Commands::SlewGuide))
        {
            SlewRateSP.s = IPS_ALERT;
            IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
            return IPS_ALERT;
        }
        MovementNSS[0].s = ISS_ON;
        MoveNS(DIRECTION_NORTH, MOTION_START);
    }

    // Set switched slew rate to "guide"
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction_ns = LX200_NORTH;
    GuideNSTID      = IEAddTimer(ms, guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState LX200Pulsar2::GuideSouth(uint32_t ms)
{
    if (!usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }
    // If already moving (no pulse command), then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        const int dir = IUFindOnSwitchIndex(&MovementNSSP);
        MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
    }
    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }
    if (usePulseCommand)
        (void)Pulsar2Commands::pulseGuide(PortFD, Pulsar2Commands::South, ms);
    else
    {
        if (!Pulsar2Commands::setSlewMode(PortFD, Pulsar2Commands::SlewGuide))
        {
            SlewRateSP.s = IPS_ALERT;
            IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
            return IPS_ALERT;
        }
        MovementNSS[1].s = ISS_ON;
        MoveNS(DIRECTION_SOUTH, MOTION_START);
    }

    // Set switch slew rate to "guide"
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction_ns = LX200_SOUTH;
    GuideNSTID      = IEAddTimer(ms, guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState LX200Pulsar2::GuideEast(uint32_t ms)
{
    if (!usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }
    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        const int dir = IUFindOnSwitchIndex(&MovementWESP);
        MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
    }
    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }
    if (usePulseCommand)
        (void)Pulsar2Commands::pulseGuide(PortFD, Pulsar2Commands::East, ms);
    else
    {
        if (!Pulsar2Commands::setSlewMode(PortFD, Pulsar2Commands::SlewGuide))
        {
            SlewRateSP.s = IPS_ALERT;
            IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
            return IPS_ALERT;
        }
        MovementWES[1].s = ISS_ON;
        MoveWE(DIRECTION_EAST, MOTION_START);
    }

    // Set switched slew rate to "guide"
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction_we = LX200_EAST;
    GuideWETID      = IEAddTimer(ms, guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

IPState LX200Pulsar2::GuideWest(uint32_t ms)
{
    if (!usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }
    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        const int dir = IUFindOnSwitchIndex(&MovementWESP);
        MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
    }
    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }
    if (usePulseCommand)
        (void)Pulsar2Commands::pulseGuide(PortFD, Pulsar2Commands::West, ms);
    else
    {
        if (!Pulsar2Commands::setSlewMode(PortFD, Pulsar2Commands::SlewGuide))
        {
            SlewRateSP.s = IPS_ALERT;
            IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
            return IPS_ALERT;
        }
        MovementWES[0].s = ISS_ON;
        MoveWE(DIRECTION_WEST, MOTION_START);
    }
    // Set switched slew to "guide"
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction_we = LX200_WEST;
    GuideWETID      = IEAddTimer(ms, guideTimeoutHelperWE, this);
    return IPS_BUSY;
}


bool LX200Pulsar2::updateTime(ln_date *utc, double utc_offset)
{
    INDI_UNUSED(utc_offset);
    bool success = true;
    if (!isSimulation())
    {
        struct ln_zonedate ltm;
        ln_date_to_zonedate(utc, &ltm, 0.0); // One should use only UTC with Pulsar!
        JD = ln_get_julian_day(utc);
        LOGF_DEBUG("New JD is %f", static_cast<float>(JD));
        success = Pulsar2Commands::setTime(PortFD, ltm.hours, ltm.minutes, ltm.seconds);
        if (success)
        {
            success = Pulsar2Commands::setDate(PortFD, ltm.days, ltm.months, ltm.years);
            if (success)
                LOG_INFO("UTC date-time is set.");
            else
                LOG_ERROR("Error setting UTC date/time.");
        }
        else
            LOG_ERROR("Error setting UTC time.");
        // Pulsar cannot set UTC offset (?)
    }

    return success;
}

bool LX200Pulsar2::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    bool success = true;
    if (!isSimulation())
    {
        success = Pulsar2Commands::setSite(PortFD, longitude, latitude);
        if (success)
        {
            char l[32], L[32];
            fs_sexa(l, latitude, 3, 3600);
            fs_sexa(L, longitude, 4, 3600);
            IDMessage(getDeviceName(), "Site coordinates updated to Lat %.32s - Long %.32s", l, L);
            LOGF_INFO("Site coordinates updated to lat: %+f, lon: %+f", latitude, longitude);
        }
        else
            LOG_ERROR("Error setting site coordinates");
    }
    return success;
}


bool LX200Pulsar2::Goto(double r, double d)
{
    const struct timespec timeout = {0, 100000000L}; // 1/10 second
    char RAStr[64], DecStr[64];
    fs_sexa(RAStr, targetRA = r, 2, 3600);
    fs_sexa(DecStr, targetDEC = d, 2, 3600);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
        if (!isSimulation() && !Pulsar2Commands::abortSlew(PortFD))
        {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
        }

        AbortSP.s = IPS_OK;
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&AbortSP, "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = IPS_IDLE;
            MovementWESP.s = IPS_IDLE;
            EqNP.s = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation())
    {
        if (!Pulsar2Commands::setObjectRADec(PortFD, targetRA, targetDEC))
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error setting RA/DEC.");
            return false;
        }
        if (!Pulsar2Commands::startSlew(PortFD))
        {
            EqNP.s = IPS_ALERT;
            IDSetNumber(&EqNP, "Error Slewing to JNow RA %s - DEC %s\n", RAStr, DecStr);
            slewError(3);
            return false;
        }
        just_started_slewing = true;
    }

    TrackState = SCOPE_SLEWING;
    //EqNP.s     = IPS_BUSY;
    LOGF_INFO("Slewing to RA: %s - DEC: %s", RAStr, DecStr);
    return true;
}

bool LX200Pulsar2::Park()
{
    const struct timespec timeout = {0, 100000000L}; // 1/10th second

    if (!isSimulation())
    {
        if (!Pulsar2Commands::isHomeSet(PortFD))
        {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "No parking position defined.");
            return false;
        }
        if (Pulsar2Commands::isParked(PortFD))
        {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "Scope has already been parked.");
            return false;
        }
    }

    // If scope is moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
        if (!isSimulation() && !Pulsar2Commands::abortSlew(PortFD))
        {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
        }

        AbortSP.s = IPS_OK;
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&AbortSP, "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = IPS_IDLE;
            MovementWESP.s = IPS_IDLE;
            EqNP.s = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);

            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }
        nanosleep(&timeout, nullptr);
    }

    if (!isSimulation() && !Pulsar2Commands::park(PortFD))
    {
        ParkSP.s = IPS_ALERT;
        IDSetSwitch(&ParkSP, "Parking Failed.");
        return false;
    }

    ParkSP.s   = IPS_BUSY;
    TrackState = SCOPE_PARKING;
    IDMessage(getDeviceName(), "Parking telescope in progress...");
    return true;
}


bool LX200Pulsar2::Sync(double ra, double dec)
{
    const struct timespec timeout = {0, 300000000L}; // 3/10 seconds
    bool success = true;
    if (!isSimulation())
    {
		if (!isSlewing())
		{
	        success = Pulsar2Commands::setObjectRADec(PortFD, ra, dec);
	        nanosleep(&timeout, nullptr); // This seems to be necessary (why?)
	        if (!success)
	        {
	            EqNP.s = IPS_ALERT;
	            IDSetNumber(&EqNP, "Error setting RA/DEC. Unable to Sync.");
	        }
	        else
	        {
	            char RAresponse[32]; memset(RAresponse, '\0', 32); // currently just for debug
	            char DECresponse[32]; memset(DECresponse, '\0', 32); // currently just for debug
	            success = PulsarTX::sendReceive2(PortFD, "#:CM#", RAresponse, DECresponse);
				if (success)
				{
	                // Pulsar returns coordinates separated/terminated by # characters (<RA>#<Dec>#).
	                // Currently, we don't check that the received coordinates match the sent coordinates.
	                LOGF_DEBUG("Sync RAresponse: %s, DECresponse: %s", RAresponse, DECresponse);
					currentRA  = ra;
					currentDEC = dec;
					EqNP.s     = IPS_OK;
					NewRaDec(currentRA, currentDEC);
					LOG_INFO("Synchronization successful.");
				}
				else
				{
					EqNP.s = IPS_ALERT;
					IDSetNumber(&EqNP, "Synchronization failed.");
					LOG_INFO("Synchronization failed.");
				}
	        }
		}
		else
		{
			success = false;
			LOG_INFO("Cannot sync while slewing");
		}
    }

    return success;
}


bool LX200Pulsar2::UnPark()
{
    if (!isSimulation())
    {
        if (!Pulsar2Commands::isParked(PortFD))
        {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "Mount is not parked.");
			LOG_INFO("Mount is not parked, so cannot unpark.");
            return false; // early exit
        }
        if (!Pulsar2Commands::unpark(PortFD))
        {
            ParkSP.s = IPS_ALERT;
            IDSetSwitch(&ParkSP, "Unparking failed.");
			LOG_INFO("Unparking failed.");
            return false; // early exit
        }
    }
    ParkSP.s   = IPS_OK;
    TrackState = SCOPE_IDLE;
    SetParked(false);
    IDMessage(getDeviceName(), "Telescope has been unparked.");

	// the following assumes we are tracking, since there is no truly
	// "idle" state for Pulsar2
    LOG_INFO("Telescope has been unparked.");
    TrackState = SCOPE_TRACKING;
    IDSetSwitch(&ParkSP, nullptr);

    return true;
}

bool LX200Pulsar2::isSlewComplete()
{
    bool result = false;
    switch (TrackState)
    {
        case SCOPE_SLEWING:
            result = !isSlewing();
            break;
        case SCOPE_PARKING:
            result = !Pulsar2Commands::isParking(PortFD);
            break;
        default:
            break;
    }
    return result;
}

bool LX200Pulsar2::checkConnection()
{
    if (isSimulation())
        return true; // early exit
        
    return LX200Generic::checkConnection(); // a reduced form of resynchronize()
}


// Note that several "definitions" are also included in the following
// functions, so we can dynamically modify some input fields
void LX200Pulsar2::getBasicData()
{
	if (!isConnected()) return; // early exit
	
    if (!isSimulation())
    {
		// first do the parent's data gathering
		LX200Telescope::getBasicData();

		// ensure long format
        if (!Pulsar2Commands::ensureLongFormat(PortFD))
        {
            LOG_DEBUG("Failed to ensure that long format coordinates are used.");
        }        

        // Determine which Pulsar firmware version we are connected to.
        // We expect a response something like: 'PULSAR V2.66aR  ,2008.12.10.     #'
		const struct timespec getVersionSleepTime = {0, 50000000L}; // 1/20th second
        char versionResponse[40]; memset(versionResponse, '\0', 40);
        if (Pulsar2Commands::getVersion(PortFD, versionResponse))
        {
			char *vp = strstr(versionResponse, "PULSAR V");
			if (*vp != LX200Pulsar2::Null)
			{
				char versionString[20]; memset(versionString, '\0', 20);
				vp += 8;
				char *vs = versionString;
				for (int i=0; i < 19 && (isalnum(*vp) || *vp == '.'); *(vs++) = *(vp++), i++);
				if (strcmp(versionString, "5.7") < 0) Pulsar2Commands::speedsExtended = true;
				LOGF_INFO("Pulsar firmware Version: %s", versionString);
				// The following commented out, as it may not always work
				//(void)sscanf(response, "PULSAR V%8s ,%4d.%2d.%2d. ", versionString, &versionYear, &versionMonth, &versionDay);
				//LOGF_INFO("%s version %s dated %04d.%02d.%02d",
				//          (Pulsar2Commands::versionString[0] > '2' ? "Pulsar2" : "Pulsar"), Pulsar2Commands::versionString, Pulsar2Commands::versionYear, Pulsar2Commands::versionMonth, Pulsar2Commands::versionDay);
			}
			else
			{
				LOG_INFO("Could not determine valid firmware version.");
			}
        }
        nanosleep(&getVersionSleepTime, nullptr);

		int nonGuideSpeedMax = Pulsar2Commands::speedsExtended ? 9999 : 999;
		int nonGuideSpeedStep = Pulsar2Commands::speedsExtended ? 100 : 10;
		const char *nonGuideSpeedLabel = Pulsar2Commands::speedsExtended ? Pulsar2Commands::nonGuideSpeedExtendedUnit : Pulsar2Commands::nonGuideSpeedUnit;

		// mount type
		Pulsar2Commands::MountType mount_type = Pulsar2Commands::getMountType(PortFD);
		MountTypeS[(int)mount_type].s = ISS_ON;
		IDSetSwitch(&MountTypeSP, nullptr);

        // PE correction (one value used for both RA and Dec)
        Pulsar2Commands::PECorrection pec_ra  = Pulsar2Commands::PECorrectionOff,
                                      pec_dec = Pulsar2Commands::PECorrectionOff;
        if (Pulsar2Commands::getPECorrection(PortFD, &pec_ra, &pec_dec))
        {
			PeriodicErrorCorrectionS[0].s = (pec_ra == Pulsar2Commands::PECorrectionOn ? ISS_OFF : ISS_ON);
			PeriodicErrorCorrectionS[1].s = (pec_ra == Pulsar2Commands::PECorrectionOn ? ISS_ON : ISS_OFF);
			IDSetSwitch(&PeriodicErrorCorrectionSP, nullptr);
        }
        else
        {
			PeriodicErrorCorrectionSP.s = IPS_ALERT;
			IDSetSwitch(&PeriodicErrorCorrectionSP, "Can't check whether PEC is enabled.");
        }

		// pole crossing
        Pulsar2Commands::PoleCrossing pole_crossing = Pulsar2Commands::PoleCrossingOff;
        if (Pulsar2Commands::getPoleCrossing(PortFD, &pole_crossing))
        {
			PoleCrossingS[0].s = (pole_crossing == Pulsar2Commands::PoleCrossingOn ? ISS_OFF : ISS_ON);
			PoleCrossingS[1].s = (pole_crossing == Pulsar2Commands::PoleCrossingOn ? ISS_ON : ISS_OFF);
            IDSetSwitch(&PoleCrossingSP, nullptr);
        }
        else
        {
            PoleCrossingSP.s = IPS_ALERT;
            IDSetSwitch(&PoleCrossingSP, "Can't check whether pole crossing is enabled.");
        }

        // refraction correction (one value used for both RA and Dec)
        Pulsar2Commands::RCorrection rc_ra = Pulsar2Commands::RCorrectionOff, rc_dec = Pulsar2Commands::RCorrectionOn;
        if (Pulsar2Commands::getRCorrection(PortFD, &rc_ra, &rc_dec))
        {
			RefractionCorrectionS[0].s = (rc_ra == Pulsar2Commands::RCorrectionOn ? ISS_OFF : ISS_ON);
			RefractionCorrectionS[1].s = (rc_ra == Pulsar2Commands::RCorrectionOn ? ISS_ON : ISS_OFF);
            IDSetSwitch(&RefractionCorrectionSP, nullptr);
        }
        else
        {
            RefractionCorrectionSP.s = IPS_ALERT;
            IDSetSwitch(&RefractionCorrectionSP, "Can't check whether refraction correction is enabled.");
        }

		// rotation
		Pulsar2Commands::Rotation rot_ra, rot_dec;
		if (Pulsar2Commands::getRotation(PortFD, &rot_ra, &rot_dec))
		{
			RotationRAS[0].s = (rot_ra == Pulsar2Commands::RotationZero ? ISS_ON : ISS_OFF);
			RotationRAS[1].s = (rot_ra == Pulsar2Commands::RotationOne ? ISS_ON : ISS_OFF);
            IDSetSwitch(&RotationRASP, nullptr);
			RotationDecS[0].s = (rot_dec == Pulsar2Commands::RotationZero ? ISS_ON : ISS_OFF);
			RotationDecS[1].s = (rot_dec == Pulsar2Commands::RotationOne ? ISS_ON : ISS_OFF);
            IDSetSwitch(&RotationDecSP, nullptr);
		}


		// - - - - - - - - - - - - - - - - - -
		// Motion Control Tab
		// - - - - - - - - - - - - - - - - - -

		// tracking rate indicator
		Pulsar2Commands::TrackingRateInd tracking_rate_ind = Pulsar2Commands::getTrackingRateInd(PortFD);
		for (int i = 0; i < static_cast<int>(LX200Pulsar2::numPulsarTrackingRates); i++) TrackingRateIndS[i].s = ISS_OFF;
		if (tracking_rate_ind != Pulsar2Commands::RateNone)
		{
			TrackingRateIndS[static_cast<int>(tracking_rate_ind)].s = ISS_ON;
			IDSetSwitch(&TrackingRateIndSP, nullptr);
		}
		else
		{
			TrackingRateIndSP.s = IPS_ALERT;
			IDSetSwitch(&TrackingRateIndSP, "Can't get the tracking rate indicator.");
		}
		defineProperty(&TrackingRateIndSP); // defined here for consistency

		// guide speed indicator
		int guide_speed_ind = Pulsar2Commands::getGuideSpeedInd(PortFD);
		if (guide_speed_ind > 0)
		{
			double guide_speed_ind_d = static_cast<double>(guide_speed_ind);
			GuideSpeedIndN[0].value = guide_speed_ind_d;
			IDSetNumber(&GuideSpeedIndNP, nullptr);
		}
        defineProperty(&GuideSpeedIndNP); // defined here, in order to match input value with controller value

		// center speed indicator
		int center_speed_ind = Pulsar2Commands::getCenterSpeedInd(PortFD);
		if (center_speed_ind > 0)
		{
			double center_speed_ind_d = static_cast<double>(center_speed_ind);
			CenterSpeedIndN[0].value = center_speed_ind_d;
			CenterSpeedIndN[0].max = nonGuideSpeedMax;
			CenterSpeedIndN[0].step = nonGuideSpeedStep;
			strcpy(CenterSpeedIndN[0].label, nonGuideSpeedLabel);
			IDSetNumber(&CenterSpeedIndNP, nullptr);
		}
        defineProperty(&CenterSpeedIndNP); // defined here, in order to match input value with controller value

		// find speed indicator
		int find_speed_ind = Pulsar2Commands::getFindSpeedInd(PortFD);
		if (find_speed_ind > 0)
		{
			double find_speed_ind_d = static_cast<double>(find_speed_ind);
			FindSpeedIndN[0].value = find_speed_ind_d;
			FindSpeedIndN[0].max = nonGuideSpeedMax;
			FindSpeedIndN[0].step = nonGuideSpeedStep;
			strcpy(FindSpeedIndN[0].label, nonGuideSpeedLabel);
			IDSetNumber(&FindSpeedIndNP, nullptr);
		}
        defineProperty(&FindSpeedIndNP); // defined here, in order to match input value with controller value

		// slew speed indicator
		int slew_speed_ind = Pulsar2Commands::getSlewSpeedInd(PortFD);
		if (slew_speed_ind > 0)
		{
			double slew_speed_ind_d = static_cast<double>(slew_speed_ind);
			SlewSpeedIndN[0].value = slew_speed_ind_d;
			SlewSpeedIndN[0].max = nonGuideSpeedMax;
			SlewSpeedIndN[0].step = nonGuideSpeedStep;
			strcpy(SlewSpeedIndN[0].label, nonGuideSpeedLabel);
			IDSetNumber(&SlewSpeedIndNP, nullptr);
		}
        defineProperty(&SlewSpeedIndNP); // defined here, in order to match input value with controller value

		// goto speed indicator
		int goto_speed_ind = Pulsar2Commands::getGoToSpeedInd(PortFD);
		if (goto_speed_ind > 0)
		{
			double goto_speed_ind_d = static_cast<double>(goto_speed_ind);
			GoToSpeedIndN[0].value = goto_speed_ind_d;
			GoToSpeedIndN[0].max = nonGuideSpeedMax;
			GoToSpeedIndN[0].step = nonGuideSpeedStep;
			strcpy(GoToSpeedIndN[0].label, nonGuideSpeedLabel);
			IDSetNumber(&GoToSpeedIndNP, nullptr);
		}
        defineProperty(&GoToSpeedIndNP); // defined here, in order to match input value with controller value


		// - - - - - - - - - - - - - - - - - -
		// Site Management Tab
		// - - - - - - - - - - - - - - - - - -

		// home position
		double hp_alt, hp_az;
		if (Pulsar2Commands::getHomePosition(PortFD, &hp_alt, &hp_az))
		{
			HomePositionN[0].value = hp_alt;
			HomePositionN[1].value = hp_az;
			IDSetNumber(&HomePositionNP, nullptr);
		}
		else
		{
			HomePositionNP.s = IPS_ALERT;
			IDSetNumber(&HomePositionNP, "Unable to get home position values from controller.");
		}
        defineProperty(&HomePositionNP); // defined here, in order to match input value with controller value


		// - - - - - - - - - - - - - - - - - -
		// Advanced Setup Tab
		// - - - - - - - - - - - - - - - - - -

		// tracking current
		int tracking_current = Pulsar2Commands::getTrackingCurrent(PortFD);
		if (tracking_current > 0)
		{
			double tracking_current_d = static_cast<double>(tracking_current);
			TrackingCurrentN[0].value = tracking_current_d;
			IDSetNumber(&TrackingCurrentNP, nullptr);
		}
		else
		{
            TrackingCurrentNP.s = IPS_ALERT;
            IDSetNumber(&TrackingCurrentNP, "Can't get tracking current value");
		}
        defineProperty(&TrackingCurrentNP); // defined here, in order to match input value with controller value

		// stop current
		int stop_current = Pulsar2Commands::getStopCurrent(PortFD);
		if (stop_current > 0)
		{
			double stop_current_d = static_cast<double>(stop_current);
			StopCurrentN[0].value = stop_current_d;
			IDSetNumber(&StopCurrentNP, nullptr);
		}
		else
		{
            StopCurrentNP.s = IPS_ALERT;
            IDSetNumber(&StopCurrentNP, "Can't get stop current value");
		}
		defineProperty(&StopCurrentNP); // defined here, in order to match input value with controller value

		// goto current
		int goto_current = Pulsar2Commands::getGoToCurrent(PortFD);
		if (goto_current > 0)
		{
			double goto_current_d = static_cast<double>(goto_current);
			GoToCurrentN[0].value = goto_current_d;
			IDSetNumber(&GoToCurrentNP, nullptr);
		}
		else
		{
            GoToCurrentNP.s = IPS_ALERT;
            IDSetNumber(&GoToCurrentNP, "Can't get goto current value");
		}
		defineProperty(&GoToCurrentNP); // defined here, in order to match input value with controller value

		// ramp
		int ra_ramp, dec_ramp;
		if (Pulsar2Commands::getRamp(PortFD, &ra_ramp, &dec_ramp))
		{
			double ra_ramp_d = static_cast<double>(ra_ramp);
			double dec_ramp_d = static_cast<double>(dec_ramp);
			RampN[0].value = ra_ramp_d;
			RampN[1].value = dec_ramp_d;
			IDSetNumber(&RampNP, nullptr);
		}
		else
		{
			RampNP.s = IPS_ALERT;
			IDSetNumber(&RampNP, "Unable to get ramp values from controller.");
		}
        defineProperty(&RampNP); // defined here, in order to match input value with controller value

		// reduction
		int red_ra, red_dec;
		if (Pulsar2Commands::getReduction(PortFD, &red_ra, &red_dec))
		{
			double ra_red_d = static_cast<double>(red_ra);
			double dec_red_d = static_cast<double>(red_dec);
			ReductionN[0].value = ra_red_d;
			ReductionN[1].value = dec_red_d;
			IDSetNumber(&ReductionNP, nullptr);
		}
		else
		{
			ReductionNP.s = IPS_ALERT;
			IDSetNumber(&ReductionNP, "Unable to get reduction values from controller.");
		}
        defineProperty(&ReductionNP); // defined here, in order to match input value with controller value

		// maingear
		int mg_ra, mg_dec;
		if (Pulsar2Commands::getMaingear(PortFD, &mg_ra, &mg_dec))
		{
			double mg_ra_d = static_cast<double>(mg_ra);
			double mg_dec_d = static_cast<double>(mg_dec);
			MaingearN[0].value = mg_ra_d;
			MaingearN[1].value = mg_dec_d;
			IDSetNumber(&MaingearNP, nullptr);
		}
		else
		{
			MaingearNP.s = IPS_ALERT;
			IDSetNumber(&MaingearNP, "Unable to get maingear values from controller.");
		}
        defineProperty(&MaingearNP); // defined here, in order to match input value with controller value

		// backlash
		int bl_min, bl_sec;
		if (Pulsar2Commands::getBacklash(PortFD, &bl_min, &bl_sec))
		{
			double bl_min_d = static_cast<double>(bl_min);
			double bl_sec_d = static_cast<double>(bl_sec);
			BacklashN[0].value = bl_min_d;
			BacklashN[1].value = bl_sec_d;
			IDSetNumber(&BacklashNP, nullptr);
		}
		else
		{
			BacklashNP.s = IPS_ALERT;
			IDSetNumber(&BacklashNP, "Unable to get backlash values from controller.");
		}
        defineProperty(&BacklashNP); // defined here, in order to match input value with controller value


		// user rate 1
		// note that the following has not been verified to work correctly,
		// and perhaps not at all for earlier firmware versions
		if (!Pulsar2Commands::speedsExtended) // a way to check for a firmware version
		{
			double ur1_ra, ur1_dec;
			if (Pulsar2Commands::getUserRate1(PortFD, &ur1_ra, &ur1_dec))
			{
				UserRate1N[0].value = ur1_ra;
				UserRate1N[1].value = ur1_dec;
				IDSetNumber(&UserRate1NP, nullptr);
			}
			else
			{
				UserRate1NP.s = IPS_ALERT;
				IDSetNumber(&UserRate1NP, "Unable to get user rate 1 values from controller.");
			}
	        //defineProperty(&UserRate1NP); // user rates are not working correctly in the controller
		}

    } // not a simulation

}

// -- -- -- -- -- -- -- -- -- -- -- -- -- --
// Other methods
// -- -- -- -- -- -- -- -- -- -- -- -- -- --

bool LX200Pulsar2::storeScopeLocation()
{
    LocationNP.s = IPS_OK;
    double lat = 29.5; // simulation default
    double lon = 48.0; // simulation default

    if (isSimulation() || Pulsar2Commands::getSiteLatitudeLongitude(PortFD, &lat, &lon))
    {
        LocationNP.np[0].value = lat;
		double stdLon = (lon < 0 ? 360.0 + lon : lon);
        LocationNP.np[1].value = stdLon;

		LOGF_DEBUG("Mount Controller Latitude: %g Longitude: %g", LocationN[LOCATION_LATITUDE].value,
               LocationN[LOCATION_LONGITUDE].value);

		IDSetNumber(&LocationNP, nullptr);
		saveConfig(true, "GEOGRAPHIC_COORD");
        if (LX200Pulsar2::verboseLogging) LOGF_INFO("Controller location read and stored; lat: %+f, lon: %+f", lat, stdLon);
    }
    else
    {
        LocationNP.s = IPS_ALERT;
        IDMessage(getDeviceName(), "Failed to get site lat/lon from Pulsar controller.");
        return false;
    }

	return true;
}

// Old-style individual latitude/longitude retrieval
//void LX200Pulsar2::sendScopeLocation()
//{
//    LocationNP.s = IPS_OK;
//    int dd = 29, mm = 30;
//    if (isSimulation() || Pulsar2Commands::getSiteLatitude(PortFD, &dd, &mm))
//    {
//        LocationNP.np[0].value = (dd < 0 ? -1 : 1) * (abs(dd) + mm / 60.0);
//        LOGF_DEBUG("Pulsar latitude: %d:%d", dd, mm);
//    }
//    else
//    {
//        IDMessage(getDeviceName(), "Failed to get site latitude from Pulsar controller.");
//        LocationNP.s = IPS_ALERT;
//    }
//    dd = 48;
//    mm = 0;
//    if (isSimulation() || Pulsar2Commands::getSiteLongitude(PortFD, &dd, &mm))
//    {
//        LocationNP.np[1].value = (dd > 0 ? 360.0 - (dd + mm / 60.0) : -(dd - mm / 60.0));
//        LOGF_DEBUG("Pulsar longitude: %d:%d", dd, mm);
//
//        saveConfig(true, "GEOGRAPHIC_COORD");
//    }
//    else
//    {
//        IDMessage(getDeviceName(), "Failed to get site longitude from Pulsar controller.");
//        LocationNP.s = IPS_ALERT;
//    }
//    IDSetNumber(&LocationNP, nullptr);
//}

bool LX200Pulsar2::sendScopeTime()
{
    struct tm ltm;
    if (isSimulation())
    {
        const time_t t = time(nullptr);
        if (gmtime_r(&t, &ltm) == nullptr)
            return true;
        return false;
    }
    else
    {
        if (!Pulsar2Commands::getUTCTime(PortFD, &ltm.tm_hour, &ltm.tm_min, &ltm.tm_sec) ||
                !Pulsar2Commands::getUTCDate(PortFD, &ltm.tm_mon, &ltm.tm_mday, &ltm.tm_year))
            return false;
        ltm.tm_mon -= 1;
        ltm.tm_year -= 1900;
    }

    // Get time epoch and convert to TimeT
    const time_t time_epoch = mktime(&ltm);
    struct tm utm;
    localtime_r(&time_epoch, &utm);

    // Format it into ISO 8601
    char cdate[32];
    strftime(cdate, sizeof(cdate), "%Y-%m-%dT%H:%M:%S", &utm);

    IUSaveText(&TimeT[0], cdate);
    IUSaveText(&TimeT[1], "0"); // Pulsar maintains time in UTC only
    if (isDebug())
    {
        IDLog("Telescope Local Time: %02d:%02d:%02d\n", ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
        IDLog("Telescope TimeT Offset: %s\n", TimeT[1].text);
        IDLog("Telescope UTC Time: %s\n", TimeT[0].text);
    }
    // Let's send everything to the client
    TimeTP.s = IPS_OK;
    IDSetText(&TimeTP, nullptr);
    
    return true;
}

bool LX200Pulsar2::isSlewing()
{
    // A problem with the Pulsar controller is that the :YGi# command starts
    // returning the value 1 as long as a few seconds after a slew has been
    // started.  This means that a (short) slew can end before this happens.
    auto mount_is_off_target = [this](void)
    {
        return (fabs(currentRA - targetRA) > 1.0 / 3600.0 || fabs(currentDEC - targetDEC) > 5.0 / 3600.0);
    };
    // Detect the end of a short slew
    bool result = (just_started_slewing ? mount_is_off_target() : true);
    if (result)
    {
        int is_slewing = -1;
        if (PulsarTX::sendReceiveInt(PortFD, "#:YGi#", &is_slewing))
        {
            if (is_slewing == 1) // We can rely on the Pulsar "is slewing" indicator from here on
            {
                just_started_slewing = false;
                result = true;
			}
            else // ... otherwise we have to rely on the value of the attribute "just_started_slewing"
                result = just_started_slewing;
        }
        else // Fallback in case of error
            result = mount_is_off_target();
    }
    // Make sure that "just_started_slewing" is reset at the end of a slew
    if (!result)
        just_started_slewing = false;
    return result;
}
