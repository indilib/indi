#pragma once

#include <cstdint>

// Forward declaration to avoid circular dependencies
class GeminiFlatpanel;

// Constants that may be needed by adapters
#define GEMINI_MIN_BRIGHTNESS 0
#define GEMINI_MAX_BRIGHTNESS 255
#define SERIAL_TIMEOUT_SEC 10
#define SERIAL_TIMEOUT_SEC_LONG 120
#define NO_VALUE 1000 // Used to indicate that no value is provided
// Note: MAXRBUF is already defined in INDI headers

// Gemini device constants
enum GeminiRevision
{
    GEMINI_REVISION_UNKNOWN = 0,
    GEMINI_REVISION_1 = 1,
    GEMINI_REVISION_2 = 2,
    GEMINI_REVISION_LITE = 3
};

enum GeminiConfigStatus
{
    GEMINI_CONFIG_NOTREADY = 0,
    GEMINI_CONFIG_READY = 1,
    GEMINI_CONFIG_CLOSED = 2,
    GEMINI_CONFIG_OPEN = 3,
};

enum GeminiBrightnessMode
{
    GEMINI_BRIGHTNESS_MODE_LOW = 0,
    GEMINI_BRIGHTNESS_MODE_HIGH = 1
};

// Status constants used by all adapters
enum GeminiCoverStatus
{
    GEMINI_COVER_STATUS_MOVING = 0,
    GEMINI_COVER_STATUS_CLOSED = 1,
    GEMINI_COVER_STATUS_OPEN = 2,
    GEMINI_COVER_STATUS_TIMED_OUT = 3
};

enum GeminiLightStatus
{
    GEMINI_LIGHT_STATUS_OFF = 0,
    GEMINI_LIGHT_STATUS_ON = 1
};

enum GeminiMotorStatus
{
    GEMINI_MOTOR_STATUS_STOPPED = 0,
    GEMINI_MOTOR_STATUS_RUNNING = 1
};

enum GeminiDirection
{
    GEMINI_DIRECTION_CLOSE = -1,
    GEMINI_DIRECTION_OPEN = 1
};

enum GeminiBeepStatus
{
    GEMINI_BEEP_OFF = 0,
    GEMINI_BEEP_ON = 1
};

/**
 * @brief Abstract adapter interface for Gemini Flatpanel firmware versions
 *
 * This interface abstracts the differences between firmware revisions,
 * providing a uniform command-oriented API for the main driver to use.
 */
class GeminiFlatpanelAdapter
{
    public:
        virtual ~GeminiFlatpanelAdapter() = default;

        // Device detection and identification

        /**
         * @brief Ping the device to check if it responds to this adapter's protocol
         * @return true if device responds correctly, false otherwise
         */
        virtual bool ping() = 0;

        /**
         * @brief Get the firmware revision this adapter supports
         * @return revision number (1, 2, etc.)
         */
        virtual int getRevision() const = 0;

        /**
         * @brief Get the firmware version from the device
         * @param version pointer to store the firmware version
         * @return true if successful, false otherwise
         */
        virtual bool getFirmwareVersion(int *version) = 0;

        // Capability checks

        /**
         * @brief Check if device supports beep functionality
         * @return true if beep is supported
         */
        virtual bool supportsBeep() const = 0;

        /**
         * @brief Check if device supports dust cap functionality
         * @return true if dust cap is supported
         */
        virtual bool supportsDustCap() const = 0;

        /**
         * @brief Check if device supports brightness mode selection (high/low)
         * @return true if brightness modes are supported
         */
        virtual bool supportsBrightnessMode() const = 0;

        // Basic device commands (supported by all revisions)

        /**
         * @brief Get the current configuration status
         * @param status pointer to store the configuration status
         * @return true if successful, false otherwise
         */
        virtual bool getConfigStatus(int *status) = 0;

        /**
         * @brief Get the current brightness level
         * @param brightness pointer to store the brightness value (0-255)
         * @return true if successful, false otherwise
         */
        virtual bool getBrightness(int *brightness) = 0;

        /**
         * @brief Set the brightness level
         * @param value brightness value (0-255)
         * @return true if successful, false otherwise
         */
        virtual bool setBrightness(int value) = 0;

        /**
         * @brief Turn the light on
         * @return true if successful, false otherwise
         */
        virtual bool lightOn() = 0;

