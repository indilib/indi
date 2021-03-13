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

/* Smart Widget-Property */
#include "indipropertynumber.h"
#include "indipropertyswitch.h"


class LX200Pulsar2 : public LX200Generic
{
  public:
    LX200Pulsar2();
    virtual ~LX200Pulsar2() {}

	static constexpr char const *ADVANCED_TAB = "Advanced Setup";
	static constexpr bool verboseLogging = false;
	static constexpr char Null = '\0';


    virtual const char *getDefaultName();

    virtual bool Connect();
    virtual bool Disconnect();
    virtual bool Handshake();
    virtual bool ReadScopeStatus();
    virtual void ISGetProperties(const char *dev) override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    static const unsigned int numPulsarTrackingRates = 7;

  protected:
    virtual bool SetSlewRate(int index);
    virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
    virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);
    virtual bool Abort();

    virtual IPState GuideNorth(uint32_t ms);
    virtual IPState GuideSouth(uint32_t ms);
    virtual IPState GuideEast(uint32_t ms);
    virtual IPState GuideWest(uint32_t ms);

    virtual bool updateTime(ln_date *utc, double utc_offset);
    virtual bool updateLocation(double latitude, double longitude, double elevation);

    virtual bool Goto(double, double);
    virtual bool Park();
    virtual bool Sync(double ra, double dec);
    virtual bool UnPark();

    virtual bool isSlewComplete();
    virtual bool checkConnection();

    virtual void getBasicData();


    // Pier Side
    INDI::PropertySwitch PierSideSP {2};
	INDI::PropertySwitch PierSideToggleSP {1};

    // Tracking Rates
    INDI::PropertySwitch TrackingRateIndSP {numPulsarTrackingRates};

    // Guide Speed Indicator
    INDI::PropertyNumber GuideSpeedIndNP {1};
    // Center Speed Indicator
    INDI::PropertyNumber CenterSpeedIndNP {1};
    // Find Speed Indicator
    INDI::PropertyNumber FindSpeedIndNP {1};
    // Slew Speed Indicator
    INDI::PropertyNumber SlewSpeedIndNP {1};
    // GoTo Speed Indicator
    INDI::PropertyNumber GoToSpeedIndNP {1};

	// Ramp
	INDI::PropertyNumber RampNP {2};

	// Reduction
	INDI::PropertyNumber ReductionNP {2};

	// Maingear
	INDI::PropertyNumber MaingearNP {2};

	// Backlash
	INDI::PropertyNumber BacklashNP {2};

	// Home Position
	INDI::PropertyNumber HomePositionNP {2};
	
	// SwapTubeDelay
	INDI::PropertyNumber SwapTubeDelayNP {1};

	// Mount Type
	INDI::PropertySwitch MountTypeSP {3};

    // Periodic error correction on or off
    INDI::PropertySwitch PeriodicErrorCorrectionSP {2};

    // Pole crossing on or off
    INDI::PropertySwitch PoleCrossingSP {2};

    // Refraction correction on or off
    INDI::PropertySwitch RefractionCorrectionSP {2};

	// Rotation RA
	INDI::PropertySwitch RotationRASP {2};
	// Rotation DEC
	INDI::PropertySwitch RotationDecSP {2};
	
	// User1 Rate
	INDI::PropertyNumber UserRate1NP {2};
	

	// Tracking Current
	INDI::PropertyNumber TrackingCurrentNP {1}; // only one entry for both RA and Dec
	// Stop Current
	INDI::PropertyNumber StopCurrentNP {1}; // only one entry for both RA and Dec
	// GoTo Current
	INDI::PropertyNumber GoToCurrentNP {1}; // only one entry for both RA and Dec

  private:

    bool storeScopeLocation();
    bool sendScopeTime();

    bool isSlewing();
    bool just_started_slewing;

	bool local_properties_updated = false;
	bool initialization_complete = false;  // actually completion of getBasicData
	
};
