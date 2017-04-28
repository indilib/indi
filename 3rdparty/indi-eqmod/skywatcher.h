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

#ifndef SKYWATCHER_H
#define SKYWATCHER_H

#include <sys/time.h>
#include <time.h>
#include <inditelescope.h>
#include <lilxml.h>

#include "eqmoderror.h"
//#include "eqmod.h"
class EQMod; // TODO

#include "simulator/simulator.h"

#define SKYWATCHER_MAX_CMD        16
#define SKYWATCHER_MAX_TRIES      3
#define SKYWATCHER_ERROR_BUFFER   1024

#define SKYWATCHER_SIDEREAL_DAY 86164.09053083288
#define SKYWATCHER_SIDEREAL_SPEED 15.04106864
#define SKYWATCHER_STELLAR_DAY 86164.098903691
#define SKYWATCHER_STELLAR_SPEED 15.041067179

#define SKYWATCHER_LOWSPEED_RATE 128
#define SKYWATCHER_MAXREFRESH 0.5

#define SKYWATCHER_BACKLASH_SPEED_RA 64
#define SKYWATCHER_BACKLASH_SPEED_DE 64

#define HEX(c) (((c) < 'A')?((c)-'0'):((c) - 'A') + 10)

class Skywatcher 
{

public:
    Skywatcher(EQMod *t);
    ~Skywatcher();
   
    bool Handshake() throw (EQModError);
    bool Disconnect() throw (EQModError);
    void setDebug (bool enable);
    const char *getDeviceName ();

    bool HasHomeIndexers();
    bool HasAuxEncoders();
    bool HasPPEC();
    
    unsigned long GetRAEncoder()  throw (EQModError);
    unsigned long GetDEEncoder()  throw (EQModError);
    unsigned long GetRAEncoderZero();
    unsigned long GetRAEncoderTotal();
    unsigned long GetRAEncoderHome();
    unsigned long GetDEEncoderZero();
    unsigned long GetDEEncoderTotal();
    unsigned long GetDEEncoderHome();
    unsigned long GetRAPeriod()  throw (EQModError);
    unsigned long GetDEPeriod()  throw (EQModError);
    void GetRAMotorStatus(ILightVectorProperty *motorLP) throw (EQModError);
    void GetDEMotorStatus(ILightVectorProperty *motorLP) throw (EQModError);
    void InquireBoardVersion(ITextVectorProperty *boardTP) throw (EQModError);
    void InquireFeatures() throw (EQModError);
    void InquireRAEncoderInfo(INumberVectorProperty *encoderNP) throw (EQModError); 
    void InquireDEEncoderInfo(INumberVectorProperty *encoderNP) throw (EQModError); 
    void Init() throw (EQModError);
    void SlewRA(double rate) throw (EQModError);
    void SlewDE(double rate) throw (EQModError);
    void StopRA() throw (EQModError);
    void StopDE() throw (EQModError);
    void SetRARate(double rate)  throw (EQModError);
    void SetDERate(double rate)  throw (EQModError);
    void SlewTo(long deltaraencoder, long deltadeencoder);
    void AbsSlewTo(unsigned long raencoder, unsigned long deencoder, bool raup, bool deup);
    void StartRATracking(double trackspeed) throw (EQModError);
    void StartDETracking(double trackspeed) throw (EQModError);
    bool IsRARunning() throw (EQModError);
    bool IsDERunning() throw (EQModError);
    // For AstroEQ (needs an explicit :G command at the end of gotos)
    void ResetMotions() throw (EQModError);
    void setSimulation(bool);
    bool isSimulation();
    bool simulation;

    // Backlash
    void SetBacklashRA(unsigned long backlash);
    void SetBacklashUseRA(bool usebacklash);
    void SetBacklashDE(unsigned long backlash);
    void SetBacklashUseDE(bool usebacklash);
    
    unsigned long GetlastreadRAIndexer()  throw (EQModError);
    unsigned long GetlastreadDEIndexer()  throw (EQModError);
    unsigned long GetRAAuxEncoder()  throw (EQModError);
    unsigned long GetDEAuxEncoder()  throw (EQModError);
    void TurnRAEncoder(bool on) throw (EQModError);
    void TurnDEEncoder(bool on) throw (EQModError);
    void TurnRAPPECTraining(bool on) throw (EQModError);
    void TurnDEPPECTraining(bool on) throw (EQModError);
    void TurnRAPPEC(bool on) throw (EQModError);
    void TurnDEPPEC(bool on) throw (EQModError);
    void GetRAPPECStatus(bool *intraining, bool *inppec) throw (EQModError);
    void GetDEPPECStatus(bool *intraining, bool *inppec) throw (EQModError);
    void ResetRAIndexer() throw (EQModError);
    void ResetDEIndexer() throw (EQModError);
    void GetRAIndexer() throw (EQModError);
    void GetDEIndexer() throw (EQModError);
    void SetRAAxisPosition(unsigned long step) throw (EQModError);
    void SetDEAxisPosition(unsigned long step) throw (EQModError);
    void SetST4RAGuideRate(unsigned char r) throw (EQModError);
    void SetST4DEGuideRate(unsigned char r) throw (EQModError);

