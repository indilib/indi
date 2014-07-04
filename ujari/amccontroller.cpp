#if 0
    INDI Ujari Driver - AMCController
    Copyright (C) 2014 Jasem Mutlaq

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

#include <indicom.h>

#include <math.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <err.h>
#include <stdlib.h>

#include "amccontroller.h"
#include "ujari.h"

#define AMC_GROUP "Motors"
#define AMC_STATUS_GROUP "Motor Status"

#define MAX_RPM             2.0         // 2 RPM
#define MIN_RPM             0           // 0 RPM
#define S_FACTOR            0.2/1.0     // 0.2 Volts / 1 RPM
#define SOF                 0xA5        // Start of Frame
#define MOTOR_ACCELERATION  0.1         // Acceleration is 0.25 RPM per Second
#define MOTOR_DECELERATION  0.1         // Deceleration is 0.25 RPM per Second

#define AMC_MAX_REFRESH     2       // Update drive status and protection every 2 seconds

// Wait 200ms between updates
#define MAX_THREAD_WAIT     200000

pthread_mutex_t ra_motor_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t de_motor_mutex = PTHREAD_MUTEX_INITIALIZER;

extern int DBG_SCOPE_STATUS;
extern int DBG_COMM;
extern int DBG_MOUNT;

void ignore_sigpipe(void)
{
        struct sigaction act;
        int r;
        memset(&act, 0, sizeof(act));
        act.sa_handler = SIG_IGN;
        act.sa_flags = SA_RESTART;
        r = sigaction(SIGPIPE, &act, NULL);
        if (r)
            err(1, "sigaction");
}

AMCController::AMCController(motorType type, Ujari* scope) : Gr1(0x0810)
{
        // Initially, not connected
        connection_status = -1;
        fd = -1;

        setType(type);
        telescope = scope;

        debug = false;
        simulation = false;
        verbose    = true;

        state = MOTOR_STOP;
}

AMCController::~AMCController()
{
    pthread_join(controller_thread, NULL);
}

AMCController::motorType AMCController::getType() const
{
    return type;
}

void AMCController::setType(const motorType &value)
{
    type = value;

    if (type == RA_MOTOR)
    {
      type_name = std::string("RA Motor");
      SLAVE_ADDRESS = 0x1;
      default_port = std::string("172.16.15.2");
    }
    else
    {
      type_name = std::string("DEC Motor");
      SLAVE_ADDRESS = 0x02;
      default_port = std::string("172.16.15.3");
    }
}


/****************************************************************
**
**
*****************************************************************/
bool AMCController::initProperties()
{

    IUFillText(&PortT[0], "PORT", "Port", default_port.c_str());
    IUFillSwitch(&MotionControlS[0], "STOP", "Stop", ISS_OFF);
    IUFillSwitch(&MotionControlS[1], "FORWARD", "Forward", ISS_OFF);
    IUFillSwitch(&MotionControlS[2], "REVERSE" , "Reverse", ISS_OFF);

    IUFillSwitch(&ResetFaultS[0], "Reset", "", ISS_OFF);

    IUFillNumber(&MotorSpeedN[0], "SPEED", "RPM", "%g",  MIN_RPM, MAX_RPM, .1, 0.);

    IUFillLight(&DriveStatusL[0], "Bridge", "", IPS_IDLE);
    IUFillLight(&DriveStatusL[1], "Dynamic Brake", "", IPS_IDLE);
    IUFillLight(&DriveStatusL[2], "Stop", "", IPS_IDLE);
    IUFillLight(&DriveStatusL[3], "Positive Stop", "", IPS_IDLE);
    IUFillLight(&DriveStatusL[4], "Negative Stop", "", IPS_IDLE);
    IUFillLight(&DriveStatusL[5], "Positive Torque", "", IPS_IDLE);
    IUFillLight(&DriveStatusL[6], "Negative Torque", "", IPS_IDLE);
    IUFillLight(&DriveStatusL[7], "External Brake", "", IPS_IDLE);

    IUFillLight(&DriveProtectionL[0], "Drive Reset", "", IPS_IDLE);
    IUFillLight(&DriveProtectionL[1], "Drive Internal Error", "", IPS_IDLE);
    IUFillLight(&DriveProtectionL[2], "Short Circuit", "", IPS_IDLE);
    IUFillLight(&DriveProtectionL[3], "Current Overshoot", "", IPS_IDLE);
    IUFillLight(&DriveProtectionL[4], "Under Voltage", "", IPS_IDLE);
    IUFillLight(&DriveProtectionL[5], "Over Voltage", "", IPS_IDLE);
    IUFillLight(&DriveProtectionL[6], "Drive Over Temperature", "", IPS_IDLE);


  if (type == RA_MOTOR)
  {
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), telescope->getDeviceName(), "RA_MOTOR_PORT", "RA Port", AMC_GROUP, IP_RW, 0, IPS_IDLE);

    IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), telescope->getDeviceName(), "RA_MOTION_CONTROL", "RA Motion", AMC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumberVector(&MotorSpeedNP, MotorSpeedN, NARRAY(MotorSpeedN), telescope->getDeviceName(), "RA_SPEED" , "RA Speed", AMC_GROUP, IP_RW, 0, IPS_IDLE);

    IUFillSwitchVector(&ResetFaultSP, ResetFaultS, NARRAY(ResetFaultS), telescope->getDeviceName(), "RA_FAULT_RESET", "RA Fault", AMC_STATUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillLightVector(&DriveStatusLP, DriveStatusL, NARRAY(DriveStatusL), telescope->getDeviceName(), "RA_DRIVE_STATUS", "RA Status", AMC_STATUS_GROUP, IPS_IDLE);

    IUFillLightVector(&DriveProtectionLP, DriveProtectionL, NARRAY(DriveProtectionL), telescope->getDeviceName(), "RA_PROTECTION_STATUS", "RA Protection", AMC_STATUS_GROUP, IPS_IDLE);

  }
  else
  {
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), telescope->getDeviceName(), "DEC_MOTOR_PORT", "DEC Port", AMC_GROUP, IP_RW, 0, IPS_IDLE);

    IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), telescope->getDeviceName(), "DEC_MOTION_CONTROL", "DEC Motion", AMC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumberVector(&MotorSpeedNP, MotorSpeedN, NARRAY(MotorSpeedN), telescope->getDeviceName(), "DEC_SPEED" , "DEC Speed", AMC_GROUP, IP_RW, 0, IPS_IDLE);

    IUFillSwitchVector(&ResetFaultSP, ResetFaultS, NARRAY(ResetFaultS), telescope->getDeviceName(), "DEC_FAULT_RESET", "DEC Fault", AMC_STATUS_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillLightVector(&DriveStatusLP, DriveStatusL, NARRAY(DriveStatusL), telescope->getDeviceName(), "DEC_DRIVE_STATUS", "DEC Status", AMC_STATUS_GROUP, IPS_IDLE);

    IUFillLightVector(&DriveProtectionLP, DriveProtectionL, NARRAY(DriveProtectionL), telescope->getDeviceName(), "DEC_PROTECTION_STATUS", "DEC Protection", AMC_STATUS_GROUP, IPS_IDLE);

  }

  return true;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::isDriveOnline()
{
    if (simulation)
        return true;

    if (connection_status == -1)
        return false;

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::connect()
{
    bool rc = true;

    if (isDriveOnline())
        return true;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "%s drive: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
        return true;
    }

    fd = openRS485Server(default_port.c_str(), 10001);

    if (fd == -1)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s drive: Failed to connect to RS485 server at %s.", type_name.c_str(), PortT[0].text);
        return false;
    }

    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "RS485 server FD is %d", fd);

    rc = enableWriteAccess();

    if (rc == false)
    {
        close (fd);
        fd=-1;
        return rc;
    }

    rc = enableBridge();

    if (rc == false)
    {
        close (fd);
        fd=-1;
    }

    connection_status = 0;

    last_update.tv_sec=0;
    last_update.tv_usec=0;

    stop();

    setupDriveParameters();

    return rc;
}

