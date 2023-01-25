/*
    Losmandy Gemini INDI driver

    Copyright (C) 2017 Jasem Mutlaq
    Copyright (C) 2018 Eric Vickery

    Difference from LX200 Generic:

    1. Added Side of Pier
    2. Reimplemented isSlewComplete to use :Gv# since it is more reliable

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

#include "lx200generic.h"

class LX200Gemini : public LX200Generic
{
    public:
        LX200Gemini();
        ~LX200Gemini() override = default;

        virtual void ISGetProperties(const char *dev) override;
    
        /* Return True if any property was successfully processed, false otherwise.*/
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char **texts, char **names, int n) override;

    protected:
        virtual const char *getDefaultName() override;

        virtual bool Connect() override;

        virtual bool initProperties() override ;
        virtual bool updateProperties() override;

        virtual bool isSlewComplete() override;
        virtual bool ReadScopeStatus() override;

        virtual bool Park()override ;
        virtual bool UnPark() override;

        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackEnabled(bool enabled) override;

        virtual bool checkConnection() override;

        virtual bool saveConfigItems(FILE *fp) override;

          // Guide Pulse Commands
        virtual int SendPulseCmd(int8_t direction, uint32_t duration_msec) override;

    private:
        void syncState();
        void syncSideOfPier();
        bool sleepMount();
        bool wakeupMount();

        bool getGeminiProperty(uint32_t propertyNumber, char* value);
        bool setGeminiProperty(uint32_t propertyNumber, char* value);

        // Checksum for private commands
        uint8_t calculateChecksum(char *cmd);

        INumber ManualSlewingSpeedN[1];
        INumberVectorProperty ManualSlewingSpeedNP;

        INumber GotoSlewingSpeedN[1];
        INumberVectorProperty GotoSlewingSpeedNP;

        INumber MoveSpeedN[1];
        INumberVectorProperty MoveSpeedNP;

        INumber GuidingSpeedBothN[1];
        INumberVectorProperty GuidingSpeedBothNP;

        INumber GuidingSpeedN[2];
        INumberVectorProperty GuidingSpeedNP;

        INumber CenteringSpeedN[1];
        INumberVectorProperty CenteringSpeedNP;

        ISwitch ParkSettingsS[3];
        ISwitchVectorProperty ParkSettingsSP;

        ITextVectorProperty PECCounterTP;
        IText PECCounterT[1];

        ISwitchVectorProperty PECControlSP;
        ISwitch PECControlS[2];

        ITextVectorProperty PECStateTP;
        IText PECStateT[6] {};
  
        INumber PECMaxStepsN[1];
        INumberVectorProperty PECMaxStepsNP;
  
        ISwitch PECEnableAtBootS[1];
        ISwitchVectorProperty PECEnableAtBootSP;

        INumber PECGuidingSpeedN[1];
        INumberVectorProperty PECGuidingSpeedNP;

        ISwitch ServoPrecisionS[2];
        ISwitchVectorProperty ServoPrecisionSP;

        ISwitch FlipControlS[2];
        ISwitchVectorProperty FlipControlSP;

        INumber FlipPositionN[2];
        INumberVectorProperty FlipPositionNP;

        ITextVectorProperty VersionTP;
        IText VersionT[5] {};

        float gemini_software_level_;

        enum
        {
	 FIRMWARE_DATE,
	 FIRMWARE_TIME,
	 FIRMWARE_LEVEL,
	 FIRMWARE_NAME
	};
  
        enum
        {
            PARK_HOME,
            PARK_STARTUP,
            PARK_ZENITH
        };

        enum
        {
            GUIDING_BOTH,
        };

        enum
        {
            GUIDING_WE,
            GUIDING_NS
        };

        enum
        {
            PEC_START_TRAINING,
            PEC_ABORT_TRAINING
        };

        enum
	{
	    PEC_STATUS_ACTIVE,
	    PEC_STATUS_FRESH_TRAINED,
	    PEC_STATUS_TRAINING_IN_PROGRESS,
	    PEC_STATUS_TRAINING_COMPLETED,
	    PEC_STATUS_WILL_TRAIN,
	    PEC_STATUS_DATA_AVAILABLE
	};

        enum
        {
            SERVO_RA,
            SERVO_DEC,
        };

        ISwitch StartupModeS[3];
        ISwitchVectorProperty StartupModeSP;
        enum
        {
            COLD_START,
            WARM_START,
            WARM_RESTART
        };

        enum
        {
            GEMINI_TRACK_SIDEREAL,
            GEMINI_TRACK_KING,
            GEMINI_TRACK_LUNAR,
            GEMINI_TRACK_SOLAR

        };

        enum MovementState
        {
            NO_MOVEMENT,
            TRACKING,
            GUIDING,
            CENTERING,
            SLEWING,
            STALLED
        };

        enum ParkingState
        {
            NOT_PARKED,
            PARKED,
            PARK_IN_PROGRESS
        };

        enum FlipPointState
	{
	    FLIP_DISABLED,
	    FLIP_EAST,
	    FLIP_WEST
	};

        enum ServoPrecisionState
	{
	    PRECISION_DISABLED,
	    RA_PRECISION_ENABLED,
	    DEC_PRECISION_ENABLED
	};

        enum FlipPointControl
	{
	    FLIP_EAST_CONTROL,
	    FLIP_WEST_CONTROL
	};

        enum FlipPointValue
	{
	    FLIP_EAST_VALUE,
	    FLIP_WEST_VALUE
	};

        const uint8_t GEMINI_TIMEOUT = 3;

        void setTrackState(INDI::Telescope::TelescopeStatus state);
        void updateParkingState();
        void updateMovementState();
        MovementState getMovementState();
        ParkingState getParkingState();

        ParkingState priorParkingState = PARK_IN_PROGRESS;
        bool m_isSleeping { false };

};
