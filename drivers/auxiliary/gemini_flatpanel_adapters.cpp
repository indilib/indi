#include "gemini_flatpanel_adapters.h"
#include "indicom.h"
#include "indibase.h"
#include <termios.h>
#include <cstring>
#include <cstdio>

//////////////////////////////////////////////////////////////////////////////
// GeminiFlatpanelRev1Adapter Implementation
//////////////////////////////////////////////////////////////////////////////

// Device detection and identification
bool GeminiFlatpanelRev1Adapter::ping()
{
    // This command is only supported by revision 1 devices
    // It is used to check if the device is a revision 1 device
    char response[MAXRBUF] = {0};

    if (!sendCommand(">P000#", response, SERIAL_TIMEOUT_SEC, false))
    {
        return false;
    }

    if (strcmp(response, "*P99OOO") != 0)
    {
        return false;
    }

    return true;
}

bool GeminiFlatpanelRev1Adapter::getFirmwareVersion(int *version)
{
    // Rev1 doesn't support firmware version reporting command
    // Always return 0 to indicate no firmware version available
    *version = 0;
    return true;
}

// Basic device commands
bool GeminiFlatpanelRev1Adapter::getConfigStatus(int *status)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('A', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'A')
    {
        return false;
    }

    *status = response[2] - '0';
    return true;
}

bool GeminiFlatpanelRev1Adapter::getBrightness(int *brightness)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('J', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'J')
    {
        return false;
    }

    // Rev1 brightness response index is at position 4
    return extractIntValue(response, 4, brightness);
}

bool GeminiFlatpanelRev1Adapter::setBrightness(int value)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (value > GEMINI_MAX_BRIGHTNESS)
    {
        value = GEMINI_MAX_BRIGHTNESS;
    }

    if (value < GEMINI_MIN_BRIGHTNESS)
    {
        value = GEMINI_MIN_BRIGHTNESS;
    }

    if (!formatCommand('B', command, value))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'B')
    {
        return false;
    }

    int response_value;
    if (!extractIntValue(response, 4, &response_value))
    {
        return false;
    }

    return (response_value == value);
}

bool GeminiFlatpanelRev1Adapter::lightOn()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('L', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    return (strlen(response) >= 3 && response[0] == '*' && response[1] == 'L');
}

bool GeminiFlatpanelRev1Adapter::lightOff()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('D', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    return (strlen(response) >= 3 && response[0] == '*' && response[1] == 'D');
}

bool GeminiFlatpanelRev1Adapter::openCover()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('O', command))
    {
        return false;
    }

    if (!sendCommand(command, response, SERIAL_TIMEOUT_SEC_LONG))
    {
        return false;
    }

    return (strcmp(response, "*O99OOO") == 0);
}

bool GeminiFlatpanelRev1Adapter::closeCover()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('C', command))
    {
        return false;
    }

    if (!sendCommand(command, response, SERIAL_TIMEOUT_SEC_LONG))
    {
        return false;
    }

    return (strcmp(response, "*C99OOO") == 0);
}

bool GeminiFlatpanelRev1Adapter::getStatus(int *coverStatus, int *lightStatus, int *motorStatus)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('S', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 7)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'S')
    {
        return false;
    }

    // Check if device ID is valid (19 or 99)
    char id_str[3] = {response[2], response[3], '\0'};
    int id = atoi(id_str);
    if (id != 19 && id != 99)
    {
        return false;
    }

    *motorStatus = response[4] - '0';
    *lightStatus = response[5] - '0';
    *coverStatus = response[6] - '0';

    return true;
}

// Motion/calibration commands
bool GeminiFlatpanelRev1Adapter::move(uint16_t value, int direction)
{
    char command[MAXRBUF] = {0};

    if (direction == -1)
    {
        snprintf(command, sizeof(command), ">M-%02d#", value);
    }
    else
    {
        snprintf(command, sizeof(command), ">M%03d#", value);
    }

    char response[MAXRBUF] = {0};

    return sendCommand(command, response, 30);
}

bool GeminiFlatpanelRev1Adapter::setClosePosition()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('F', command))
    {
        return false;
    }

    return sendCommand(command, response);
}

bool GeminiFlatpanelRev1Adapter::setOpenPosition()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('E', command))
    {
        return false;
    }

    return sendCommand(command, response);
}

