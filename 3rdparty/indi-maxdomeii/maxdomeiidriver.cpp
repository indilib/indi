/*
    Max Dome II Driver
    Copyright (C) 2009 Ferran Casarramona (ferran.casarramona@gmail.com)
    Refactored by Juan Menendez

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

#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <indicom.h>
#include <indilogger.h>

#include "maxdomeiidriver.h"

#define MAXDOME_TIMEOUT 5  // FD timeout in seconds
#define BUFFER_SIZE     16 // Maximum message length

// Start byte
#define START_BYTE 0x01

// Message Destination
#define TO_MAXDOME  0x00
#define TO_COMPUTER 0x80

// Commands available
#define ABORT_CMD   0x03 // Abort azimuth movement
#define HOME_CMD    0x04 // Move until 'home' position is detected
#define GOTO_CMD    0x05 // Go to azimuth position
#define SHUTTER_CMD 0x06 // Send a command to Shutter
#define STATUS_CMD  0x07 // Retrieve status
#define TICKS_CMD   0x09 // Set the number of tick per revolution of the dome
#define ACK_CMD     0x0A // ACK (?)
#define SETPARK_CMD 0x0B // Set park coordinates and if need to park before to operating shutter

// Shutter commands
#define OPEN_SHUTTER            0x01
#define OPEN_UPPER_ONLY_SHUTTER 0x02
#define CLOSE_SHUTTER           0x03
#define EXIT_SHUTTER            0x04 // Command send to shutter on program exit
#define ABORT_SHUTTER           0x07


// Error messages
const char *ErrorMessages[] = {
	"Ok",                              // no error
	"No response from MAX DOME",       // -1
	"Invalid declared message length", // -2
	"Message too short",               // -3
	"Checksum error",                  // -4
	"Could not send command",          // -5
	"Response do not match command",   // -6
	""
};

char device_str[MAXINDIDEVICE] = "MaxDome II";


void hexDump(char *buf, const char *data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", (unsigned char)data[i]);

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

void MaxDomeIIDriver::SetPortFD(int port_fd)
{
    fd = port_fd;
}

// This method is required by the logging macros
const char *MaxDomeIIDriver::getDeviceName()
{
    return device_str;
}

void MaxDomeIIDriver::SetDevice(const char *name)
{
    strncpy(device_str, name, MAXINDIDEVICE);
}

/*
	Opens serial port with proper configuration

	@param device String to device (/dev/ttyS0)
	@return -1 if can't connect. File descriptor, otherwise.
*/
int MaxDomeIIDriver::Connect(const char *device)
{
    if (tty_connect(device, 19200, 8, 0, 1, &fd) != TTY_OK)
        return -1;

    return fd;
}

/*
	Inform Max Dome from a disconection and closes serial port

	@param fd File descriptor
	@return 0 Ok,
*/
int MaxDomeIIDriver::Disconnect()
{
    ExitShutter(); // Really don't know why this is needed, but ASCOM driver does it
    tty_disconnect(fd);

    return 0;
}

/*
	Calculates or checks the checksum
	It ignores first byte
	For security we limit the checksum calculation to BUFFER_SIZE length

	@param msg Pointer to unsigned char array with the message
	@param len Length of the message
	@return Checksum byte
*/
signed char computeChecksum(char *msg, int len)
{
    char checksum = 0;

    for (int i=1; i<len && i<BUFFER_SIZE; i++)
        checksum -= msg[i];

    return checksum;
}

