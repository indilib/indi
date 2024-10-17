/*
  INDI Driver for DreamFocuser

  Copyright (C) 2016 Piotr Dlugosz

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

*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <memory>
#include <indicom.h>

#include "dreamfocuser.h"
#include "config.h"
#include "connectionplugins/connectionserial.h"

/*
COMMANDS:

MMabcd0z - set position x
response - MMabcd0z

MH00000z - stop
response - MH00000z

MP00000z - read position
response - MPabcd0z

MI00000z - is moving
response - MI000d0z - d = 1: yes, 0: no

MT00000z - read temperature
response - MT00cd0z - temperature = ((c<<8)|d)/16.0

MA0000nz - read memory dword - n = address
response - MAabcd0z(?)

MBabcdnz - write memory dword - abcd = content, n = address
response - MBabcd0z(?)

MC0000nz - read memory word - n = address
response - 

MDab00nz - write memory word - ab = content, n = address
response - 

----

MR000d0z - move with speed d & 0b1111111 (0 - 127), direction d >> 7 (1 up, 0 down)
response - MR000d0z

MW00000z - is calibrated
response - MW000d0z - d = 1: yes (absolute mode), 0: no (relative mode)

MZabcd0z - calibrate toposition x
response - MZabcd0z

MV00000z - firmware version
response - MV00cd0z - version: c.d

MG00000z - park
response - MG00000z
*/

#define PARK_PARK 0
#define PARK_UNPARK 1

// We declare an auto pointer to DreamFocuser.
static std::unique_ptr<DreamFocuser> dreamFocuser(new DreamFocuser());

void ISPoll(void *p);


/****************************************************************
**
**
*****************************************************************/

DreamFocuser::DreamFocuser()
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);

    isAbsolute = false;
    isMoving = false;
    isParked = 0;
    isVcc12V = false;

    setVersion(2, 1);

}

