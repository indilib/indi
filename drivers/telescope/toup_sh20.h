#pragma once

#include "lx200generic.h"
#include "indipropertyswitch.h"
#include "indipropertynumber.h"

class ToupSH20 : public LX200Generic
{
    public:
        ToupSH20();
        virtual ~ToupSH20() override = default;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

    protected:
        virtual const char *getDefaultName() override;

        virtual bool checkConnection() override;

        virtual bool ReadScopeStatus() override;

        // Location
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        // Date
        virtual bool setUTCOffset(double offset) override;
        virtual bool setLocalDate(uint8_t days, uint8_t months, uint16_t years) override;
        // Goto
        virtual bool Goto(double ra, double dec) override;
        // Slew Rate
        virtual bool SetSlewRate(int index) override;
        // Track
        virtual bool SetTrackEnabled(bool enabled) override;
        // Park
        virtual bool SetCurrentPark() override;
        virtual bool Park() override;
        virtual bool UnPark() override;
        // Home
        virtual IPState ExecuteHomeAction(TelescopeHomeAction action) override;

    private:
        /**
        * @brief sendCommand Send a string command to device.
        * @param cmd Command to be sent. Can be either NULL TERMINATED or just byte buffer.
        * @param res If not nullptr, the function will wait for a response from the device. If nullptr, it returns true immediately after the command is successfully sent.
        * @param cmd_len if -1, it is assumed that the @a cmd is a null-terminated string. Otherwise, it would write @a cmd_len bytes from the @a cmd buffer.
        * @param res_len if -1 and if @a res is not nullptr, the function will read until it detects the default delimiter DRIVER_STOP_CHAR up to DRIVER_MAX_LEN length.
        * Otherwise, the function will read @a res_len from the device and store it in @a res.
        * @return True if successful, false otherwise.
        */
        bool sendCommand(const char * cmd, char * res = nullptr, int cmd_len = -1, int res_len = -1);
        void hexDump(char * buf, const char * data, int size);
        
        void getStartupData();

        // Mount type
        bool setMountType(int type);
        bool getMountType();
        // Home
        bool goHome();
        bool findHome();
        // Track mode
        bool getTrackMode();
        // Guide rate
        bool setGuideRate(double value);
        bool getGuideRate();
        // Buzzer
        bool setBuzzer(int value);
        bool getBuzzer();
        // Meridian Flip
        bool setMeridianFlipSettings(bool enabled, bool track, double limit);
        bool getMeridianFlipSettings();

        static constexpr uint8_t        SLEW_RATE_COUNT = 10;
        static constexpr uint8_t        DRIVER_LEN = 64;
        static constexpr uint8_t        DRIVER_TIMEOUT = 3;
        static constexpr char           DRIVER_STOP_CHAR = 0x23;
        
        enum TypeSwitch
        {
            TYPE_AZIMUTH,
            TYPE_EQUATORIAL
        };

        enum BuzzerSwitch
        {
            BUZZER_OFF,
            BUZZER_LOW,
            BUZZER_HIGH
        };

        enum MeridianTrackSwitch
        {
            MERIDIAN_TRACK,
            MERIDIAN_STOP
        };

        INDI::PropertySwitch        MountTypeSP {2};
        INDI::PropertyNumber        GuideRateNP {1};
        INDI::PropertySwitch        BuzzerSP {3};
        INDI::PropertySwitch        MeridianFlipSP {2};
        INDI::PropertySwitch        MeridianTrackSP {2};
        INDI::PropertyNumber        MeridianLimitNP {1};
};
