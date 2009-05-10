/*
    KNRO Observatory INDI Driver
    Copyright (C) 2009 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    Change Log:

*/

#ifndef KNRO_H
#define KNRO_H

#include <pthread.h>
#include <stdio.h>

#include "encoder.h"

using std::string;

// Handy macros
#define currentAZ		HorizontalCoordsN[EQ_AZ].value
#define currentALT		HorizontalCoordsN[EQ_ALT].value
#define currentLat 		GeoCoordsN[0].value
#define currentLong     	GeoCoordsN[1].value
#define slewAZTolerance		SlewPrecisionN[0].value
#define slewALTTolerance	SlewPrecisionN[1].value
#define trackAZTolerance	TrackPrecisionN[0].value
#define trackALTTolerance	TrackPrecisionN[1].value
#define KNRO_DEBUG		DebugS[0].s==ISS_ON

#define CMD_BUF_SIZE		512

class knroObservatory
{
 public:

 knroObservatory();
 ~knroObservatory();

 void ISGetProperties (const char *dev);
 void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 void ISPoll ();

 inline bool is_connected() { return ((ConnectS[0].s == ISS_ON) ? true : false); }

 private:

   knroEncoder *AltEncoder;
   knroEncoder *AzEncoder;
  
    /************************/
    /* FOR SIMULATION ONLY */
    ISwitch AZEncS[2];
    ISwitch ALTEncS[2];
    ISwitchVectorProperty AZEncSP;
    ISwitchVectorProperty ALTEncSP;
    /**** END SIMULATION ****/

    /* Switches */
    ISwitch ConnectS[2];
    ISwitch AZSlewSpeedS[3];
    ISwitch ALTSlewSpeedS[3];
    ISwitch MovementNSS[2];
    ISwitch MovementWES[2];
    ISwitch AbortSlewS[1];
    ISwitch StopAllS[1];
    ISwitch OnCoordSetS[1];
    ISwitch ParkS[1];
    ISwitch DebugS[2];

    /* Texts */
    IText PortT[1];

    /* Lights */
    ILight AltSafetyL[1];

    /* Numbers */
   INumber HorizontalCoordsN[2];
   INumber GeoCoordsN[2];
   INumber UTCOffsetN[1];
   INumber SlewPrecisionN[2];
   INumber TrackPrecisionN[2];

    /* BLOBs */
    
    /* Switch vectors */
    ISwitchVectorProperty ConnectSP;				/* Connection switch */
    ISwitchVectorProperty AZSlewSpeedSP;			/* Slew Speed */
    ISwitchVectorProperty ALTSlewSpeedSP;			/* Slew Speed */
    ISwitchVectorProperty MovementNSSP;				/* North/South */
    ISwitchVectorProperty MovementWESP;				/* West/East */
    ISwitchVectorProperty AbortSlewSP;				/* Abort Slew */
    ISwitchVectorProperty StopAllSP;				/* Stop All, exactly like Abort Slew */
    ISwitchVectorProperty OnCoordSetSP;				/* What happens when new coords are set */
    ISwitchVectorProperty ParkSP;				/* Park Telescope */
    ISwitchVectorProperty DebugSP;				/* Debugging Switch */

    /* Text Vectors */
    ITextVectorProperty PortTP;

    /* Light Vectors */
    ILightVectorProperty AltSafetyLP;

    /* Number vectors */
    INumberVectorProperty HorizontalCoordsNP;
    INumberVectorProperty GeoCoordsNP;
    INumberVectorProperty UTCOffsetNP;
    INumberVectorProperty SlewPrecisionNP;
    INumberVectorProperty TrackPrecisionNP;

    /* BLOB vectors */
    
    /* Other */

    enum SlewSpeed { SLOW_SPEED = 0 , MEDIUM_SPEED, FAST_SPEED };
    enum SlewStages { SLEW_NONE, SLEW_NOW, SLEW_TRACK };
    enum EqCoords { EQ_AZ = 0, EQ_ALT =1 };
    enum NSDirection { KNRO_NORTH = 0, KNRO_SOUTH = 1};
    enum WEDirection { KNRO_WEST = 0, KNRO_EAST = 1 };
    enum knroErrCode { SUCCESS = 0, BELOW_HORIZON_ERROR = -1, SAFETY_LIMIT_ERROR = -2, UNKNOWN_ERROR = -3};

    // ============
    // Slew Regions
    // We designate regions for telescope slew speeds that depend on the angular seperation
    // between the current coordinate and target coordinate. For instance, if the distance between two
    // coords is more than 30 degrees, we make the telescope move fast, and when the distance is reduced to
    // 5 degrees we switch to medium speed, and finally when we get to 2 degrees we switch to slow speed until
    // we hit our target. The values are somewhat arbitrary for now, they will be finely tuned depending on how
    // the telescope behaves.

    static const double NS_FAST_REGION   = 30.0;
    static const double NS_MEDIUM_REGION = 5.0;
    static const double NS_SLOW_REGION   = 2.0;

