#if 0
    INDI Ujari Driver
    Copyright (C) 2014 Jasem Mutlaq

    Based on EQMod INDI Driver by Jean-Luc
    
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

#endif

#ifndef FORKMOUNT_H
#define FORKMOUNT_H

#include <sys/time.h>
#include <time.h>
#include <inditelescope.h>
#include <lilxml.h>

#include "ujarierror.h"

#define FORKMOUNT_MAX_CMD        16
#define FORKMOUNT_MAX_TRIES      3
#define FORKMOUNT_ERROR_BUFFER   1024

#define FORKMOUNT_SIDEREAL_DAY 86164.09053083288
#define FORKMOUNT_SIDEREAL_SPEED 15.04106864
#define FORKMOUNT_STELLAR_DAY 86164.098903691
#define FORKMOUNT_STELLAR_SPEED 15.041067179

#define FORKMOUNT_LOWSPEED_RATE 128
#define FORKMOUNT_MAXREFRESH 0.5

#define HEX(c) (((c) < 'A')?((c)-'0'):((c) - 'A') + 10)

class Ujari;
class Encoder;
class AMCController;

class ForkMount
{

public:
   ForkMount(Ujari *t);
   ~ForkMount();

        bool Connect() throw (UjariError);
        bool Disconnect() throw (UjariError);
        bool initProperties();
        void ISGetProperties(const char *dev);
        bool updateProperties();
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        void setDebug (bool enable);
        const char *getDeviceName ();


        unsigned long GetRAEncoder()  throw (UjariError);
        unsigned long GetDEEncoder()  throw (UjariError);
        unsigned long GetRAEncoderZero();
        unsigned long GetRAEncoderTotal();
        unsigned long GetRAEncoderHome();
        unsigned long GetDEEncoderZero();
        unsigned long GetDEEncoderTotal();
        unsigned long GetDEEncoderHome();
        unsigned long GetRAPeriod()  throw (UjariError);
        unsigned long GetDEPeriod()  throw (UjariError);
        void GetRAMotorStatus(ILightVectorProperty *motorLP) throw (UjariError);
        void GetDEMotorStatus(ILightVectorProperty *motorLP) throw (UjariError);
        void Init(ISwitchVectorProperty *parkSP) throw (UjariError);
        void SlewRA(double rate) throw (UjariError);
        void SlewDE(double rate) throw (UjariError);
        void StopRA() throw (UjariError);
        void StopDE() throw (UjariError);
        void SetRARate(double rate)  throw (UjariError);
        void SetDERate(double rate)  throw (UjariError);
        void SlewTo(long deltaraencoder, long deltadeencoder);
        void StartRATracking(double trackspeed) throw (UjariError);
        void StartDETracking(double trackspeed) throw (UjariError);
        bool IsRARunning() throw (UjariError);
        bool IsDERunning() throw (UjariError);

        void setSimulation(bool);
        bool isSimulation();
        bool simulation;

        // Park
        unsigned long GetRAEncoderPark();
        unsigned long GetRAEncoderParkDefault();
        unsigned long GetDEEncoderPark();
        unsigned long GetDEEncoderParkDefault();
        unsigned long SetRAEncoderPark(unsigned long steps);
        unsigned long SetRAEncoderParkDefault(unsigned long steps);
        unsigned long SetDEEncoderPark(unsigned long steps);
        unsigned long SetDEEncoderParkDefault(unsigned long steps);
        void SetParked(bool parked);
        bool isParked();
        bool WriteParkData();

     private:

        AMCController *RAMotor, *DECMotor;
        Encoder *RAEncoder, *DECEncoder;

        // Official ForkMount Protocol
        // See http://code.google.com/p/ForkMount/wiki/ForkMountProtocol
        // Constants
        static const char ForkMountLeadingChar = ':';
        static const char ForkMountTrailingChar= 0x0d;
        static const double MIN_RATE=0.05;
        static const double MAX_RATE=800.0;
        unsigned long minperiods[2];