bool DreamFocuser::initProperties()
{
    INDI::Focuser::initProperties();

    // Default speed
    //FocusSpeedN[0].min = 0;
    //FocusSpeedN[0].max = 127;
    //FocusSpeedN[0].value = 50;
    //IUUpdateMinMax(&FocusSpeedNP);

    // Max Position
    //    IUFillNumber(&MaxPositionN[0], "MAXPOSITION", "Ticks", "%.f", 1., 500000., 1000., 300000);
    //    IUFillNumberVector(&MaxPositionNP, MaxPositionN, 1, getDeviceName(), "MAXPOSITION", "Max Absolute Position", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);

    //    IUFillNumber(&MaxTravelN[0], "MAXTRAVEL", "Ticks", "%.f", 1., 500000., 1000., 300000.);
    //    IUFillNumberVector(&MaxTravelNP, MaxTravelN, 1, getDeviceName(), "MAXTRAVEL", "Max Relative Travel", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE );

    //    // Focus Sync
    //    IUFillSwitch(&SyncS[0], "SYNC", "Synchronize", ISS_OFF);
    //    IUFillSwitchVector(&SyncSP, SyncS, 1, getDeviceName(), "SYNC", "Synchronize", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Focus Park
    IUFillSwitch(&ParkS[PARK_PARK], "PARK", "Park", ISS_OFF);
    IUFillSwitch(&ParkS[PARK_UNPARK], "UNPARK", "Unpark", ISS_OFF);
    IUFillSwitchVector(&ParkSP, ParkS, 2, getDeviceName(), "PARK", "Park", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -100, 100, 0, 0);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser humidity and dewpoint
    IUFillNumber(&WeatherN[0], "FOCUS_HUMIDITY", "Humidity [%]", "%6.1f", 0, 100, 0, 0);
    IUFillNumber(&WeatherN[1], "FOCUS_DEWPOINT", "Dew point [C]", "%6.1f", -100, 100, 0, 0);
    IUFillNumberVector(&WeatherNP, WeatherN, 2, getDeviceName(), "FOCUS_WEATHER", "Weather", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // We init here the property we wish to "snoop" from the target device
    IUFillSwitch(&StatusS[0], "ABSOLUTE", "Absolute", ISS_OFF);
    IUFillSwitch(&StatusS[1], "MOVING", "Moving", ISS_OFF);
    IUFillSwitch(&StatusS[2], "PARKED", "Parked", ISS_OFF);
    IUFillSwitchVector(&StatusSP, StatusS, 3, getDeviceName(), "STATUS", "Status", MAIN_CONTROL_TAB, IP_RO, ISR_NOFMANY, 0, IPS_IDLE);

    //    PresetN[0].min = PresetN[1].min = PresetN[2].min = FocusAbsPosN[0].min = -MaxPositionN[0].value;
    //    PresetN[0].max = PresetN[1].max = PresetN[2].max = FocusAbsPosN[0].max = MaxPositionN[0].value;
    //    strcpy(PresetN[0].format, "%6.0f");
    //    strcpy(PresetN[1].format, "%6.0f");
    //    strcpy(PresetN[2].format, "%6.0f");
    //    PresetN[0].step = PresetN[1].step = PresetN[2].step = FocusAbsPosN[0].step = DREAMFOCUSER_STEP_SIZE;

    // Maximum position can't be changed from driver
    FocusMaxPosNP.p = IP_RO;

    FocusAbsPosN[0].value = 0;
    FocusRelPosN[0].min = -FocusMaxPosN[0].max;
    FocusRelPosN[0].max = FocusMaxPosN[0].max;
    FocusRelPosN[0].step = DREAMFOCUSER_STEP_SIZE;
    FocusRelPosN[0].value = 5 * DREAMFOCUSER_STEP_SIZE;

    serialConnection->setDefaultPort("/dev/ttyACM0");
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    setDefaultPollingPeriod(500);

    return true;
}

bool DreamFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        //defineProperty(&SyncSP);
        defineProperty(&ParkSP);
        defineProperty(&TemperatureNP);
        defineProperty(&WeatherNP);
        defineProperty(&StatusSP);
        //defineProperty(&MaxPositionNP);
        //defineProperty(&MaxTravelNP);
    }
    else
    {
        //deleteProperty(SyncSP.name);
        deleteProperty(ParkSP.name);
        deleteProperty(TemperatureNP.name);
        deleteProperty(WeatherNP.name);
        deleteProperty(StatusSP.name);
        //deleteProperty(MaxPositionNP.name);
        //deleteProperty(MaxTravelNP.name);
    }
    return true;
}


/*
void DreamFocuser::ISGetProperties(const char *dev)
{
  if(dev && strcmp(dev,getDeviceName()))
  {
    defineProperty(&MaxPositionNP);
    loadConfig(true, "MAXPOSITION");
  };
  return INDI::Focuser::ISGetProperties(dev);
}
*/


//bool DreamFocuser::saveConfigItems(FILE *fp)
//{
//    INDI::Focuser::saveConfigItems(fp);

//    IUSaveConfigNumber(fp, &MaxPositionNP);
//    IUSaveConfigNumber(fp, &MaxTravelNP);

//    return true;
//}

//bool DreamFocuser::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
//{

//    if(strcmp(dev, getDeviceName()) == 0)
//    {
//        // Max Position
//        if (!strcmp(MaxPositionNP.name, name))
//        {
//            IUUpdateNumber(&MaxPositionNP, values, names, n);

//            if (MaxPositionN[0].value > 0)
//            {
//                PresetN[0].min = PresetN[1].min = PresetN[2].min = FocusAbsPosN[0].min = -MaxPositionN[0].value;;
//                PresetN[0].max = PresetN[1].max = PresetN[2].max = FocusAbsPosN[0].max = MaxPositionN[0].value;
//                IUUpdateMinMax(&FocusAbsPosNP);
//                IUUpdateMinMax(&PresetNP);
//                IDSetNumber(&FocusAbsPosNP, nullptr);