// Helper methods for Rev1
bool GeminiFlatpanelRev1Adapter::formatCommand(char commandLetter, char *commandString, int value)
{
    if (commandString == nullptr)
    {
        return false;
    }

    // Rev1 format: >X000# (no value) or >XNNN# (with value) where X is command letter and NNN is 3-digit value
    if (value == NO_VALUE)
    {
        // No value provided, pad with 000
        snprintf(commandString, MAXRBUF, ">%c000#", commandLetter);
    }
    else
    {
        // Value provided, pad to 3 digits
        snprintf(commandString, MAXRBUF, ">%c%03d#", commandLetter, value);
    }

    return true;
}

bool GeminiFlatpanelRev1Adapter::extractIntValue(const char *response, int startPos, int *value)
{
    if (response == nullptr || value == nullptr)
    {
        return false;
    }

    // Revision 1 responses have a 3-digit numeric part 0 padded
    int length = 3;

    if (length <= 0)
    {
        return false;
    }

    // Extract the numeric part
    char value_str[MAXRBUF] = {0};
    strncpy(value_str, response + startPos, length);
    *value = atoi(value_str);

    return true;
}

bool GeminiFlatpanelRev1Adapter::sendCommand(const char *command, char *response, int timeout, bool log)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(portFD, TCIOFLUSH);
    if ((rc = tty_write_string(portFD, command, &nbytes_written)) != TTY_OK)
    {
        if (log)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            // Note: Cannot use LOGF_ERROR here as we don't have access to logging
            // The main driver should handle logging of errors
        }
        return false;
    }

    if (response == nullptr)
        return true;

    if ((rc = tty_nread_section(portFD, response, MAXRBUF, getCommandTerminator(), timeout, &nbytes_read)) != TTY_OK)
    {
        if (log)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            // Note: Cannot use LOGF_ERROR here as we don't have access to logging
        }
        return false;
    }

    // Remove trailing newline character if present
    if (nbytes_read > 0 && response[nbytes_read - 1] == '\n')
    {
        response[nbytes_read - 1] = '\0';
    }
    tcflush(portFD, TCIOFLUSH);

    if (response[0] != '*' || response[1] != command[1])
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// GeminiFlatpanelRev2Adapter Implementation
//////////////////////////////////////////////////////////////////////////////

// Device detection and identification
bool GeminiFlatpanelRev2Adapter::ping()
{
    // This command is only supported by revision 2 devices
    // It is used to check if the device is a revision 2 device
    char response[MAXRBUF] = {0};

    if (!sendCommand(">H#", response, SERIAL_TIMEOUT_SEC, false))
    {
        return false;
    }

    if (strcmp(response, "*HGeminiFlatPanel#") != 0)
    {
        return false;
    }

    return true;
}

bool GeminiFlatpanelRev2Adapter::getFirmwareVersion(int *version)
{
    // This command is only supported by revision 2 devices
    // It is used to get the firmware version
    char response[MAXRBUF] = {0};

    // Initialize version to 0 in case of errors
    *version = 0;

    if (!sendCommand(">V#", response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'V')
    {
        return false;
    }

    int firmwareVersion;
    if (!extractIntValue(response, 2, &firmwareVersion))
    {
        return false;
    }

    if (firmwareVersion < 402)
    {
        // Firmware version too old. Should be 402 or later.
        return false;
    }

    *version = firmwareVersion;
    return true;
}

// Basic device commands
bool GeminiFlatpanelRev2Adapter::getConfigStatus(int *status)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('A', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'A')
    {
        return false;
    }

    *status = response[2] - '0';
    return true;
}

bool GeminiFlatpanelRev2Adapter::getBrightness(int *brightness)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('J', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'J')
    {
        return false;
    }

    // Rev2 brightness response index is at position 2
    return extractIntValue(response, 2, brightness);
}

bool GeminiFlatpanelRev2Adapter::setBrightness(int value)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (value > GEMINI_MAX_BRIGHTNESS)
    {
        value = GEMINI_MAX_BRIGHTNESS;
    }

    if (value < GEMINI_MIN_BRIGHTNESS)
    {
        value = GEMINI_MIN_BRIGHTNESS;
    }

    if (!formatCommand('B', command, value))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'B')
    {
        return false;
    }

    int response_value;
    if (!extractIntValue(response, 2, &response_value))
    {
        return false;
    }

    return (response_value == value);
}

bool GeminiFlatpanelRev2Adapter::lightOn()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('L', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    return (strlen(response) >= 3 && response[0] == '*' && response[1] == 'L');
}

