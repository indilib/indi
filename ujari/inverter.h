#if 0
    INDI Ujari Driver - Inverter
    Copyright (C) 2014 Jasem Mutlaq

    Based on EQMod INDI Driver by Jean-Luc
    
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

#ifndef INVERTER_H
#define INVERTER_H

#include <indidevapi.h>
#include <indicom.h>
#include <string>
#include <bitset>
#include <iostream>

#include "hy_modbus.h"

using namespace std;

class Ujari;

class Inverter
{

    public:

        enum inverterType { DOME_INVERTER, SHUTTER_INVERTER };
        enum inverterMotion { INVERTER_STOP, INVERTER_FORWARD, INVERTER_REVERSE};


        Inverter(inverterType new_type, Ujari *scope);
        ~Inverter();

          bool move_forward();
          bool move_reverse();
          bool stop();

          bool set_speed(float newHz);
          float get_speed() { return InverterSpeedN[0].value; }

          void set_type(inverterType new_type);
          inverterType get_type() { return type; }

          bool connect();
          void disconnect();

        bool is_in_motion();
        // Simulation
        void setSimulation(bool enable);
        bool isSimulation() { return simulation;}

        void enable_debug() { debug = true; mb_param.debug = 1; }
        void disable_debug() { debug = false; mb_param.debug = 0;}

        // Standard INDI interface fucntions
        virtual bool initProperties();
        virtual void ISGetProperties();
        virtual bool updateProperties(bool connected);
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

        void reset_all_properties();

        void set_verbose(bool enable) { verbose = enable; }

    private:

        // Modbus functions
        bool enable_drive();
        bool disable_drive();
        bool init_drive();

        // INDI Properties

        // Inverter Speed (Hz)
        INumber InverterSpeedN[1];
        INumberVectorProperty InverterSpeedNP;

        // Motion Control
        ISwitch MotionControlS[3];
        ISwitchVectorProperty MotionControlSP;

        // Inverter Port
        ITextVectorProperty PortTP;
        IText PortT[1];

        // Functions
        bool check_drive_connection();

        // Variable
        int connection_status;
        inverterType type;
        bool simulation;
        bool debug;
        bool verbose;

        string type_name;
        string forward_motion;
        string reverse_motion;
        string default_port;

        //modbus_t * mb_param;

        modbus_param_t mb_param;
        modbus_data_t mb_data;

        uint SLAVE_ADDRESS;

        // Stop, Forward, Reverse
        inverterMotion motionStatus;

        Ujari *telescope;

};

#endif

