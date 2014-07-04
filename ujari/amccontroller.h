#if 0
    INDI Ujari Driver - AMC Controller
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

#ifndef AMCCONTROLLER_H
#define AMCCONTROLLER_H

#include <indidevapi.h>
#include <string>

class Ujari;


// Drive Status Bits
#define DS_BRIDGE                   1<<0
#define DS_DYNAMIC_BRAKE            1<<1
#define DS_STOP                     1<<2
#define DS_POSITIVE_STOP            1<<3
#define DS_NEGATIVE_STOP            1<<4
#define DS_POSITIVE_TORQUE_INHIBIT  1<<5
#define DS_NEGATIVE_TORQUE_INHIBIT  1<<6
#define DS_EXTERNAL_BRAKE           1<<7

// Drive Protection Bits
#define DP_DRIVE_RESET              1<<0
#define DP_DRIVE_INTERNAL_ERROR     1<<1
#define DP_SHORT_CIRCUT             1<<2
#define DP_CURRENT_OVERSHOOT        1<<3
#define DP_UNDER_VOLTAGE            1<<4
#define DP_OVER_VOLTAGE             1<<5
#define DP_DRIVE_OVER_TEMPERATURE   1<<6

// Drive Control Parameters
#define CP_COMMANDED_STOP           1<<6
#define CP_RESET_EVENTS             1<<12

class AMCController
{

public:
    typedef enum { RA_MOTOR, DEC_MOTOR } motorType;
    typedef enum { MOTOR_STOP, MOTOR_FORWARD, MOTOR_REVERSE} motorMotion;
    typedef enum { AMC_COMMAND_COMPLETE, AMC_COMMAND_INCOMPLETE, AMC_INVALID_COMMAND, AMC_NO_WRITE_ACCESS, AMC_CRC_ERROR, AMC_COMM_ERROR, AMC_UNKNOWN_ERROR } driveStatus;

    AMCController(motorType type, Ujari* scope);
    ~AMCController();

    // Standard INDI interface fucntions
    virtual bool initProperties();
    virtual bool updateProperties(bool connected);
    virtual void ISGetProperties();
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

    bool connect();
    void disconnect();
    bool isDriveOnline();

    bool setSpeed(double rpm);
    double getSpeed() { return MotorSpeedN[0].value; }

    bool moveForward();
    bool moveReverse();

    bool stop();
    bool enableMotion();

    bool resetFault();
    bool setControlParameter(unsigned short command);
    bool isProtectionTriggered();

    // Simulation
    void setSimulation(bool enable) { simulation = enable;}
    // Debug
    void setDebug(bool enable) { debug = enable; }
    // Verbose
    void setVerbose(bool enable) { verbose = enable; }

    motorType getType() const;
    void setType(const motorType &value);

    motorMotion getMotionStatus() { return state; }
    bool isMotionActive();

    // Update
    static void * update_helper(void *context);
    bool update();

private:

    int openRS485Server (const char *host, int rs485_port);
    bool setupDriveParameters();
    void CrunchCRC (unsigned int &accum, char x);

    bool enableWriteAccess();
    bool enableBridge();
    bool setAcceleration(motorMotion dir, double rpmAcceleration);
    bool setDeceleration(motorMotion dir, double rpmDeAcceleration);

    bool setMotion(motorMotion dir);

    driveStatus readDriveStatus();
    driveStatus readDriveData(unsigned char *data, unsigned char len);

    void flushFD();

    const char *driveStatusString(driveStatus status);

    void lock_mutex();
    void unlock_mutex();

    // Inverter Port
    ITextVectorProperty PortTP;
    IText PortT[1];

    // Motor Speed (RPM)
    INumber MotorSpeedN[1];
    INumberVectorProperty MotorSpeedNP;

    // Motion Control
    ISwitch MotionControlS[3];
    ISwitchVectorProperty MotionControlSP;

    ISwitch ResetFaultS[1];
    ISwitchVectorProperty ResetFaultSP;

    ILight DriveStatusL[8];
    ILightVectorProperty DriveStatusLP;

    ILight DriveProtectionL[7];
    ILightVectorProperty DriveProtectionLP;

    motorType type;

    std::string default_port;
    std::string type_name;

    motorMotion state;
    bool simulation;
    bool debug;
    bool verbose;
    int connection_status;
    int fd;
    unsigned char SLAVE_ADDRESS;
    const unsigned int Gr1;
    double currentRPM, targetRPM;
    struct timeval last_update;

    Ujari *telescope;

    pthread_t controller_thread;
   
};

#endif