bool GeminiFlatpanelRev2Adapter::lightOff()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('D', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    return (strlen(response) >= 3 && response[0] == '*' && response[1] == 'D');
}

bool GeminiFlatpanelRev2Adapter::openCover()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('O', command))
    {
        return false;
    }

    if (!sendCommand(command, response, SERIAL_TIMEOUT_SEC_LONG))
    {
        return false;
    }

    return (strcmp(response, "*OOpened#") == 0);
}

bool GeminiFlatpanelRev2Adapter::closeCover()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('C', command))
    {
        return false;
    }

    if (!sendCommand(command, response, SERIAL_TIMEOUT_SEC_LONG))
    {
        return false;
    }

    return (strcmp(response, "*CClosed#") == 0);
}

bool GeminiFlatpanelRev2Adapter::getStatus(int *coverStatus, int *lightStatus, int *motorStatus)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('S', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 7)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'S')
    {
        return false;
    }

    // Check if device ID is valid (19 or 99)
    char id_str[3] = {response[2], response[3], '\0'};
    int id = atoi(id_str);
    if (id != 19 && id != 99)
    {
        return false;
    }

    *motorStatus = response[4] - '0';
    *lightStatus = response[5] - '0';
    *coverStatus = response[6] - '0';

    return true;
}

// Motion/calibration commands
bool GeminiFlatpanelRev2Adapter::move(uint16_t value, int direction)
{
    char command[MAXRBUF] = {0};

    if (direction == -1)
    {
        snprintf(command, sizeof(command), ">M-%02d#", value);
    }
    else
    {
        snprintf(command, sizeof(command), ">M%03d#", value);
    }

    char response[MAXRBUF] = {0};

    return sendCommand(command, response, 30);
}

bool GeminiFlatpanelRev2Adapter::setClosePosition()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('F', command))
    {
        return false;
    }

    return sendCommand(command, response);
}

bool GeminiFlatpanelRev2Adapter::setOpenPosition()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('E', command))
    {
        return false;
    }

    return sendCommand(command, response);
}

// Advanced commands
bool GeminiFlatpanelRev2Adapter::setBeep(bool enable)
{
    char command[MAXRBUF] = {0};

    if (!formatCommand('T', command, enable ? GEMINI_BEEP_ON : GEMINI_BEEP_OFF))
    {
        return false;
    }

    return sendCommand(command, nullptr);
}

bool GeminiFlatpanelRev2Adapter::setBrightnessMode(int mode)
{
    if (mode != GEMINI_BRIGHTNESS_MODE_LOW && mode != GEMINI_BRIGHTNESS_MODE_HIGH)
    {
        return false;
    }

    char command[MAXRBUF] = {0};

    if (!formatCommand('Y', command, mode))
    {
        return false;
    }

    return sendCommand(command, nullptr);
}

// Helper methods for Rev2
bool GeminiFlatpanelRev2Adapter::formatCommand(char commandLetter, char *commandString, int value)
{
    if (commandString == nullptr)
    {
        return false;
    }

    // Rev2 format: >X# (no value) or >Xnnn# (with value) where X is command letter and nnn is value
    if (value == NO_VALUE)
    {
        // No value provided, no padding
        snprintf(commandString, MAXRBUF, ">%c#", commandLetter);
    }
    else
    {
        // Value provided, no padding
        snprintf(commandString, MAXRBUF, ">%c%d#", commandLetter, value);
    }

    return true;
}

bool GeminiFlatpanelRev2Adapter::extractIntValue(const char *response, int startPos, int *value)
{
    if (response == nullptr || value == nullptr)
    {
        return false;
    }

    const char *terminator = strchr(response, '#');
    if (terminator == nullptr)
    {
        return false;
    }

    // Calculate the length of the numeric part
    int length = terminator - (response + startPos);

    if (length <= 0)
    {
        return false;
    }

    // Extract the numeric part
    char value_str[MAXRBUF] = {0};
    strncpy(value_str, response + startPos, length);
    *value = atoi(value_str);

    return true;
}

