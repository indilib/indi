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

#pragma once

#include "eqmoderror.h"

#include <inditelescope.h>

#include <lilxml.h>

#include <time.h>
#include <sys/time.h>

class EQMod; // TODO

#include "simulator/simulator.h"

#define SKYWATCHER_MAX_CMD      16
#define SKYWATCHER_MAX_TRIES    3
#define SKYWATCHER_ERROR_BUFFER 1024

#define SKYWATCHER_SIDEREAL_DAY   86164.09053083288
#define SKYWATCHER_SIDEREAL_SPEED 15.04106864
#define SKYWATCHER_STELLAR_DAY    86164.098903691
#define SKYWATCHER_STELLAR_SPEED  15.041067179

#define SKYWATCHER_LOWSPEED_RATE 128
#define SKYWATCHER_MAXREFRESH    0.5

#define SKYWATCHER_BACKLASH_SPEED_RA 64
#define SKYWATCHER_BACKLASH_SPEED_DE 64

#define HEX(c) (((c) < 'A') ? ((c) - '0') : ((c) - 'A') + 10)

class Skywatcher
{
  public:
    Skywatcher(EQMod *t);
    ~Skywatcher();

    bool Handshake();
    bool Disconnect();
    void setDebug(bool enable);
    const char *getDeviceName();

    bool HasHomeIndexers();
    bool HasAuxEncoders();
    bool HasPPEC();

    uint32_t GetRAEncoder();
    uint32_t GetDEEncoder();
    uint32_t GetRAEncoderZero();
    uint32_t GetRAEncoderTotal();
    uint32_t GetRAEncoderHome();
    uint32_t GetDEEncoderZero();
    uint32_t GetDEEncoderTotal();
    uint32_t GetDEEncoderHome();
    uint32_t GetRAPeriod();
    uint32_t GetDEPeriod();
    void GetRAMotorStatus(ILightVectorProperty *motorLP);
    void GetDEMotorStatus(ILightVectorProperty *motorLP);
    void InquireBoardVersion(ITextVectorProperty *boardTP);
    void InquireFeatures();
    void InquireRAEncoderInfo(INumberVectorProperty *encoderNP);
    void InquireDEEncoderInfo(INumberVectorProperty *encoderNP);
    void Init();
    void SlewRA(double rate);
    void SlewDE(double rate);
    void StopRA();
    void StopDE();
    void SetRARate(double rate);
    void SetDERate(double rate);
    void SlewTo(int32_t deltaraencoder, int32_t deltadeencoder);
    void AbsSlewTo(uint32_t raencoder, uint32_t deencoder, bool raup, bool deup);
    void StartRATracking(double trackspeed);
    void StartDETracking(double trackspeed);
    bool IsRARunning();
    bool IsDERunning();
    // For AstroEQ (needs an explicit :G command at the end of gotos)
    void ResetMotions();
    void setSimulation(bool);
    bool isSimulation();
    bool simulation;

    // Backlash
    void SetBacklashRA(uint32_t backlash);
    void SetBacklashUseRA(bool usebacklash);
    void SetBacklashDE(uint32_t backlash);
    void SetBacklashUseDE(bool usebacklash);

    uint32_t GetlastreadRAIndexer();
    uint32_t GetlastreadDEIndexer();
    uint32_t GetRAAuxEncoder();
    uint32_t GetDEAuxEncoder();
    void TurnRAEncoder(bool on);
    void TurnDEEncoder(bool on);
    void TurnRAPPECTraining(bool on);
    void TurnDEPPECTraining(bool on);
    void TurnRAPPEC(bool on);
    void TurnDEPPEC(bool on);
    void GetRAPPECStatus(bool *intraining, bool *inppec);
    void GetDEPPECStatus(bool *intraining, bool *inppec);
    void ResetRAIndexer();
    void ResetDEIndexer();
    void GetRAIndexer();
    void GetDEIndexer();
    void SetRAAxisPosition(uint32_t step);
    void SetDEAxisPosition(uint32_t step);
    void SetST4RAGuideRate(unsigned char r);
    void SetST4DEGuideRate(unsigned char r);

    void setPortFD(int value);

  private:
    // Official Skywatcher Protocol
    // See http://code.google.com/p/skywatcher/wiki/SkyWatcherProtocol
    // Constants
    static const char SkywatcherLeadingChar  = ':';
    static const char SkywatcherTrailingChar = 0x0d;
    static constexpr double MIN_RATE         = 0.05;
    static constexpr double MAX_RATE         = 800.0;
    uint32_t minperiods[2];