        /**
         * @brief Turn the light off
         * @return true if successful, false otherwise
         */
        virtual bool lightOff() = 0;

        /**
         * @brief Open the dust cover
         * @return true if successful, false otherwise
         */
        virtual bool openCover() = 0;

        /**
         * @brief Close the dust cover
         * @return true if successful, false otherwise
         */
        virtual bool closeCover() = 0;

        /**
         * @brief Get the current device status
         * @param coverStatus pointer to store cover status
         * @param lightStatus pointer to store light status
         * @param motorStatus pointer to store motor status
         * @return true if successful, false otherwise
         */
        virtual bool getStatus(int *coverStatus, int *lightStatus, int *motorStatus) = 0;

        // Motion/calibration commands

        /**
         * @brief Move the cover by a specified amount
         * @param value number of steps to move
         * @param direction direction of movement (1 for open, -1 for close)
         * @return true if successful, false otherwise
         */
        virtual bool move(uint16_t value, int direction) = 0;

        /**
         * @brief Set the current position as the closed position
         * @return true if successful, false otherwise
         */
        virtual bool setClosePosition() = 0;

        /**
         * @brief Set the current position as the open position
         * @return true if successful, false otherwise
         */
        virtual bool setOpenPosition() = 0;

        // Advanced commands (may not be supported by all revisions)

        /**
         * @brief Enable or disable beep functionality
         * @param enable true to enable beep, false to disable
         * @return true if successful, false if not supported or failed
         */
        virtual bool setBeep(bool enable) = 0;

        /**
         * @brief Set the brightness mode (high/low)
         * @param mode brightness mode (GEMINI_BRIGHTNESS_MODE_LOW or GEMINI_BRIGHTNESS_MODE_HIGH)
         * @return true if successful, false if not supported or failed
         */
        virtual bool setBrightnessMode(int mode) = 0;

        // Communication setup

        /**
         * @brief Get the command terminator character for this revision
         * @return terminator character ('\n' for rev1, '#' for rev2)
         */
        virtual char getCommandTerminator() const = 0;

        /**
         * @brief Set up communication parameters for this revision
         * @param portFD file descriptor for the serial port
         */
        virtual void setupCommunication(int portFD) = 0;
};

/**
 * @brief Concrete adapter implementation for Gemini Flatpanel Revision 1 firmware
 *
 * This adapter handles the specific protocol and commands for revision 1 devices:
 * - Command format: >X000# (no value) or >XNNN# (3-digit padded value)
 * - Command terminator: '\n'
 * - Response format: *Xnnn with 3-digit zero-padded numeric values
 * - Limited features: no beep, no brightness mode selection
 */
class GeminiFlatpanelRev1Adapter : public GeminiFlatpanelAdapter
{
    private:
        int portFD;

        // Internal helper methods for Rev1 protocol

        /**
         * @brief Format a command according to Rev1 protocol
         * @param commandLetter the command character (B, L, D, etc.)
         * @param commandString buffer to store the formatted command
         * @param value optional value to include (NO_VALUE if not needed)
         * @return true if successful, false otherwise
         */
        bool formatCommand(char commandLetter, char *commandString, int value = NO_VALUE);

        /**
         * @brief Send a command to the device and receive response
         * @param command command string to send
         * @param response buffer to store response (can be nullptr if no response expected)
         * @param timeout timeout in seconds
         * @param log whether to log errors
         * @return true if successful, false otherwise
         */
        bool sendCommand(const char *command, char *response, int timeout = SERIAL_TIMEOUT_SEC, bool log = true);

        /**
         * @brief Extract integer value from Rev1 response format
         * @param response response string from device
         * @param startPos position to start extracting from
         * @param value pointer to store extracted value
         * @return true if successful, false otherwise
         */
        bool extractIntValue(const char *response, int startPos, int *value);

    public:
        GeminiFlatpanelRev1Adapter() : portFD(-1) {}

        // Device detection and identification
        bool ping() override;
        int getRevision() const override
        {
            return GEMINI_REVISION_1;
        }
        bool getFirmwareVersion(int *version) override;

        // Capability checks
        bool supportsBeep() const override
        {
            return false;
        }
        bool supportsDustCap() const override
        {
            return true;
        }
        bool supportsBrightnessMode() const override
        {
            return false;
        }