        // Types
        enum ForkMountCommand {
          Initialize ='F',
          InquireMotorBoardVersion='e',
          InquireGridPerRevolution='a',
          InquireTimerInterruptFreq='b',
          InquireHighSpeedRatio='g',
          InquirePECPeriod='s',
          InstantAxisStop='L',
          NotInstantAxisStop='K',
          SetAxisPosition='E',
          GetAxisPosition='j',
          GetAxisStatus='f',
          SetSwitch='O',
          SetMotionMode='G',
          SetGotoTargetIncrement='H',
          SetBreakPointIncrement='M',
          SetBreakSteps='U',
          SetStepPeriod='I',
          StartMotion='J',
          GetStepPeriod='D', // See Merlin protocol http://www.papywizard.org/wiki/DevelopGuide
          ActivateMotor='B', // See eq6direct implementation http://pierre.nerzic.free.fr/INDI/
          SetGuideRate='P',  // See EQASCOM driver
          Deactivate='d',
          NUMBER_OF_FORKMOUNTCommand
        };


        enum ForkMountAxis {
          Axis1='1',       // RA/AZ
          Axis2='2',       // DE/ALT
          NUMBER_OF_FORKMOUNTAXIS
        };

        enum ForkMountDirection {BACKWARD=0, FORWARD=1};
        enum ForkMountSlewMode { SLEW=0, GOTO=1  };
        enum ForkMountSpeedMode { LOWSPEED=0, HIGHSPEED=1  };
        typedef struct ForkMountAxisStatus {ForkMountDirection direction; ForkMountSlewMode slewmode; ForkMountSpeedMode speedmode; } ForkMountAxisStatus;
        enum ForkMountError { NO_ERROR, ER_1, ER_2, ER_3 };

        struct timeval lastreadmotorstatus[NUMBER_OF_FORKMOUNTAXIS];
        struct timeval lastreadmotorposition[NUMBER_OF_FORKMOUNTAXIS];

        // Functions
        void CheckMotorStatus(ForkMountAxis axis)  throw (UjariError);
        void ReadMotorStatus(ForkMountAxis axis) throw (UjariError);
        void SetMotion(ForkMountAxis axis, ForkMountAxisStatus newstatus) throw (UjariError);
        void SetSpeed(ForkMountAxis axis, unsigned long period) throw (UjariError);
        void SetTarget(ForkMountAxis axis, unsigned long increment) throw (UjariError);
        void SetTargetBreaks(ForkMountAxis axis, unsigned long increment) throw (UjariError);
        void StartMotor(ForkMountAxis axis) throw (UjariError);
        void StopMotor(ForkMountAxis axis)  throw (UjariError);
        void InstantStopMotor(ForkMountAxis axis)  throw (UjariError);
        void StopWaitMotor(ForkMountAxis axis) throw (UjariError);

        bool read_eqmod()  throw (UjariError);
        bool dispatch_command(ForkMountCommand cmd, ForkMountAxis axis, char *arg)  throw (UjariError);

        unsigned long Revu24str2long(char *);
        unsigned long Highstr2long(char *);
        void long2Revu24str(unsigned long ,char *);

        double get_min_rate();
        double get_max_rate();
        bool isDebug();

        // Variables
        //string default_port;
        // See ForkMount protocol
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
        ForkMountAxisStatus RAStatus, DEStatus;

        int fd;
        char command[FORKMOUNT_MAX_CMD];
        char response[FORKMOUNT_MAX_CMD];

        bool debug;
        const char *deviceName;
        bool debugnextread;
        Ujari *telescope;

        //Park
        void initPark();
        char *LoadParkData(const char *filename);
        char *WriteParkData(const char *filename);
        unsigned long RAParkPosition;
        unsigned long RADefaultParkPosition;
        unsigned long DEParkPosition;
        unsigned long DEDefaultParkPosition;
        bool parked;
        const char *ParkDeviceName;
        const char * Parkdatafile;
        XMLEle *ParkdataXmlRoot, *ParkdeviceXml, *ParkstatusXml, *ParkpositionXml, *ParkpositionRAXml, *ParkpositionDEXml;
    };


#endif