    // Types
    enum SkywatcherCommand
    {
        Initialize                = 'F',
        InquireMotorBoardVersion  = 'e',
        InquireGridPerRevolution  = 'a',
        InquireTimerInterruptFreq = 'b',
        InquireHighSpeedRatio     = 'g',
        InquirePECPeriod          = 's',
        InstantAxisStop           = 'L',
        NotInstantAxisStop        = 'K',
        SetAxisPositionCmd        = 'E',
        GetAxisPosition           = 'j',
        GetAxisStatus             = 'f',
        SetSwitch                 = 'O',
        SetMotionMode             = 'G',
        SetGotoTargetIncrement    = 'H',
        SetBreakPointIncrement    = 'M',
        SetGotoTarget             = 'S',
        SetBreakStep              = 'U',
        SetStepPeriod             = 'I',
        StartMotion               = 'J',
        GetStepPeriod             = 'D', // See Merlin protocol http://www.papywizard.org/wiki/DevelopGuide
        ActivateMotor             = 'B', // See eq6direct implementation http://pierre.nerzic.free.fr/INDI/
        SetST4GuideRateCmd        = 'P',
        GetHomePosition           = 'd', // Get Home position encoder count (default at startup)
        SetFeatureCmd             = 'W', // EQ8/AZEQ6/AZEQ5 only
        GetFeatureCmd             = 'q', // EQ8/AZEQ6/AZEQ5 only
        InquireAuxEncoder         = 'd', // EQ8/AZEQ6/AZEQ5 only
        NUMBER_OF_SkywatcherCommand
    };

    enum SkywatcherAxis
    {
        Axis1 = 0, // RA/AZ
        Axis2 = 1, // DE/ALT
        NUMBER_OF_SKYWATCHERAXIS
    };
    char AxisCmd[2] {'1', '2'};

    enum SkywatcherDirection
    {
        BACKWARD = 0,
        FORWARD  = 1
    };
    enum SkywatcherSlewMode
    {
        SLEW = 0,
        GOTO = 1
    };
    enum SkywatcherSpeedMode
    {
        LOWSPEED  = 0,
        HIGHSPEED = 1
    };

    typedef struct SkyWatcherFeatures
    {
        bool inPPECTraining = false;
        bool inPPEC = false;
        bool hasEncoder = false;
        bool hasPPEC = false;
        bool hasHomeIndexer = false;
        bool isAZEQ = false;
        bool hasPolarLed = false;
        bool hasCommonSlewStart = false; // supports :J3
        bool hasHalfCurrentTracking = false;
        bool hasWifi = false;
    } SkyWatcherFeatures;

    enum SkywatcherGetFeatureCmd
    {
        GET_INDEXER_CMD  = 0x00,
        GET_FEATURES_CMD = 0x01
    };
    enum SkywatcherSetFeatureCmd
    {
        START_PPEC_TRAINING_CMD            = 0x00,
        STOP_PPEC_TRAINING_CMD             = 0x01,
        TURN_PPEC_ON_CMD                   = 0x02,
        TURN_PPEC_OFF_CMD                  = 0X03,
        ENCODER_ON_CMD                     = 0x04,
        ENCODER_OFF_CMD                    = 0x05,
        DISABLE_FULL_CURRENT_LOW_SPEED_CMD = 0x0006,
        ENABLE_FULL_CURRENT_LOW_SPEED_CMD  = 0x0106,
        RESET_HOME_INDEXER_CMD             = 0x08
    };

    typedef struct SkywatcherAxisStatus
    {
        SkywatcherDirection direction;
        SkywatcherSlewMode slewmode;
        SkywatcherSpeedMode speedmode;
    } SkywatcherAxisStatus;
    enum SkywatcherError
    {
        NO_ERROR,
        ER_1,
        ER_2,
        ER_3
    };

    struct timeval lastreadmotorstatus[NUMBER_OF_SKYWATCHERAXIS];
    struct timeval lastreadmotorposition[NUMBER_OF_SKYWATCHERAXIS];

