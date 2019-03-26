/*******************************************************************************
 Copyright(c) 2017 Rozeware Development Ltd. All rights reserved.
 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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

#ifndef NexDome_H
#define NexDome_H

#include <indidome.h>

/*  Some headers we need */
#include <math.h>
#include <sys/time.h>

class NexDome : public INDI::Dome
{
    public:
        NexDome();
        virtual ~NexDome() = default;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool initProperties();
        const char *getDefaultName();
        bool updateProperties();

    protected:

        bool Handshake();
        void TimerHit();

        virtual IPState Move(DomeDirection dir, DomeMotionCommand operation);
        virtual IPState MoveAbs(double az);
        virtual IPState Park();
        virtual IPState UnPark();
        virtual IPState ControlShutter(ShutterOperation operation);
        virtual bool Abort();

        // Config
        virtual bool saveConfigItems(FILE * fp);

        // Parking
        virtual bool SetCurrentPark();
        virtual bool SetDefaultPark();

        /////////////////////////////////////////////////////////////////////////////
        /// Properties
        /////////////////////////////////////////////////////////////////////////////

        ISwitchVectorProperty HomeSP;
        ISwitch HomeS[1];

        ISwitchVectorProperty CalibrateSP;
        ISwitch CalibrateS[1];

        INumberVectorProperty SyncPositionNP;
        INumber SyncPositionN[1];

        INumberVectorProperty HomePositionNP;
        INumber HomePositionN[1];

        INumberVectorProperty BatteryLevelNP;
        INumber BatteryLevelN[2];

        ITextVectorProperty FirmwareVersionTP;
        IText FirmwareVersionT[1] {};

        ISwitchVectorProperty ReversedSP;
        ISwitch ReversedS[2];

    private:
        ///////////////////////////////////////////////////////////////////////////////
        /// Utility Functions
        ///////////////////////////////////////////////////////////////////////////////
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);

        ///////////////////////////////////////////////////////////////////////////////
        /// Read Dome data
        ///////////////////////////////////////////////////////////////////////////////
        bool readStartupParameters();
        void readDomeStatus();
        bool readBatteryLevel();
        bool readStepsPerRevolution();
        bool readMotionStatus();
        bool readPosition();
        bool readReversedStatus();

        ///////////////////////////////////////////////////////////////////////////////
        /// Home Commands
        ///////////////////////////////////////////////////////////////////////////////
        bool readHomeSensor();
        bool readHomeError();
        bool readHomePosition();

        ///////////////////////////////////////////////////////////////////////////////
        /// Shutter Commands
        ///////////////////////////////////////////////////////////////////////////////
        bool readShutterStatus();
        bool readShutterPosition();

        void processDomeMessage(const char *);
        int ClearSerialBuffers();

        double m_BatteryMain { 0 };
        double m_BatteryShutter { 0 };
        bool m_MotorPower { false };

        double m_ShutterPosition { 0 };
        double m_HomeError { 0 };
        double m_HomeAz { -1 };

        double m_StoredPark { - 1};

        int m_ShutterState { -1 };
        int m_DomeReversed { -1 };
        int m_StepsPerDomeTurn { 0 };

        time_t CalStartTime;

        bool m_InMotion { false };
        bool AtHome;
        bool m_Calibrating { false };
        int m_TimeSinceUpdate {0};

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        // CR is the stop char
        static const char DRIVER_STOP_CHAR { 0xD };
        // Wait up to a maximum of 3 seconds for serial input
        static constexpr const uint8_t DRIVER_TIMEOUT {1};
        // Maximum buffer for sending/receving.
        static constexpr const uint8_t DRIVER_LEN {64};
};

#endif
