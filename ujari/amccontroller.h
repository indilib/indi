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

class AMCController
{

public:
    typedef enum { RA_MOTOR, DEC_MOTOR } motorType;
    enum motorMotion { MOTOR_STOP, MOTOR_FORWARD, MOTOR_REVERSE};

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
    bool check_drive_connection();

    bool set_speed(double rpm);
    bool move_forward();
    bool move_reverse();
    bool stop();
    bool is_in_motion();

    // Simulation
    void set_simulation(bool enable) { simulation = enable;}
    // Debug
    void set_debug(bool enable) { debug = enable; }
    // Verbose
    void set_verbose(bool enable) { verbose = enable; }

    motorType getType() const;
    void setType(const motorType &value);



private:

    int openRS485Server (const char *host, int rs485_port);
    bool setupDriveParameters();
    void ResetCRC();
    void CrunchCRC (char x);

    bool setAcceleration(motorMotion dir, double rpmAcceleration);
    bool setDeceleration(motorMotion dir, double rpmDeAcceleration);

    // Inverter Port
    ITextVectorProperty PortTP;
    IText PortT[1];

    // Motor Speed (RPM)
    INumber MotorSpeedN[1];
    INumberVectorProperty MotorSpeedNP;

    // Motion Control
    ISwitch MotionControlS[3];
    ISwitchVectorProperty MotionControlSP;

    motorType type;

    std::string default_port;
    std::string type_name;

    bool simulation;
    bool debug;
    bool verbose;
    int connection_status;
    int fd;
    unsigned char SLAVE_ADDRESS;
    const unsigned int Gr1;
    unsigned int accum;
    unsigned char command[8];

    Ujari *telescope;
   
};

#endif