    // Functions
    void CheckMotorStatus(SkywatcherAxis axis);
    void ReadMotorStatus(SkywatcherAxis axis);
    void SetMotion(SkywatcherAxis axis, SkywatcherAxisStatus newstatus);
    void SetSpeed(SkywatcherAxis axis, uint32_t period);
    void SetTarget(SkywatcherAxis axis, uint32_t increment);
    void SetTargetBreaks(SkywatcherAxis axis, uint32_t increment);
    void SetAbsTarget(SkywatcherAxis axis, uint32_t target);
    void SetAbsTargetBreaks(SkywatcherAxis axis, uint32_t breakstep);
    void StartMotor(SkywatcherAxis axis);
    void StopMotor(SkywatcherAxis axis);
    void InstantStopMotor(SkywatcherAxis axis);
    void StopWaitMotor(SkywatcherAxis axis);
    void SetFeature(SkywatcherAxis axis, uint32_t command);
    void GetFeature(SkywatcherAxis axis, uint32_t command);
    void TurnEncoder(SkywatcherAxis axis, bool on);
    uint32_t ReadEncoder(SkywatcherAxis axis);
    void ResetIndexer(SkywatcherAxis axis);
    void GetIndexer(SkywatcherAxis axis);
    void SetST4GuideRate(SkywatcherAxis axis, unsigned char r);
    void SetAxisPosition(SkywatcherAxis axis, uint32_t step);
    void TurnPPECTraining(SkywatcherAxis axis, bool on);
    void TurnPPEC(SkywatcherAxis axis, bool on);
    void GetPPECStatus(SkywatcherAxis axis, bool *intraining, bool *inppec);

    bool read_eqmod();
    bool dispatch_command(SkywatcherCommand cmd, SkywatcherAxis axis, char *arg);

    uint32_t Revu24str2long(char *);
    uint32_t Highstr2long(char *);
    void long2Revu24str(uint32_t, char *);

    double get_min_rate();
    double get_max_rate();
    bool isDebug();

    // Variables
    // See Skywatcher protocol
    uint32_t MCVersion; // Motor Controller Version
    uint32_t MountCode; //

    uint32_t RASteps360;
    uint32_t DESteps360;
    uint32_t RAStepsWorm;
    uint32_t DEStepsWorm;
    // Motor controller multiplies speed values by this ratio when in low speed mode
    uint32_t RAHighspeedRatio;
    // This is a reflect of either using a timer interrupt with an interrupt count greater than 1 for low speed
     // or of using microstepping only for low speeds and half/full stepping for high speeds
    uint32_t DEHighspeedRatio;

    uint32_t RAStep;     // Current RA encoder position in step
    uint32_t DEStep;     // Current DE encoder position in step
    uint32_t RAStepInit; // Initial RA position in step
    uint32_t DEStepInit; // Initial DE position in step
    uint32_t RAStepHome; // Home RA position in step
    uint32_t DEStepHome; // Home DE position in step
    uint32_t RAPeriod {256};   // Current RA worm period
    uint32_t DEPeriod {256};   // Current DE worm period

    uint32_t lastRAStep {0xFFFFFFFF};
    uint32_t lastDEStep {0xFFFFFFFF};
    uint32_t lastRAPeriod {0xFFFFFFFF};
    uint32_t lastDEPeriod {0xFFFFFFFF};

    bool RAInitialized, DEInitialized, RARunning, DERunning;
    bool wasinitialized;
    SkywatcherAxisStatus RAStatus, DEStatus;
    SkyWatcherFeatures AxisFeatures[NUMBER_OF_SKYWATCHERAXIS];

    int PortFD = -1;
    char command[SKYWATCHER_MAX_CMD];
    char response[SKYWATCHER_MAX_CMD];

    bool debug;
    bool debugnextread;
    EQMod *telescope;
    bool reconnect;

    // Backlash
    uint32_t Backlash[NUMBER_OF_SKYWATCHERAXIS];
    bool UseBacklash[NUMBER_OF_SKYWATCHERAXIS];
    uint32_t Target[NUMBER_OF_SKYWATCHERAXIS];
    uint32_t TargetBreaks[NUMBER_OF_SKYWATCHERAXIS];
    SkywatcherAxisStatus LastRunningStatus[NUMBER_OF_SKYWATCHERAXIS];
    SkywatcherAxisStatus NewStatus[NUMBER_OF_SKYWATCHERAXIS];
    uint32_t backlashperiod[NUMBER_OF_SKYWATCHERAXIS];

    uint32_t lastreadIndexer[NUMBER_OF_SKYWATCHERAXIS];

    const uint8_t EQMOD_TIMEOUT = 5;
    const uint8_t EQMOD_MAX_RETRY = 3;
};
