#if 0
    INDI Ujari Driver - Encoder
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

#ifndef ENCODER_H
#define ENCODER_H

#include <indidevapi.h>
#include <string>

#include "ujarierror.h"

using namespace std;

class Ujari;

class Encoder
{

public:
        typedef enum { RA_ENCODER, DEC_ENCODER, DOME_ENCODER } encoderType;
        enum { EN_HOME_POSITION, EN_HOME_OFFSET, EN_TOTAL };

        Encoder(encoderType value, Ujari *scope);
        ~Encoder();

        // Standard INDI interface fucntions
        virtual bool initProperties();
        virtual bool updateProperties(bool connected);
        virtual void ISGetProperties();
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
        virtual bool saveConfigItems(FILE *fp);

        bool connect();
        void disconnect();
        bool update();

        // Simulation
        void setSimulation(bool enable) { simulation = enable;}
        // Debug
        void setDebug(bool enable) { debug = enable; }
        // Verbose
        void setVerbose(bool enable) { verbose = enable; }

        encoderType getType() const;
        void setType(const encoderType &value);

        unsigned long getEncoderValue();
        void setEncoderValue(unsigned long value);

        unsigned long GetEncoder()  throw (UjariError);
        unsigned long GetEncoderZero();
        unsigned long GetEncoderTotal();
        unsigned long GetEncoderHome();

        // 0 forward/1 reverese
        void simulateEncoder(double speed, int dir);

private:
        // Encoder settings
        INumber encoderSettingsN[3];
        INumberVectorProperty encoderSettingsNP;

        // encoder value
        INumber encoderValueN[1];
        INumberVectorProperty encoderValueNP;

        encoderType type;

        std::string type_name;

        bool simulation;
        bool debug;
        bool verbose;
        int connection_status;
        unsigned short SLAVE_ADDRESS;

        double encoderAngle;
        double ticksToDegreeRatio;

        Ujari *telescope;
   
};

#endif
