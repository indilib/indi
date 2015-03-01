/* Copyright 2012 Geehalel (geehalel AT gmail DOT com) */
/* This file is part of the Skywatcher Protocol INDI driver.

    The Skywatcher Protocol INDI driver is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The Skywatcher Protocol INDI driver is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Skywatcher Protocol INDI driver.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EQMOD_H
#define EQMOD_H

#include <inditelescope.h>
#include <indiguiderinterface.h>
#include <libnova.h>
#include <indicontroller.h>

#include "config.h"
#include "skywatcher.h"
#include "align/align.h"
#ifdef WITH_SIMULATOR
#include "simulator/simulator.h"
#endif
#ifdef WITH_SCOPE_LIMITS
#include "scope-limits/scope-limits.h"
#endif

	typedef struct SyncData {
	  double lst,jd;
	  double targetRA, targetDEC;
	  double telescopeRA, telescopeDEC;
	  double deltaRA, deltaDEC;
	  unsigned long targetRAEncoder, targetDECEncoder;
	  unsigned long telescopeRAEncoder, telescopeDECEncoder;
	  long deltaRAEncoder, deltaDECEncoder;
	} SyncData;

class EQMod : public INDI::Telescope, public INDI::GuiderInterface
{
    protected:
    private:
    Skywatcher *mount;
	Align *align;

	unsigned long currentRAEncoder, zeroRAEncoder, totalRAEncoder;
	unsigned long currentDEEncoder, zeroDEEncoder, totalDEEncoder;

	unsigned long homeRAEncoder, parkRAEncoder;
	unsigned long homeDEEncoder, parkDEEncoder;	

    double currentRA, currentHA;
    double currentDEC;
	double alignedRA, alignedDEC;
    double targetRA;
    double targetDEC;
    TelescopeStatus RememberTrackState;
    bool Parked;
    int last_motion_ns;
    int last_motion_ew;
	
	/* for use with libnova */
	struct ln_equ_posn lnradec;
	struct ln_lnlat_posn lnobserver; 
	struct ln_hrz_posn lnaltaz;
        
	/* Time variables */
	struct tm utc;
	struct ln_date lndate;
	struct timeval lasttimeupdate;
	struct timespec lastclockupdate;
	double juliandate;

	int GuideTimerNS;

	int GuideTimerWE;

    INumber *GuideRateN;
    INumberVectorProperty *GuideRateNP;

	ITextVectorProperty *MountInformationTP;
	INumberVectorProperty *SteppersNP;
	INumberVectorProperty *CurrentSteppersNP;
	INumberVectorProperty *PeriodsNP;
	INumberVectorProperty *JulianNP;
	INumberVectorProperty *TimeLSTNP;
	ITextVectorProperty *TimeUTCTP;
	ILightVectorProperty *RAStatusLP;
	ILightVectorProperty *DEStatusLP;
	INumberVectorProperty *SlewSpeedsNP;
	ISwitchVectorProperty *SlewModeSP;
	ISwitchVectorProperty *HemisphereSP;
	ISwitchVectorProperty *PierSideSP;
	ISwitchVectorProperty *TrackModeSP;
	ISwitchVectorProperty *TrackDefaultSP;
       	INumberVectorProperty *TrackRatesNP;
	//ISwitchVectorProperty *AbortMotionSP;
	INumberVectorProperty *HorizontalCoordNP;
	INumberVectorProperty *StandardSyncNP;
	INumberVectorProperty *StandardSyncPointNP;
	INumberVectorProperty *SyncPolarAlignNP;
	ISwitchVectorProperty *SyncManageSP; 
  	INumberVectorProperty *ParkPositionNP;
	ISwitchVectorProperty *ParkOptionSP;
    ISwitchVectorProperty *ReverseDECSP;
  	INumberVectorProperty *BacklashNP;
	ISwitchVectorProperty *UseBacklashSP;

	enum Hemisphere {NORTH=0, SOUTH=1 };
	enum PierSide {WEST=0, EAST=1};
	typedef struct GotoParams {
	  double ratarget, detarget, racurrent, decurrent;
	  unsigned long ratargetencoder, detargetencoder, racurrentencoder, decurrentencoder;
          unsigned long limiteast, limitwest;
	  unsigned int iterative_count;
	  bool forcecwup, checklimits, outsidelimits, completed;
	} GotoParams;

	Hemisphere Hemisphere;
	PierSide pierside;
	bool RAInverted, DEInverted;
        GotoParams gotoparams;
	SyncData syncdata, syncdata2;

	double tpa_alt, tpa_az;

    INDI::Controller *controller;

	void EncodersToRADec(unsigned long rastep, unsigned long destep, double lst, double *ra, double *de, double *ha);
	double EncoderToHours(unsigned long destep, unsigned long initdestep, unsigned long totalrastep, enum Hemisphere h);
	double EncoderToDegrees(unsigned long destep, unsigned long initdestep, unsigned long totalrastep, enum Hemisphere h);
	double EncoderFromHour(double hour, unsigned long initstep, unsigned long totalstep, enum Hemisphere h);
	double EncoderFromRA(double ratarget, double detarget, double lst, 
			     unsigned long initstep, unsigned long totalstep, enum Hemisphere h);
	double EncoderFromDegree(double degree, PierSide p, unsigned long initstep, unsigned long totalstep, enum Hemisphere h);
	double EncoderFromDec( double detarget, PierSide p, 
				      unsigned long initstep, unsigned long totalstep, enum Hemisphere h);
	void EncoderTarget(GotoParams *g);
	double rangeHA(double r);
	double range24(double r);
	double range360(double r);
	double rangeDec(double r);
	void SetSouthernHemisphere(bool southern);
	PierSide SideOfPier(double ha);
	double GetRATrackRate();
	double GetDETrackRate();
	double GetDefaultRATrackRate();
	double GetDefaultDETrackRate();
	static void timedguideNSCallback(void *userpointer);
	static void timedguideWECallback(void *userpointer);
	double GetRASlew();
	double GetDESlew();
	bool gotoInProgress();

    bool loadProperties();
	void setStepperSimulation (bool enable);

    void processNSWE(double mag, double angle);
    void processSlewPresets(double mag, double angle);
    void processJoystick(const char * joystick_n, double mag, double angle);
    void processButton(const char * button_n, ISState state);

	void computePolarAlign(SyncData s1, SyncData s2, double lat, double *tpaalt, double *tpaaz);
	void starPolarAlign(double lst, double ra, double dec, double theta, double gamma, double *tra, double *tdec);     

    public:
        EQMod();
        virtual ~EQMod();

        virtual const char *getDefaultName();
        virtual bool Connect();
        virtual bool Connect(char *);
        virtual bool Disconnect();
        virtual void TimerHit();
        virtual bool ReadScopeStatus();
        virtual bool initProperties();
        virtual bool updateProperties();
        virtual void ISGetProperties(const char *dev);
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool ISSnoopDevice(XMLEle *root);
        virtual bool saveConfigItems(FILE *fp);

        virtual bool MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand command);
        virtual bool MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand command);
        virtual bool Abort();

        virtual bool GuideNorth(float ms);
        virtual bool GuideSouth(float ms);
        virtual bool GuideEast(float ms);
        virtual bool GuideWest(float ms);


        bool Goto(double ra,double dec);
        bool Park();
        bool UnPark();
        bool Sync(double ra,double dec);

        bool updateTime(ln_date *lndate_utc, double utc_offset);
        bool updateLocation(double latitude, double longitude, double elevation);

        double getLongitude();
        double getLatitude();
        double getJulianDate();
        double getLst(double jd, double lng);

        static void joystickHelper(const char * joystick_n, double mag, double angle, void *context);
        static void buttonHelper(const char * button_n, ISState state, void *context);

#ifdef WITH_SIMULATOR
	EQModSimulator *simulator;
#endif

#ifdef WITH_SCOPE_LIMITS
	HorizonLimits *horizon;
#endif

};

#endif // EQMOD_H