    void setPortFD(int value);

private:

    // Official Skywatcher Protocol
    // See http://code.google.com/p/skywatcher/wiki/SkyWatcherProtocol
    // Constants
    static const char SkywatcherLeadingChar = ':';
    static const char SkywatcherTrailingChar= 0x0d;
    static constexpr double MIN_RATE=0.05;
    static constexpr double MAX_RATE=800.0;
    unsigned long minperiods[2];

    // Types
    enum SkywatcherCommand {      
      Initialize ='F',
      InquireMotorBoardVersion='e',
      InquireGridPerRevolution='a',
      InquireTimerInterruptFreq='b',
      InquireHighSpeedRatio='g',
      InquirePECPeriod='s',
      InstantAxisStop='L',
      NotInstantAxisStop='K',
      SetAxisPositionCmd='E',
      GetAxisPosition='j',
      GetAxisStatus='f',
      SetSwitch='O',
      SetMotionMode='G',
      SetGotoTargetIncrement='H',
      SetBreakPointIncrement='M',
      SetGotoTarget='S',
      SetBreakStep='U',
      SetStepPeriod='I',
      StartMotion='J',
      GetStepPeriod='D', // See Merlin protocol http://www.papywizard.org/wiki/DevelopGuide
      ActivateMotor='B', // See eq6direct implementation http://pierre.nerzic.free.fr/INDI/
      SetST4GuideRateCmd='P',  
      GetHomePosition='d',    // Get Home position encoder count (default at startup)
      SetFeatureCmd='W', // EQ8/AZEQ6/AZEQ5 only
      GetFeatureCmd='q', // EQ8/AZEQ6/AZEQ5 only
      InquireAuxEncoder='d', // EQ8/AZEQ6/AZEQ5 only
      NUMBER_OF_SkywatcherCommand
    };
    
    
    enum SkywatcherAxis { 
      Axis1='1',       // RA/AZ
      Axis2='2',       // DE/ALT
      NUMBER_OF_SKYWATCHERAXIS
    };
  
    enum SkywatcherDirection {BACKWARD=0, FORWARD=1};
    enum SkywatcherSlewMode { SLEW=0, GOTO=1  };
    enum SkywatcherSpeedMode { LOWSPEED=0, HIGHSPEED=1  };
    
    typedef struct SkyWatcherFeatures {
      bool inPPECTraining;
      bool inPPEC;
      bool hasEncoder;
      bool hasPPEC;
      bool hasHomeIndexer;
      bool isAZEQ;
      bool hasPolarLed;
      bool hasCommonSlewStart; // supports :J3
      bool hasHalfCurrentTracking;
      bool hasWifi;
    } SkyWatcherFeatures;

    enum SkywatcherGetFeatureCmd {GET_INDEXER_CMD=0x00, GET_FEATURES_CMD=0x01};
    enum SkywatcherSetFeatureCmd {START_PPEC_TRAINING_CMD=0x00, STOP_PPEC_TRAINING_CMD=0x01,
				  TURN_PPEC_ON_CMD=0x02, TURN_PPEC_OFF_CMD=0X03,
				  ENCODER_ON_CMD=0x04, ENCODER_OFF_CMD=0x05,
				  DISABLE_FULL_CURRENT_LOW_SPEED_CMD=0x0006, ENABLE_FULL_CURRENT_LOW_SPEED_CMD=0x0106,
				  RESET_HOME_INDEXER_CMD=0x08};

    typedef struct SkywatcherAxisStatus {SkywatcherDirection direction; SkywatcherSlewMode slewmode; SkywatcherSpeedMode speedmode; } SkywatcherAxisStatus;
    enum SkywatcherError { NO_ERROR, ER_1, ER_2, ER_3 };
  
    struct timeval lastreadmotorstatus[NUMBER_OF_SKYWATCHERAXIS];
    struct timeval lastreadmotorposition[NUMBER_OF_SKYWATCHERAXIS];