/*
	Reads a response from MaxDome II
	It verifies message sintax and checksum.
    Read data is stored in MaxDomeIIDriver::buffer

	@return
      - Respose size if message is Ok
      - -1: no response or no start caracter found
      - -2: invalid declared message length
      - -3: message too short
      - -4: checksum error
*/
int MaxDomeIIDriver::ReadResponse()
{
    int nbytes;
    int err = TTY_OK;

    buffer[0] = 0x00;
    buffer[13] = 0x00;

    // Look for a starting byte until time out occurs or BUFFER_SIZE bytes were read
    while (*buffer != START_BYTE && err == TTY_OK)
        err = tty_read(fd, buffer, 1, MAXDOME_TIMEOUT, &nbytes);

    if (err != TTY_OK || buffer[0] != START_BYTE) {
        LOG_ERROR(ErrorMessages[1]);
        return -1;
    }

    // Read message length
    err = tty_read(fd, buffer + 1, 1, MAXDOME_TIMEOUT, &nbytes);
    if (err != TTY_OK || buffer[1] < 0x02 || buffer[1] > 0x0e) {
        LOG_ERROR(ErrorMessages[2]);
        return -2;
    }

    int len = buffer[1];

    // Read the rest of the message
    err = tty_read(fd, buffer + 2, len, MAXDOME_TIMEOUT, &nbytes);
    if (err != TTY_OK || nbytes != len) {
        LOG_ERROR(ErrorMessages[3]);
        return -3;
    }

    if (computeChecksum(buffer, len + 2) != 0) {
        LOG_ERROR(ErrorMessages[4]);
        return -4;
    }

    return len + 2;
}

/*
	Send a command to MaxDome II

	@param cmdId Command identifier code
	@param payload Payload data
	@param payloadLen Length of payload data
	@return
      -  0: command received by MAX DOME
      - -1: no response or no start caracter found
      - -2: invalid declared message length
      - -3: message too short
      - -4: checksum error
      - -5: couldn't send command
      - -6: response don't match command
*/
int MaxDomeIIDriver::SendCommand(char cmdId, const char *payload, int payloadLen)
{
    int err;
    int nbytes;
    char errmsg[MAXRBUF];
    char cmd[BUFFER_SIZE];
    //char hexbuf[3*BUFFER_SIZE];

    cmd[0] = START_BYTE;
    cmd[1] = payloadLen + 2;
    cmd[2] = cmdId;
    cmd[3 + payloadLen] = computeChecksum(cmd, 3 + payloadLen);

    memcpy(cmd + 3, payload, payloadLen);

    //hexDump(hexbuf, cmd, 4 + payloadLen);
    //LOGF_DEBUG("CMD (%s)", hexbuf);

    tcflush(fd, TCIOFLUSH);

    if ((err = tty_write(fd, cmd, 4 + payloadLen, &nbytes)) != TTY_OK)
    {
        tty_error_msg(err, errmsg, MAXRBUF);
        LOG_ERROR(errmsg);
        return -5;
    }

    nbytes = ReadResponse();
    if (nbytes < 0)
        return nbytes;

    if (buffer[2] != (char)(cmdId | TO_COMPUTER)) {
        LOG_ERROR(ErrorMessages[6]);
        return -6;
    }

    //hexDump(hexbuf, buffer, nbytes);
    //LOGF_DEBUG("RES (%s)", hexbuf);

    return 0;
}

/*
	Abort azimuth movement

	@return Same as SendCommand
*/
int MaxDomeIIDriver::AbortAzimuth()
{
    LOG_INFO("Azimuth movement aborted");
    return SendCommand(ABORT_CMD, nullptr, 0);
}

/*
	Move until 'home' position is detected

	@return Same as SendCommand
*/
int MaxDomeIIDriver::HomeAzimuth()
{
    LOG_INFO("Homing azimuth");
    return SendCommand(HOME_CMD, nullptr, 0);
}

/*
	Go to a new azimuth position

	@param nDir Direcction of the movement. 0 E to W. 1 W to E
	@param nTicks Ticks from home position in E to W direction.
	@return Same as SendCommand
*/
int MaxDomeIIDriver::GotoAzimuth(int nDir, int nTicks)
{
    LOGF_DEBUG("Moving dome to azimuth: %d", nTicks);

    char payload[3];
    payload[0] = (char)nDir;
    payload[1] = (char)(nTicks / 256);
    payload[2] = (char)(nTicks % 256);

    return SendCommand(GOTO_CMD, payload, sizeof(payload));
}