    static const double WE_FAST_REGION   = 2.0;
    static const double WE_MEDIUM_REGION = 0.33;
    static const double WE_SLOW_REGION   = 0.13;

    // =========================================================
    static const double MAXIMUM_HOUR_ANGLE = 2.67;
    // =========================================================

    static const double MINIMUM_SAFE_ALT = 40.0;

    // Saftey Zones (%). We warn if ALT reaches 60 degrees
    // and we enter danger is alt reaches 52 degrees
    static const double ALT_DANGER_ZONE  = 42.0;
    static const double ALT_WARNING_ZONE = 50.0;
    // We reach critical Alt of 45 degrees at which the telescope
    // will automatically go to HOME position.
    static const double ALT_CRITICAL_ZONE = 37.0;

    // =========================================================

    // ================
    // Tolerance levels
    // The tolerance level is defined as the angular seperation, in arc minutes, between the requested
    // coords and the current coords before we can declare that the slew is successful. BEI 12 bit encoder
    // reports 400 counts/degree in ALT, and 60 counts/degree in AZ giving it a theorotical resolution
    // 15 arcsec in ALT, and 1 arcmin in AZ. 
    // Given the telescope mechanics, I'd be more than happy to settle for
    // an accuracy of 20 arc minutes. This value will be finely tuned as actual telescope testing goes on.

   static const double SLEW_AZ_TOLERANCE  = 1.0;
   static const double SLEW_ALT_TOLERANCE = 1.0;

    static const double TRACK_AZ_TOLERANCE  = 1.0;
    static const double TRACK_ALT_TOLEAZNCE = 1.0;

    // ================
    // Update Rate: 10 Hz
    static const int update_period = 100000;
    // ================

    // ALT counts per degree
    static const double ALT_CPD = 564.08;
    //static const double AZ_CPD = 389.0;
    static const double AZ_CPD = 392.9;
    
    static int WE_ZERO_OFFSET;
    static int NS_ZERO_OFFSET;

    /* Functions */
    /* Safety checks functions */
    void check_safety();

    /* Error log functions */
    const char *get_knro_error_string(knroErrCode code);

    /* Telescope Motion Control functions */
    void update_alt_speed();
    void update_az_speed();
    void update_alt_dir(NSDirection newDir);
    void update_az_dir(WEDirection newDir);

    /* Send command buffer to NI card */
    void execute_slew();

    /* Move telescope in AZ/ALT */
    void move_telescope_ns();
    void move_telescope_we();

    /* Mode sidereally using PWM */
    void start_sidereal();
    void stop_sidereal();

    /* Disengage tracking */
    void disable_tracking();

    /* Modular stop functions */
    knroErrCode stop_all();
    knroErrCode stop_telescope();
    knroErrCode stop_ns();
    knroErrCode stop_we();

    void park_telescope(bool is_emergency=false);
    void terminate_parking();

    void play_calibration_error();

    /* Return true if telescope is within tolerance limits */
    bool is_az_done();
    bool is_alt_done();

    /* Encoder Thread Functions */
    static void * init_encoders(void * arg);
    static void sync_calibration(void *arg);

    /* General functions */
    void init_properties();
    void init_encoder_thread();
    void reset_all_properties(bool reset_to_idle=false);

    /* Was the speed changed? */
    bool az_speed_change;
    bool alt_speed_change;

    /* Coordinates & Time */
    double targetAZ;
    double lastAZ;
    double targetALT;
    double currentAlt;
    double targetObjectAz;
    double currentLST;
    double currentHA;

    double rawAZ;
    double rawALT;


    /* Timing variables */
    static const int MAXIMUM_IDLE_TIME = 1800;			// Maximum 30 minutes idle time
    time_t last_execute_time;
    time_t now;

    /* Type of active slew stage */
    SlewStages slew_stage;

    /* Encoder relative and absolute counts */
    //static unsigned int ns_encoder_count;
    //static unsigned int we_encoder_count;

    unsigned int alt_absolute_position;
    unsigned int az_absolute_position;

    static unsigned int encoder_error;

    /* Threading variables */
    pthread_t encoder_thread;
    static pthread_mutex_t encoder_mutex;

    /* Location data */
    //GeoLocation observatory;

    /* Simulation Rates */
    static const int ALT_RATE_FAST = 1000;
    static const int ALT_RATE_MEDIUM = 500;
    static const int ALT_RATE_SLOW = 10;
    static const int AZ_RATE_FAST  = 295;
    static const int AZ_RATE_MEDIUM  = 147;
    static const int AZ_RATE_SLOW  = 10;

    /* Current simulation AZ & ALT rates */
    int current_az_rate;
    int current_alt_rate;
    
    /* Warning sounds */
    /*OggFile park_alert;
    OggFile slew_complete;
    OggFile slew_error;
    OggFile calibration_error;*/

    /* Encoder Check File */
    FILE *en_record;

};


#endif