/****************************************************************
**
**
*****************************************************************/
void AMCController::disconnect()
{
    connection_status = -1;

    if (simulation)
        return;


}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::enableWriteAccess()
{
    unsigned char command[16];
    unsigned int accum=0;
    int nbytes_written=0;

    // Address 07.00h
    // Offset 0
    // Date Type: unsigned 16bit (2 bytes, 1 word)

    /*****************************
    *** START OF HEADER SECTION **
    ******************************/

    command[0] = SOF;               // Start of Frame
    command[1] = SLAVE_ADDRESS;     // Node Address
    command[2] = 0x02;              // Write
    command[3] = 0x07;              // Index
    command[4] = 0;                 // Offset
    command[5] = 1;                 // 1 Word = 16bit

    for (int i=0; i<=5; i++)
            CrunchCRC(accum, command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[6] =  accum >> 8;
    command[7] =  accum & 0xFF;

    /******************************
    **** END OF HEADER SECTION ****
    ******************************/

    /******************************
    **** START OF DATA SECTION ****
    ******************************/

    command[8] = 0xF;
    command[9] = 0;

    /******************************
    **** END OF DATA SECTION ****
    ******************************/

    accum=0;

    for (int i=8; i<=9; i++)
            CrunchCRC(accum, command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[10] =  accum >> 8;
    command[11] =  accum & 0xFF;

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "EnableWriteAccess Command: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     command[0], command[1],command[2],command[3],command[4],command[5],command[6],command[7],command[8],command[9],command[10],command[11]);

    lock_mutex();

    flushFD();

    if ( (nbytes_written = send(fd, command, 12, 0)) !=  12)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error gaining write access to %s drive. %s", type_name.c_str(), strerror(errno));
        unlock_mutex();
        return false;
    }

    driveStatus status;

    if ( (status = readDriveStatus()) != AMC_COMMAND_COMPLETE)
    {        
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "%s Drive status error: %s", __FUNCTION__, driveStatusString(status));
        unlock_mutex();
        return false;
    }

    unlock_mutex();

    return true;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::enableBridge()
{
    unsigned char command[16];
    unsigned int accum=0;
    int nbytes_written=0;

    // Address 01.00h
    // Offset 0
    // Date Type: unsigned 16bit (2 bytes, 1 word)

    /*****************************
    *** START OF HEADER SECTION **
    ******************************/

    command[0] = SOF;               // Start of Frame
    command[1] = SLAVE_ADDRESS;     // Node Address
    command[2] = 0x02;              // Write
    command[3] = 0x01;              // Index
    command[4] = 0;                 // Offset
    command[5] = 1;                 // 1 Word = 16bit

    accum=0;

    for (int i=0; i<=5; i++)
            CrunchCRC(accum, command[i]);

    CrunchCRC(accum, 0);
    CrunchCRC(accum, 0);

    // CRC - MSB First
    command[6] =  accum >> 8;
    command[7] =  accum & 0xFF;

    /******************************
    **** END OF HEADER SECTION ****
    ******************************/

    /******************************
    **** START OF DATA SECTION ****
    ******************************/

    command[8] = 0;
    command[9] = 0;

    /******************************
    **** END OF DATA SECTION ****
    ******************************/

    // No need to calculate CRC for data since it is 0
    command[10] = 0;
    command[11] = 0;

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "EnableBridge Command: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     command[0], command[1],command[2],command[3],command[4],command[5],command[6],command[7],command[8],command[9],command[10],command[11]);

    lock_mutex();

    flushFD();

    if ( (nbytes_written = send(fd, command, 12, 0)) != 12)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error gaining write access to %s drive. %s", type_name.c_str(), strerror(errno));
        unlock_mutex();
        return false;
    }

    driveStatus status;

    if ( (status = readDriveStatus()) != AMC_COMMAND_COMPLETE)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "%s Drive status error: %s", __FUNCTION__, driveStatusString(status));
        unlock_mutex();
        return false;
    }

    unlock_mutex();

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::setMotion(motorMotion dir)
{
    int nbytes_written=0;
    unsigned char command[16];
    unsigned int accum;

    // Address 45.00h
    // Offset 0
    // Date Type: Signed 32bit (4 bytes, 2 words)

    /*****************************
    *** START OF HEADER SECTION **
    ******************************/

    command[0] = SOF;               // Start of Frame
    command[1] = SLAVE_ADDRESS;     // Node Address
    command[2] = 0x02;              // Write
    command[3] = 0x45;              // Index
    command[4] = 0;                 // Offset
    command[5] = 2;                 // 2 Words = 32bit

    accum=0;

    for (int i=0; i<=5; i++)
            CrunchCRC(accum, command[i]);

    CrunchCRC(accum, 0);
    CrunchCRC(accum, 0);

    // CRC - MSB First
    command[6] =  accum >> 8;
    command[7] =  accum & 0xFF;

    /******************************
    **** END OF HEADER SECTION ****
    ******************************/

    /******************************
    **** START OF DATA SECTION ****
    ******************************/

    // Formula to calculate drive velocity from AMC
    //int velocityValue = round ((dir == MOTOR_FORWARD ? targetRPM : targetRPM * -1) * S_FACTOR * pow(2, 13) / 10);
    int velocityValue = round ((dir == MOTOR_FORWARD ? targetRPM : targetRPM * -1) * pow(2, 14));

    // Break it down into 32bit signed integer (4 bytes) starting with LSB
    command[8] = velocityValue & 0xFF;
    command[9] = (velocityValue >> 8) & 0xFF;
    command[10] = (velocityValue >> 16) & 0xFF;
    command[11] = (velocityValue >> 24) & 0xFF;

    /******************************
    **** END OF DATA SECTION ****
    ******************************/

    accum=0;

    for (int i=8; i<=11; i++)
            CrunchCRC(accum, command[i]);

    CrunchCRC(accum, 0);
    CrunchCRC(accum, 0);

    // CRC - MSB First
    command[12] =  accum >> 8;
    command[13] =  accum & 0xFF;

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "SetMotion Command: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     command[0], command[1],command[2],command[3],command[4],command[5],command[6],command[7],command[8],command[9],command[10],command[11],command[12],command[13]);

    lock_mutex();

    flushFD();

    if ( (nbytes_written = send(fd, command, 14, 0)) != 14)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error writing velocity to %s drive. %s", type_name.c_str(), strerror(errno));
        MotionControlSP.s = IPS_ALERT;
        IUResetSwitch(&MotionControlSP);
        MotionControlS[MOTOR_STOP].s = ISS_ON;
        IDSetSwitch(&MotionControlSP, NULL);
        unlock_mutex();
        return false;
    }

    driveStatus status;

    if ( (status = readDriveStatus()) != AMC_COMMAND_COMPLETE)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "%s Drive status error: %s", __FUNCTION__, driveStatusString(status));
        unlock_mutex();
        return false;
    }

    unlock_mutex();

    currentRPM = (dir == MOTOR_FORWARD ? targetRPM : targetRPM * -1);

    return true;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::moveForward()
{
    if (!isDriveOnline())
        return false;

    // Already moving forward
    if (state == MOTOR_FORWARD && currentRPM == targetRPM)
        return true;
    else if (state == MOTOR_REVERSE)
        stop();

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "%s drive: Simulating forward command.", type_name.c_str());
        MotionControlSP.s = IPS_BUSY;
        IDSetSwitch(&MotionControlSP, NULL);
        state = MOTOR_FORWARD;
        return true;
    }

    targetRPM = MotorSpeedN[0].value;   

    bool rc = setMotion(MOTOR_FORWARD);

    if (rc)
    {
        enableMotion();
        IUResetSwitch(&MotionControlSP);
        MotionControlS[MOTOR_FORWARD].s = ISS_ON;
        MotionControlSP.s = IPS_BUSY;
        state = MOTOR_FORWARD;
    }
    else
    {
        IUResetSwitch(&MotionControlSP);
        MotionControlS[MOTOR_STOP].s = ISS_ON;
        MotionControlSP.s = IPS_ALERT;
    }

    IDSetSwitch(&MotionControlSP, NULL);

    return rc;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::moveReverse()
{
    //int ret;

    if (!isDriveOnline())
        return false;

    // Already moving backwards
    if (state == MOTOR_REVERSE && currentRPM == targetRPM)
        return true;
    else if (state == MOTOR_FORWARD)
        stop();

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "%s drive: Simulating reverse command.", type_name.c_str());
        MotionControlSP.s = IPS_BUSY;
        IDSetSwitch(&MotionControlSP, NULL);
        state = MOTOR_REVERSE;
        return true;
    }

    targetRPM = MotorSpeedN[0].value;

    bool rc = setMotion(MOTOR_REVERSE);

    if (rc)
    {
        enableMotion();
        IUResetSwitch(&MotionControlSP);
        MotionControlS[MOTOR_REVERSE].s = ISS_ON;
        MotionControlSP.s = IPS_BUSY;
        state = MOTOR_REVERSE;
    }
    else
    {
        IUResetSwitch(&MotionControlSP);
        MotionControlS[MOTOR_STOP].s = ISS_ON;
        MotionControlSP.s = IPS_ALERT;
    }

    IDSetSwitch(&MotionControlSP, NULL);

    return rc;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::stop()
{
    if (!isDriveOnline())
        return false;

    // Already stopped
    /*if (state == MOTOR_STOP && MotionControlSP.s != IPS_ALERT)
        return true;*/

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating stop command.", type_name.c_str());
        MotionControlSP.s = IPS_IDLE;
        IDSetSwitch(&MotionControlSP, "%s drive is stopped", type_name.c_str());
        state = MOTOR_STOP;
        return true;
     }

    targetRPM = 0;

    // Alternative Method , STOP DRIVE
    unsigned short param = CP_COMMANDED_STOP;

    bool rc = setControlParameter(param);

    if (rc)
    {
        IUResetSwitch(&MotionControlSP);
        MotionControlS[MOTOR_STOP].s = ISS_ON;
        MotionControlSP.s = IPS_OK;
        state = MOTOR_STOP;
    }
    else
        MotionControlSP.s = IPS_ALERT;

    IDSetSwitch(&MotionControlSP, NULL);

    return rc;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::setSpeed(double rpm)
{
    if (!isDriveOnline())
    {
        MotorSpeedNP.s = IPS_IDLE;
        IDSetNumber(&MotorSpeedNP, NULL);
        return false;
    }

    if (rpm < MIN_RPM || rpm > MAX_RPM)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "setSpeed: requested RPM %g is outside boundary limits (0,5) RPM", rpm);
        MotorSpeedNP.s = IPS_ALERT;
        IDSetNumber(&MotorSpeedNP, NULL);
        return false;
    }

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "%s: Simulating set speed to %g RPM", type_name.c_str(), rpm);
        targetRPM = rpm;
        MotorSpeedN[0].value = targetRPM;
        MotorSpeedNP.s = IPS_OK;
        IDSetNumber(&MotorSpeedNP, NULL);
        return true;
    }

    targetRPM = rpm;

    MotorSpeedN[0].value = targetRPM;
    MotorSpeedNP.s = IPS_OK;
    IDSetNumber(&MotorSpeedNP, NULL);

    if (targetRPM == currentRPM)
        return true;

    if (isMotionActive())
    {
        if (MotionControlS[MOTOR_FORWARD].s == ISS_ON)
            moveForward();
        else
            moveReverse();
    }

    return true;
}

