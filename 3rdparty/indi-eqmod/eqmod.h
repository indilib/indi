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

#include "config.h"
#include "skywatcher.h"
#include "align/align.h"
#ifdef WITH_SIMULATOR
#include "simulator/simulator.h"
#endif

class EQMod : public INDI::Telescope, public INDI::GuiderInterface
{
    protected:
    private:
        Skywatcher *mount;
	Align *align;

	unsigned long currentRAEncoder, zeroRAEncoder, totalRAEncoder;
	unsigned long currentDEEncoder, zeroDEEncoder, totalDEEncoder;
	
        double currentRA, currentHA; 
        double currentDEC;
	double alignedRA, alignedDEC;
        double targetRA;
        double targetDEC;
	TelescopeStatus RememberTrackState;
        bool Parked;
	/* for use with libnova */
	struct ln_equ_posn lnradec;
	struct ln_lnlat_posn lnobserver; 
	struct ln_hrz_posn lnaltaz;
        
	int GuideTimerNS;

	int GuideTimerWE;

        INumber *GuideRateN;
        INumberVectorProperty *GuideRateNP;

	ITextVectorProperty *MountInformationTP;
	INumberVectorProperty *SteppersNP;
	INumberVectorProperty *CurrentSteppersNP;
	INumberVectorProperty *PeriodsNP;
	INumberVectorProperty *DateNP;
	ILightVectorProperty *RAStatusLP;
	ILightVectorProperty *DEStatusLP;
	INumberVectorProperty *SlewSpeedsNP;
	ISwitchVectorProperty *SlewModeSP;
	ISwitchVectorProperty *HemisphereSP;
	ISwitchVectorProperty *PierSideSP;
	ISwitchVectorProperty *TrackModeSP;
       	INumberVectorProperty *TrackRatesNP;
	//ISwitchVectorProperty *AbortMotionSP;
	INumberVectorProperty *HorizontalCoordsNP;

	enum Hemisphere {NORTH=0, SOUTH=1 };
	enum PierSide {WEST=0, EAST=1};
	typedef struct GotoParams {
	  double ratarget, detarget, racurrent, decurrent;
	  unsigned long ratargetencoder, detargetencoder, racurrentencoder, decurrentencoder;
          unsigned long limiteast, limitwest;
	  unsigned int iterative_count;
	  bool forcecwup, checklimits, outsidelimits, completed;
	} GotoParams;
	typedef struct SyncData {
	  double lst,jd;
	  double targetRA, targetDEC;
	  double telescopeRA, telescopeDEC;
	  double deltaRA, deltaDEC;
	} SyncData;

	Hemisphere Hemisphere;
	PierSide pierside;
	bool RAInverted, DEInverted;
	bool isParked;
        GotoParams gotoparams;
	SyncData syncdata;

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
	static void timedguideNSCallback(void *userpointer);
	static void timedguideWECallback(void *userpointer);
	double GetRASlew();
	double GetDESlew();

	void setLogDebug (bool enable);
	void setStepperSimulation (bool enable);
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
        virtual void ISGetProperties (const char *dev);
        virtual bool updateProperties();
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
	virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

        virtual bool MoveNS(TelescopeMotionNS dir);
        virtual bool MoveWE(TelescopeMotionWE dir);
	virtual bool Abort();

	virtual bool GuideNorth(float ms);
	virtual bool GuideSouth(float ms);
	virtual bool GuideEast(float ms);
	virtual bool GuideWest(float ms);


        bool Goto(double ra,double dec);
        bool Park();
	bool Sync(double ra,double dec);
        virtual bool canSync();
        virtual bool canPark();

	double getLongitude();
	double getLatitude();
#ifdef WITH_SIMULATOR
	EQModSimulator *simulator;
#endif
};

#endif // EQMOD_H