/*
	Ask Max Dome status

	@param shStatus Returns shutter status
	@param azStatus Returns azimuth status
	@param azimuthPos Returns azimuth current position (in ticks from home position)
	@param homePos Returns last position where home was detected
	@return Same as SendCommand
*/
int MaxDomeIIDriver::Status(ShStatus *shStatus, AzStatus *azStatus,
        unsigned *azimuthPos, unsigned *homePos)
{
    int ret = SendCommand(STATUS_CMD, nullptr, 0);
    if (ret != 0) {
        return ret;
    }

    *shStatus   = (ShStatus)buffer[3];
    *azStatus   = (AzStatus)buffer[4];
    *azimuthPos = (unsigned)(((uint8_t)buffer[5]) * 256 + ((uint8_t)buffer[6]));
    *homePos    = ((unsigned)buffer[7]) * 256 + ((unsigned)buffer[8]);

    LOGF_DEBUG("Dome status: az=%d home=%d", *azimuthPos, *homePos);
    return 0;
}

/*
	Ack comunication

	@return Same as SendCommand
*/
int MaxDomeIIDriver::Ack()
{
    LOG_DEBUG("ACK sent");
    return SendCommand(ACK_CMD, nullptr, 0);
}

/*
	Set park coordinates and if need to park before to operating shutter

	@param nParkOnShutter
      - 0 operate shutter at any azimuth.
      - 1 go to park position before operating shutter
	@param nTicks Ticks from home position in E to W direction.
	@return Same as SendCommand
*/
int MaxDomeIIDriver::SetPark(int nParkOnShutter, int nTicks)
{
    LOGF_INFO("Setting park position: %d", nTicks);

    char payload[3];
    payload[0] = (char)nParkOnShutter;
    payload[1] = (char)(nTicks / 256);
    payload[2] = (char)(nTicks % 256);

    return SendCommand(SETPARK_CMD, payload, sizeof(payload));
}

/*
    Set ticks per turn of the dome

    @param nTicks Ticks from home position in E to W direction.
	@return Same as SendCommand
 */
int MaxDomeIIDriver::SetTicksPerTurn(int nTicks)
{
    LOGF_INFO("Setting ticks per turn: %d", nTicks);

    char payload[2];
    payload[0] = (char)(nTicks / 256);
    payload[1] = (char)(nTicks % 256);

    return SendCommand(TICKS_CMD, payload, sizeof(payload));
}

///////////////////////////////////////////////////////////////////////////
//
//  Shutter commands
//
///////////////////////////////////////////////////////////////////////////

/*
	Opens the shutter fully

	@return Same as SendCommand
*/
int MaxDomeIIDriver::OpenShutter()
{
    LOG_INFO("Opening shutter");

    char payload[] = { OPEN_SHUTTER };
    return SendCommand(SHUTTER_CMD, payload, sizeof(payload));
}

/*
	Opens the upper shutter only

	@return Same as SendCommand
*/
int MaxDomeIIDriver::OpenUpperShutterOnly()
{
    LOG_INFO("Opening upper shutter");

    char payload[] = { OPEN_UPPER_ONLY_SHUTTER };
    return SendCommand(SHUTTER_CMD, payload, sizeof(payload));
}

/*
	Close the shutter

	@return Same as SendCommand
*/
int MaxDomeIIDriver::CloseShutter()
{
    LOG_INFO("Closing shutter");

    char payload[] = { CLOSE_SHUTTER };
    return SendCommand(SHUTTER_CMD, payload, sizeof(payload));
}

/*
	Abort shutter movement

	@return Same as SendCommand
*/
int MaxDomeIIDriver::AbortShutter()
{
    LOG_INFO("Aborting shutter operation");

    char payload[] = { ABORT_SHUTTER };
    return SendCommand(SHUTTER_CMD, payload, sizeof(payload));
}

/*
	Exit shutter (?) Normally send at program exit

	@return Same as SendCommand
*/
int MaxDomeIIDriver::ExitShutter()
{
    LOG_INFO("Exiting shutter");

    char payload[] = { EXIT_SHUTTER };
    return SendCommand(SHUTTER_CMD, payload, sizeof(payload));
}