bool AMCController::isMotionActive()
{
    return (MotionControlSP.s == IPS_BUSY);
}

/****************************************************************
**
**
*****************************************************************/
void AMCController::ISGetProperties()
{
    telescope->defineText(&PortTP);
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::updateProperties(bool connected)
{
    if (connected)
    {
        telescope->defineSwitch(&MotionControlSP);
        telescope->defineNumber(&MotorSpeedNP);
        telescope->defineSwitch(&ResetFaultSP);
        telescope->defineLight(&DriveStatusLP);
        telescope->defineLight(&DriveProtectionLP);

        int err = pthread_create(&controller_thread, NULL, &AMCController::update_helper, this);
        if (err != 0)
        {
            DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_ERROR, "%s controller: Can't create controller thread (%s)", type_name.c_str(), strerror(err));
            return false;
        }

    }
    else
    {
        telescope->deleteProperty(MotionControlSP.name);
        telescope->deleteProperty(MotorSpeedNP.name);
        telescope->deleteProperty(ResetFaultSP.name);
        telescope->deleteProperty(DriveStatusLP.name);
        telescope->deleteProperty(DriveProtectionLP.name);

        pthread_join(controller_thread, NULL);
    }
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(MotorSpeedNP.name, name))
    {
        bool rc = setSpeed(values[0]);

        return rc;
    }

    return false;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // Device Port Text
    if (!strcmp(PortTP.name, name))
    {
        if (IUUpdateText(&PortTP, texts, names, n) < 0)
            return false;

        PortTP.s = IPS_OK;
        IDSetText(&PortTP, "Please reconnect when ready.");

        return true;
    }

    return false;
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{      
    if (!strcmp(MotionControlSP.name, name))
    {
        if (IUUpdateSwitch(&MotionControlSP, states, names, n) < 0)
            return false;

        bool rc=false;

        if (MotionControlS[MOTOR_STOP].s == ISS_ON)       
          rc= stop();
        else if (MotionControlS[MOTOR_FORWARD].s == ISS_ON)
          rc = moveForward();
        else if (MotionControlS[MOTOR_REVERSE].s == ISS_ON)
           rc = moveReverse();

        return rc;
     }

    if (!strcmp(ResetFaultSP.name, name))
    {
        bool rc = resetFault();
        ResetFaultSP.s = rc ? IPS_OK : IPS_ALERT;
        IUResetSwitch(&ResetFaultSP);
        IDSetSwitch(&ResetFaultSP, NULL);
        return true;
    }

    return false;
}

