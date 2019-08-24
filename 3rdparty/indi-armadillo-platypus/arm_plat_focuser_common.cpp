/*
    Lunatico Armadillo & Platypus Focusers

    (c) Lunatico Astronomia 2017, Jaime Alemany
    Based on previous drivers by Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "arm_plat_focuser_common.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#ifdef ARMADILLO
#define CONTROLLER_NAME "Armadillo"
#else
#define CONTROLLER_NAME "Platypus"
#endif

#define ArmPlat_TIMEOUT 2
#define FOCUS_SETTINGS_TAB "Settings"

#define SLP_SEND_BUF_SIZE 80

#define OPERATIVES	2		// relating the hw/fw info from the controller
#define MODELS		4

static std::unique_ptr<ArmPlat> armplat(new ArmPlat());

void ISGetProperties(const char *dev)
{
    armplat->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    armplat->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    armplat->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    armplat->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    armplat->ISSnoopDevice(root);
}

ArmPlat::ArmPlat()
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT    |
                      FOCUSER_CAN_SYNC     |
                      FOCUSER_HAS_BACKLASH);
}

bool ArmPlat::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Internal/external temperature sensor backlash
    IUFillSwitch(&IntExtTempSensorS[INT_TEMP_SENSOR], "Internal", "", ISS_ON);
    IUFillSwitch(&IntExtTempSensorS[EXT_TEMP_SENSOR], "External", "", ISS_OFF);
    IUFillSwitchVector(&IntExtTempSensorSP, IntExtTempSensorS, 2, getDeviceName(), "Temperature sensor in use", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Peripheral Port
    IUFillSwitch(&PerPortS[PORT_MAIN], "Main", "", ISS_ON );
    IUFillSwitch(&PerPortS[PORT_EXP], "Exp", "", ISS_OFF );
#   ifdef PLATYPUS
    IUFillSwitch(&PerPortS[PORT_THIRD], "Third", "", ISS_OFF );
    IUFillSwitchVector(&PerPortSP, PerPortS, 3, getDeviceName(), "Port in use", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
#   else
    IUFillSwitchVector(&PerPortSP, PerPortS, 2, getDeviceName(), "Port in use", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
#   endif

    // HalfStep
    IUFillSwitch(&HalfStepS[HALFSTEP_OFF], "Off", "", ISS_ON);
    IUFillSwitch(&HalfStepS[HALFSTEP_ON], "On", "", ISS_OFF);
    IUFillSwitchVector(&HalfStepSP, HalfStepS, 2, getDeviceName(), "Half step", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Wiring
    IUFillSwitch(&WiringS[WIRING_LUNATICO_NORMAL], "Lunatico Normal", "", ISS_ON);
    IUFillSwitch(&WiringS[WIRING_LUNATICO_REVERSED], "Lunatico Reverse", "", ISS_OFF);
    IUFillSwitch(&WiringS[WIRING_RFMOONLITE_NORMAL], "RF/Moonlite Normal", "", ISS_OFF);
    IUFillSwitch(&WiringS[WIRING_RFMOONLITE_REVERSED], "RF/Moonlite Reverse", "", ISS_OFF);
    IUFillSwitchVector(&WiringSP, WiringS, 4, getDeviceName(), "Wiring", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Max Speed
    // our internal speed is in usec/step, with a reasonable range from 500.000.usec for dc motors simulating steps to
    // 50 usec optimistic speed for very small steppers
    // So our range is 10.000.-
    // and the conversion is usec/step = 500000 - ((INDISpeed - 1) * 50)
    // with our default and standard 10.000usec being 9800 (9801 actually)
    IUFillNumber(&MaxSpeedN[0], "Value", "", "%6.0f", 1., 10000., 100., 9800.);		// min, max, step, value
    IUFillNumberVector(&MaxSpeedNP, MaxSpeedN, 1, getDeviceName(), "MaxSpeed", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_OK);

    // Enable/Disable backlash
//    IUFillSwitch(&BacklashCompensationS[BACKLASH_ENABLED], "Enable", "", ISS_OFF);
//    IUFillSwitch(&BacklashCompensationS[BACKLASH_DISABLED], "Disable", "", ISS_ON);
//    IUFillSwitchVector(&FocuserBacklashSP, BacklashCompensationS, 2, getDeviceName(), "Backlash Compensation", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

//    // Backlash Value
//    IUFillNumber(&BacklashN[0], "Value", "", "%.f", 0, 200, 1., 0.);
//    IUFillNumberVector(&BacklashNP, BacklashN, 1, getDeviceName(), "Backlash", "", FOCUS_SETTINGS_TAB, IP_RW, 0, IPS_IDLE);
    FocusBacklashN[0].min = 0;
    FocusBacklashN[0].max = 200;
    FocusBacklashN[0].step = 10;
    FocusBacklashN[0].value = 0;

    // Motor Types
    IUFillSwitch(&MotorTypeS[MOTOR_UNIPOLAR], "Unipolar", "", ISS_ON);
    IUFillSwitch(&MotorTypeS[MOTOR_BIPOLAR], "Bipolar", "", ISS_OFF);
    IUFillSwitch(&MotorTypeS[MOTOR_DC], "DC", "", ISS_OFF);
    IUFillSwitch(&MotorTypeS[MOTOR_STEPDIR], "Step-Dir", "", ISS_OFF);
    IUFillSwitchVector(&MotorTypeSP, MotorTypeS, 4, getDeviceName(), "Motor Type", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware Version
    IUFillText(&FirmwareVersionT[0], "Version", "Version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);


    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 5000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 100;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 100000.;
    FocusAbsPosN[0].value = 50000;
    FocusAbsPosN[0].step  = 5000;

    // Focus Sync
//    IUFillNumber(&SyncN[0], "SYNC", "Ticks", "%.f", 0, 100000., 0., 0.);
//    IUFillNumberVector(&SyncNP, SyncN, 1, getDeviceName(), "Sync", "", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE );

    addDebugControl();

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    return true;
}


bool ArmPlat::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineSwitch(&PerPortSP);
        defineNumber(&MaxSpeedNP);
        defineNumber(&TemperatureNP);
        defineSwitch(&IntExtTempSensorSP);
//        defineSwitch(&FocuserBacklashSP);
//        defineNumber(&BacklashNP);
        defineSwitch(&HalfStepSP);
        defineSwitch(&MotorTypeSP);
        defineSwitch(&WiringSP);
        //defineNumber(&SyncNP);
        defineText(&FirmwareVersionTP);
        if ( !loadConfig() )
                LOG_ERROR("Error loading config" );
    }
    else
    {
        //deleteProperty(TemperatureNP.name);

        deleteProperty(PerPortSP.name);
        deleteProperty(WiringSP.name);
        deleteProperty(HalfStepSP.name);
        deleteProperty(TemperatureNP.name);
        deleteProperty(IntExtTempSensorSP.name);
//        deleteProperty(FocuserBacklashSP.name);
//        deleteProperty(BacklashNP.name);
        deleteProperty(MotorTypeSP.name);
        deleteProperty(MaxSpeedNP.name);
        deleteProperty(FirmwareVersionTP.name);
        //deleteProperty(SyncNP.name);
    }

    return true;
}

//bool ArmPlat::Connect()
//{
//        INDI::Focuser::Connect();

//        return true;
//}


bool ArmPlat::Handshake()
{
    if (echo())
    {
        LOG_INFO(CONTROLLER_NAME " is online.");
        return true;
    }

    LOG_INFO("Error communicating with the " CONTROLLER_NAME ", please ensure it is powered and the port is correct.");
    return false;
}

const char *ArmPlat::getDefaultName()
{
    return  CONTROLLER_NAME " focuser";
}

bool ArmPlat::echo()
{
    int rc = -1;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    sprintf( cmd, "!seletek version#" );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        const char *operative[ OPERATIVES + 1 ] = { "", "Bootloader", "Error" };
        const char *models[ MODELS + 1 ] = { "Error", "Seletek", "Armadillo", "Platypus", "Dragonfly" };
        int fwmaj, fwmin, model, oper;
        char txt[ 80 ];

        oper = rc / 10000;		// 0 normal, 1 bootloader
        model = ( rc / 1000 ) % 10;	// 1 seletek, etc.
        fwmaj = ( rc / 100 ) % 10;
        fwmin = ( rc % 100 );
        if ( oper >= OPERATIVES ) oper = OPERATIVES;
        if ( model >= MODELS ) model = 0;
        sprintf( txt, "%s %s fwv %d.%d", operative[ oper ], models[ model ], fwmaj, fwmin );
        if ( strcmp( models[ model ], CONTROLLER_NAME ) )
                DEBUGF( INDI::Logger::DBG_WARNING, "Actual model (%s) and driver (" CONTROLLER_NAME ") mismatch - can lead to limited operability", models[model] );

        IUSaveText( &FirmwareVersionT[0], txt );
        LOGF_INFO("Setting version to [%s]", txt );

        return true;
    }
    return false;
}

bool ArmPlat::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    //IDLog( "NewSwitch: %s %s %s %d\n", dev, name, names[ 0 ], n );
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Connection!
        /////////////////////////////////////////////
        // if (!strcmp(name, ConnectionSP.name))
        // {
            // return true;
        // }
        /////////////////////////////////////////////
        // Backlash
        /////////////////////////////////////////////
//        if (!strcmp(name, FocuserBacklashSP.name))
//        {
//            IUUpdateSwitch(&FocuserBacklashSP, states, names, n);
//            bool rc = false;
//            if (IUFindOnSwitchIndex(&FocuserBacklashSP) == BACKLASH_ENABLED)
//                rc = setBacklash(BacklashN[0].value);
//            else
//                rc = setBacklash(0);

//            FocuserBacklashSP.s = rc ? IPS_OK : IPS_ALERT;
//            IDSetSwitch(&FocuserBacklashSP, nullptr);
//            return true;
//        }
        /////////////////////////////////////////////
        // Temp sensor in use
        /////////////////////////////////////////////
        if (!strcmp(name, IntExtTempSensorSP.name))
        {
            IUUpdateSwitch(&IntExtTempSensorSP, states, names, n);
            tempSensInUse = IUFindOnSwitchIndex(&IntExtTempSensorSP);
            bool rc = setTempSensorInUse( IUFindOnSwitchIndex(&IntExtTempSensorSP));
            IntExtTempSensorSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&IntExtTempSensorSP, nullptr);
            return true;
        }
        /////////////////////////////////////////////
        // Halfstep
        /////////////////////////////////////////////
        else if (!strcmp(name, HalfStepSP.name))
        {
            IUUpdateSwitch(&HalfStepSP, states, names, n);
            bool rc = setHalfStep(IUFindOnSwitchIndex(&HalfStepSP) == HALFSTEP_ON );
            HalfStepSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&HalfStepSP, nullptr);
            return true;
        }
        /////////////////////////////////////////////
        // Wiring
        /////////////////////////////////////////////
        else if (!strcmp(name, WiringSP.name))
        {
            IUUpdateSwitch(&WiringSP, states, names, n);
            bool rc = setWiring(IUFindOnSwitchIndex(&WiringSP));
            WiringSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&WiringSP, nullptr);
            return true;
        }
        /////////////////////////////////////////////
        // Peripheral Port
        /////////////////////////////////////////////
        else if (!strcmp(name, PerPortSP.name))
        {
            IUUpdateSwitch(&PerPortSP, states, names, n);
            bool rc = setPort( IUFindOnSwitchIndex(&PerPortSP));
            PerPortSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&PerPortSP, nullptr);
            return true;
        }
        /////////////////////////////////////////////
        // Motor Type
        /////////////////////////////////////////////
        else if (!strcmp(name, MotorTypeSP.name))
        {
            IUUpdateSwitch(&MotorTypeSP, states, names, n);
            bool rc = setMotorType(IUFindOnSwitchIndex(&MotorTypeSP));
            MotorTypeSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&MotorTypeSP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool ArmPlat::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //IDLog( "NewNumber: %s %s %lf %s %d\n", dev, name, values[ 0 ], names[ 0 ], n );
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Backlash
        /////////////////////////////////////////////
//        if (strcmp(name, BacklashNP.name) == 0)
//        {
//            IUUpdateNumber(&BacklashNP, values, names, n);
//            // Only update backlash value if compensation is enabled
//            if (BacklashCompensationS[BACKLASH_ENABLED].s == ISS_ON)
//            {
//                bool rc = setBacklash(BacklashN[0].value);
//                BacklashNP.s = rc ? IPS_OK : IPS_ALERT;
//            }
//            else
//            {
//                backlash = 0;
//                BacklashNP.s = IPS_OK;
//            }
//            IDSetNumber(&BacklashNP, nullptr);
//            return true;
//        }

//        if (strcmp(name, SyncNP.name) == 0)
//        {
//            bool rc = sync(static_cast<uint32_t>(values[0]));
//            SyncNP.s = rc ? IPS_OK : IPS_ALERT;
//            if (rc)
//                SyncN[0].value = values[0];

//            IDSetNumber(&SyncNP, nullptr);
//            return true;
//        }
        /////////////////////////////////////////////
        // Relative goto
        /////////////////////////////////////////////
        if (strcmp(name, FocusRelPosNP.name) == 0)
        {
            IUUpdateNumber(&FocusRelPosNP, values, names, n);
            IDSetNumber(&FocusRelPosNP, nullptr);
            MoveRelFocuser( FocusMotionS[ 0 ].s == ISS_ON ? FOCUS_INWARD : FOCUS_OUTWARD, (uint32_t)values[ 0 ] );
            return true;
        }
        /////////////////////////////////////////////
        // MaxSpeed
        /////////////////////////////////////////////
        else if (strcmp(name, MaxSpeedNP.name) == 0)
        {
            IUUpdateNumber(&MaxSpeedNP, values, names, n);
            bool rc = setMaxSpeed(MaxSpeedN[0].value);
            MaxSpeedNP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetNumber(&MaxSpeedNP, nullptr);
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

//bool ArmPlat::sync(uint32_t newPosition)
bool ArmPlat::SyncFocuser(uint32_t ticks)
{
    if ( port == -1 )
        return false;

    int rc = -1;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    sprintf(cmd, "!step setpos %d %d#", port, ticks);

    if ( slpSendRxInt( cmd, &rc ) )
    {
        if ( rc == 0 )
                return true;
    }

    return false;
}

bool ArmPlat::getCurrentPos( uint32_t *curPos )
{
    if ( port == -1 )
        return false;

    int rc = -1;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    sprintf( cmd, "!step getpos %d#", port );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        *curPos = rc;
        return true;
    }

    return false;
}

bool ArmPlat::getCurrentTemp( uint32_t *curTemp )
{
    int rc = -1;
    char cmd[SLP_SEND_BUF_SIZE]={0};
    double idC1, idC2, idF;

    sprintf( cmd, "!read temps %d#", tempSensInUse );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        // this approach works for all Armadillo and +. Not perfect for older bare Seleteks
        // assumes LM61, both internal and external
        if ( tempSensInUse == 0 )	// internal
        {
                idC1 = 261;
                idC2 = 250;
                idF = 1.8;
        }
        else
        {
                idC1 = 192;
                idC2 = 0;
                idF = 1.7;
        }

        *curTemp = (((rc - idC1) * idF) - idC2) / 10;
        return true;
    }

    return false;
}

bool ArmPlat::setMaxSpeed(uint16_t nspeed)
{
    speed = nspeed;	// saved for later, and for possible change of port
    if ( port == -1 )
        return false;

    int rc;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    rc = (int)(500000-((nspeed-1)*50));
    if ( ( rc < 50 ) || ( rc > 500000 ) )
    {
        LOGF_ERROR("Wrong speed %d", nspeed );
        return false;
    }

    sprintf(cmd, "!step speedrangeus %d %d %d#", port, rc, rc );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        if ( rc == 0 )
                return true;
    }

    return false;
}

bool ArmPlat::setWiring( uint16_t newwiring)
{
    wiring = newwiring;	// saved for later, and for possible change of port
    if ( port == -1 )
        return false;

    int rc = -1;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    sprintf(cmd, "!step wiremode %d %d#", port, newwiring );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        if ( rc == 0 )
                return true;
    }

    return false;
}

bool ArmPlat::setHalfStep( bool active )
{
    halfstep = active ? 1:0;	// saved for later, and for possible change of port
    if ( port == -1 )
        return false;

    LOGF_DEBUG("Halfstep set to %s", active ? "true" : "false" );
    int rc = -1;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    sprintf(cmd, "!step halfstep %d %d#", port, active ? 1 : 0 );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        if ( rc == 0 )
                return true;
    }

    return false;
}

bool ArmPlat::setMotorType(uint16_t type)
{
    motortype = type;	// saved for later and optional port change
    if ( port == -1 )
        return false;

    int rc = -1;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    LOGF_DEBUG("Motor type set to %d", type );
    sprintf(cmd, "!step model %d %d#", port, type );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        if ( rc == 0 )
                return true;
    }

    return false;
}

bool ArmPlat::setTempSensorInUse( uint16_t sensor )
{
    LOGF_DEBUG("Temp sensor set to %d", sensor );
    tempSensInUse = sensor;

    return true;
}

bool ArmPlat::setPort( uint16_t newport )
{
    LOGF_DEBUG("Port set to %d", newport );
    if ( port != newport )
    {
        port = newport;
        if ( halfstep != -1 )
                setHalfStep( (bool)halfstep );
        if ( wiring != -1 )
                setWiring( (uint16_t)wiring );
        if ( speed != -1 )
                setMaxSpeed( (uint16_t)speed );
        if ( motortype != -1 )
                setMotorType( (uint16_t)motortype );

        LOG_INFO("Applying motor config, as port is active now" );
        //loadConfig();
    }

    return true;
}

//bool ArmPlat::setBacklash(uint16_t value)
bool ArmPlat::SetFocuserBacklash(int32_t steps)
{
    if ( port == -1 )
        return false;

    LOGF_DEBUG("Backlash %d", steps );
    backlash = steps;

    return true;
}

IPState ArmPlat::MoveAbsFocuser(uint32_t targetTicks)
{
    if ( port == -1 )
        return IPS_ALERT;

    char cmd[SLP_SEND_BUF_SIZE]={0};
    int rc = -1;

    LOGF_DEBUG("Abs move to %d", targetTicks );
    sprintf(cmd, "!step goto %d %i %i#", port, targetTicks, backlash );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        if ( rc == 0 )
        {
                isMoving = true;
                FocusAbsPosNP.s = IPS_BUSY;
                return IPS_BUSY;
        }
    }

   return IPS_ALERT;
}

IPState ArmPlat::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    //IDLog( "Rel focuser move %d %d\n", (int)dir, ticks );
    if ( port == -1 )
        return IPS_ALERT;

    int rc = -1;
    int realticks;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    if (dir == FOCUS_INWARD)
        realticks = -ticks;
    else
        realticks = ticks;

    LOGF_DEBUG("Rel move to %d", realticks );
    sprintf(cmd, "!step gopr %d %d#", port, realticks );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        if ( rc == 0 )
        {
            isMoving = true;
            FocusRelPosN[0].value = ticks;
            FocusRelPosNP.s       = IPS_BUSY;

            return IPS_BUSY;
        }
   }
   return IPS_ALERT;
}

void ArmPlat::TimerHit()
{
    uint32_t data;

    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    if ( port == -1 )
    {
        if ( !portWarned )
        {
                DEBUG( INDI::Logger::DBG_WARNING, "Port must be selected (and configuration saved)" );
                portWarned = true;
        }
        SetTimer(POLLMS);
        return;
    }
    else
        portWarned = false;

    bool rc = getCurrentPos( &data );

    if (rc)
    {
        if ( data != FocusAbsPosN[0].value )
        {
                FocusAbsPosN[0].value = data;
                IDSetNumber(&FocusAbsPosNP, nullptr);
        }
        else
                isMoving = false;

        if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
        {
            if (isMoving == false)
            {
                FocusAbsPosNP.s = IPS_OK;
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusRelPosNP, nullptr);
                IDSetNumber(&FocusAbsPosNP, nullptr);
                LOG_INFO("Focuser reached requested position.");
            }
        }
    }

    if (isMoving == false)
    {
        bool rc = getCurrentTemp( &data );

        if (rc)
        {
                if ( data != TemperatureN[0].value )
                {
                        TemperatureN[0].value = data;
                        IDSetNumber(&TemperatureNP, nullptr);
                }
        }
    }

    SetTimer(POLLMS);
}

bool ArmPlat::AbortFocuser()
{
    if ( port == -1 )
        return false;

    int rc = -1;
    char cmd[SLP_SEND_BUF_SIZE]={0};

    LOG_DEBUG("Aborting motion" );
    sprintf(cmd, "!step stop %d#", port );

    if ( slpSendRxInt( cmd, &rc ) )
    {
        if ( rc == 0 )
        {
                FocusAbsPosNP.s = IPS_IDLE;
                FocusRelPosNP.s = IPS_IDLE;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                return true;
        }
    }

    return true;
}

bool ArmPlat::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &IntExtTempSensorSP);
    IUSaveConfigSwitch(fp, &PerPortSP);
    IUSaveConfigSwitch(fp, &HalfStepSP);
    IUSaveConfigSwitch(fp, &WiringSP);
//    IUSaveConfigNumber(fp, &BacklashNP);
//    IUSaveConfigSwitch(fp, &FocuserBacklashSP);
    IUSaveConfigSwitch(fp, &MotorTypeSP);
    IUSaveConfigNumber(fp, &MaxSpeedNP);

    return true;
}

bool ArmPlat::slpSendRxInt( char *command, int *rcode )
{
    int nbytes_wrrd = 0;
    int rc;
    char errstr[MAXRBUF];
    char res[SLP_SEND_BUF_SIZE]={0};

    LOGF_DEBUG("Tx [%s]", command);
    //tty_set_debug( 1 );
    if ((rc = tty_write_string(PortFD, command, &nbytes_wrrd)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        //IDLog( "ERROR Tx: <%s>\n", errstr );
        LOGF_ERROR("Send error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res, '#', ArmPlat_TIMEOUT, &nbytes_wrrd)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        //IDLog( "ERROR Rx: <%s> error msg <%s>\n", res, errstr );
        LOGF_ERROR("Echo receiving error: %s.", errstr);
        return false;
    }
    LOGF_DEBUG("Rx [%s]", res);
    //if ( ( strstr( command, "getpos" ) == nullptr ) && ( strstr( command, "temps" ) == nullptr ) )
        //IDLog( "Rx: <%s>\n", res );
    return getIntResultCode( command, res, rcode );
}

bool ArmPlat::getIntResultCode( char *sent, char *rxed, int *rcode )
{
        sent[ strlen( sent ) - 1 ] = '\0';
        char *cp = std::strtok( rxed, ":" );
        if ( strcmp( cp, sent ) )
        {
                LOGF_DEBUG("ERROR retrieving answer: Tx[%s] Rx[%s]", sent, rxed );
                return false;
        }
        cp = std::strtok( nullptr, ":" );
        *rcode = atoi( cp );
        return true;
}
