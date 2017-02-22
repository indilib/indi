#ifndef MGENAUTOGUIDER_H
#define MGENAUTOGUIDER_H

/*
   INDI Developers Manual
   Tutorial #1

   "Hello INDI"

   We construct a most basic (and useless) device driver to illustate INDI.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file mgenautoguider.h
    \brief Construct a basic INDI device for the Lacerta MGen Autoguider.
    \author TallFurryMan

    \example mgenautoguider.h
    A very minimal autoguider! It connects/disconnects to the autoguider, and manages operational modes.
*/

#include "defaultdevice.h"

class MGenAutoguider : public INDI::DefaultDevice
{
public:
    MGenAutoguider();
    virtual ~MGenAutoguider();

public:
    typedef unsigned char FirmwareVersion[2];

public:
    enum OpMode
    {
        OPM_UNKNOWN,        /**< Unknown mode, no exchange done yet or connection error */
        OPM_COMPATIBLE,     /**< Compatible mode, just after boot */
        OPM_BOOT,           /**< Boot mode */
        OPM_APPLICATION,    /**< Normal applicative mode */
    };

    enum CommandByte
    {
        MGCP_QUERY_DEVICE,          /**< [COMPATIBLE] Query device state */
        MGCP_ENTER_NORMAL_MODE,     /**< [COMPATIBLE] Enter mode APPLICATIVE */
        MGCMD_NOP0,                 /**< [BOOT/APP] No operation - reply is MGCMD_NOP1 in BOOT, MGCMD_NOP0 in APP */
        MGCMD_NOP1,                 /**< [BOOT/APP] No operation - reply is MGCMD_NOP0 in BOOT, MGCMD_NOP1 in APP */
        MGCMDB_GET_VERSION,         /**< [BOOT] Get boot software's version number */
        MGCMDB_GET_FW_VERSION,      /**< [BOOT] Get uploaded firmware's version if any */
        MGCMDB_GET_CAMERA_VERSIONS, /**< [BOOT] Get boot and uploaded camera's version if there is any */
        MGCMDB_RUN_FIRMWARE,        /**< [BOOT] Try to start the uploaded firmware */
        MGCMDB_POWER_OFF,           /**< [BOOT] Immediately makes the MGen go to power-down state */
        MGCMD_ENTER_BOOT_MODE,      /**< [APP] After applying this the device will immediately restart in BOOT mode */
        MGCMD_GET_FW_VERSION,       /**< [APP] Get the running firmware's version number */
        MGCMD_READ_ADCS,            /**< [APP] Get the latest 10-bit ADC conversion values */
        MGCMD_GET_LAST_FRAME,       /**< [APP] The last guiding frame's data from the camera */
        MGCMD_GET_LAST_FRAME_FLAGS, /**< [APP] Flags for the last guiding frame's data from the camera */
        MGCMD_IO_FUNCTIONS,         /**< [APP] A command that groups several input/output functions */
        MGIO_INSERT_BUTTON,         /**< [APP] IO - Button code to insert into the button input buffer */
        MGIO_GET_LED_STATES,        /**< [APP] IO - Flags telling what to query about the LED indicators */
        MGIO_READ_DISPLAY,          /**< [APP] IO - Read the display buffer content */
        MGCMD_RD_FUNCTIONS,         /**< [APP] A command that groups functions for Random Displacement feature */
        MGCMD_EXPO_FUNCTIONS,       /**< [APP] A command that groups functions related to exposure control */
        MGEXP_SET_EXTERNAL,         /**< [APP] EXP - Tell MGen the external exposure state */
        MGEXP_SET_EXTERNAL_OFF,     /**< [APP] EXP - Exposure is off */
        MGEXP_SET_EXTERNAL_ON,      /**< [APP] EXP - Exposure is on */
    };

    enum CommandStatus
    {
        CS_UNKNOWN,         /**< Unknown command */
        CS_RUNNING,         /**< Command running */
        CS_FAILURE,         /**< Command failed */
        CS_SUCCESS,         /**< Command successful */
    };


//    typedef struct commandMessage
//    {
//        enum OpMode mode;
//        enum CommandByte command;
//        std::queue<unsigned char> buffer;
//    };

protected:
    //ITextVectorProperty *DevPathSP;
    char dev_path[MAXINDINAME];
    int fd;

    struct connectionStatus
    {
        bool is_active;
        enum OpMode mode;
        enum CommandByte last_command;
        enum CommandStatus last_status;
        struct version
        {
            unsigned short uploaded_firmware;
            unsigned short camera_firmware;
        } version;
    } connectionStatus;

protected:
    bool initProperties();

protected:
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();

protected:
    unsigned short getUploadedFirmwareVersion();

protected:
    enum MGenAutoguider::OpMode getCurrentMode() const { return connectionStatus.mode; };
    int switchToMode( enum MGenAutoguider::OpMode );

protected:

protected:
    static void* connectionThreadWrapper( void* );
    void connectionThread();

    friend class MGC;

protected:
    int queryDevice( enum CommandByte commandByte, char * buffer, int buffer_len, int * io_len );
    bool verifyOpCode(enum CommandByte commandByte, unsigned char const *buffer, int bytes_read);
    int const getOpCodeAnswerLength( enum CommandByte command ) const;
    char const * const getOpCodeString(enum CommandByte) const;
    char const * const getOpModeString(enum OpMode) const;
    int heartbeat(struct ftdi_context * const ftdi);
    int setOpModeBaudRate(struct ftdi_context * const ftdi, enum MGenAutoguider::OpMode const mode);

public:
    unsigned char getOpCode( enum MGenAutoguider::CommandByte );
};

#endif // MGENAUTOGUIDER_H
