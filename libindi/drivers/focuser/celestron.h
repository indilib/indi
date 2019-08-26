/*
    Celestron Focuser for SCT and EDGEHD

    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2019 Chris Rowland

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

#pragma once

#include "indifocuser.h"

#include <chrono>
#include "celestronauxpacket.h"

class CelestronSCT : public INDI::Focuser
{
    public:
        CelestronSCT();

        const char * getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        //virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        /**
         * @brief Handshake Try to communicate with Focuser and see if there is a valid response.
         * @return True if communication is successful, false otherwise.
         */
        virtual bool Handshake() override;

        /**
         * @brief MoveAbsFocuser Move to an absolute target position
         * @param targetTicks target position
         * @return IPS_BUSY if motion is in progress. IPS_OK if motion is small and already complete. IPS_ALERT for trouble.
         */
        virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;

        /**
         * @brief MoveRelFocuser Move focuser for a relative amount of ticks in a specific direction
         * @param dir Directoin of motion
         * @param ticks steps to move
         * @return IPS_BUSY if motion is in progress. IPS_OK if motion is small and already complete. IPS_ALERT for trouble.
         */
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;

        /**
         * @brief AbortFocuser Abort Focuser motion
         * @return True if successful, false otherwise.
         */
        virtual bool AbortFocuser() override;

        /**
         * @brief TimerHit Primary Loop called every POLLMS milliseconds (set in Options) to check
         * on the focuser status like read position, temperature.. and check if the focuser reached the required position.
         */
        virtual void TimerHit() override;
        virtual bool SetFocuserBacklash(int32_t steps) override;

    private:
        Aux::Communicator communicator;

        // Do we have a response from the focuser?
        bool Ack();

        // Get initial focuser parameters when we first connect
        bool getStartupParameters();

        ///////////////////////////////////////////////////////////////////////////////
        /// Read Data From Controller
        ///////////////////////////////////////////////////////////////////////////////
        // Read and update Position
        bool readPosition();
        // Are we moving?
        bool isMoving();
        // read limits
        bool readLimits();

        ///////////////////////////////////////////////////////////////////////////////
        /// Write Data to Controller
        ///////////////////////////////////////////////////////////////////////////////
        bool startMove(uint32_t position);

        ///////////////////////////////////////////////////////////////////////////////
        /// Properties
        ///////////////////////////////////////////////////////////////////////////////
        //        INumber BacklashN[1];
        //        INumberVectorProperty BacklashNP;

        INumber FocusMinPosN[1];
        INumberVectorProperty FocusMinPosNP;

        bool backlashMove;      // set if a final move is needed
        uint32_t finalPosition;

        ISwitch CalibrateS[2];
        ISwitchVectorProperty CalibrateSP;

        IText CalibrateStateT[1];
        ITextVectorProperty CalibrateStateTP;

        bool calibrateInProgress;
        int calibrateState;
        bool focuserIsCalibrated;

        /////////////////////////////////////////////////////////////////////////////
        /// Static Helper Values
        /////////////////////////////////////////////////////////////////////////////
        //        // Celestron Buffer
        //        static const uint8_t CELESTRON_LEN { 32 };
        //        // Celestorn Delimeter
        //        static const char CELESTRON_DEL { '#' };
        //        // Celestron Tiemout in seconds
        //        static const uint8_t CELESTRON_TIMEOUT { 3 };
};