bool GeminiFlatpanelRev2Adapter::sendCommand(const char *command, char *response, int timeout, bool log)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(portFD, TCIOFLUSH);
    if ((rc = tty_write_string(portFD, command, &nbytes_written)) != TTY_OK)
    {
        if (log)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            // Note: Cannot use LOGF_ERROR here as we don't have access to logging
            // The main driver should handle logging of errors
        }
        return false;
    }

    if (response == nullptr)
        return true;

    if ((rc = tty_nread_section(portFD, response, MAXRBUF, getCommandTerminator(), timeout, &nbytes_read)) != TTY_OK)
    {
        if (log)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            // Note: Cannot use LOGF_ERROR here as we don't have access to logging
        }
        return false;
    }

    // Remove trailing newline character if present
    if (nbytes_read > 0 && response[nbytes_read - 1] == '\n')
    {
        response[nbytes_read - 1] = '\0';
    }
    tcflush(portFD, TCIOFLUSH);

    if (response[0] != '*' || response[1] != command[1])
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// GeminiFlatpanelLiteAdapter Implementation
//////////////////////////////////////////////////////////////////////////////

// Device detection and identification
bool GeminiFlatpanelLiteAdapter::ping()
{
    // This command is only supported by Lite devices
    // It is used to check if the device is a Lite device
    char response[MAXRBUF] = {0};

    if (!sendCommand(">H#", response, SERIAL_TIMEOUT_SEC, false))
    {
        return false;
    }

    if (strcmp(response, "*HGeminiFlatPanelLite#") != 0)
    {
        return false;
    }

    return true;
}

bool GeminiFlatpanelLiteAdapter::getFirmwareVersion(int *version)
{
    // This command is supported by Lite devices
    // It is used to get the firmware version
    char response[MAXRBUF] = {0};

    // Initialize version to 0 in case of errors
    *version = 0;

    if (!sendCommand(">V#", response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'V')
    {
        return false;
    }

    int firmwareVersion;
    if (!extractIntValue(response, 2, &firmwareVersion))
    {
        return false;
    }

    if (firmwareVersion < 205)
    {
        // Firmware version too old. Should be 205 or later for Lite.
        return false;
    }

    *version = firmwareVersion;
    return true;
}

// Basic device commands
bool GeminiFlatpanelLiteAdapter::getConfigStatus(int *status)
{
    // Lite devices don't have a separate config status command
    // We'll assume they're always ready since they don't have motorized parts
    *status = GEMINI_CONFIG_READY;
    return true;
}

bool GeminiFlatpanelLiteAdapter::getBrightness(int *brightness)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('J', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'J')
    {
        return false;
    }

    // Lite brightness response index is at position 2
    return extractIntValue(response, 2, brightness);
}

bool GeminiFlatpanelLiteAdapter::setBrightness(int value)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (value > GEMINI_MAX_BRIGHTNESS)
    {
        value = GEMINI_MAX_BRIGHTNESS;
    }

    if (value < GEMINI_MIN_BRIGHTNESS)
    {
        value = GEMINI_MIN_BRIGHTNESS;
    }

    if (!formatCommand('B', command, value))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 3)
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'B')
    {
        return false;
    }

    int response_value;
    if (!extractIntValue(response, 2, &response_value))
    {
        return false;
    }

    return (response_value == value);
}

bool GeminiFlatpanelLiteAdapter::lightOn()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('L', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    return (strlen(response) >= 3 && response[0] == '*' && response[1] == 'L');
}

bool GeminiFlatpanelLiteAdapter::lightOff()
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('D', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    return (strlen(response) >= 3 && response[0] == '*' && response[1] == 'D');
}

bool GeminiFlatpanelLiteAdapter::openCover()
{
    // Lite devices don't have motorized covers
    return false;
}

bool GeminiFlatpanelLiteAdapter::closeCover()
{
    // Lite devices don't have motorized covers
    return false;
}

bool GeminiFlatpanelLiteAdapter::getStatus(int *coverStatus, int *lightStatus, int *motorStatus)
{
    char command[MAXRBUF] = {0};
    char response[MAXRBUF] = {0};

    if (!formatCommand('S', command))
    {
        return false;
    }

    if (!sendCommand(command, response))
    {
        return false;
    }

    if (strlen(response) < 5)  // "*SLMB#" minimum
    {
        return false;
    }

    if (response[0] != '*' || response[1] != 'S')
    {
        return false;
    }

    // Lite status format: "*SLMB#" where:
    // L = light status (0 off, 1 on)
    // M = brightness Mode (0 Low, 1 High)
    // B = Beep on/off (0 off, 1 on)

    *lightStatus = response[2] - '0';
    // For Lite devices, cover is always "open" since there's no physical cover
    *coverStatus = GEMINI_COVER_STATUS_OPEN;
    // Motor is always stopped since there's no motor
    *motorStatus = GEMINI_MOTOR_STATUS_STOPPED;

    return true;
}

