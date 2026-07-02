/*******************************************************************************
  Copyright(c) 2025 Frank Wang & Jérémie Klein. All rights reserved.

  WandererRotator Lite V1

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

#include "wanderer_rotator_base.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <sstream>
#include <regex>
#include <chrono>
#include <algorithm>

using namespace std::literals;

void WandererReader::init()
{
    std::string name = wanderer->getRotatorHandshakeName();
    this->patterns["handshake"] =
        // {name}A{firmware}A{θ}A{backlash}A{reversed}A\r\n
        name + "A([0-9]{8})A([-+]?[0-9]+)A([^A]+)A([01])A\r\n";

    // {Δθ}A{θ}A\r\n
    this->patterns["movement"] = "([^A]+)A([-+]?[0-9]+)A\r\n";
    this->patterns["volterr"] = "NP\r\n";
}

WandererReader::~WandererReader()
{
    this->stop();
}

const char * WandererReader::getDeviceName() const {
    return this->wanderer->getDeviceName();
}

bool WandererReader::start(int fd)
{
    if (this->read_thread.joinable())
        return false;

    this->failures = 0;
    this->timeouts = 0;
    this->stop_requested = false;

    this->read_thread = std::thread([this, fd](){
        char buf[256]; // some large-enough buffer for all conceivable inputs
        int totbytes = 0;
        this->did_stop = false;
        while (!this->stop_requested) {
            int nbytes = 0;
            memset(buf, 0, sizeof(buf));
            int rc = tty_nread_section(fd, buf + totbytes,
                                       sizeof(buf) - 1 - totbytes, '\n', 3,
                                       &nbytes);
            const auto now = std::chrono::system_clock::now();

            totbytes += nbytes;

            if (nbytes == 0) {
                if (rc != TTY_TIME_OUT) {
                    LOG_ERROR("Received zero bytes but didn't time out!?!");
                    this->stop_requested = true;
                }
                ++this->timeouts;
                continue;
            } else if (rc != TTY_OK) {
                char errorMessage[MAXRBUF];
                tty_error_msg(rc, errorMessage, MAXRBUF);
                LOGF_ERROR("Device read error: %s", errorMessage);
                ++this->failures;
                if (this->failures > MAX_FAILURES)
                    break;
                continue;
            } else if (buf[totbytes-1] != '\n') {
                if (rc != TTY_TIME_OUT)
                    LOG_ERROR(
                      "Didn't time out but didn't read until endline char");
                continue;
            }

            buf[totbytes] = '\0'; // ensure this thing is null terminated
            totbytes = 0; // reset this for next while-loop iteration
            std::string sbuf = buf;

            /* we have a full line with a possibly valid message--parse it. */
            bool valid = false;
            for (const auto & pat : this->patterns) {
                std::smatch matches;

                if (!std::regex_match(sbuf, matches, pat.second))
                    // does not match--go on to test other regular expressions
                    continue;

                //thread-safety:
                std::lock_guard<std::recursive_mutex> lock(this->mutex);
                valid = true;
                // regex matched one of the expected messages, figure out which
                if (pat.first == "handshake") {
                    this->_firmware   = {now, std::stoi(matches[1].str())};
                    this->_angle      = {now, std::stoi(matches[2].str())*1e-3};
                    this->_backlash   = {now, std::stof(matches[3].str())};
                    this->_reversed   = {now, std::stoi(matches[4].str())};
                } else if (pat.first == "movement") {
                    this->_completed  = {now, std::stof(matches[1].str())};
                    this->_angle      = {now, std::stoi(matches[2].str())*1e-3};
                } else if (pat.first == "volterr") {
                    this->_voltage_error = {now, true};
                } else {
                    LOGF_ERROR(
                      "Unimplemented (%s).  This code should never be reached.",
                      pat.first);
                }
                this->cv.notify_all();

                // no need to keep trying other patterns, so break out of "for".
                break;
            }

            if (!valid)
                LOGF_ERROR("Invalid message read from serial: \"%s\"", buf);
        }

        this->did_stop = true;
    });
    return true;
}