/****************************************************************
** open a connection to the given host and port or die.
* return socket fd.
**
*****************************************************************/
int AMCController::openRS485Server (const char * host, int rs485_port)
{
    struct sockaddr_in serv_addr;
    struct hostent *hp;
    int sockfd;

    if (fd != -1)
        return fd;

    /* lookup host address */
    hp = gethostbyname (host);
    if (!hp)
    {
        fprintf (stderr, "gethostbyname(%s): %s\n", host, strerror(errno));
        return -1;
    }

    /* create a socket to the INDI server */
    (void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    serv_addr.sin_port = htons(rs485_port);
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        DEBUGFDEVICE (telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "socket(%s,%d): %s", host, rs485_port,strerror(errno));
        return -1;
    }

    /* connect */
    if (::connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
        DEBUGFDEVICE (telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "connect(%s,%d): %s", host,rs485_port,strerror(errno));
        return -1;
    }

    ignore_sigpipe();

    /* ok */
    return (sockfd);
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::setupDriveParameters()
{
    bool rc = false;
    /* Linear Ramp Positive Target Positive Change
     * This parameter limits the acceleration for a velocity
     * commanded in the positive direction.
     * Address: 3C.00h
     * Offset : 0
    */
    rc = setAcceleration(MOTOR_FORWARD, MOTOR_ACCELERATION);

    if (rc == false)
        return rc;

    /* Linear Ramp Positive Target Negative Change
     * This parameter limits the deceleration for a velocity
     * commanded in the positive direction.
     * Address: 3C.03h ((3Ch + 03h)
     * Offset : 3 bytes
    */
    rc = setDeceleration(MOTOR_FORWARD, MOTOR_DECELERATION);

    if (rc == false)
        return rc;

    /* Linear Ramp Negative Target Negative Change
     * This parameter limits the acceleration for a velocity
     * commanded in the negative direction.
     * Address: 3C.06h ((3Ch + 06h)
     * Offset : 6 bytes
    */
    rc = setAcceleration(MOTOR_REVERSE, MOTOR_ACCELERATION);

    if (rc == false)
        return rc;

    /*  Linear Ramp Negative Target Positive Change
     *  This parameter limits the deceleration for a velocity
     *  commanded in the negative direction.
     *  Address: 3C.09h ((3Ch + 09h)
     *  Offset : 9 bytes
    */
    rc = setDeceleration(MOTOR_REVERSE, MOTOR_DECELERATION);

    return rc;
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::setAcceleration(motorMotion dir, double rpmAcceleration)
{
    int nbytes_written=0;
    unsigned char command[16];
    unsigned int accum;

    /*****************************
    *** START OF HEADER SECTION **
    ******************************/

    command[0] = SOF;               // Start of Frame
    command[1] = SLAVE_ADDRESS;     // Node Address
    command[2] = 0x02;              // Write
    command[3] = 0x3C;              // Index

    switch (dir)
    {
        // 3C.00h
        // Offset 0
        case MOTOR_FORWARD:
        command[4] = 0;
        break;

        // 3C.06h
        // Offset 6
        case MOTOR_REVERSE:
        command[4] = 6;
        break;

        default:
            return false;

    }

    command[5] = 3;     // 3 words (48 bit = 6 bytes)

    accum=0;

    for (int i=0; i<=5; i++)
            CrunchCRC(accum,command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[6] =  accum >> 8;
    command[7] =  accum & 0xFF;

    /******************************
    **** END OF HEADER SECTION ****
    ******************************/

    /******************************
    **** START OF DATA SECTION ****
    ******************************/

    // Formula to calculate drive acceleration from AMC
    unsigned long accelValue = round (rpmAcceleration * S_FACTOR * pow(2, 29) / pow(10, 3));

    // Break it down into 48bit unsigned integer (6 bytes) starting with LSB
    command[8] = accelValue & 0xFF;
    command[9] = (accelValue >> 8) & 0xFF;
    command[10] = (accelValue >> 16) & 0xFF;
    command[11] = (accelValue >> 24) & 0xFF;
    command[12] = (accelValue >> 32) & 0xFF;
    command[13] = (accelValue >> 40) & 0xFF;

    accum=0;

    for (int i=8; i<=13; i++)
            CrunchCRC(accum,command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[14] =  accum >> 8;
    command[15] =  accum & 0xFF;

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "setAcceleration Command: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     command[0], command[1],command[2],command[3],command[4],command[5],command[6],command[7],command[8],command[9],command[10],command[11],command[12],command[13],command[14],command[15]);

    lock_mutex();

    flushFD();

    if ( (nbytes_written = send(fd, command, 16, 0)) != 16)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error writing acceleration to %s drive. %s", type_name.c_str(), strerror(errno));
        unlock_mutex();
        return false;
    }

    driveStatus status;

    if ( (status = readDriveStatus()) != AMC_COMMAND_COMPLETE)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "%s Drive status error: %s", __FUNCTION__, driveStatusString(status));
        unlock_mutex();
        return false;
    }

    unlock_mutex();

    return true;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::setDeceleration(motorMotion dir, double rpmDeAcceleration)
{
    unsigned char command[16];
    unsigned int accum;
    int nbytes_written=0;

    /*****************************
    *** START OF HEADER SECTION **
    ******************************/

    command[0] = SOF;               // Start of Frame
    command[1] = SLAVE_ADDRESS;     // Node Address
    command[2] = 0x02;              // Write
    command[3] = 0x3C;              // Index

    switch (dir)
    {
        // 3C.03h
        // Offset 0
        case MOTOR_FORWARD:
        command[4] = 3;
        break;

        // 3C.09h
        // Offset 9
        case MOTOR_REVERSE:
        command[4] = 9;
        break;

        default:
            return false;

    }

    command[5] = 3;     // 3 words (48 bit = 6 bytes)

    accum=0;

    for (int i=0; i<=5; i++)
            CrunchCRC(accum,command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[6] =  accum >> 8;
    command[7] =  accum & 0xFF;

    /******************************
    **** END OF HEADER SECTION ****
    ******************************/

    /******************************
    **** START OF DATA SECTION ****
    ******************************/

    // Formula to calculate drive acceleration from AMC
    unsigned long accelValue = rpmDeAcceleration * S_FACTOR * pow(2, 29) / pow(10, 3);

    // Break it down into 48bit unsigned integer (6 bytes) starting with LSB
    command[8] = accelValue & 0xFF;
    command[9] = (accelValue >> 8) & 0xFF;
    command[10] = (accelValue >> 16) & 0xFF;
    command[11] = (accelValue >> 24) & 0xFF;
    command[12] = (accelValue >> 32) & 0xFF;
    command[13] = (accelValue >> 40) & 0xFF;

    accum=0;

    for (int i=8; i<=13; i++)
            CrunchCRC(accum, command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[14] =  accum >> 8;
    command[15] =  accum & 0xFF;

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "Set Decelration Command: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     command[0], command[1],command[2],command[3],command[4],command[5],command[6],command[7],command[8],command[9],command[10],command[11],command[12],command[13],command[14],command[15]);

    lock_mutex();

    flushFD();

    if ( (nbytes_written = send(fd, command, 16, 0)) != 16)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error writing deceleration to %s drive. %s", type_name.c_str(), strerror(errno));
        unlock_mutex();
        return false;
    }

    driveStatus status;

    if ( (status = readDriveStatus()) != AMC_COMMAND_COMPLETE)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "%s Drive status error: %s", __FUNCTION__, driveStatusString(status));
        unlock_mutex();
        return false;
    }

    unlock_mutex();

    return true;

}


/****************************************************************
**
**
*****************************************************************/
void AMCController::CrunchCRC (unsigned int &accum, char x)
{
    // Compute CRC using BitbyBit
    int i, k;
    for (k=0; k<8; k++)
    {
        i = (x >> 7) & 1;
        if (accum & 0x8000)
        {
            accum = ((accum ^ Gr1) << 1) + (i ^ 1);
        }
        else
        {
            accum = (accum << 1) + i;
        }

        accum &= 0x0ffff;
        x <<= 1;
    }
}

/****************************************************************
**
**
*****************************************************************/
AMCController::driveStatus AMCController::readDriveStatus()
{
    unsigned char response[8];
    int nbytes_read=0;

    fd_set          rd;
    int             rc;
    struct timeval  t;

    if (simulation)
        return AMC_COMMAND_COMPLETE;

    /* 0.25 second waiting */
    t.tv_sec = 0;
    t.tv_usec = 250000;

    /* set descriptor */
    FD_ZERO(&rd);
    FD_SET(fd, &rd);

    for (int retry=0; retry < 3; retry++)
    {
        rc = select(fd + 1, &rd, NULL, NULL, &t);
        if (rc == -1)
        {
          /* select( ) error */
            DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveStatus: select() error %s.", strerror(errno));
            continue;
        }
        else if (rc == 0)
        {
          /* no input available */
            DEBUGDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveStatus: No input available.");
            t.tv_usec += 250000;
            continue;
        }
        else if (FD_ISSET(fd, &rd))
        {
            /*if ( (nbytes_read += recv(fd, response+nbytes_read, 8-nbytes_read, 0)) <= 0)
            {
                // RS485 server disconnected
                if (nbytes_read == 0)
                    DEBUGDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveStatus: Lost connection to RS485 server.");
                else
                    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveStatus: read error: %s", strerror(errno));
                //return AMC_COMM_ERROR;
                continue;
            }

            if (nbytes_read != 8)
                continue;

            for (int i=0; i < nbytes_read; i++)
                DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "response[%d]=%02X", i, response[i]);

            if (response[0] != SOF)
            {
                DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "readDriveStatus: Invalid Start of Frame %02X (%s,%d)", response[0], response, nbytes_read);
                return AMC_COMM_ERROR;
            }*/

            bool SOFFound=false;
            while ( (nbytes_read = recv(fd, response, 1, 0)) > 0)
            {
                if (response[0] == SOF)
                {
                    SOFFound = true;
                    break;
                }
            }

            if (SOFFound)
            {
                nbytes_read=1;
                int response_size;
                while ((response_size = recv(fd, response+nbytes_read, 8-nbytes_read, 0)) > 0)
                {
                    nbytes_read += response_size;
                    if (nbytes_read ==8)
                    {
                       DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "<%02X><%02X><%02X><%02X><%02X><%02X><%02X><%02X>", response[0],
                               response[1],response[2],response[3],response[4],response[5],response[6],response[7]);
                        break;
                    }
                }
                break;
            }
        }
    }

    if (nbytes_read != 8)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveStatus: nbytes read is %d while it should be 8", nbytes_read);
        return AMC_COMM_ERROR;
    }

    if (response[3] == 0x01)
        return AMC_COMMAND_COMPLETE;
    else if (response[3] == 0x02)
        return AMC_COMMAND_INCOMPLETE;
    else if (response[3] == 0x04)
        return AMC_INVALID_COMMAND;
    else if (response[3] == 0x06)
        return AMC_NO_WRITE_ACCESS;
    else if (response[3] == 0x08)
        return AMC_CRC_ERROR;
    else
        return AMC_UNKNOWN_ERROR;

}

