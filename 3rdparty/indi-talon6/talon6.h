/*******************************************************************************
 Copyright(c) 2017 Rozeware Development Ltd. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef Talon6_H
#define Talon6_H

#include <indidome.h>

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>


class Talon6 : public INDI::Dome
{

    public:
        Talon6();
        virtual ~Talon6();

        virtual bool ISNewSwitch(const char *dev,const char *name,ISState *states, char *names[],int n);
        virtual bool ISNewNumber(const char *dev,const char *name,double values[],char *names[],int n);
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

        virtual bool initProperties();
        virtual void ISGetProperties(const char *dev) override;

        const char *getDefaultName();
        bool updateProperties();

    protected:

        // Get Status button from device
        ISwitch StatusS[1];
        ISwitchVectorProperty StatusSP;
        // Safety Condition switch
        ISwitch SafetyS[2];
        ISwitchVectorProperty SafetySP;

        // Roof Go To Value
        INumber GoToN[1] {};
        INumberVectorProperty GoToNP;

        // Status values read from device
        IText StatusValueT[8] {};
        ITextVectorProperty StatusValueTP;
        // Firmware version
        IText FirmwareVersionT[1] {};
        ITextVectorProperty FirmwareVersionTP;
        // Encoder Max Ticks
        INumber EncoderTicksN[1] {};
        INumberVectorProperty EncoderTicksNP;
        // Sensors
        ILight SensorsL[5] {};
        ILightVectorProperty SensorsLP;
        // Switches
        ILight SwitchesL[5] {};
        ILightVectorProperty SwitchesLP;

         virtual bool saveConfigItems(FILE *fp) override;

        bool Disconnect();
        void TimerHit();
        ISState fullOpenRoofSwitch { ISS_ON };
        ISState fullClosedRoofSwitch { ISS_OFF };
        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);

        virtual IPState Park();
        virtual IPState UnPark();
        virtual IPState DomeGoTo(int GoTo);
        virtual bool Abort();

    private:

        bool Handshake();
        int PortFD{-1};
        double MotionRequest { 0 };
        void getDeviceStatus();
        void getFirmwareVersion();
        int ReadString(char *,int);
        int WriteString(const char *);
        void ProcessDomeMessage(char *);
        char ShiftChar(char shiftChar);

};

#endif