void WandererReader::stop()
{
    if (!this->read_thread.joinable())
        return;

    this->stop_requested = true;
    this->read_thread.join();
    // ensure that our thread object is no longer joinable.
    this->read_thread = std::thread();
}

bool WandererReader::updated_since(
  const unsigned int updates, const std::chrono::system_clock::time_point & t0)
{
    std::lock_guard<std::recursive_mutex> lock(this->mutex);
    int recent = 0;
    if (t0 < this->_firmware.first)
        recent |= UPDATES::FIRMWARE;
    if (t0 < this->_angle.first)
        recent |= UPDATES::ANGLE;
    if (t0 < this->_backlash.first)
        recent |= UPDATES::BACKLASH;
    if (t0 < this->_reversed.first)
        recent |= UPDATES::REVERSED;
    if (t0 < this->_completed.first)
        recent |= UPDATES::COMPLETED;
    if (t0 < this->_voltage_error.first)
        recent |= UPDATES::VOLTERR;

    if (updates == 0)
        return recent != 0;
    return (recent & updates) == updates;
}

WandererRotatorBase::WandererRotatorBase() : reader(this)
{
    setVersion(1, 0);
}

bool WandererRotatorBase::initProperties()
{
    this->reader.init(); // delayed init to allow for vptr to be correct first
    INDI::Rotator::initProperties();

    SetCapability(ROTATOR_CAN_REVERSE | ROTATOR_CAN_ABORT | ROTATOR_CAN_HOME);

    addPollPeriodControl();
    addAuxControls();
    // Calibrate
    SetZeroSP[0].fill("Set_Zero", "Mechanical Zero", ISS_OFF);
    SetZeroSP.fill(getDeviceName(), "Set_Zero", "Set Current As", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // BACKLASH
    BacklashNP[0].fill( "BACKLASH", "Degree", "%.2f", 0, 3, 0.1, 0);
    BacklashNP.fill(getDeviceName(), "BACKLASH", "Backlash", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    EstimatedSpeedNP[0].fill("SPEED", "Speed [°/s]", "%.2f", 0, 5, 0.1, 3.0);
    EstimatedSpeedNP.fill(getDeviceName(), "ESTIMATED", "Estimated",
                          OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    EstimatedSpeedNP.load();

    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    /* for the Wanderer, we will just *always* have the center of the safe zone
     * (if used) be at mechanical angle == 0.0.  The user should reset the
     * mechanical zero if they need it at a specific location. */
    m_RotatorOffset = 0.0;

    return true;
}

bool WandererRotatorBase::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        defineProperty(SetZeroSP);
        defineProperty(BacklashNP);
        defineProperty(EstimatedSpeedNP);
    }
    else
    {
        deleteProperty(SetZeroSP);
        deleteProperty(BacklashNP);
        deleteProperty(EstimatedSpeedNP);
    }
    return true;
}

bool WandererRotatorBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (SetZeroSP.isNameMatch(name))
        {
            SetZeroSP.setState(send_zero_cmd(true) ? IPS_OK : IPS_ALERT);
            SetZeroSP.apply();
            GotoRotatorNP[0].setValue(0);
            GotoRotatorNP.apply();
            LOG_INFO("Virtual Mechanical Angle is set to zero.");
            return true;
        }
    }
    return Rotator::ISNewSwitch(dev, name, states, names, n);
}

bool WandererRotatorBase::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        // backlash
        if (BacklashNP.isNameMatch(name))
        {
            BacklashNP.update(values, names, n);
            double backlash = BacklashNP[0].getValue();

            char cmd[16];
            snprintf(cmd, 16, "%d", static_cast<int>(backlash * 10 + 1600000));

            BacklashNP.setState(sendCommand(cmd) ? IPS_OK : IPS_ALERT);
            BacklashNP.apply();
            LOG_INFO("Backlash Set");
            return true;
        }
        else if (EstimatedSpeedNP.isNameMatch(name))
        {
            EstimatedSpeedNP.update(values, names, n);
            EstimatedSpeedNP.setState(IPS_OK);
            EstimatedSpeedNP.apply();
            return true;
        }
    }
    return Rotator::ISNewNumber(dev, name, values, names, n);
}

