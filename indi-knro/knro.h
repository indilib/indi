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
#include <list>

#include "encoder.h"
#include "inverter.h"

using std::string;
using std::list;

// Handy macros
#define currentAz		HorizontalCoordsNR[0].value
#define currentAlt		HorizontalCoordsNR[1].value
#define targetAz		HorizontalCoordsNW[0].value
#define targetAlt		HorizontalCoordsNW[1].value

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

   knroInverter *AltInverter;
   knroInverter *AzInverter;
 
    /************************/
    /* FOR SIMULATION ONLY */
    ISwitch AZEncS[2];
    ISwitch ALTEncS[2];
    ISwitchVectorProperty AZEncSP;
    ISwitchVectorProperty ALTEncSP;
    /**** END SIMULATION ****/

    /* Switches */
    ISwitch ConnectS[2];
    ISwitch AbortSlewS[1];
    ISwitch StopAllS[1];
    ISwitch OnCoordSetS[1];
    ISwitch ParkS[1];
    ISwitch DebugS[2];
    ISwitch SimulationS[2];
    ISwitch MovementNSS[2];
    ISwitch MovementWES[2];

    /* Texts */
    IText PortT[1];

    /* Lights */
    ILight AzSafetyL[1];

    /* Numbers */
   INumber HorizontalCoordsNR[2];
   INumber HorizontalCoordsNW[2];
   INumber GeoCoordsN[2];
   INumber UTCOffsetN[1];
   INumber SlewPrecisionN[2];
   INumber TrackPrecisionN[2];

    /* BLOBs */
    
    /* Switch vectors */
    ISwitchVectorProperty ConnectSP;				/* Connection switch */
    ISwitchVectorProperty AbortSlewSP;				/* Abort Slew */
    ISwitchVectorProperty StopAllSP;				/* Stop All, exactly like Abort Slew */
    ISwitchVectorProperty OnCoordSetSP;				/* What happens when new coords are set */
    ISwitchVectorProperty ParkSP;				/* Park Telescope */
    ISwitchVectorProperty DebugSP;				/* Debugging Switch */
    ISwitchVectorProperty SimulationSP;				/* Simulation Switch */
    ISwitchVectorProperty MovementNSSP;				/* North/South Movement */
    ISwitchVectorProperty MovementWESP;				/* West/East Movement */

    /* Text Vectors */
    ITextVectorProperty PortTP;

    /* Light Vectors */
    ILightVectorProperty AzSafetyLP;

    /* Number vectors */
    INumberVectorProperty HorizontalCoordsNRP;
    INumberVectorProperty HorizontalCoordsNWP;
    INumberVectorProperty GeoCoordsNP;
    INumberVectorProperty UTCOffsetNP;
    INumberVectorProperty SlewPrecisionNP;
    INumberVectorProperty TrackPrecisionNP;

    /* Other */

    enum SlewSpeed { SLOW_SPEED = 0 , MEDIUM_SPEED, FAST_SPEED };
    enum SlewStages { SLEW_NONE, SLEW_NOW, SLEW_TRACK };
    enum HorzCoords { KNRO_AZ = 0, KNRO_ALT =1 };
    enum AltDirection { KNRO_NORTH = 0, KNRO_SOUTH = 1};
    enum AzDirection { KNRO_WEST = 0, KNRO_EAST = 1 };
    enum knroErrCode { SUCCESS = 0, BELOW_HORIZON_ERROR = -1, SAFETY_LIMIT_ERROR = -2, INVERTER_ERROR=-3, UNKNOWN_ERROR = -4};

    // ============
    // Slew Regions
    // We designate regions for telescope slew speeds that depend on the angular seperation
    // between the current coordinate and target coordinate. For instance, if the distance between two
    // coords is more than 30 degrees, we make the telescope move fast, and when the distance is reduced to
    // 5 degrees we switch to medium speed, and finally when we get to 2 degrees we switch to slow speed until
    // we hit our target. The values are somewhat arbitrary for now, they will be finely tuned depending on how
    // the telescope behaves.

    static const double ALT_FAST_REGION   = 30;
    static const double ALT_MEDIUM_REGION = 20;
    static const double ALT_SLOW_REGION   = 10;

    static const double AZ_FAST_REGION   = 30;
    static const double AZ_MEDIUM_REGION = 15;
    static const double AZ_SLOW_REGION   = 5;

    #if 0
    // FIXME This was applicable for Ujari. Update for KNRO
    
    // Saftey Zones (%). We warn if ALT reaches 60 degrees
    // and we enter danger is alt reaches 52 degrees
    static const double ALT_DANGER_ZONE  = 42.0;
    static const double ALT_WARNING_ZONE = 50.0;
    // We reach critical Alt of 45 degrees at which the telescope
    // will automatically go to HOME position.
    static const double ALT_CRITICAL_ZONE = 37.0;
    #endif
    
    // Three basic speeds in Hz
    static const float KNRO_FAST = 50.0;
    static const float KNRO_MEDIUM = 25.0;
    static const float KNRO_SLOW = 10.0;

    // ================
    // Tolerance levels
    // The tolerance level is defined as the angular seperation, in arc minutes, between the requested
    // coords and the current coords before we can declare that the slew is successful. 

   static const double SLEW_AZ_TOLERANCE  = 30.0;
   static const double SLEW_ALT_TOLERANCE = 30.0;

    static const double TRACK_AZ_TOLERANCE  = 30.0;
    static const double TRACK_ALT_TOLEAZNCE = 30.0;

    // ================
    // Update Rate: 10 Hz
    static const int update_period = 100000;
    // ================
    
    /* Functions */
 
    /* connect observatory */
    void connect();
    void disconnect();
    
    /* Safety checks functions */
    void check_safety();

    /* Error log functions */
    const char *get_knro_error_string(knroErrCode code);

    /* Send command buffer to inverter */
    void execute_slew();

    /* Modular stop functions */
    knroErrCode stop_all();
    void stop_az();
    void stop_alt();
    
    /* Simulation */
    void enable_simulation();
    void disable_simulation();    

    void park_telescope();
    void terminate_parking();

    void play_calibration_error();

    /* Return true if telescope is within tolerance limits in Az */
    bool is_az_done();
    /* Return true if telescope is within tolerance limits in Alt */
    bool is_alt_done();
    void update_alt_speed();
    void update_alt_dir(AltDirection dir);
    void update_az_speed();
    void update_az_dir(AzDirection dir);

    /* General functions */
    void init_properties();
    void reset_all_properties();

    /* Coordinates & Time */
    double lastAZ;
   
    /* Timing variables */
    static const int MAXIMUM_IDLE_TIME = 1800;			// Maximum 30 minutes idle time
    time_t last_execute_time;
    time_t now;

    /* Type of active slew stage */
    SlewStages slew_stage;

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
 
    /* Simulation */
    bool simulation;
    
    list <ISwitchVectorProperty *> switch_list;
    list <INumberVectorProperty *> number_list;
    list <ITextVectorProperty *> text_list;
    list <ILightVectorProperty *> light_list;

};


#endif
