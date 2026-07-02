/*******************************************************************************
  Copyright(c) 2025 Frank Wang & Jérémie Klein. All rights reserved.

  WandererRotator Base

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

#pragma once

#include "defaultdevice.h"
#include "indirotator.h"
#include "indirotatorinterface.h"
#include "indipropertyswitch.h"
#include <condition_variable>
#include <optional>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <mutex>
#include <regex>
#include <map>

class WandererRotatorBase;

/** Simple class to continuously read and parse the serial stream from the
 * rotator (in a separate thread). */
class WandererReader {
public:
    static const unsigned int MAX_FAILURES = 10;
    struct UPDATES {
        enum VALUES {
            FIRMWARE  = 1,
            ANGLE     = 2,
            BACKLASH  = 4,
            REVERSED  = 8,
            HANDSHAKE = FIRMWARE | ANGLE | BACKLASH | REVERSED,
            COMPLETED = 16,
            VOLTERR   = 32,
            ANY       = 0,
            ALL       = FIRMWARE | ANGLE | BACKLASH | REVERSED | COMPLETED
                      | VOLTERR,
        };
    };

    WandererReader(WandererRotatorBase * wanderer) : wanderer(wanderer) { }
    ~WandererReader();

    /** Initialize reader patterns.
     * Call to this should be delayed to something like
     * Wanderer...::initProperties to allow for wanderer vptr to be correct
     * first.
     */
    void init();

    bool start(int fd);
    void stop();
    bool stopped() { return this->did_stop; }
    const char * getDeviceName() const;

    auto firmware() {
        std::lock_guard<std::recursive_mutex> lock(this->mutex);
        return this->_firmware;
    }
    auto angle() {
        std::lock_guard<std::recursive_mutex> lock(this->mutex);
        return this->_angle;
    }
    auto backlash() {
        std::lock_guard<std::recursive_mutex> lock(this->mutex);
        return this->_backlash;
    }
    auto reversed() {
        std::lock_guard<std::recursive_mutex> lock(this->mutex);
        return this->_reversed;
    }
    auto completed() {
        std::lock_guard<std::recursive_mutex> lock(this->mutex);
        return this->_completed;
    }
    auto voltage_error() {
        std::lock_guard<std::recursive_mutex> lock(this->mutex);
        return this->_voltage_error;
    }
    bool pop_voltage_error() {
        std::lock_guard<std::recursive_mutex> lock(this->mutex);
        bool retval = this->_voltage_error.second;
        this->_voltage_error.second = false;
        return retval;
    }

    /** Check if *all* updates requested have occurred since the given time.
     * If `updates` is given as zero, then this function returns if *any* item
     * was updated since the given time.
     */
    bool updated_since(const unsigned int updates,
                       const std::chrono::system_clock::time_point &);

    void wait(const unsigned int updates = UPDATES::ANY,
              std::optional<std::chrono::system_clock::time_point> t0 = {}) {
        if (!t0)
            t0 = {std::chrono::system_clock::now()};
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        this->cv.wait(lock, [this, updates, t0](){
            return this->updated_since(updates, t0.value());
        });
    }

    template< class Rep, class Period >
    bool wait_for(const std::chrono::duration<Rep, Period> & dt,
                  const unsigned int updates = UPDATES::ANY,
                  std::optional<std::chrono::system_clock::time_point> t0 = {}) {
        if (!t0)
            t0 = {std::chrono::system_clock::now()};
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        return this->cv.wait_for(lock, dt, [this, t0, updates](){
            return this->updated_since(updates, t0.value());
        });
    }

protected:
    /** Pointer back to parent rotator instance. */
    WandererRotatorBase * wanderer;

    /** Map of regex expressions for various message types. */
    std::map<std::string, std::regex> patterns;

    /** Firmware taken from last valid handshake. */
    std::pair<std::chrono::system_clock::time_point, int> _firmware;

    /** Mechanical angle from last relevant message. */
    std::pair<std::chrono::system_clock::time_point, double> _angle;

    /** Backlash angle from last valid handshake. */
    std::pair<std::chrono::system_clock::time_point, float> _backlash;

    /** Reversed from last valid handshake. */
    std::pair<std::chrono::system_clock::time_point, bool> _reversed;

    /** Completed angle, in degrees from last relevant message. */
    std::pair<std::chrono::system_clock::time_point, float> _completed;

    /** Voltage error.  The single-page "datasheet" indicates that this is
     * asserted when the power voltage goes below 11V.
     */
    std::pair<std::chrono::system_clock::time_point, bool> _voltage_error;

    /** Simple mutux to keep things thread-safe. */
    std::recursive_mutex mutex;
    std::condition_variable_any cv;
    std::thread read_thread;
    std::atomic<bool> stop_requested = true;
    std::atomic<bool> did_stop = true;
    std::atomic<unsigned int> timeouts = 0;
    std::atomic<unsigned int> failures = 0;
};

class WandererRotatorBase : public INDI::Rotator
{
public:
    WandererRotatorBase();

    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewSwitch(const char *dev, const char *name,
                             ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name,
                             double values[], char *names[], int n) override;

protected:
    virtual const char * getDefaultName() override = 0;
    virtual const char * getRotatorHandshakeName() = 0;
    virtual int getMinimumCompatibleFirmwareVersion() = 0;
    virtual int getStepsPerDegree() = 0;

    virtual IPState MoveRotator(double angle) override;
    virtual IPState HomeRotator() override;
    virtual bool ReverseRotator(bool enabled) override;
    virtual bool AbortRotator() override;
    virtual void TimerHit() override;
    virtual bool Handshake() override;
    virtual bool Disconnect() override;
    virtual bool saveConfigItems(FILE *fp) override;

    /** Send command to zero virtual origin for mechanical angle.
     * @return system_clock time if successful and empty optional otherwise.
     */
    std::optional<std::chrono::system_clock::time_point>
    send_zero_cmd(bool requery=true);

    /** Send basic handshake command and optionally (default yes) wait for a
     * response.
     * @return system_clock time if successful and empty optional otherwise.
     */
    std::optional<std::chrono::system_clock::time_point>
    send_handshake(bool wait=true);
    std::optional<std::chrono::system_clock::time_point> stop();
    std::optional<std::chrono::system_clock::time_point> move(double abs_angle);

    /** Send a serial command to the rotator.
     * @return If successful, returns system_clock time immediately before the
     *         serial command.
     *         If not successful, returns an empty std::optional<...>
     */
    std::optional<std::chrono::system_clock::time_point>
    sendCommand(std::string command, const std::string & termination="\n");

    INDI::PropertySwitch SetZeroSP{1};
    INDI::PropertyNumber BacklashNP{1};
    /** Estimated speed.  Used to predict where the rotator is because the
     * Wanderer firmware has no way of querying the current position without
     * halting the motion.
     */
    INDI::PropertyNumber EstimatedSpeedNP{1};

    WandererReader reader;

    /** information for estimating position tracking. */
    struct {
        std::chrono::system_clock::time_point t0;
        double angle_0 = 0;
        double angle_f = 0;
    } tracking;

    /** States for a software state machine for the wanderer. */
    enum class State
    {
        MOVING,
        HALTED,
    } state;

    friend WandererReader;
};
