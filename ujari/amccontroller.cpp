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

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#include "amccontroller.h"
#include "ujari.h"

#define AMC_GROUP "Motors"

#define MAX_RPM             2.0         // 2 RPM
#define S_FACTOR            0.2/1.0     // 0.2 Volts / 1 RPM
#define SOF                 0xA5        // Start of Frame
#define MOTOR_ACCELERATION  0.1         // Acceleration is 0.25 RPM per Second
#define MOTOR_DECELERATION  0.1         // Deceleration is 0.25 RPM per Second


//const unsigned int AMCController::Gr1 = 0x0810;

AMCController::AMCController(motorType type, Ujari* scope) : Gr1(0x0810)
{
        // Initially, not connected
        connection_status = -1;
        fd = -1;
        accum =0;

        setType(type);
        telescope = scope;

        debug = false;
        simulation = false;
        verbose    = true;
}

AMCController::~AMCController()
{
}

AMCController::motorType AMCController::getType() const
{
    return type;
}

void AMCController::setType(const motorType &value)
{
    type = value;

    // FIXME change to real IP address of RS485 to Ethernet adapter
    default_port = std::string("192.168.1.XXX");

    if (type == RA_MOTOR)
    {
      type_name = std::string("RA Motor");
      // TODO FIXME - SET Address of RA Motor in Hardware
      SLAVE_ADDRESS = 0x3F;
    }
    else
    {
      // TODO FIXME - SET Address of DEC Motor in Hardware
      type_name = std::string("DEC Motor");
      SLAVE_ADDRESS = 0x3F;
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

    IUFillNumber(&MotorSpeedN[0], "SPEED", "RPM", "%g",  0., 5., .1, 0.);

  if (type == RA_MOTOR)
  {
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), telescope->getDeviceName(), "RA_MOTOR_PORT", "RA Port", AMC_GROUP, IP_RW, 0, IPS_IDLE);

    IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), telescope->getDeviceName(), "RA_MOTION_CONTROL", "RA Motion", AMC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumberVector(&MotorSpeedNP, MotorSpeedN, NARRAY(MotorSpeedN), telescope->getDeviceName(), "RA_SPEED" , "RA Speed", AMC_GROUP, IP_RW, 0, IPS_IDLE);
  }
  else
  {
    IUFillTextVector(&PortTP, PortT, NARRAY(PortT), telescope->getDeviceName(), "DEC_MOTOR_PORT", "DEC Port", AMC_GROUP, IP_RW, 0, IPS_IDLE);

    IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), telescope->getDeviceName(), "DEC_MOTION_CONTROL", "DEC Motion", AMC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumberVector(&MotorSpeedNP, MotorSpeedN, NARRAY(MotorSpeedN), telescope->getDeviceName(), "DEC_SPEED" , "DEC Speed", AMC_GROUP, IP_RW, 0, IPS_IDLE);
  }

  return true;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::check_drive_connection()
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
    if (check_drive_connection())
        return true;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating connecting to port %s.", type_name.c_str(), PortT[0].text);
        connection_status = 0;
        return true;
    }

    fd = openRS485Server(default_port.c_str(), 10001);

    if (fd == -1)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Failed to connect to RS485 server at %s.", type_name.c_str(), PortT[0].text);
        return false;
    }

    return false;
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
bool AMCController::move_forward()
{
    //int ret;

    if (!check_drive_connection())
        return false;

    // Already moving forward
   //if (MotionCommand[0] == INVERTER_FORWARD)
        //return true;

    //MotionCommand[0] = INVERTER_FORWARD;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating forward command.", type_name.c_str());
        MotionControlSP.s = IPS_BUSY;
        IDSetSwitch(&MotionControlSP, NULL);
        return true;
    }

    /*
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Forward Command ERROR. force_multiple_coils (%d)\n", ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, WRITE_ADDRESS, 1);

    MotionCommand[0] = 0;
    MotionControlSP.s = IPS_ALERT;
    IUResetSwitch(&MotionControlSP);
    MotionControlS[0].s = ISS_ON;
    IDSetSwitch(&MotionControlSP, "Error: %s drive failed to move %s", type_name.c_str(), forward_motion.c_str());
    */
    return false;
}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::move_reverse()
{
    //int ret;

    if (!check_drive_connection())
        return false;

    // Already moving backwards
    //if (MotionCommand[0] == INVERTER_REVERSE)
        //return true;

    //MotionCommand[0] = INVERTER_REVERSE;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating reverse command.", type_name.c_str());
        MotionControlSP.s = IPS_BUSY;
        IDSetSwitch(&MotionControlSP, NULL);
        return true;
    }

   /*
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Forward Command ERROR. force_multiple_coils (%d)\n", ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, WRITE_ADDRESS, 1);

    MotionCommand[0] = 0;
    MotionControlSP.s = IPS_ALERT;
    IUResetSwitch(&MotionControlSP);
    MotionControlS[0].s = ISS_ON;
    IDSetSwitch(&MotionControlSP, "Error: %s drive failed to move %s", type_name.c_str(), reverse_motion.c_str());
    */
    return false;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::stop()
{
    //int ret;

    if (!check_drive_connection())
        return false;

    // Already moving backwards
    //if (MotionCommand[0] == INVERTER_STOP)
        //return true;

    //MotionCommand[0] = INVERTER_STOP;

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(),INDI::Logger::DBG_SESSION, "%s drive: Simulating stop command.", type_name.c_str());
        MotionControlSP.s = IPS_IDLE;
        IDSetSwitch(&MotionControlSP, "%s drive is stopped", type_name.c_str());
        return true;
     }

   /* DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Stop Command ERROR. force_multiple_coils (%d)\n", ret);
    DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Slave = %d, address = %d, nb = %d\n", SLAVE_ADDRESS, WRITE_ADDRESS, 1);

    MotionCommand[0] = 0;
    MotionControlSP.s = IPS_ALERT;
    IUResetSwitch(&MotionControlSP);
    MotionControlS[0].s = ISS_ON;
    IDSetSwitch(&MotionControlSP, "Error: %s drive failed to stop", type_name.c_str());
    */
    return false;

}