        // Basic device commands
        bool getConfigStatus(int *status) override;
        bool getBrightness(int *brightness) override;
        bool setBrightness(int value) override;
        bool lightOn() override;
        bool lightOff() override;
        bool openCover() override;
        bool closeCover() override;
        bool getStatus(int *coverStatus, int *lightStatus, int *motorStatus) override;

        // Motion/calibration commands
        bool move(uint16_t value, int direction) override;
        bool setClosePosition() override;
        bool setOpenPosition() override;

        // Advanced commands (not supported by Rev1)
        bool setBeep(bool enable) override
        {
            (void)enable;    // Not supported
            return false;
        }
        bool setBrightnessMode(int mode) override
        {
            (void)mode;    // Not supported
            return false;
        }

        // Communication setup
        char getCommandTerminator() const override
        {
            return '\n';
        }
        void setupCommunication(int portFD) override
        {
            this->portFD = portFD;
        }
};

/**
 * @brief Concrete adapter implementation for Gemini Flatpanel Revision 2 firmware
 *
 * This adapter handles the specific protocol and commands for revision 2 devices:
 * - Command format: >X# (no value) or >Xnnn# (with value, no padding)
 * - Command terminator: '#'
 * - Response format: *X<variable_length_number># or *X<text>#
 * - Enhanced features: beep control, brightness mode selection
 * - Firmware version reporting
 */
class GeminiFlatpanelRev2Adapter : public GeminiFlatpanelAdapter
{
    private:
        int portFD;

        // Internal helper methods for Rev2 protocol

        /**
         * @brief Format a command according to Rev2 protocol
         * @param commandLetter the command character (B, L, D, etc.)
         * @param commandString buffer to store the formatted command
         * @param value optional value to include (NO_VALUE if not needed)
         * @return true if successful, false otherwise
         */
        bool formatCommand(char commandLetter, char *commandString, int value = NO_VALUE);

        /**
         * @brief Send a command to the device and receive response
         * @param command command string to send
         * @param response buffer to store response (can be nullptr if no response expected)
         * @param timeout timeout in seconds
         * @param log whether to log errors
         * @return true if successful, false otherwise
         */
        bool sendCommand(const char *command, char *response, int timeout = SERIAL_TIMEOUT_SEC, bool log = true);

        /**
         * @brief Extract integer value from Rev2 response format
         * @param response response string from device
         * @param startPos position to start extracting from
         * @param value pointer to store extracted value
         * @return true if successful, false otherwise
         */
        bool extractIntValue(const char *response, int startPos, int *value);

    public:
        GeminiFlatpanelRev2Adapter() : portFD(-1) {}

        // Device detection and identification
        bool ping() override;
        int getRevision() const override
        {
            return GEMINI_REVISION_2;
        }
        bool getFirmwareVersion(int *version) override;

        // Capability checks
        bool supportsBeep() const override
        {
            return true;
        }
        bool supportsDustCap() const override
        {
            return true;
        }
        bool supportsBrightnessMode() const override
        {
            return true;
        }

        // Basic device commands
        bool getConfigStatus(int *status) override;
        bool getBrightness(int *brightness) override;
        bool setBrightness(int value) override;
        bool lightOn() override;
        bool lightOff() override;
        bool openCover() override;
        bool closeCover() override;
        bool getStatus(int *coverStatus, int *lightStatus, int *motorStatus) override;

        // Motion/calibration commands
        bool move(uint16_t value, int direction) override;
        bool setClosePosition() override;
        bool setOpenPosition() override;

        // Advanced commands (fully supported by Rev2)
        bool setBeep(bool enable) override;
        bool setBrightnessMode(int mode) override;

        // Communication setup
        char getCommandTerminator() const override
        {
            return '#';
        }
        void setupCommunication(int portFD) override
        {
            this->portFD = portFD;
        }
};

/**
 * @brief Concrete adapter implementation for Gemini Flatpanel Lite firmware
 *
 * This adapter handles the specific protocol and commands for Lite devices:
 * - Command format: >X# (no value) or >Xnnn# (with value)
 * - Command terminator: '#'
 * - Response format: *X<variable_length_number># or *X<text>#
 * - Features: light control, beep control, brightness mode selection
 * - No dust cap/motor support (lite version is not motorized)
 */
class GeminiFlatpanelLiteAdapter : public GeminiFlatpanelAdapter
{
    private:
        int portFD;

        // Internal helper methods for Lite protocol