// Advanced commands
bool GeminiFlatpanelLiteAdapter::setBeep(bool enable)
{
    char command[MAXRBUF] = {0};

    if (!formatCommand('T', command, enable ? GEMINI_BEEP_ON : GEMINI_BEEP_OFF))
    {
        return false;
    }

    return sendCommand(command, nullptr);
}

bool GeminiFlatpanelLiteAdapter::setBrightnessMode(int mode)
{
    if (mode != GEMINI_BRIGHTNESS_MODE_LOW && mode != GEMINI_BRIGHTNESS_MODE_HIGH)
    {
        return false;
    }

    char command[MAXRBUF] = {0};

    if (!formatCommand('Y', command, mode))
    {
        return false;
    }

    return sendCommand(command, nullptr);
}

// Helper methods for Lite
bool GeminiFlatpanelLiteAdapter::formatCommand(char commandLetter, char *commandString, int value)
{
    if (commandString == nullptr)
    {
        return false;
    }

    // Lite format: >X# (no value) or >Xnnn# (with value) where X is command letter and nnn is value
    if (value == NO_VALUE)
    {
        // No value provided
        snprintf(commandString, MAXRBUF, ">%c#", commandLetter);
    }
    else
    {
        // Value provided
        snprintf(commandString, MAXRBUF, ">%c%d#", commandLetter, value);
    }

    return true;
}

bool GeminiFlatpanelLiteAdapter::extractIntValue(const char *response, int startPos, int *value)
{
    if (response == nullptr || value == nullptr)
    {
        return false;
    }

    const char *terminator = strchr(response, '#');
    if (terminator == nullptr)
    {
        return false;
    }

    // Calculate the length of the numeric part
    int length = terminator - (response + startPos);

    if (length <= 0)
    {
        return false;
    }

    // Extract the numeric part
    char value_str[MAXRBUF] = {0};
    strncpy(value_str, response + startPos, length);
    *value = atoi(value_str);

    return true;
}