    // Functions
    void CheckMotorStatus(SkywatcherAxis axis)  throw (EQModError);
    void ReadMotorStatus(SkywatcherAxis axis) throw (EQModError);
    void SetMotion(SkywatcherAxis axis, SkywatcherAxisStatus newstatus) throw (EQModError);
    void SetSpeed(SkywatcherAxis axis, unsigned long period) throw (EQModError);
    void SetTarget(SkywatcherAxis axis, unsigned long increment) throw (EQModError);
    void SetTargetBreaks(SkywatcherAxis axis, unsigned long increment) throw (EQModError);
    void SetAbsTarget(SkywatcherAxis axis, unsigned long target) throw (EQModError);
    void SetAbsTargetBreaks(SkywatcherAxis axis, unsigned long breakstep) throw (EQModError);
    void StartMotor(SkywatcherAxis axis) throw (EQModError);
    void StopMotor(SkywatcherAxis axis)  throw (EQModError);
    void InstantStopMotor(SkywatcherAxis axis)  throw (EQModError);
    void StopWaitMotor(SkywatcherAxis axis) throw (EQModError);
    void SetFeature(SkywatcherAxis axis, unsigned long command) throw (EQModError);
    void GetFeature(SkywatcherAxis axis, unsigned long command) throw (EQModError);
    void TurnEncoder(SkywatcherAxis axis, bool on) throw (EQModError);
    unsigned long ReadEncoder(SkywatcherAxis axis) throw (EQModError);
    void ResetIndexer(SkywatcherAxis axis) throw (EQModError);
    void GetIndexer(SkywatcherAxis axis) throw (EQModError);
    void SetST4GuideRate(SkywatcherAxis axis, unsigned char r) throw (EQModError);
    void SetAxisPosition(SkywatcherAxis axis, unsigned long step) throw (EQModError);
    void TurnPPECTraining(SkywatcherAxis axis, bool on) throw (EQModError);
    void TurnPPEC(SkywatcherAxis axis, bool on) throw (EQModError);
    void GetPPECStatus(SkywatcherAxis axis, bool *intraining, bool *inppec) throw (EQModError);
    
    bool read_eqmod()  throw (EQModError);
    bool dispatch_command(SkywatcherCommand cmd, SkywatcherAxis axis, char *arg)  throw (EQModError);

    unsigned long Revu24str2long(char *);
    unsigned long Highstr2long(char *);
    void long2Revu24str(unsigned long ,char *);

    double get_min_rate();
    double get_max_rate();
    bool isDebug();

    // Variables
    //string default_port;
    // See Skywatcher protocol
    unsigned long MCVersion; // Motor Controller Version
    unsigned long MountCode; // 

    unsigned long RASteps360;
    unsigned long DESteps360;
    unsigned long RAStepsWorm;
    unsigned long DEStepsWorm;
    unsigned long RAHighspeedRatio; // Motor controller multiplies speed values by this ratio when in low speed mode
    unsigned long DEHighspeedRatio; // This is a reflect of either using a timer interrupt with an interrupt count greater than 1 for low speed
                                    // or of using microstepping only for low speeds and half/full stepping for high speeds
    unsigned long RAStep;  // Current RA encoder position in step
    unsigned long DEStep;  // Current DE encoder position in step
    unsigned long RAStepInit;  // Initial RA position in step
    unsigned long DEStepInit;  // Initial DE position in step
    unsigned long RAStepHome;  // Home RA position in step
    unsigned long DEStepHome;  // Home DE position in step
    unsigned long RAPeriod;  // Current RA worm period
    unsigned long DEPeriod;  // Current DE worm period

    bool RAInitialized, DEInitialized, RARunning, DERunning;
    bool wasinitialized;
    SkywatcherAxisStatus RAStatus, DEStatus;
    SkyWatcherFeatures AxisFeatures[NUMBER_OF_SKYWATCHERAXIS];

    int PortFD=-1;
    char command[SKYWATCHER_MAX_CMD];
    char response[SKYWATCHER_MAX_CMD];

    bool debug;
    const char *deviceName;
    bool debugnextread;
    EQMod *telescope;
    bool reconnect;

    // Backlash
    unsigned long Backlash[NUMBER_OF_SKYWATCHERAXIS];
    bool UseBacklash[NUMBER_OF_SKYWATCHERAXIS];
    unsigned long Target[NUMBER_OF_SKYWATCHERAXIS];
    unsigned long TargetBreaks[NUMBER_OF_SKYWATCHERAXIS];
    SkywatcherAxisStatus LastRunningStatus[NUMBER_OF_SKYWATCHERAXIS];
    SkywatcherAxisStatus NewStatus[NUMBER_OF_SKYWATCHERAXIS];
    unsigned long backlashperiod[NUMBER_OF_SKYWATCHERAXIS];

    unsigned long lastreadIndexer[NUMBER_OF_SKYWATCHERAXIS];

};

#endif
