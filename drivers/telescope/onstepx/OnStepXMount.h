/*
    OnStepX INDI Driver — Mount device class (indi_onstepx)
    Copyright (C) 2026 Ed Lee

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

    Top-level device class for the mount binary.  Composes helper classes
    for each subsystem: OnStepXAlignment, OnStepXCore,
    OnStepXLimits, OnStepXMeridian, OnStepXPec, OnStepXSite, OnStepXTracking,
    and OnStepXWeather.  Mixes in GuiderInterface and WeatherInterface.
    Focusers, the rotator and the aux outputs/USB ports are child devices
    (OnStepXFocuser, OnStepXRotatorDevice, OnStepXPowerDevice).  All protocol commands are implemented in the
    individual helper classes.

    Pointing: OnStepX applies its own pointing model for every mount type
    (including AltAz and AltAlt) and :GR#/:GD# always return model-corrected
    RA/Dec, so the INDI alignment subsystem is deliberately not used.
*/

#pragma once

#include "OnStepXAlignment.h"
#include "OnStepXModelBuilder.h"
#include "OnStepXCore.h"
#include "OnStepXDriverStatus.h"
#include "OnStepXFocuser.h"
#include "OnStepXGuide.h"
#include "OnStepXInfo.h"
#include "OnStepXLimits.h"
#include "OnStepXMeridian.h"
#include "OnStepXPec.h"
#include "OnStepXRotatorDevice.h"
#include "OnStepXSite.h"
#include "OnStepXStatus.h"
#include "OnStepXTracking.h"
#include "OnStepXWeather.h"
#include "OnStepXPowerDevice.h"

#include <array>
#include <memory>

#include <inditelescope.h>
#include <indiguiderinterface.h>
#include <indiweatherinterface.h>


class OnStepXMount : public INDI::Telescope,
    public INDI::GuiderInterface,
    public INDI::WeatherInterface
{
    public:
        OnStepXMount();

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool saveConfigItems(FILE *fp) override;

    protected:
        virtual bool Handshake() override;
        virtual bool Disconnect() override;
        virtual bool ReadScopeStatus() override;
        // Disconnects when the controller has stopped replying (see OnStepXComm::linkLost()).
        virtual void TimerHit() override;
        virtual bool Goto(double ra, double dec) override;
        virtual bool Sync(double ra, double dec) override;
        virtual bool Flip(double ra, double dec) override;
        virtual bool Abort() override;
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        virtual bool SetTrackEnabled(bool enabled) override;
        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackRate(double raRate, double deRate) override;
        virtual bool SetSlewRate(int index) override;
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool updateTime(ln_date *utc, double utc_offset) override;
        virtual IPState ExecuteHomeAction(TelescopeHomeAction action) override;

        // GuiderInterface
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

    protected:
        // WeatherInterface — called by WI::checkWeatherUpdate() every polling cycle
        virtual IPState updateWeather() override;

    private:
        // Status refresh — sends :Gu# (preferred) or :GU# (fallback)
        bool refreshMountStatus();

        // Track mode mirrored from the firmware (base rate, or Custom when it has
        // RA/Dec rate offsets).
        static int trackModeFor(OnStepXTracking::BaseRate rate);
        void showTrackMode(int mode);
        void syncTrackModeFromFirmware();

        // ReadScopeStatus delegates
        void logPollFailure(const char *msg);
        bool updateCoordinates();
        void updateTrackingState(const MountStatus &s);
        void updateHomeState(const MountStatus &s);
        void createFocusers();

        // Throttled subsystem updaters
        void updateFocuserStates();
        void updateRotatorState();
        void updateWeatherDirect();
        void updatePowerStates();
        void updateAlignmentStatus();
        void updatePecStatus();


        OnStepXAlignment   m_alignment;
        OnStepXModelBuilder m_modelBuilder;
        OnStepXCore        m_core;
        OnStepXGuide       m_guide;
        OnStepXInfo        m_info;
        OnStepXLimits      m_limits;
        OnStepXMeridian    m_meridian;
        OnStepXPec         m_pec;
        OnStepXSite        m_site;
        OnStepXTracking    m_tracking;
        OnStepXWeather     m_weather;
        OnStepXDriverStatus m_driverStatus;

        // Focuser child devices — index slot-1, created for each slot in
        // Capabilities::focuserMask.  Kept across reconnects; disconnected
        // (not destroyed) when the mount disconnects.
        OnStepXFocuser::Children m_focusers;
        std::unique_ptr<OnStepXRotatorDevice> m_rotatorDev;   // "<name> Rotator", when present
        std::unique_ptr<OnStepXPowerDevice>   m_powerDev;     // "<name> Power", when present
        void disconnectFocusers();

        MountStatus    m_status;

        // Poll throttle — counts ReadScopeStatus calls
        int  m_pollCount { 0 };

        // WeatherInterface parameters must be added exactly once.
        bool m_weatherParamsAdded { false };

        // Set once the firmware reports homing after a Find Home request, so
        // the HOME switch can be completed when homing ends.
        bool m_homingSeen { false };

        // Apply TrackRateNP as OnStepX RA/Dec rate offsets (custom tracking).
        bool applyCustomTrackRate(double raRate, double deRate);
        void reapplyCustomTrackRate();

        // Send :Sr#/:Sd#; fills raStr/decStr (len bytes each) for logging.
        bool setTarget(double ra, double dec, const char *op,
                       char *raStr, char *decStr, size_t len);

};