/****************************************************************
**
**
*****************************************************************/
bool AMCController::set_speed(double rpm)
{
    if (!check_drive_connection())
        return false;

    if (rpm < 0. || rpm > 5.)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_ERROR, "set_speed: requested RPM %g is outside boundary limits (0,5) RPM", rpm);
        return false;
    }

    if (simulation)
    {
        DEBUGFDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_SESSION, "Simulating set speed to %g RPM", rpm);
        MotorSpeedNP.s = IPS_OK;
        IDSetNumber(&MotorSpeedNP, NULL);
        return true;
    }
    // TODO: Set drive speed here

    return false;
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

    }
    else
    {
        telescope->deleteProperty(MotionControlSP.name);
        telescope->deleteProperty(MotorSpeedNP.name);
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
        set_speed(values[0]);

        return true;
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

        if (MotionControlS[MOTOR_STOP].s == ISS_ON)
          stop();
        else if (MotionControlS[MOTOR_FORWARD].s == ISS_ON)
          move_forward();
        else if (MotionControlS[MOTOR_REVERSE].s == ISS_ON)
            move_reverse();

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
        fprintf (stderr, "socket(%s,%d): %s\n", host, rs485_port,strerror(errno));
        return -1;
    }

    /* connect */
    if (::connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
    {
        fprintf (stderr, "connect(%s,%d): %s\n", host,rs485_port,strerror(errno));
        return -1;
    }

    /* ok */
    return (sockfd);
}

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

    /* Linear Ramp Positive Target Negative Change
     * This parameter limits the deceleration for a velocity
     * commanded in the positive direction.
     * Address: 3C.03h ((3Ch + 03h)
     * Offset : 3 bytes
    */
    rc = setDeceleration(MOTOR_FORWARD, MOTOR_DECELERATION);

    /* Linear Ramp Negative Target Negative Change
     * This parameter limits the acceleration for a velocity
     * commanded in the negative direction.
     * Address: 3C.06h ((3Ch + 06h)
     * Offset : 6 bytes
    */
    rc = setAcceleration(MOTOR_REVERSE, MOTOR_ACCELERATION);

    /*  Linear Ramp Negative Target Positive Change
     *  This parameter limits the deceleration for a velocity
     *  commanded in the negative direction.
     *  Address: 3C.09h ((3Ch + 09h)
     *  Offset : 9 bytes
    */
    rc = setDeceleration(MOTOR_REVERSE, MOTOR_DECELERATION);
}

bool AMCController::setAcceleration(motorMotion dir, double rpmAcceleration)
{
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

    // Reset & Calculate CRC
    ResetCRC();

    for (int i=0; i<=5; i++)
            CrunchCRC(command[i]);

    CrunchCRC(0);
    CrunchCRC(0);

    // CRC - MSB First
    command[6] =  accum >> 8;
    command[7] =  accum & 0xFF;

    /******************************
    **** END OF HEADER SECTION ****
    ******************************/

    /******************************
    **** START OF DATA SECTION ****
    ******************************/





    return true;

}

bool AMCController::setDeceleration(motorMotion dir, double rpmDeAcceleration)
{
    switch (dir)
    {
        case MOTOR_FORWARD:
        break;

        case MOTOR_REVERSE:
        break;

        default:
            return false;

    }



    return true;

}


void AMCController::ResetCRC()
{
    // Resets the Accumulator
    // Call before each new CRCaccum = 0;
    accum = 0;
}

void AMCController::CrunchCRC (char x)
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