bool GeminiFlatpanelLiteAdapter::sendCommand(const char *command, char *response, int timeout, bool log)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(portFD, TCIOFLUSH);
    if ((rc = tty_write_string(portFD, command, &nbytes_written)) != TTY_OK)
    {
        if (log)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            // Note: Cannot use LOGF_ERROR here as we don't have access to logging
            // The main driver should handle logging of errors
        }
        return false;
    }

    if (response == nullptr)
        return true;

    if ((rc = tty_nread_section(portFD, response, MAXRBUF, getCommandTerminator(), timeout, &nbytes_read)) != TTY_OK)
    {
        if (log)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            // Note: Cannot use LOGF_ERROR here as we don't have access to logging
        }
        return false;
    }

    // Remove trailing newline character if present
    if (nbytes_read > 0 && response[nbytes_read - 1] == '\n')
    {
        response[nbytes_read - 1] = '\0';
    }
    tcflush(portFD, TCIOFLUSH);

    if (response[0] != '*' || response[1] != command[1])
    {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// GeminiFlatpanelSimulationAdapter Implementation
//////////////////////////////////////////////////////////////////////////////

GeminiFlatpanelSimulationAdapter::GeminiFlatpanelSimulationAdapter(bool simulateRev2Features)
    : simulateRev2Features(simulateRev2Features)
{
    // Initialize simulation state
    simulationValues[SIM_MOTOR] = GEMINI_MOTOR_STATUS_STOPPED;
    simulationValues[SIM_LIGHT] = GEMINI_LIGHT_STATUS_OFF;
    simulationValues[SIM_COVER] = GEMINI_COVER_STATUS_CLOSED;
    simulationValues[SIM_BRIGHTNESS] = 128;
    simulationValues[SIM_CONFIG_STATUS] = GEMINI_CONFIG_READY;
    simulationValues[SIM_BEEP_ENABLED] = 0; // Beep off by default
    simulationValues[SIM_BRIGHTNESS_MODE] = GEMINI_BRIGHTNESS_MODE_LOW;

    // Set simulated revision and firmware version
    if (simulateRev2Features)
    {
        simulatedRevision = GEMINI_REVISION_2;
        simulatedFirmwareVersion = 450; // A version >= 402
    }
    else
    {
        simulatedRevision = GEMINI_REVISION_1;
        simulatedFirmwareVersion = 0; // Rev1 doesn't report version
    }
}

bool GeminiFlatpanelSimulationAdapter::ping()
{
    // Simulation always responds to ping
    return true;
}

int GeminiFlatpanelSimulationAdapter::getRevision() const
{
    return simulatedRevision;
}

bool GeminiFlatpanelSimulationAdapter::getFirmwareVersion(int *version)
{
    if (!simulateRev2Features)
    {
        // Rev1 doesn't report firmware version
        return false;
    }

    *version = simulatedFirmwareVersion;
    return true;
}

bool GeminiFlatpanelSimulationAdapter::supportsBeep() const
{
    return simulateRev2Features;
}

bool GeminiFlatpanelSimulationAdapter::supportsDustCap() const
{
    return true; // Both revisions support dust cap
}

bool GeminiFlatpanelSimulationAdapter::supportsBrightnessMode() const
{
    return simulateRev2Features;
}

bool GeminiFlatpanelSimulationAdapter::getConfigStatus(int *status)
{
    *status = simulationValues[SIM_CONFIG_STATUS];
    return true;
}

bool GeminiFlatpanelSimulationAdapter::getBrightness(int *brightness)
{
    *brightness = simulationValues[SIM_BRIGHTNESS];
    return true;
}

bool GeminiFlatpanelSimulationAdapter::setBrightness(int value)
{
    if (value < GEMINI_MIN_BRIGHTNESS || value > GEMINI_MAX_BRIGHTNESS)
    {
        return false;
    }

    simulationValues[SIM_BRIGHTNESS] = value;
    return true;
}

bool GeminiFlatpanelSimulationAdapter::lightOn()
{
    simulationValues[SIM_LIGHT] = GEMINI_LIGHT_STATUS_ON;
    return true;
}

bool GeminiFlatpanelSimulationAdapter::lightOff()
{
    simulationValues[SIM_LIGHT] = GEMINI_LIGHT_STATUS_OFF;
    return true;
}

bool GeminiFlatpanelSimulationAdapter::openCover()
{
    simulationValues[SIM_COVER] = GEMINI_COVER_STATUS_OPEN;
    return true;
}

bool GeminiFlatpanelSimulationAdapter::closeCover()
{
    simulationValues[SIM_COVER] = GEMINI_COVER_STATUS_CLOSED;
    return true;
}

bool GeminiFlatpanelSimulationAdapter::getStatus(int *coverStatus, int *lightStatus, int *motorStatus)
{
    *coverStatus = simulationValues[SIM_COVER];
    *lightStatus = simulationValues[SIM_LIGHT];
    *motorStatus = simulationValues[SIM_MOTOR];
    return true;
}

bool GeminiFlatpanelSimulationAdapter::move(uint16_t value, int direction)
{
    // Simulate brief motor activity
    simulationValues[SIM_MOTOR] = GEMINI_MOTOR_STATUS_RUNNING;
    // In real implementation, motor would stop after movement
    // For simulation, we immediately set it back to stopped
    simulationValues[SIM_MOTOR] = GEMINI_MOTOR_STATUS_STOPPED;

    // Unused parameters
    (void)value;
    (void)direction;

    return true;
}

bool GeminiFlatpanelSimulationAdapter::setClosePosition()
{
    return true;
}

bool GeminiFlatpanelSimulationAdapter::setOpenPosition()
{
    return true;
}

bool GeminiFlatpanelSimulationAdapter::setBeep(bool enable)
{
    if (!supportsBeep())
    {
        return false;
    }

    simulationValues[SIM_BEEP_ENABLED] = enable ? 1 : 0;
    return true;
}

bool GeminiFlatpanelSimulationAdapter::setBrightnessMode(int mode)
{
    if (!supportsBrightnessMode())
    {
        return false;
    }

    if (mode != GEMINI_BRIGHTNESS_MODE_LOW && mode != GEMINI_BRIGHTNESS_MODE_HIGH)
    {
        return false;
    }

    simulationValues[SIM_BRIGHTNESS_MODE] = mode;
    return true;
}

char GeminiFlatpanelSimulationAdapter::getCommandTerminator() const
{
    // Use Rev2 terminator for consistency (most features)
    return simulateRev2Features ? '#' : '\n';
}

void GeminiFlatpanelSimulationAdapter::setupCommunication(int /*portFD*/)
{
    // No actual communication setup needed for simulation
}
