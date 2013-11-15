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
#include <libnova.h>

#include <ogg_util.h>

#include <inditelescope.h>
#include <indilogger.h>

#include "encoder.h"
#include "inverter.h"

using std::string;
using std::list;

#define CMD_BUF_SIZE		512

class knroObservatory : public INDI::Telescope
{
 public:

 knroObservatory();
 ~knroObservatory();

 virtual const char *getDefaultName();
 virtual bool Connect();
 virtual bool Disconnect();
 virtual bool ReadScopeStatus();
 virtual bool initProperties();
 virtual void ISGetProperties (const char *dev);
 virtual bool updateProperties();
 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

protected:

 virtual bool MoveNS(TelescopeMotionNS dir);
 virtual bool MoveWE(TelescopeMotionWE dir);
 virtual bool Abort();

 virtual bool updateLocation(double latitude, double longitude, double elevation);

 bool Goto(double RA,double DEC);
 bool Park();
 bool canSync() { return false;}
 bool canPark() { return true;}

 void simulationTriggered(bool enable);
 
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

    /* Az Safety */
    ILight AzSafetyL[1];  
    ILightVectorProperty AzSafetyLP;

    /* Horizontal Coords */
    INumber               HorizontalCoordsN[2];
    INumberVectorProperty HorizontalCoordsNP;
    /* Other */

    enum SlewSpeed { SLOW_SPEED = 0 , MEDIUM_SPEED, FAST_SPEED };
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

    static const double ALT_MEDIUM_REGION = 5;
    static const double ALT_SLOW_REGION   = 2;
    static const double AZ_MEDIUM_REGION = 15;
    static const double AZ_SLOW_REGION   = 5;

    // Telescope mechanical limits.
    static const double ALT_ZENITH_LIMIT = 90.0;
    static const double ALT_HORIZON_LIMIT = 46.1;

    /* After 10 counters (10 x 500 ms POLL = 5 seconds), check to see if the motion was stopped due
       to hitting the limit switch or due to some mechanical failure
       NOTE: Should find an active way of monitoring the limit switch.
    */
    static const int ALT_STOP_LIMIT = 10;

    /* How far from ALT_HORIZON_LIMIT or ALT_ZENITH_LIMIT the dish must be to be considered in OK motion state
       That is, it got stopped by a limit switch and not due to mechanical failure
       NOTE: Should find an active way of monitoring the limit switch.
    */
    static const int ALT_STOP_RANGE = 2;


    /* Limit switches are at 180 degrees */
    static const double AZ_LIMIT = 180.0;
    static const int AZ_STOP_LIMIT = 10;
    static const int AZ_STOP_RANGE = 2;

    
    // Three basic speeds in Hz
    static const float AZ_KNRO_FAST = 50.0;
    static const float AZ_KNRO_MEDIUM = 45.0;
    static const float AZ_KNRO_SLOW = 40;
    static const float AZ_KNRO_TRACK = 10;
    
    static const float ALT_KNRO_FAST = 50.0;
    static const float ALT_KNRO_MEDIUM = 45.0;
    static const float ALT_KNRO_SLOW = 40;
    static const float ALT_KNRO_TRACK = 10;


    // Slewing & Tracking threshold
    static const float AZ_TRACKING_THRESHOLD = 0.5;
    static const float ALT_TRACKING_THRESHOLD = 0.25;
    static const float AZ_SLEWING_THRESHOLD = 0.1;
    static const float ALT_SLEWING_THRESHOLD = 0.1;

    /* Functions */
     
    /* Safety checks functions */
    void check_safety();

    /* Error log functions */
    const char *get_knro_error_string(knroErrCode code);

    /* Modular stop functions */
    knroErrCode stop_all();
    bool stop_az();
    bool stop_alt();
    
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

    /* Coordinates & Time */
    double lastAz;
    double lastAlt;
    double initialAz;
    unsigned int maxAltStopCounter;
    unsigned int maxAzStopCounter;
   
    /* Timing variables */
    static const int MAXIMUM_IDLE_TIME = 1800;			// Maximum 30 minutes idle time
    time_t last_execute_time;
    time_t now;

   /* Threading variables */
    pthread_t az_encoder_thread;
    pthread_t alt_encoder_thread;
    static pthread_mutex_t az_encoder_mutex;
    static pthread_mutex_t alt_encoder_mutex;
   
    /* Warning sounds */
    /*OggFile park_alert;
    OggFile calibration_error;*/
    OggFile slew_complete;
    OggFile slew_error;
    OggFile slew_busy;

    /* Simulation */
    bool simulation;
    
    ln_lnlat_posn observer;

    ln_hrz_posn currentHorCoords;
    ln_hrz_posn targetHorCoords;

    ln_equ_posn currentEQCoords;
    ln_equ_posn targetEQCoords;

};


#endif