bool WandererRotatorBase::Handshake()
{
    PortFD = serialConnection->getPortFD();
    tcflush(PortFD, TCIOFLUSH);
    this->reader.stop();
    this->reader.start(PortFD);

    if (!send_handshake())
        return false;

    //Device Model//////////////////////////////////////////////////////////////
    LOGF_INFO("Connected to device %s", this->getRotatorHandshakeName());

    // Firmware version/////////////////////////////////////////////////////////
    if(this->reader.firmware().second < getMinimumCompatibleFirmwareVersion())
    {
        LOG_ERROR("Firmware is outdated, please upgrade to latest firmware!");
        LOGF_ERROR("Current firmware is %s.", this->reader.firmware().second);
        return false;
    }

    // Angle////////////////////////////////////////////////////////////////////
    if(abs(this->reader.angle().second) > 400)
    {
        LOG_WARN("Virtual Mechanical Angle > 400°, setting to zero!");
        if (!send_zero_cmd(true))
        {
            LOG_ERROR("Could not redo handshake after zeroing virtual offset");
            return false;
        }
    }
    GotoRotatorNP[0].setValue(range360(this->reader.angle().second));
    //backlash//////////////////////////////////////////////////////////////////
    BacklashNP[0].setValue(this->reader.backlash().second);
    BacklashNP.setState(IPS_OK);
    BacklashNP.apply();
    //reverse///////////////////////////////////////////////////////////////////
    ReverseRotatorSP.reset();
    // sync software settings to hardware settings:
    if(this->reader.reversed().second)
    {
        ReverseRotatorSP[DefaultDevice::INDI_ENABLED].setState(ISS_ON);
    }
    else
    {
        ReverseRotatorSP[DefaultDevice::INDI_DISABLED].setState(ISS_ON);
    }
    ReverseRotatorSP.apply();

    this->state = State::HALTED;
    return true;
}

bool WandererRotatorBase::Disconnect()
{
    this->reader.stop();
    return this->INDI::Rotator::Disconnect();
}

IPState WandererRotatorBase::MoveRotator(double angle, double delta)
{
    if (this->move(angle, delta))
        return IPS_BUSY;
    return IPS_ALERT;
}

std::optional<std::chrono::system_clock::time_point>
WandererRotatorBase::move(double abs_angle, std::optional<double> delta)
{
    typedef WandererReader::UPDATES UP;
    if (this->state == State::MOVING)
    {
        /* rotator is already moving; abort and wait for position. */
        this->reader.wait_for(5s, UP::ANGLE, this->stop());
    }

    double current_angle = this->reader.angle().second;
    if (!delta) {
        auto best_path = calculateBestPath(abs_angle);
        if (!best_path)
            return {};
        abs_angle = best_path.value().first;
        delta = {best_path.value().second};
    }
    int steps = static_cast<int>(std::round(delta.value()*getStepsPerDegree()));

    if (steps == 0)
      return std::chrono::system_clock::now();

    /* Create the command.  The +1000000 is an undocumented offset in the
     * commands for movement.  The 20240226 serial protocol documentation shows
     * that one can use commands centered around zero instead of centered around
     * 1000000.  Perhaps we continue using this offset because it still does
     * work for the Mini v1/v2 and because of the other rotators that are
     * supported.  Documentation for the serial protocols of the other rotators
     * do not seem to be available online.
     */
    char cmd[16];
    snprintf(cmd, 16, "%d", steps + 1000000);
    auto t0 = sendCommand(cmd);
    if (t0) {
        /* Get "tracking" info ready to present predicted rotation angles while
         * motion is active since the Wanderer rotators do not have a way to
         * query the current position or motion progress without interrupting
         * the motion.
         */
        this->state = State::MOVING;
        this->tracking.t0 = t0.value();
        this->tracking.angle_0 = current_angle;
        this->tracking.angle_f = current_angle + delta.value();
    }
    return t0;
}

std::optional<std::chrono::system_clock::time_point> WandererRotatorBase::stop()
{
    tcflush(PortFD, TCOFLUSH);
    LOG_DEBUG("CMD Stop");
    auto t0 = sendCommand("Stop", "");
    if (t0)
        this->state = State::HALTED;
    return t0;
}