/****************************************************************
**
**
*****************************************************************/
AMCController::driveStatus AMCController::readDriveData(unsigned char *data, unsigned char len)
{
    int nbytes_read=0;
    unsigned char data_crc[2];

    if (simulation)
    {
        data[0] = 1;
        for (int i=1; i < len; i++)
            data[i] = 0;

        return AMC_COMMAND_COMPLETE;
    }

    fd_set          rd;
    int             rc;
    struct timeval  t;

    /* 0.25 second waiting */
    t.tv_sec = 0;
    t.tv_usec = 250000;

    /* set descriptor */
    FD_ZERO(&rd);
    FD_SET(fd, &rd);

    for (int retry=0; retry < 3; retry++)
    {
        rc = select(fd + 1, &rd, NULL, NULL, &t);
        if (rc == -1)
        {
          /* select( ) error */
            DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveData: select() error %s.", strerror(errno));
            continue;
        }
        else if (rc == 0)
        {
          /* no input available */
            DEBUGDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveData: No input available.");
            continue;
        }
        else if (FD_ISSET(fd, &rd))
        {
            if ( (nbytes_read += recv(fd, data+nbytes_read, len-nbytes_read, 0)) <= 0)
            {
                // RS485 server disconnected
                if (nbytes_read == 0)
                    DEBUGDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveData: Lost connection to RS485 server.");
                else
                    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "readDriveData: read error: %s", strerror(errno));
                return AMC_COMM_ERROR;
            }

            if (nbytes_read != len)
                continue;

            // Read CRC
            recv(fd, data_crc, 2, 0);

            break;
        }
    }

    if (nbytes_read == len)
        return AMC_COMMAND_COMPLETE;
    else
        return AMC_COMM_ERROR;

}

