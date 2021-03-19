

#pragma once

#include "indifocuser.h"
#include <chrono>

class RBFOCUS : public INDI::Focuser
{
    public:
        RBFOCUS();
        virtual ~RBFOCUS() override = default;
          typedef enum {HOLD_OFF, HOLD_ON } focuserHold;
        typedef enum {REVERSED, NORMAL } dir;

        const char * getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;
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


        /**
         * @brief SyncFocuser Set the supplied position as the current focuser position
         * @param ticks target position
         * @return IPS_OK if focuser position is now set to ticks. IPS_ALERT for problems.
         */
        virtual bool SyncFocuser(uint32_t ticks) override;
        virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
        virtual bool AbortFocuser() override;
        virtual void TimerHit() override;

    private:
        bool Ack();
        /**
         * @brief sendCommand Send a string command to RBFocus.
         * @param cmd Command to be sent, must already have the necessary delimeter ('#')
         * @param res If not nullptr, the function will read until it detects the default delimeter ('#') up to ML_RES length.
         *        if nullptr, no read back is done and the function returns true.
         * @return True if successful, false otherwise.
         */
        bool sendCommand(const char * cmd, char * res = nullptr);

        // Read and update Temperature
        bool readTemperature();
        // Read and update Position
        bool readPosition();
        // Read Version
        bool readVersion();
        bool readHold();
        bool setHold();
        bool readDir();
        bool setDir();
        bool isMoving();
        bool MaxPos();
        // Read Only Temperature Reporting
        INumber TemperatureN[1];
        INumberVectorProperty TemperatureNP;
        ISwitch focuserHoldS[2];
        ISwitchVectorProperty focuserHoldSP;
        ISwitch dirS[3];
        ISwitchVectorProperty dirSP;
        double targetPos { 0 }, lastPos { 0 };
        double lastTemperature { 0 };


        static const uint32_t DRIVER_RES { 32 };

        static const char DRIVER_DEL { '#' };
        static const char DRIVER_DEL2 { ' ' };

        static const uint8_t DRIVER_TIMEOUT { 10 };


};
