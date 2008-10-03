/////////////////////////////////////////////////////////////
//
// Apn.h:  Common constants for the Alta (and derivative) camera systems
// 
// Copyright (c) 2003-2006 Apogee Instruments, Inc.
//
/////////////////////////////////////////////////////////////


#define APOGEE_DLL_OUTPUT 1
#ifdef APOGEE_DLL_OUTPUT

#ifdef LINUX
#define AltaDebugOutputString(__X__) fprintf(stderr,__X__)
#else
#define AltaDebugString(__X__) OutputDebugString(__X__)
#endif

#define AltaDebugPrint( __A__, __B__, __C__ ) sprintf( __A__, __B__, __C__ )
#else
#define AltaDebugString(__X__)
#define AltaDebugPrint( __A__, __B__, __C__ )
#endif
 
 
#define APN_DRIVER_VERSION				"3.1.8"
 
///////////////////////////////////
// Alta Plaform Constants
///////////////////////////////////
 

#define APN_HBINNING_MAX_ALTA                           10
#define APN_VBINNING_MAX_ALTA                           2048

#define APN_TIMER_RESOLUTION_ALTA                       0.00000256
#define APN_PERIOD_TIMER_RESOLUTION_ALTA        0.000000040

#define APN_TIMER_OFFSET_COUNT_ALTA                     3

#define APN_SEQUENCE_DELAY_RESOLUTION_ALTA      0.000327
#define APN_SEQUENCE_DELAY_MAXIMUM_ALTA         21.429945
#define APN_SEQUENCE_DELAY_MINIMUM_ALTA         0.000327

#define APN_EXPOSURE_TIME_MIN_ALTA                      0.00001         // 10us is the defined min.
#define APN_EXPOSURE_TIME_MAX_ALTA                      10990.0         // seconds

#define APN_TDI_RATE_RESOLUTION_ALTA            0.00000512
#define APN_TDI_RATE_MIN_ALTA                           0.00000512
#define APN_TDI_RATE_MAX_ALTA                           0.336
#define APN_TDI_RATE_DEFAULT_ALTA                       0.100

#define APN_VOLTAGE_RESOLUTION_ALTA                     0.00439453

#define APN_SHUTTER_CLOSE_DIFF_ALTA                     0.00001024

#define APN_STROBE_POSITION_MIN_ALTA            0.00000331
#define APN_STROBE_POSITION_MAX_ALTA            0.1677
#define APN_STROBE_POSITION_DEFAULT_ALTA        0.001

#define APN_STROBE_PERIOD_MIN_ALTA                      0.000000045
#define APN_STROBE_PERIOD_MAX_ALTA                      0.0026
#define APN_STROBE_PERIOD_DEFAULT_ALTA          0.001

#define APN_TEMP_COUNTS_ALTA                            4096
#define APN_TEMP_KELVIN_SCALE_OFFSET_ALTA       273.16

#define APN_TEMP_SETPOINT_MIN_ALTA                      213
#define APN_TEMP_SETPOINT_MAX_ALTA                      313

#define APN_TEMP_HEATSINK_MIN_ALTA                      240
#define APN_TEMP_HEATSINK_MAX_ALTA                      340

#define APN_TEMP_SETPOINT_ZERO_POINT_ALTA       2458
#define APN_TEMP_HEATSINK_ZERO_POINT_ALTA       1351

#define APN_TEMP_DEGREES_PER_BIT_ALTA           0.024414

#define APN_FAN_SPEED_OFF_ALTA                          0
#define APN_FAN_SPEED_LOW_ALTA                          3100
#define APN_FAN_SPEED_MEDIUM_ALTA                       3660
#define APN_FAN_SPEED_HIGH_ALTA                         4095

#define APN_GUIDER_RELAY_RESOLUTION_ALTA        0.0007509
#define APN_GUIDER_RELAY_MIN_ALTA                       0.005
#define APN_GUIDER_RELAY_MAX_ALTA                       40.0
#define APN_GUIDER_RELAY_OPEN_TIME_ALTA         0.0004
#define APN_GUIDER_RELAY_CLOSE_TIME_ALTA        0.0011



///////////////////////////////////
// Ascent Platform Constants
///////////////////////////////////

