/*******************************************************************************
  Copyright(c) 2026 Makoto Kasahara All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file LICENSE.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include <indicom.h>
#include <inditelescope.h>

class MXHDSerial;

class MXHDTelescope : public INDI::Telescope
{
public:
    MXHDTelescope();
    ~MXHDTelescope() override = default;

    const char *getDefaultName() override;

    bool initProperties() override;
    bool updateProperties() override;

    bool Handshake() override;

    bool Connect() override;
    bool Disconnect() override;

    bool updateLocation(double latitude, double longitude, double elevation) override;
    bool updateTime(ln_date *utc, double utc_offset) override;

    // Mount control
    bool Goto(double ra, double dec) override;
    bool Sync(double ra, double dec) override;
    bool Abort() override;

    bool Park() override;
    bool UnPark() override;

    bool SetSlewRate(int index) override;
    bool SetTrackMode(uint8_t mode) override;
    bool SetTrackEnabled(bool enabled) override;

    // Manual motion (NSEW buttons)
    bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;

    // Timed guide pulses (for PHD2 and other guider clients)
    IPState GuideNorth(uint32_t ms);
    IPState GuideSouth(uint32_t ms);
    IPState GuideEast(uint32_t ms);
    IPState GuideWest(uint32_t ms);

    // INDI switch handler for tracking rate selection.
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

protected:
    void TimerHit() override;
    bool ReadScopeStatus() override;

private:
    // Protocol helpers
    bool mxhdQueryRaDec(double &raHours, double &decDeg);
    bool mxhdSetTargetRaDec(double raHours, double decDeg);
    bool mxhdApplyCachedSiteTime();
    bool mxhdApplySiteCoordinates(double latitudeDeg, double longitudeDeg);
    bool mxhdApplyTime(ln_date *utc, double utcOffsetHoursEast);
    bool mxhdSendUtcOffset(double utcOffsetHoursEast, int timeoutMs = 10000);
    bool mxhdSendLocalDateTime(ln_date *utc, double utcOffsetHoursEast, int timeoutMs = 10000);
    void mxhdUpdatePierSide(double raHours);

    bool mxhdSend(const std::string &cmd);
    bool mxhdQueryHash(const std::string &cmd, std::string &resp);
    bool mxhdQueryAck(const std::string &cmd, char &ack, int timeoutMs = 2000);
    bool mxhdReadStatus(uint8_t &b1, uint8_t &b2, uint8_t &b3);

    // Tracking rate commands (MX-HD specific)
    enum class TrackRateMode
    {
        Sidereal = 0,
        Solar,
        Lunar
    };

    bool mxhdSetTrackRate(TrackRateMode mode);

    // Status decode
    void decodeStatus(uint8_t b1, uint8_t b2, uint8_t b3);

    // Abort state machine (ported conceptually from ASCOM driver)
    void maybeAdvanceAbortFinalize();
    void startAbortFinalize(uint32_t delaySeconds);

    // Pulse-guide helpers
    enum class GuideAxis
    {
        RA = 0,
        DEC
    };

    IPState startTimedGuidePulse(GuideAxis axis, int direction, uint32_t ms, const char *moveCmd);
    void completeTimedGuidePulse(GuideAxis axis, uint64_t token, bool sendStop);
    void setGuidePropertyIdle(GuideAxis axis, IPState state);

private:
    MXHDSerial *m_io {nullptr};
    std::mutex m_ioMutex;
    std::mutex m_siteTimeMutex;

    // Cached status
    uint8_t m_st1 {0}, m_st2 {0}, m_st3 {0};
    bool m_isHoming {false};
    bool m_isSlewing {false};
    double m_voltageV {0.0};
    bool m_parkCommandActive {false};
    bool m_unparkCommandActive {false};
    bool m_homeCommandActive {false};

    // Tracking state
    bool m_trackingEnabled {true};
    TrackRateMode m_trackRateMode {TrackRateMode::Sidereal};

    // Cached site/time information received from INDI clients.
    bool m_hasCachedSiteCoordinates {false};
    bool m_hasCachedTime {false};
    double m_cachedSiteLatitudeDeg {0.0};
    double m_cachedSiteLongitudeDeg {0.0};
    ln_date m_cachedUtcDate {};
    double m_cachedUtcOffsetHoursEast {0.0};
    bool m_hasPierSideCache {false};
    bool m_pierSideEast {false};

    // Abort finalization scheduling
    bool m_abortFinalizeActive {false};
    time_t m_abortRequestedAt {0};
    uint32_t m_abortDelaySeconds {0};
    bool m_abortSawHomeCleared {false};
    time_t m_abortHomeClearedAt {0};

    // Pulse-guide state
    std::mutex m_guideMutex;
    std::atomic<uint64_t> m_guideTokenGen {0};
    bool m_guideActive {false};
    GuideAxis m_guideAxis {GuideAxis::RA};
    int m_guideDirection {0};

    // Properties
    ISwitch HomeS[1];
    ISwitchVectorProperty HomeSP;

    INumber GuideNSN[2];
    INumberVectorProperty GuideNSNP;

    INumber GuideWEN[2];
    INumberVectorProperty GuideWENP;
};