        /**
         * @brief Format a command according to Lite protocol
         * @param commandLetter the command character (B, L, D, etc.)
         * @param commandString buffer to store the formatted command
         * @param value optional value to include (NO_VALUE if not needed)
         * @return true if successful, false otherwise
         */
        bool formatCommand(char commandLetter, char *commandString, int value = NO_VALUE);

        /**
         * @brief Send a command to the device and receive response
         * @param command command string to send
         * @param response buffer to store response (can be nullptr if no response expected)
         * @param timeout timeout in seconds
         * @param log whether to log errors
         * @return true if successful, false otherwise
         */
        bool sendCommand(const char *command, char *response, int timeout = SERIAL_TIMEOUT_SEC, bool log = true);

        /**
         * @brief Extract integer value from Lite response format
         * @param response response string from device
         * @param startPos position to start extracting from
         * @param value pointer to store extracted value
         * @return true if successful, false otherwise
         */
        bool extractIntValue(const char *response, int startPos, int *value);

    public:
        GeminiFlatpanelLiteAdapter() : portFD(-1) {}

        // Device detection and identification
        bool ping() override;
        int getRevision() const override
        {
            return GEMINI_REVISION_LITE;
        }
        bool getFirmwareVersion(int *version) override;

        // Capability checks
        bool supportsBeep() const override
        {
            return true;
        }
        bool supportsDustCap() const override
        {
            return false;    // Lite version is not motorized
        }
        bool supportsBrightnessMode() const override
        {
            return true;
        }

        // Basic device commands
        bool getConfigStatus(int *status) override;
        bool getBrightness(int *brightness) override;
        bool setBrightness(int value) override;
        bool lightOn() override;
        bool lightOff() override;
        bool openCover() override;   // Not supported, always returns false
        bool closeCover() override;  // Not supported, always returns false
        bool getStatus(int *coverStatus, int *lightStatus, int *motorStatus) override;

        // Motion/calibration commands (not supported by Lite)
        bool move(uint16_t value, int direction) override
        {
            (void)value;
            (void)direction;
            return false;
        }
        bool setClosePosition() override
        {
            return false;
        }
        bool setOpenPosition() override
        {
            return false;
        }

        // Advanced commands (fully supported by Lite)
        bool setBeep(bool enable) override;
        bool setBrightnessMode(int mode) override;

        // Communication setup
        char getCommandTerminator() const override
        {
            return '#';
        }
        void setupCommunication(int portFD) override
        {
            this->portFD = portFD;
        }
};

/**
 * @brief Simulation adapter for testing and development
 *
 * This adapter simulates device behavior without requiring actual hardware.
 * It can simulate either Rev1 or Rev2 features based on configuration.
 */
class GeminiFlatpanelSimulationAdapter : public GeminiFlatpanelAdapter
{
    public:
        GeminiFlatpanelSimulationAdapter(bool simulateRev2Features = true);

        // Device detection and identification
        bool ping() override;
        int getRevision() const override;
        bool getFirmwareVersion(int *version) override;

        // Capability checks
        bool supportsBeep() const override;
        bool supportsDustCap() const override;
        bool supportsBrightnessMode() const override;

        // Basic device commands
        bool getConfigStatus(int *status) override;
        bool getBrightness(int *brightness) override;
        bool setBrightness(int value) override;
        bool lightOn() override;
        bool lightOff() override;
        bool openCover() override;
        bool closeCover() override;
        bool getStatus(int *coverStatus, int *lightStatus, int *motorStatus) override;

        // Motion/calibration commands
        bool move(uint16_t value, int direction) override;
        bool setClosePosition() override;
        bool setOpenPosition() override;

        // Advanced commands
        bool setBeep(bool enable) override;
        bool setBrightnessMode(int mode) override;

        // Communication setup
        char getCommandTerminator() const override;
        void setupCommunication(int portFD) override;

    private:
        // Simulation state
        enum SimulationIndex
        {
            SIM_MOTOR = 0,
            SIM_LIGHT,
            SIM_COVER,
            SIM_BRIGHTNESS,
            SIM_CONFIG_STATUS,
            SIM_BEEP_ENABLED,
            SIM_BRIGHTNESS_MODE,
            SIM_N
        };

        int simulationValues[SIM_N];
        bool simulateRev2Features;
        int simulatedRevision;
        int simulatedFirmwareVersion;
};