#define APN_HBINNING_MAX_ASCENT                                 8
#define APN_VBINNING_MAX_ASCENT                                 4095

#define APN_TIMER_RESOLUTION_ASCENT                             0.00000133

#define APN_PERIOD_TIMER_RESOLUTION_ASCENT              0.00000002078

#define APN_TIMER_OFFSET_COUNT_ASCENT                   3

#define APN_SEQUENCE_DELAY_RESOLUTION_ASCENT    0.00037547
#define APN_SEQUENCE_DELAY_MAXIMUM_ASCENT               24.606426
#define APN_SEQUENCE_DELAY_MINIMUM_ASCENT               0.00037547

#define APN_EXPOSURE_TIME_MIN_ASCENT                    0.00001
#define APN_EXPOSURE_TIME_MAX_ASCENT                    5712.3

#define APN_TDI_RATE_RESOLUTION_ASCENT                  0.00000533
#define APN_TDI_RATE_MIN_ASCENT                                 0.00000533

#define APN_TDI_RATE_MAX_ASCENT                                 0.349
#define APN_TDI_RATE_DEFAULT_ASCENT                             0.100

#define APN_VOLTAGE_RESOLUTION_ASCENT                   0.00439453

#define APN_SHUTTER_CLOSE_DIFF_ASCENT                   0.00000533

#define APN_STROBE_POSITION_MIN_ASCENT                  0.00000533
#define APN_STROBE_POSITION_MAX_ASCENT                  0.3493
#define APN_STROBE_POSITION_DEFAULT_ASCENT              0.001

#define APN_STROBE_PERIOD_MIN_ASCENT                    0.000000026
#define APN_STROBE_PERIOD_MAX_ASCENT                    0.00136
#define APN_STROBE_PERIOD_DEFAULT_ASCENT                0.001

#define APN_TEMP_COUNTS_ASCENT                                  4096

#define APN_TEMP_KELVIN_SCALE_OFFSET_ASCENT             273.16

#define APN_TEMP_SETPOINT_MIN_ASCENT                    213
#define APN_TEMP_SETPOINT_MAX_ASCENT                    313

#define APN_TEMP_HEATSINK_MIN_ASCENT                    240
#define APN_TEMP_HEATSINK_MAX_ASCENT                    240

#define APN_TEMP_SETPOINT_ZERO_POINT_ASCENT             2458
#define APN_TEMP_HEATSINK_ZERO_POINT_ASCENT             1351

#define APN_TEMP_DEGREES_PER_BIT_ASCENT                 0.024414

#define APN_FAN_SPEED_OFF_ASCENT                                0
#define APN_FAN_SPEED_LOW_ASCENT                                49611

#define APN_FAN_SPEED_MEDIUM_ASCENT                             58573
#define APN_FAN_SPEED_HIGH_ASCENT                               65535


#define APN_GUIDER_RELAY_RESOLUTION_ASCENT              0.0007509
#define APN_GUIDER_RELAY_MIN_ASCENT                             0.005
#define APN_GUIDER_RELAY_MAX_ASCENT                             40.0
#define APN_GUIDER_RELAY_OPEN_TIME_ASCENT               0.0004
#define APN_GUIDER_RELAY_CLOSE_TIME_ASCENT              0.0011

 

///////////////////////////////////
// Filter Wheel Constants
///////////////////////////////////

#define APN_FILTER_UNKNOWN_DESCR                                "Unknown"
#define APN_FILTER_UNKNOWN_MAX_POSITIONS                0

#define APN_FILTER_FW50_9R_DESCR                                "AI FW50 9R"
#define APN_FILTER_FW50_9R_MAX_POSITIONS                9

#define APN_FILTER_FW50_7S_DESCR                                "AI FW50 7S"
#define APN_FILTER_FW50_7S_MAX_POSITIONS                7

#define APN_FILTER_AFW30_7R_DESCR                               "AI AFW30 7R"
#define APN_FILTER_AFW30_7R_MAX_POSITIONS               7

#define APN_FILTER_AFW50_5R_DESCR                               "AI AFW50 5R"
#define APN_FILTER_AFW50_5R_MAX_POSITIONS               5

#define APN_FILTER_AFW25_4R_DESCR                               "AI AFW25 4R"
#define APN_FILTER_AFW25_4R_MAX_POSITIONS               4


