/*******************************************************************************
 ScopeDome Dome INDI Driver

 Copyright(c) 2017 Jarno Paananen. All rights reserved.

 based on:

 ScopeDome Windows ASCOM driver version 5.1.30

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

#include "scopedome_dome.h"
#include "indicom.h"

#include <termios.h>

#define SCOPEDOME_TIMEOUT 2
#define SCOPEDOME_MAX_READS 10

static const uint8_t header = 0xaa;

bool ScopeDomeUSB21::detect()
{
    int rc = -1;
    ScopeDomeCommand cmd;
    LOGF_INFO("Detect! %d", rc);
    rc = write(ConnectionTest);
    LOGF_INFO("write rc: %d", rc);
    rc = read(cmd);
    LOGF_INFO("read rc: %d, cmd %d", rc, (int)cmd);

    if (cmd != ConnectionTest)
    {
        return false;
    }
    // Disable "safe" communication mode that resets connection after few seconds
    rc = write(StopSafeCommunication);
    rc = read(cmd);
    return (cmd == StopSafeCommunication);
}

uint8_t ScopeDomeUSB21::CRC(uint8_t crc, uint8_t data)
{
    crc ^= data;

    for (int i = 0; i < 8; i++)
    {
        if (crc & 1)
            crc = ((crc >> 1) ^ 0x8C);
        else
            crc >>= 1;
    }
    return crc;
}

int ScopeDomeUSB21::writeBuf(ScopeDomeCommand cmd, uint8_t len, uint8_t *buff)
{
    int BytesToWrite   = len + 4;
    int BytesWritten   = 0;
    int nbytes_written = 0, rc = -1;
    char errstr[MAXRBUF];

    BytesWritten = 0;
    uint8_t cbuf[BytesToWrite];
    cbuf[0] = header;
    cbuf[3] = CRC(0, cbuf[0]);
    cbuf[1] = len;
    cbuf[3] = CRC(cbuf[3], cbuf[1]);
    cbuf[2] = cmd;
    cbuf[3] = CRC(cbuf[3], cbuf[2]);

    for (int i = 0; i < len; i++)
    {
        cbuf[i + 4] = buff[i];
        cbuf[3]     = CRC(cbuf[3], buff[i]);
    }

    tcflush(PortFD, TCIOFLUSH);

    prevcmd = cmd;

    // Write buffer
    if ((rc = tty_write(PortFD, (const char *)cbuf, sizeof(cbuf), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command: %s. Cmd %d", errstr, cmd);
    }
    return rc;
}

int ScopeDomeUSB21::write(ScopeDomeCommand cmd)
{
    int nbytes_written = 0, rc = -1;
    uint8_t cbuf[4];
    char errstr[MAXRBUF];

    tcflush(PortFD, TCIOFLUSH);

    cbuf[0] = header;
    cbuf[3] = CRC(0, cbuf[0]);
    cbuf[1] = 0;
    cbuf[3] = CRC(cbuf[3], cbuf[1]);
    cbuf[2] = cmd;
    cbuf[3] = CRC(cbuf[3], cbuf[2]);

    prevcmd = cmd;

    // Write buffer
    //LOGF_ERROR("write cmd: %x %x %x %x", cbuf[0], cbuf[1], cbuf[2], cbuf[3]);
    if ((rc = tty_write(PortFD, (const char *)cbuf, sizeof(cbuf), &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error writing command: %s. Cmd: %d", errstr, cmd);
    }
    return rc;
}

int ScopeDomeUSB21::readBuf(ScopeDomeCommand &cmd, uint8_t len, uint8_t *buff)
{
    int nbytes_read = 0, rc = -1;
    int BytesToRead           = len + 4;
    uint8_t cbuf[BytesToRead];
    char errstr[MAXRBUF];

    // Read buffer
    if ((rc = tty_read(PortFD, (char *)cbuf, sizeof(cbuf), SCOPEDOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error reading: %s. Cmd: %d", errstr, prevcmd);
        return rc;
    }

    //LOGF_ERROR("readbuf cmd: %x %x %x %x", cbuf[0], cbuf[1], cbuf[2], cbuf[3]);
    uint8_t Checksum = CRC(0, cbuf[0]);
    Checksum         = CRC(Checksum, cbuf[1]);
    cmd              = (ScopeDomeCommand)cbuf[2];
    Checksum         = CRC(Checksum, cbuf[2]);

    for (int i = 0; i < len; i++)
    {
        buff[i]  = cbuf[i + 4];
        Checksum = CRC(Checksum, cbuf[i + 4]);
    }

    if (cbuf[3] != Checksum)
    {
        LOGF_ERROR("readbuf checksum error, cmd: %d", prevcmd);
        return CHECKSUM_ERROR;
    }
    if (cmd == FunctionNotSupported)
    {
        LOG_ERROR("readbuf not supported error");
        return FUNCTION_NOT_SUPPORTED_BY_FIRMWARE;
    }

    if (cbuf[1] != len)
    {
        LOGF_ERROR("readbuf packet length error, cmd: %d", prevcmd);
        return PACKET_LENGTH_ERROR;
    }
    return rc;
}

int ScopeDomeUSB21::read(ScopeDomeCommand &cmd)
{
    int nbytes_read = 0, rc = -1;
    int err         = 0;
    uint8_t cbuf[4] = { 0 };
    char errstr[MAXRBUF];

    // Read buffer
    if ((rc = tty_read(PortFD, (char *)cbuf, sizeof(cbuf), SCOPEDOME_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Error reading: %s. Cmd %d", errstr, prevcmd);
        return rc;
    }

    //LOGF_ERROR("read cmd: %x %x %x %x", cbuf[0], cbuf[1], cbuf[2], cbuf[3]);
    uint8_t Checksum = CRC(0, cbuf[0]);
    Checksum         = CRC(Checksum, cbuf[1]);
    cmd              = (ScopeDomeCommand)cbuf[2];
    Checksum         = CRC(Checksum, cbuf[2]);

    if (cbuf[3] != Checksum || cbuf[1] != 0)
    {
        LOGF_ERROR("read checksum error, cmd: %d", prevcmd);
        return CHECKSUM_ERROR;
    }
    switch (cmd)
    {
        case MotionConflict:
            LOG_ERROR("read motion conflict");
            err = MOTION_CONFLICT;
            break;

        case FunctionNotSupported:
            LOG_ERROR("read function not supported");
            err = FUNCTION_NOT_SUPPORTED;
            break;

        case ParamError:
            LOG_ERROR("read param error");
            err = PARAM_ERROR;
            break;
        default:
            break;
    }
    return err;
}