//                LOGF_DEBUG("Focuser absolute limits: min (%g) max (%g)", FocusAbsPosN[0].min, FocusAbsPosN[0].max);
//            }

//            MaxPositionNP.s = IPS_OK;
//            IDSetNumber(&MaxPositionNP, nullptr);
//            return true;
//        }


//        // Max Travel
//        if (!strcmp(MaxTravelNP.name, name))
//        {
//            IUUpdateNumber(&MaxTravelNP, values, names, n);

//            if (MaxTravelN[0].value > 0)
//            {
//                FocusRelPosN[0].min = 0;
//                FocusRelPosN[0].max = MaxTravelN[0].value;
//                IUUpdateMinMax(&FocusRelPosNP);
//                IDSetNumber(&FocusRelPosNP, nullptr);

//                LOGF_DEBUG("Focuser relative limits: min (%g) max (%g)", FocusRelPosN[0].min, FocusRelPosN[0].max);
//            }

//            MaxTravelNP.s = IPS_OK;
//            IDSetNumber(&MaxTravelNP, nullptr);
//            return true;
//        }

//    }

//    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);

//}


bool DreamFocuser::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev, getDeviceName()) == 0)
    {
        // Park
        if (!strcmp(ParkSP.name, name))
        {
            IUUpdateSwitch(&ParkSP, states, names, n);
            int index = IUFindOnSwitchIndex(&ParkSP);
            IUResetSwitch(&ParkSP);

            if ( (isParked && (index == PARK_UNPARK)) || ( !isParked && (index == PARK_PARK)) )
            {
                LOG_INFO("Park, issuing command.");
                if ( setPark() )
                {
                    //ParkSP.s = IPS_OK;
                    FocusAbsPosNP.s = IPS_OK;
                    IDSetNumber(&FocusAbsPosNP, nullptr);
                }
                else
                    ParkSP.s = IPS_ALERT;
            }
            IDSetSwitch(&ParkSP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool DreamFocuser::SyncFocuser(uint32_t ticks)
{
    return setSync(ticks);
}

/****************************************************************
**
**
*****************************************************************/

bool DreamFocuser::getTemperature()
{
    if ( dispatch_command('T') )
    {
        currentTemperature = ((short int)( (currentResponse.c << 8) | currentResponse.d )) / 10.;
        currentHumidity = ((short int)( (currentResponse.a << 8) | currentResponse.b )) / 10.;
    }
    else
        return false;
    return true;
}

bool DreamFocuser::Handshake()
{
    return getStatus();
}

bool DreamFocuser::getStatus()
{
    LOG_DEBUG("getStatus.");
    if ( dispatch_command('I') )
    {
        isMoving = ( currentResponse.d & 3 ) != 0 ? true : false;
        //isZero = ( (currentResponse.d>>2) & 1 )  == 1;
        isParked = (currentResponse.d>>3) & 3;
        isVcc12V = ( (currentResponse.d>>5) & 1 ) == 1;
    }
    else
        return false;

    if ( dispatch_command('W') ) // Is absolute?
        isAbsolute = currentResponse.d == 1 ? true : false;
    else
        return false;

    return true;
}


bool DreamFocuser::getPosition()
{
    //int32_t pos;

    if ( dispatch_command('P') )
        currentPosition = (currentResponse.a << 24) | (currentResponse.b << 16) | (currentResponse.c << 8) | currentResponse.d;
    else
        return false;

    return true;
}


bool DreamFocuser::setPosition( int32_t position)
{
    if ( dispatch_command('M', position) )
        if ( ((currentResponse.a << 24) | (currentResponse.b << 16) | (currentResponse.c << 8) | currentResponse.d) == position )
        {
            LOGF_DEBUG("Moving to position %d", position);
            return true;
        };
    return false;
}

bool DreamFocuser::getMaxPosition()
{
    if ( dispatch_command('A', 0, 3) )
    {
        currentMaxPosition = (currentResponse.a << 24) | (currentResponse.b << 16) | (currentResponse.c << 8) | currentResponse.d;
        LOGF_DEBUG("getMaxPosition: %d", currentMaxPosition);
        return true;
    }
    else
      LOG_ERROR("getMaxPosition error");

    return false;
}


bool DreamFocuser::setSync( uint32_t position)
{
    if ( dispatch_command('Z', position) )
        if ( static_cast<uint32_t>((currentResponse.a << 24) | (currentResponse.b << 16) | (currentResponse.c << 8) | currentResponse.d) == position )
        {
            LOGF_DEBUG("Syncing to position %d", position);
            return true;
        };
        LOG_ERROR("Sync failed.");
        return false;
}

bool DreamFocuser::setPark()
{
    if (isAbsolute == false)
    {
        LOG_ERROR("Focuser is not in Absolute mode. Please sync before to allow parking.");
        return false;
    }

    if ( dispatch_command('G') )
    {
      LOG_INFO( "Focuser park command.");
      return true;
    }
    LOG_ERROR("Park failed.");
    return false;
}

bool DreamFocuser::AbortFocuser()
{
    if ( dispatch_command('H') )
    {
        LOG_INFO("Focusing aborted.");
        return true;
    };
    LOG_ERROR("Abort failed.");
    return false;
}


/*
IPState DreamFocuser::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
  DreamFocuserCommand c;
  unsigned char d = (unsigned char) (speed & 0b01111111) | (dir == FOCUS_INWARD) ? 0b10000000 : 0;

  if ( dispatch_command('R', d) )
  {
    gettimeofday(&focusMoveStart,nullptr);
    focusMoveRequest = duration/1000.0;
    if ( read_response() )
      if ( ( currentResponse.k == 'R' ) && (currentResponse.d == d) )
        if (duration <= getCurrentPollingPeriod())
        {
          usleep(getCurrentPollingPeriod() * 1000);
          AbortFocuser();
          return IPS_OK;
        }
        else
          return IPS_BUSY;
  }
  return IPS_ALERT;
}
*/

IPState DreamFocuser::MoveAbsFocuser(uint32_t ticks)
{
    LOGF_DEBUG("MoveAbsPosition: %d", ticks);

    if (isAbsolute == false)
    {
        LOG_ERROR("Focuser is not in Absolute mode. Please sync.");
        return IPS_ALERT;
    }

    if (isParked != 0)
    {
        LOG_ERROR("Please unpark before issuing any motion commands.");
        return IPS_ALERT;
    }
    if ( setPosition(ticks) )
    {
        FocusAbsPosNP.s = IPS_OK;
        IDSetNumber(&FocusAbsPosNP, nullptr);
        return IPS_OK;
    }
    return IPS_ALERT;
}

IPState DreamFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t finalTicks = currentPosition + ((int32_t)ticks * (dir == FOCUS_INWARD ? -1 : 1));

    LOGF_DEBUG("MoveRelPosition: %d", finalTicks);

    if (isParked != 0)
    {
        LOG_ERROR("Please unpark before issuing any motion commands.");
        return IPS_ALERT;
    }

    if ( setPosition(finalTicks) )
    {
        FocusRelPosNP.s = IPS_OK;
        IDSetNumber(&FocusRelPosNP, nullptr);
        return IPS_OK;
    }
    return IPS_ALERT;
}


void DreamFocuser::TimerHit()
{

    if ( ! isConnected() )
        return;

    int oldAbsStatus = FocusAbsPosNP.s;
    int32_t oldPosition = currentPosition;

    if ( getMaxPosition() )
    {
        if ( FocusMaxPosN[0].value != currentMaxPosition ) {
            FocusMaxPosN[0].value = currentMaxPosition;
            FocusMaxPosNP.s = IPS_OK;
            IDSetNumber(&FocusMaxPosNP, nullptr);
            SetFocuserMaxPosition(currentMaxPosition);
        }
    }
    else
        FocusMaxPosNP.s = IPS_ALERT;

    if ( getStatus() )
    {

        StatusSP.s = IPS_OK;
        if ( isMoving )
        {
            //LOG_INFO("Moving" );
            FocusAbsPosNP.s = IPS_BUSY;
            StatusS[1].s = ISS_ON;
        }
        else
        {
            if ( FocusAbsPosNP.s != IPS_IDLE )
                FocusAbsPosNP.s = IPS_OK;
            StatusS[1].s = ISS_OFF;
        };

        if ( isParked == 1 )
        {
            ParkSP.s = IPS_BUSY;
            StatusS[2].s = ISS_ON;
            ParkS[0].s = ISS_ON;
        }
        else if ( isParked == 2 )
        {
            ParkSP.s = IPS_OK;
            StatusS[2].s = ISS_ON;
            ParkS[0].s = ISS_ON;
        }
        else
        {
            StatusS[2].s = ISS_OFF;
            ParkS[1].s = ISS_ON;
            ParkSP.s = IPS_IDLE;
        }

        if ( isAbsolute )
        {
            StatusS[0].s = ISS_ON;
            if ( FocusAbsPosN[0].min != 0 )
            {
                FocusAbsPosN[0].min = 0;
                IDSetNumber(&FocusAbsPosNP, nullptr);
            }
        }
        else
        {
            if ( FocusAbsPosN[0].min == 0 )
            {
                FocusAbsPosN[0].min = -FocusAbsPosN[0].max;
                IDSetNumber(&FocusAbsPosNP, nullptr);
            }
            StatusS[0].s = ISS_OFF;
        }

    }
    else
        StatusSP.s = IPS_ALERT;

    if ( getTemperature() )
    {
        TemperatureNP.s = TemperatureN[0].value != currentTemperature ? IPS_BUSY : IPS_OK;
        WeatherNP.s = WeatherN[1].value != currentHumidity ? IPS_BUSY : IPS_OK;
        TemperatureN[0].value = currentTemperature;
        WeatherN[0].value = currentHumidity;
        WeatherN[1].value = pow(currentHumidity / 100, 1.0 / 8) * (112 + 0.9 * currentTemperature) + 0.1 * currentTemperature - 112;
    }
    else
    {
        TemperatureNP.s = IPS_ALERT;
        WeatherNP.s = IPS_ALERT;
    }

    if ( FocusAbsPosNP.s != IPS_IDLE )
    {
        if ( getPosition() )
        {
            if ( oldPosition != currentPosition )
            {
                FocusAbsPosNP.s = IPS_BUSY;
                StatusS[1].s = ISS_ON;
                FocusAbsPosN[0].value = currentPosition;
            }
            else
            {
                StatusS[1].s = ISS_OFF;
                FocusAbsPosNP.s = IPS_OK;
            }
            //if ( currentPosition < 0 )
            // FocusAbsPosNP.s = IPS_ALERT;
        }
        else
            FocusAbsPosNP.s = IPS_ALERT;
    }


    if ((oldAbsStatus != FocusAbsPosNP.s) || (oldPosition != currentPosition))
        IDSetNumber(&FocusAbsPosNP, nullptr);

    IDSetNumber(&TemperatureNP, nullptr);
    IDSetNumber(&WeatherNP, nullptr);
    //IDSetSwitch(&SyncSP, nullptr);
    IDSetSwitch(&StatusSP, nullptr);
    IDSetSwitch(&ParkSP, NULL);

    SetTimer(getCurrentPollingPeriod());

}


/****************************************************************
**
**
*****************************************************************/

unsigned char DreamFocuser::calculate_checksum(DreamFocuserCommand c)
{
    unsigned char z;

    // calculate checksum
    z = (c.M + c.k + c.a + c.b + c.c + c.d + c.addr) & 0xff;
    return z;
}

bool DreamFocuser::send_command(char k, uint32_t l, unsigned char addr)
{
    DreamFocuserCommand c;
    int err_code = 0, nbytes_written = 0;
    char dreamFocuser_error[DREAMFOCUSER_ERROR_BUFFER];
    unsigned char *x = (unsigned char *)&l;

    switch(k)
    {
        case 'M':
        case 'Z':
            c.a = x[3];
            c.b = x[2];
            c.c = x[1];
            c.d = x[0];
            break;
        case 'H':
        case 'P':
        case 'I':
        case 'T':
        case 'W':
        case 'G':
        case 'V':
            c.a = 0;
            c.b = 0;
            c.c = 0;
            c.d = 0;
            break;
        case 'R':
            c.a = 0;
            c.b = 0;
            c.c = 0;
            c.d = x[0];
            break;
        case 'A':
        case 'C':
            c.a = 0;
            c.b = 0;
            c.c = 0;
            c.d = x[0];
            break;
        case 'B':
        case 'D':
            c.a = x[3];
            c.b = x[2];
            c.c = 0;
            c.d = x[0];
            break;
        default:
            DEBUGF(INDI::Logger::DBG_ERROR, "Unknown command: '%c'", k);
            return false;
    }
    c.k = k;
    c.addr = addr;
    c.z = calculate_checksum(c);

    LOGF_DEBUG("Sending command: c=%c, a=%hhu, b=%hhu, c=%hhu, d=%hhu ($%hhx), n=%hhu, z=%hhu", c.k, c.a, c.b, c.c, c.d, c.d, c.addr, c.z);

    tcflush(PortFD, TCIOFLUSH);

    if ( (err_code = tty_write(PortFD, (char *)&c, sizeof(c), &nbytes_written) != TTY_OK))
    {
        tty_error_msg(err_code, dreamFocuser_error, DREAMFOCUSER_ERROR_BUFFER);
        LOGF_ERROR("TTY error detected: %s", dreamFocuser_error);
        return false;
    }

    LOGF_DEBUG("Sending complete. Number of bytes written: %d", nbytes_written);

    return true;
}

bool DreamFocuser::read_response()
{
    int err_code = 0, nbytes_read = 0, z;
    char err_msg[DREAMFOCUSER_ERROR_BUFFER];

    //LOG_DEBUG("Read response");

    // Read a single response
    if ( (err_code = tty_read(PortFD, (char *)&currentResponse, sizeof(currentResponse), 5, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(err_code, err_msg, 32);
        LOGF_ERROR("TTY error detected: %s", err_msg);
        return false;
    }
    LOGF_DEBUG("Response: %c, a=%hhu, b=%hhu, c=%hhu, d=%hhu ($%hhx), n=%hhu, z=%hhu", currentResponse.k, currentResponse.a, currentResponse.b, currentResponse.c, currentResponse.d, currentResponse.d, currentResponse.addr, currentResponse.z);

    if ( nbytes_read != sizeof(currentResponse) )
    {
        LOGF_ERROR("Number of bytes read: %d, expected: %d", nbytes_read, sizeof(currentResponse));
        return false;
    }

    z = calculate_checksum(currentResponse);
    if ( z != currentResponse.z )
    {
        LOGF_ERROR("Response checksum in not correct %hhu, expected: %hhu", currentResponse.z, z );
        return false;
    }

    if ( currentResponse.k == '!' )
    {
        LOG_ERROR("Focuser reported unrecognized command.");
        return false;
    }

    if ( currentResponse.k == '?' )
    {
        LOG_ERROR("Focuser reported bad checksum.");
        return false;
    }

    return true;
}

bool DreamFocuser::dispatch_command(char k, uint32_t l, unsigned char addr)
{
    LOG_DEBUG("send_command");
    if ( send_command(k, l, addr) )
    {
        if ( read_response() )
        {
            LOG_DEBUG("check currentResponse.k");
            if ( currentResponse.k == k )
                return true;
        }
    }
    return false;
}

/****************************************************************
**
**
*****************************************************************/

const char * DreamFocuser::getDefaultName()
{
    return (char *)"DreamFocuser";
}