/****************************************************************
**
**
*****************************************************************/
const char * AMCController::driveStatusString(driveStatus status)
{
    switch (status)
    {
        case AMC_COMMAND_COMPLETE:
            return (char *) "Command Complete";
            break;

        case AMC_COMMAND_INCOMPLETE:
            return (char *) "Command Incomplete";
            break;

        case AMC_INVALID_COMMAND:
            return (char *) "Invalid Command";
            break;

        case AMC_NO_WRITE_ACCESS:
            return (char *) "Do not have write access";
            break;

        case AMC_CRC_ERROR:
            return (char *) "Frame or CRC error";
            break;

         case AMC_COMM_ERROR:
            return (char *) "Communication error";
            break;

         case AMC_UNKNOWN_ERROR:
         default:
            return (char *) "Unknown error";
            break;
    }
}

/****************************************************************
**
**
*****************************************************************/
void AMCController::refresh()
{
     lock_mutex();
     IDSetLight(&DriveStatusLP, NULL);
     IDSetLight(&DriveProtectionLP, NULL);
     unlock_mutex();
}

/****************************************************************
**
**
*****************************************************************/
void * AMCController::update_helper(void *context)
{
    ((AMCController*)context)->update();
    return 0;
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::update()
{
    /*struct timeval now;
    gettimeofday (&now, NULL);
    if (((now.tv_sec - last_update.tv_sec) + ((now.tv_usec - last_update.tv_usec)/1e6)) > AMC_MAX_REFRESH)
    {
        last_update.tv_sec=now.tv_sec;
        last_update.tv_usec=now.tv_usec;
    }
    else
        return true;
        */

    //TODO MUST Send heatbeat to driver. Configure drive to ABORT if heatbeat is missed.
    //TODO Set heartbeat for 2000ms in AMC drive to be safe.

    int nbytes_written=0;
    unsigned char command[16];
    unsigned int accum=0;

    // STATUS: Address 02.00h
    // Offset 0
    // Date Type: unsigned 16bit (2 bytes, 1 word)
    // PROTECTION: Address 02.01h
    // Offset 1
    // Date Type: unsigned 16bit (2 bytes, 1 word)
    // Total: 32bit = 4 bytes

    /*****************************
    *** START OF HEADER SECTION **
    ******************************/

    command[0] = SOF;               // Start of Frame
    command[1] = SLAVE_ADDRESS;     // Node Address
    command[2] = 0x01;              // Read
    command[3] = 0x02;              // Index
    command[4] = 0;                 // Offset
    command[5] = 2;                 // 2 Words = 32bit

    accum=0;

    for (int i=0; i<=5; i++)
            CrunchCRC(accum,command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[6] =  accum >> 8;
    command[7] =  accum & 0xFF;

    /******************************
    **** END OF HEADER SECTION ****
    ******************************/

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "UpdateStatus command: %02X %02X %02X %02X %02X %02X %02X %02X",
                     command[0], command[1],command[2],command[3],command[4],command[5],command[6],command[7]);

    while (connection_status != -1)
    {

    lock_mutex();

    flushFD();

    if (simulation == false && (nbytes_written = send(fd, command, 8, 0)) != 8)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error updating status for %s drive. %s", type_name.c_str(), strerror(errno));
        return false;
    }

    driveStatus status;

    if ( (status = readDriveStatus()) != AMC_COMMAND_COMPLETE)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_DEBUG, "%s Drive status error: %s", __FUNCTION__, driveStatusString(status));
        return false;
    }

    // Read 2x16bit words, 32 bit data or 4 bytes
    unsigned char status_data[4];
    unsigned short dStatus, dProtection=0;

    if ( (status = readDriveData(status_data, 4)) != AMC_COMMAND_COMPLETE)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "%s Drive data read error: %s", __FUNCTION__, driveStatusString(status));
        DriveStatusLP.s = IPS_ALERT;
        DriveProtectionLP.s = IPS_ALERT;
        IDSetLight(&DriveStatusLP, NULL);
        IDSetLight(&DriveProtectionLP, NULL);
        return false;
    }

    // Though not strictly necessary since status bits are LSB always, let's put them back as 16bit unsigned shorts
    dStatus     = (status_data[0] | (status_data[1] << 8));
    dProtection = (status_data[2] | (status_data[3] << 8));

    DriveStatusL[0].s = (dStatus & DS_BRIDGE) ? IPS_OK : IPS_ALERT;
    DriveStatusL[1].s = (dStatus & DS_DYNAMIC_BRAKE) ? IPS_OK : IPS_IDLE;
    DriveStatusL[2].s = (dStatus & DS_STOP) ? IPS_OK : IPS_IDLE;
    DriveStatusL[3].s = (dStatus & DS_POSITIVE_STOP) ? IPS_OK : IPS_IDLE;
    DriveStatusL[4].s = (dStatus & DS_NEGATIVE_STOP) ? IPS_OK : IPS_IDLE;
    DriveStatusL[5].s = (dStatus & DS_POSITIVE_TORQUE_INHIBIT) ? IPS_OK : IPS_IDLE;
    DriveStatusL[6].s = (dStatus & DS_NEGATIVE_TORQUE_INHIBIT) ? IPS_OK : IPS_IDLE;
    DriveStatusL[7].s = (dStatus & DS_EXTERNAL_BRAKE) ? IPS_OK : IPS_IDLE;

    DriveProtectionL[0].s = (dProtection & DP_DRIVE_RESET) ? IPS_ALERT : IPS_IDLE;
    DriveProtectionL[1].s = (dProtection & DP_DRIVE_INTERNAL_ERROR) ? IPS_ALERT : IPS_IDLE;
    DriveProtectionL[2].s = (dProtection & DP_SHORT_CIRCUT) ? IPS_ALERT : IPS_IDLE;
    DriveProtectionL[3].s = (dProtection & DP_CURRENT_OVERSHOOT) ? IPS_ALERT : IPS_IDLE;
    DriveProtectionL[4].s = (dProtection & DP_UNDER_VOLTAGE) ? IPS_ALERT : IPS_IDLE;
    DriveProtectionL[5].s = (dProtection & DP_OVER_VOLTAGE) ? IPS_ALERT : IPS_IDLE;
    DriveProtectionL[6].s = (dProtection & DP_DRIVE_OVER_TEMPERATURE) ? IPS_ALERT : IPS_IDLE;

    DriveStatusLP.s = IPS_OK;
    DriveProtectionLP.s = IPS_OK;

    unlock_mutex();

   // IDSetLight(&DriveStatusLP, NULL);
   // IDSetLight(&DriveProtectionLP, NULL);

    usleep(MAX_THREAD_WAIT);
  }

    return true;

}

