/*
    Pulsar 2 INDI driver

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

#pragma once

#include "lx200generic.h"


class LX200Pulsar2 : public LX200Generic
{
  public:
    LX200Pulsar2();
    virtual ~LX200Pulsar2() {}

	static constexpr char const *ADVANCED_TAB = "Advanced Setup";
	static constexpr bool verboseLogging = false;
	static constexpr char Null = '\0';


    virtual const char *getDefaultName() override;

    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual bool Handshake() override;
    virtual bool ReadScopeStatus() override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    static const unsigned int numPulsarTrackingRates = 7;

  protected:
    virtual bool SetSlewRate(int index) override;
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
    virtual bool Abort() override;

    virtual IPState GuideNorth(uint32_t ms) override;
    virtual IPState GuideSouth(uint32_t ms) override;
    virtual IPState GuideEast(uint32_t ms) override;
    virtual IPState GuideWest(uint32_t ms) override;

    virtual bool updateTime(ln_date *utc, double utc_offset) override;
    virtual bool updateLocation(double latitude, double longitude, double elevation) override;

    virtual bool Goto(double, double) override;
    virtual bool Park() override;
    virtual bool Sync(double ra, double dec) override;
    virtual bool UnPark() override;

    virtual bool isSlewComplete() override;
    virtual bool checkConnection() override;

    virtual void getBasicData() override;


    // Pier Side
    ISwitch PierSideS[2];
    ISwitchVectorProperty PierSideSP;
	ISwitch PierSideToggleS[1];
	ISwitchVectorProperty PierSideToggleSP;

    // Tracking Rates
    ISwitchVectorProperty TrackingRateIndSP;
    ISwitch TrackingRateIndS[numPulsarTrackingRates];

    // Guide Speed Indicator
    INumberVectorProperty GuideSpeedIndNP;
    INumber GuideSpeedIndN[1];
    // Center Speed Indicator
    INumberVectorProperty CenterSpeedIndNP;
    INumber CenterSpeedIndN[1];
    // Find Speed Indicator
    INumberVectorProperty FindSpeedIndNP;
    INumber FindSpeedIndN[1];
    // Slew Speed Indicator
    INumberVectorProperty SlewSpeedIndNP;
    INumber SlewSpeedIndN[1];
    // GoTo Speed Indicator
    INumberVectorProperty GoToSpeedIndNP;
    INumber GoToSpeedIndN[1];

	// Ramp
	INumberVectorProperty RampNP;
	INumber RampN[2];

	// Reduction
	INumberVectorProperty ReductionNP;
	INumber ReductionN[2];

	// Maingear
	INumberVectorProperty MaingearNP;
	INumber MaingearN[2];

	// Backlash
	INumberVectorProperty BacklashNP;
	INumber BacklashN[2];

	// Home Position
	INumberVectorProperty HomePositionNP;
	INumber HomePositionN[2];
	
	// SwapTubeDelay
	INumberVectorProperty SwapTubeDelayNP;
	INumber SwapTubeDelayN[1];

	// Mount Type
	ISwitchVectorProperty MountTypeSP;
	ISwitch MountTypeS[3];

    // Periodic error correction on or off
    ISwitchVectorProperty PeriodicErrorCorrectionSP;
    ISwitch PeriodicErrorCorrectionS[2];

    // Pole crossing on or off
    ISwitchVectorProperty PoleCrossingSP;
    ISwitch PoleCrossingS[2];

    // Refraction correction on or off
    ISwitchVectorProperty RefractionCorrectionSP;
    ISwitch RefractionCorrectionS[2];

	// Rotation RA
	ISwitchVectorProperty RotationRASP;
	ISwitch RotationRAS[2];
	// Rotation DEC
	ISwitchVectorProperty RotationDecSP;
	ISwitch RotationDecS[2];
	
	// User1 Rate
	INumberVectorProperty UserRate1NP;
	INumber UserRate1N[2];
	

	// Tracking Current
	INumberVectorProperty TrackingCurrentNP;
	INumber TrackingCurrentN[1]; // only one entry for both RA and Dec
	// Stop Current
	INumberVectorProperty StopCurrentNP;
	INumber StopCurrentN[1]; // only one entry for both RA and Dec
	// GoTo Current
	INumberVectorProperty GoToCurrentNP;
	INumber GoToCurrentN[1]; // only one entry for both RA and Dec

  private:

    bool storeScopeLocation();
    virtual bool sendScopeTime() override;

    bool isSlewing();
    bool just_started_slewing;

	bool local_properties_updated = false;
	bool initialization_complete = false;  // actually completion of getBasicData
	
};