bool WandererRotatorBase::AbortRotator()
{
    return this->stop().has_value();
}

//////////////////////////////////////////////////////////////////////////////////////////////
/// \brief WandererRotatorBase::HomeRotator
/// \return
///
IPState WandererRotatorBase::HomeRotator()
{
    if (this->reader.angle().second != 0)
    {
        LOG_INFO("Moving to zero...");
        if (!this->move(0))
            return IPS_ALERT;
    }
    return IPS_OK;
}


bool WandererRotatorBase::ReverseRotator(bool enabled)
{
    auto t0 = sendCommand(enabled ? "1700001" : "1700000");
    if (!t0 || !send_handshake())
        return false;
    return this->reader.reversed().second == enabled;
}


void WandererRotatorBase::TimerHit()
{
    if (isConnected())
    {
        if (this->reader.stopped())
        {
            /* send alert back to user and give up on TimerHit.  Only remedy may
             * be for user to disconnect and reconnect.
             */
            GotoRotatorNP.setState(IPS_ALERT);
            GotoRotatorNP.apply();
            return;
        } else if (this->state == State::HALTED) {
            /* Query the position of the motor and update INDI. */
            send_handshake();
            GotoRotatorNP[0].setValue(range360(this->reader.angle().second));
            GotoRotatorNP.setState(IPS_OK);
            GotoRotatorNP.apply();
        } else if (this->state == State::MOVING) {
            auto current = this->reader.angle();

            if (tracking.t0 < current.first) {
                /* We have a measurement after we started motion.  We assume
                 * that the only way this would have happened is that the motion
                 * was interrupted or stopped. */
                this->state = State::HALTED;
                GotoRotatorNP[0].setValue(range360(current.second));
                GotoRotatorNP.setState(IPS_OK);
                GotoRotatorNP.apply();
            } else {

                double m = EstimatedSpeedNP[0].getValue();
                auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now() - tracking.t0
                ).count() * 1e-9;

                auto sgn = std::copysign(1, tracking.angle_f -tracking.angle_0);
                double angle = tracking.angle_0 + sgn * m * dt;

                if (sgn < 0)
                    angle =std::clamp(angle, tracking.angle_f,tracking.angle_0);
                else
                    angle =std::clamp(angle, tracking.angle_0,tracking.angle_f);

                GotoRotatorNP[0].setValue(range360(angle));
                GotoRotatorNP.setState(IPS_BUSY);
                GotoRotatorNP.apply();
            }
        } else {
            LOG_ERROR("Invalid state should not be seen");
        }

        SetTimer(getCurrentPollingPeriod());
    }
}


std::optional<std::chrono::system_clock::time_point>
WandererRotatorBase::send_zero_cmd(bool requery)
{
    auto t0 = sendCommand("1500002");
    if (!t0)
        return {};

    if (requery && !send_handshake(true))
    {
        return {};
    }
    return t0;
}

std::optional<std::chrono::system_clock::time_point>
WandererRotatorBase::send_handshake(bool wait)
{
    typedef WandererReader::UPDATES UP;
    auto t0 = sendCommand("1500001");
    if (!t0 || (wait && !this->reader.wait_for(3s, UP::HANDSHAKE, t0)))
    {
        return {};
    }
    return t0;
}

std::optional<std::chrono::system_clock::time_point>
WandererRotatorBase::sendCommand(std::string command,
                                 const std::string & termination)
{
    int nbytes_written = 0, rc = -1;
    LOGF_DEBUG("CMD: %s", command.c_str());
    auto t0 = std::chrono::system_clock::now();
    if ((rc = tty_write_string(PortFD, (command + termination).c_str(),
                               &nbytes_written)) != TTY_OK)
    {
        char errorMessage[MAXRBUF];
        tty_error_msg(rc, errorMessage, MAXRBUF);
        LOGF_ERROR("Serial write error: %s", errorMessage);
        return {};
    }
    return t0;
}

bool WandererRotatorBase::saveConfigItems(FILE *fp)
{
    INDI::Rotator::saveConfigItems(fp);
    EstimatedSpeedNP.save(fp);
    return true;
}
