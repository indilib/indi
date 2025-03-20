/*******************************************************************************
 Copyright(c) 2015 Camiel Severijns. All rights reserved.

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

#include "smartfocus.h"

#include "indicom.h"

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>

namespace
{
static constexpr const SmartFocus::Position PositionInvalid { static_cast<SmartFocus::Position>(0xFFFF) };
// Interval to check the focuser state (in milliseconds)
static constexpr const int TimerInterval { 500 };
// in seconds
static constexpr const int ReadTimeOut { 1 };

// SmartFocus command and response characters
static constexpr const char goto_position { 'g' };
static constexpr const char stop_focuser { 's' };
static constexpr const char read_id_register { 'b' };
static constexpr const char read_id_respons { 'j' };
static constexpr const char read_position { 'p' };
static constexpr const char read_flags { 't' };
static constexpr const char motion_complete { 'c' };
static constexpr const char motion_error { 'r' };
static constexpr const char motion_stopped { 's' };
} // namespace

std::unique_ptr<SmartFocus> smartFocus(new SmartFocus());

SmartFocus::SmartFocus()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);
}

bool SmartFocus::initProperties()
{
    INDI::Focuser::initProperties();

    // No speed for SmartFocus
    FocusSpeedNP[0].setMin(1);
    FocusSpeedNP[0].setMax(1);
    FocusSpeedNP[0].setValue(1);
    FocusSpeedNP.updateMinMax();

    IUFillLight(&FlagsL[STATUS_SERIAL_FRAMING_ERROR], "SERIAL_FRAMING_ERROR", "Serial framing error", IPS_OK);
    IUFillLight(&FlagsL[STATUS_SERIAL_OVERRUN_ERROR], "SERIAL_OVERRUN_ERROR", "Serial overrun error", IPS_OK);
    IUFillLight(&FlagsL[STATUS_MOTOR_ENCODE_ERROR], "MOTOR_ENCODER_ERROR", "Motor/encoder error", IPS_OK);
    IUFillLight(&FlagsL[STATUS_AT_ZERO_POSITION], "AT_ZERO_POSITION", "At zero position", IPS_OK);
    IUFillLight(&FlagsL[STATUS_AT_MAX_POSITION], "AT_MAX_POSITION", "At max. position", IPS_OK);
    IUFillLightVector(&FlagsLP, FlagsL, STATUS_NUM_FLAGS, getDeviceName(), "FLAGS", "Status Flags", MAIN_CONTROL_TAB,
                      IPS_IDLE);

    IUFillNumber(&MotionErrorN[0], "MOTION_ERROR", "Motion error", "%6.0f", -100., 100., 1., 0.);
    IUFillNumberVector(&MotionErrorNP, MotionErrorN, 1, getDeviceName(), "MOTION_ERROR", "Motion error",
                       OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(FocusMaxPosNP[0].getValue()); //MaxPositionN[0].value);
    FocusRelPosNP[0].setValue(10);
    FocusRelPosNP[0].setStep(1);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(FocusMaxPosNP[0].getValue()); //MaxPositionN[0].value);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1);
    setCurrentPollingPeriod(TimerInterval);
    return true;
}

                        bool SmartFocus::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(&FlagsLP);
        defineProperty(&MotionErrorNP);
        SFgetState();
        IDMessage(getDeviceName(), "SmartFocus focuser ready for use.");
    }
    else
    {
        deleteProperty(FlagsLP.name);
        deleteProperty(MotionErrorNP.name);
    }
    return true;
}

bool SmartFocus::Handshake()
{
    if (isSimulation())
        return true;

    if (!SFacknowledge())
    {
        LOG_DEBUG("SmartFocus is not communicating.");
        return false;
    }

    LOG_DEBUG("SmartFocus is communicating.");
    return true;
}

const char *SmartFocus::getDefaultName()
{
    return "SmartFocus";
}

bool SmartFocus::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (strcmp(name, MotionErrorNP.name) == 0)
        {
            IUUpdateNumber(&MotionErrorNP, values, names, n);
            MotionErrorNP.s = IPS_OK;
            IDSetNumber(&MotionErrorNP, nullptr);
            return true;
        }
    }
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

//bool SmartFocus::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n) {
//  return INDI::Focuser::ISNewSwitch(dev,name,states,names,n);
//}

bool SmartFocus::AbortFocuser()
{
    bool result = true;
    if (!isSimulation() && SFisMoving())
    {
        LOG_DEBUG("AbortFocuser: stopping motion");
        result = send(&stop_focuser, sizeof(stop_focuser), "AbortFocuser");
        // N.B.: The response to this stop command will be captured in the TimerHit method!
    }
    return result;
}

IPState SmartFocus::MoveAbsFocuser(uint32_t targetPosition)
{
    Position destination = static_cast<Position>(targetPosition);
    IPState result       = IPS_ALERT;
    if (isSimulation())
    {
        position = destination;
        state    = Idle;
        result   = IPS_OK;
    }
    else
    {
        constexpr bool Correct_Positions = true;
        if (Correct_Positions)
        {
            const int error = MotionErrorN[0].value; // The NGF-S overshoots motions by 3 steps.
            if (destination > position) destination -= error, destination = std::max(position, destination);
            if (destination < position) destination += error, destination = std::min(position, destination);
        }
        if (destination != position)
        {
            char command[3];
            command[0] = goto_position;
            command[1] = ((destination >> 8) & 0xFF);
            command[2] = (destination & 0xFF);
            LOGF_DEBUG("MoveAbsFocuser: destination= %d", destination);
            tcflush(PortFD, TCIOFLUSH);
            if (send(command, sizeof(command), "MoveAbsFocuser"))
            {
                char respons;
                if (recv(&respons, sizeof(respons), "MoveAbsFocuser"))
                {
                    LOGF_DEBUG("MoveAbsFocuser received echo: %c", respons);
                    if (respons != goto_position)
                        LOGF_ERROR("MoveAbsFocuser received unexpected respons: %c (0x02x)", respons,
                                   respons);
                    else
                    {
                        state  = MovingTo;
                        result = IPS_BUSY;
                    }
                }
            }
        }
        else
            result = IPS_OK;
    }
    return result;
}

IPState SmartFocus::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    return MoveAbsFocuser(position + (dir == FOCUS_INWARD ? -ticks : ticks));
}

class NonBlockingIO
{
    public:
        NonBlockingIO(const char *_device, const int _fd) : device(_device), fd(_fd), flags(fcntl(_fd, F_GETFL, 0))
        {
            if (flags == -1)
                DEBUGFDEVICE(device, INDI::Logger::DBG_ERROR, "NonBlockingIO::NonBlockingIO() fcntl get error: errno=%d",
                             errno);
            else if (fcntl(fd, F_SETFL, (flags | O_NONBLOCK)) == -1)
                DEBUGFDEVICE(device, INDI::Logger::DBG_ERROR, "NonBlockingIO::NonBlockingIO() fcntl set error: errno=%d",
                             errno);
        }

        ~NonBlockingIO()
        {
            if (flags != -1 && fcntl(fd, F_SETFL, flags) == -1)
                DEBUGFDEVICE(device, INDI::Logger::DBG_ERROR,
                             "NonBlockinIO::~NonBlockingIO() fcntl set error: errno=%d", errno);
        }

    private:
        const char *device;
        const int fd;
        const int flags;
};

void SmartFocus::TimerHit()
{
    // Wait for the end-of-motion character (c,r, or s) when the focuser is moving
    // due to a request from this driver. Otherwise, retrieve the current position
    // and state flags of the SmartFocus unit to keep the driver up-to-date with
    // motion commands issued manually.
    // TODO: What happens when the smartFocus unit is switched off?
    if (!isConnected())
        return;

    if (!isSimulation() && SFisMoving())
    {
        NonBlockingIO non_blocking(getDeviceName(), PortFD); // Automatically controls blocking IO by its scope
        char respons;
        if (read(PortFD, &respons, sizeof(respons)) == sizeof(respons))
        {
            LOGF_DEBUG("TimerHit() received character: %c (0x%02x)", respons, respons);
            if (respons != motion_complete && respons != motion_error && respons != motion_stopped)
                LOGF_ERROR("TimerHit() received unexpected character: %c (0x%02x)", respons,
                           respons);
            state = Idle;
        }
    }
    if (SFisIdle())
        SFgetState();
    timer_id = SetTimer(TimerInterval);
}

bool SmartFocus::saveConfigItems(FILE *fp)
{
    Focuser::saveConfigItems(fp);
    IUSaveConfigNumber(fp, &MotionErrorNP);
    return true;
}

bool SmartFocus::SFacknowledge()
{
    bool success = false;
    if (isSimulation())
        success = true;
    else
    {
        tcflush(PortFD, TCIOFLUSH);
        if (send(&read_id_register, sizeof(read_id_register), "SFacknowledge"))
        {
            char respons[2];
            if (recv(respons, sizeof(respons), "SFacknowledge", false))
            {
                LOGF_DEBUG("SFacknowledge received: %c%c", respons[0], respons[1]);
                success = (respons[0] == read_id_register && respons[1] == read_id_respons);
                if (!success)
                    LOGF_ERROR("SFacknowledge received unexpected respons: %c%c (0x02 0x02x)",
                               respons[0], respons[1], respons[0], respons[1]);
            }
        }
    }
    return success;
}

SmartFocus::Position SmartFocus::SFgetPosition()
{
    Position result = PositionInvalid;
    if (isSimulation())
        result = position;
    else
    {
        tcflush(PortFD, TCIOFLUSH);
        if (send(&read_position, sizeof(read_position), "SFgetPosition"))
        {
            char respons[3];
            if (recv(respons, sizeof(respons), "SFgetPosition"))
            {
                if (respons[0] == read_position)
                {
                    result = (((static_cast<Position>(respons[1]) << 8) & 0xFF00) |
                              (static_cast<Position>(respons[2]) & 0x00FF));
                    LOGF_DEBUG("SFgetPosition: position=%d", result);
                }
                else
                    LOGF_ERROR("SFgetPosition received unexpected respons: %c (0x02x)", respons[0],
                               respons[0]);
            }
        }
    }
    return result;
}

SmartFocus::Flags SmartFocus::SFgetFlags()
{
    Flags result = 0x00;
    if (!isSimulation())
    {
        tcflush(PortFD, TCIOFLUSH);
        if (send(&read_flags, sizeof(read_flags), "SFgetFlags"))
        {
            char respons[2];
            if (recv(respons, sizeof(respons), "SFgetFlags"))
            {
                if (respons[0] == read_flags)
                {
                    result = static_cast<Flags>(respons[1]);
                    LOGF_DEBUG("SFgetFlags: flags=0x%02x", result);
                }
                else
                    LOGF_ERROR("SFgetFlags received unexpected respons: %c (0x02x)", respons[0],
                               respons[0]);
            }
        }
    }
    return result;
}

void SmartFocus::SFgetState()
{
    const Flags flags = SFgetFlags();

    FlagsL[STATUS_SERIAL_FRAMING_ERROR].s = (flags & SerFramingError ? IPS_ALERT : IPS_OK);
    FlagsL[STATUS_SERIAL_OVERRUN_ERROR].s = (flags & SerOverrunError ? IPS_ALERT : IPS_OK);
    FlagsL[STATUS_MOTOR_ENCODE_ERROR].s   = (flags & MotorEncoderError ? IPS_ALERT : IPS_OK);
    FlagsL[STATUS_AT_ZERO_POSITION].s     = (flags & AtZeroPosition ? IPS_ALERT : IPS_OK);
    FlagsL[STATUS_AT_MAX_POSITION].s      = (flags & AtMaxPosition ? IPS_ALERT : IPS_OK);
    IDSetLight(&FlagsLP, nullptr);

    if ((position = SFgetPosition()) == PositionInvalid)
    {
        FocusAbsPosNP.setState(IPS_ALERT);
        LOG_ERROR("Error while reading SmartFocus position");
    }
    else
    {
        FocusAbsPosNP[0].setValue(position);
        FocusAbsPosNP.setState(IPS_OK);
        FocusAbsPosNP.apply();
    }
}

bool SmartFocus::send(const char *command, const size_t nbytes, const char *from, const bool log_error)
{
    int nbytes_written = 0;
    const int rc       = tty_write(PortFD, command, nbytes, &nbytes_written);
    const bool success = (rc == TTY_OK && nbytes_written == (int)nbytes);

    if (!success && log_error)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s (%d of %d bytes written).", from, errstr, nbytes_written, nbytes);
    }
    return success;
}

bool SmartFocus::recv(char *respons, const size_t nbytes, const char *from, const bool log_error)
{
    int nbytes_read    = 0;
    const int rc       = tty_read(PortFD, respons, nbytes, ReadTimeOut, &nbytes_read);
    const bool success = (rc == TTY_OK && nbytes_read == (int)nbytes);

    if (!success && log_error)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("%s: %s (%d of %d bytes read).", errstr, from, nbytes_read, nbytes);
    }
    return success;
}
