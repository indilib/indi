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

// FIXME This should equal the RPM required to achive sidereal tracking
#define FORKMOUNT_RATE_TO_RPM   1.0/250.0

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
        virtual bool saveConfigItems(FILE *fp);

        void setDebug (bool enable);
        const char *getDeviceName ();
        bool update();
        void setSimulation(bool);
        bool isSimulation();
        bool simulation;
        bool isProtectionTrigged();

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
        void SlewTo(long targetraencoder, long targetdeencoder);
        void StartRATracking(double trackspeed) throw (UjariError);
        void StartDETracking(double trackspeed) throw (UjariError);
        bool IsRARunning() throw (UjariError);
        bool IsDERunning() throw (UjariError);
        void SetRATargetEncoder(unsigned long tEncoder);
        void SetDETargetEncoder(unsigned long tEncoder);

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

        AMCController *RAMotor, *DEMotor;
        Encoder *RAEncoder, *DEEncoder;

        // TODO Check if this is valid for Ujari
        static constexpr double MIN_RATE=0.05;
        static constexpr double MAX_RATE=600.0;
        double minrpms[2];

        enum ForkMountAxis
        {
          Axis1='1',       // RA/AZ
          Axis2='2',       // DE/ALT
          NUMBER_OF_FORKMOUNTAXIS
        };

        enum ForkMountDirection {BACKWARD=0, FORWARD=1};
        enum ForkMountSlewMode { SLEW=0, GOTO=1, TRACK=2  };
        typedef struct ForkMountAxisStatus {ForkMountDirection direction; ForkMountSlewMode slewmode; } ForkMountAxisStatus;
        enum ForkMountError { NO_ERROR, ER_1, ER_2, ER_3 };

        struct timeval lastreadmotorstatus[NUMBER_OF_FORKMOUNTAXIS];
        struct timeval lastreadmotorposition[NUMBER_OF_FORKMOUNTAXIS];

        // Functions
        void CheckMotorStatus(ForkMountAxis axis)  throw (UjariError);
        void ReadMotorStatus(ForkMountAxis axis) throw (UjariError);
        void SetMotion(ForkMountAxis axis, ForkMountAxisStatus newstatus) throw (UjariError);
        void SetSpeed(ForkMountAxis axis, double rpm) throw (UjariError);
        bool StartMotor(ForkMountAxis axis, ForkMountAxisStatus newstatus);
        void StopMotor(ForkMountAxis axis)  throw (UjariError);
        void StopWaitMotor(ForkMountAxis axis) throw (UjariError);        
        double GetGotoSpeed(ForkMountAxis axis);
        double GetRADiff();
        double GetDEDiff();

        unsigned long Revu24str2long(char *);
        unsigned long Highstr2long(char *);
        void long2Revu24str(unsigned long ,char *);

        double get_min_rate();
        double get_max_rate();
        bool isDebug();

        unsigned long RASteps360;
        unsigned long DESteps360;
        unsigned long RAStep;  // Current RA encoder position in step
        unsigned long DEStep;  // Current DE encoder position in step
        unsigned long RAStepInit;  // Initial RA position in step
        unsigned long DEStepInit;  // Initial DE position in step
        unsigned long RAStepHome;  // Home RA position in step
        unsigned long DEStepHome;  // Home DE position in step
        unsigned long RAEncoderTarget;  // RA Encoder Target
        unsigned long DEEncoderTarget;  // DEC Encoder Target

        bool RAInitialized, DEInitialized, RARunning, DERunning;
        bool wasinitialized;
        ForkMountAxisStatus RAStatus, DEStatus;

        int fd;
        bool debug;
        const char *deviceName;
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
