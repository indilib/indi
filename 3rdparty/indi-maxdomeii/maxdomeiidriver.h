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

#pragma once

// Direction fo azimuth movement
#define MAXDOMEII_EW_DIR 0x01
#define MAXDOMEII_WE_DIR 0x02

// Azimuth motor status. When motor is idle, sometimes returns 0, sometimes 4. After connect, it returns 5
enum AzStatus
{
    AS_IDLE = 1,
    AS_MOVING_WE,
    AS_MOVING_EW,
    AS_IDLE2,
    AS_ERROR
};

// Shutter status
enum ShStatus
{
    SS_CLOSED = 0,
    SS_OPENING,
    SS_OPEN,
    SS_CLOSING,
    SS_ABORTED,
    SS_ERROR
};

extern const char *ErrorMessages[];

void hexDump(char *buf, const char *data, int size);


class MaxDomeIIDriver
{
    public:
        MaxDomeIIDriver() { fd = 0; }

        const char *getDeviceName();
        void SetPortFD(int port_fd);
        void SetDevice(const char *name);

        int Connect(const char *device);
        int Disconnect();

        int AbortAzimuth();
        int HomeAzimuth();
        int GotoAzimuth(int nDir, int nTicks);
        int Status(ShStatus *shStatus, AzStatus *azStatus,
                unsigned *azimuthPos, unsigned *homePos);
        int Ack();
        int SetPark(int nParkOnShutter, int nTicks);
        int SetTicksPerTurn(int nTicks);
        int Park();

        //  Shutter commands
        int OpenShutter();
        int OpenUpperShutterOnly();
        int CloseShutter();
        int AbortShutter();
        int ExitShutter();

    protected:
        int ReadResponse();
        int SendCommand(char cmdId, const char *payload, int payloadLen);

    private:
        int fd;
        char buffer[16];
};