bool AMCController::isProtectionTriggered()
{
    bool triggered=false;
    lock_mutex();
    triggered =  (DriveProtectionL[2].s == IPS_ALERT || DriveProtectionL[3].s == IPS_ALERT || DriveProtectionL[4].s == IPS_ALERT || DriveProtectionL[5].s == IPS_ALERT || DriveProtectionL[6].s == IPS_ALERT);
    unlock_mutex();

    return triggered;
}

bool AMCController::resetFault()
{
    if (!isDriveOnline())
        return false;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "%s drive: Simulating reset fault.", type_name.c_str());
        return true;
     }

     unsigned short param = CP_RESET_EVENTS;

     return setControlParameter(param);
}

bool AMCController::setControlParameter(unsigned short param)
{
    int nbytes_written=0;
    unsigned char command[16];
    unsigned int accum=0;

    // Address 01.00h
    // Offset 0
    // Date Type: unsigned 16bit (2 bytes, 1 word)

    /*****************************
    *** START OF HEADER SECTION **
    ******************************/

    command[0] = SOF;               // Start of Frame
    command[1] = SLAVE_ADDRESS;     // Node Address
    command[2] = 0x02;              // Write
    command[3] = 0x01;              // Index
    command[4] = 0;                 // Offset
    command[5] = 1;                 // 1 Word = 16bit

    accum=0;

    for (int i=0; i<=5; i++)
            CrunchCRC(accum,command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[6] =  accum >> 8;
    command[7] =  accum & 0xFF;

    /******************************
    **** END OF HEADER SECTION ****
    ******************************/

    /******************************
    **** START OF DATA SECTION ****
    ******************************/

    command[8] = param & 0xFF;
    command[9] = (param >> 8) & 0xFF;

    /******************************
    **** END OF DATA SECTION ****
    ******************************/

    accum=0;

    for (int i=8; i<=9; i++)
            CrunchCRC(accum,command[i]);

    CrunchCRC(accum,0);
    CrunchCRC(accum,0);

    // CRC - MSB First
    command[10] =  accum >> 8;
    command[11] =  accum & 0xFF;

    DEBUGFDEVICE(telescope->getDeviceName(), DBG_COMM, "SetControlParameter Command: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     command[0], command[1],command[2],command[3],command[4],command[5],command[6],command[7],command[8],command[9],command[10],command[11]);

    lock_mutex();

    flushFD();

    if ( (nbytes_written = send(fd, command, 12, 0)) !=  12)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "Error gaining write access to %s drive. %s", type_name.c_str(), strerror(errno));
        unlock_mutex();
        return false;
    }

    driveStatus status;

    if ( (status = readDriveStatus()) != AMC_COMMAND_COMPLETE)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "%s Drive status error: %s", __FUNCTION__, driveStatusString(status));
        unlock_mutex();
        return false;
    }

    unlock_mutex();

    return true;

}

bool AMCController::enableMotion()
{
    if (!isDriveOnline())
        return false;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_DEBUG, "%s drive: Simulating enable motion.", type_name.c_str());
        return true;
     }

     unsigned short param = 0;

     return setControlParameter(param);

}

void AMCController::flushFD()
{
    fd_set rd;
    struct timeval t;
    char buff[100];
    t.tv_sec=0;
    t.tv_usec=0;
    FD_ZERO(&rd);
    FD_SET(fd,&rd);
    while(select(fd+1,&rd,NULL,NULL,&t))
    {
        read(fd,buff,100);
    }
}

/****************************************************************
**
**
*****************************************************************/
void AMCController::lock_mutex()
{
    switch (type)
    {
        case RA_MOTOR:
          pthread_mutex_lock( &ra_motor_mutex );
          break;

       case DEC_MOTOR:
            pthread_mutex_lock( &de_motor_mutex );
            break;

    }
}

/****************************************************************
**
**
*****************************************************************/
void AMCController::unlock_mutex()
{

    switch (type)
    {
        case RA_MOTOR:
          pthread_mutex_unlock( &ra_motor_mutex );
          break;

       case DEC_MOTOR:
            pthread_mutex_unlock( &de_motor_mutex );
            break;

    }
}
