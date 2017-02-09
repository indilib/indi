/*
   INDI Developers Manual
   Tutorial #1

   "Hello INDI"

   We construct a most basic (and useless) device driver to illustate INDI.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpledevice.cpp
    \brief Construct a basic INDI device with only one property to connect and disconnect.
    \author Jasem Mutlaq

    \example mgenautoguider.cpp
    A very minimal device! It also allows you to connect/disconnect and performs no other functions.
*/

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
//#include <termios.h>
//#include <sys/ioctl.h>
//#include <linux/serial.h>
#include <libftdi1/ftdi.h>
#include <libusb-1.0/libusb.h>

#include <memory>
#include <string>
#include <vector>

#include "indidevapi.h"
#include "indicom.h"
#include "indilogger.h"

#include "mgenautoguider.h"
#include "mgencommand.h"

#define ERROR_MSG_LENGTH 250

using namespace std;
std::unique_ptr<MGenAutoguider> mgenAutoguider(new MGenAutoguider());

/**************************************************************************************
** Return properties of device.
***************************************************************************************/
void ISGetProperties (const char *dev)
{
    IDLog("%s: get properties.", __FUNCTION__);
    mgenAutoguider->ISGetProperties(dev);
}

/**************************************************************************************
** Process new switch from client
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    IDLog("%s: new switch '%s'.", __FUNCTION__, name);
    mgenAutoguider->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Process new text from client
***************************************************************************************/
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    IDLog("%s: get new text '%s'.", __FUNCTION__, name);
    mgenAutoguider->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Process new number from client
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    IDLog("%s: new number '%s'.", __FUNCTION__, name);
    mgenAutoguider->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
** Process new blob from client
***************************************************************************************/
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    IDLog("%s: get new blob '%s'.", __FUNCTION__, name);
    mgenAutoguider->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
** Process snooped property from another driver
***************************************************************************************/
void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}

MGenAutoguider::MGenAutoguider()
: fd(-1)
{
    strncpy(dev_path, "/dev/tty.mgen", MAXINDINAME);
}

MGenAutoguider::~MGenAutoguider()
{
    Disconnect();
}

bool MGenAutoguider::initProperties()
{
    INDI::DefaultDevice::initProperties();

    addDebugControl();

    return true;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool MGenAutoguider::Connect()
{
    if(connectionStatus.is_active)
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: ignoring connection request received while already connected.", __FUNCTION__); usleep(100000); }
        return true;
    }

    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: initiating connection.", __FUNCTION__); usleep(100000); }

    unsigned long int thread = 0;
    //struct threadData arg = { NULL, MGenAutoguider::MGCMD_NOP0 };
    pthread_mutex_t lock;

    connectionStatus.is_active = false;
    connectionStatus.mode = OPM_UNKNOWN;
    connectionStatus.version.camera_firmware = 0;
    connectionStatus.version.uploaded_firmware = 0;

    //if( !pthread_mutex_init(&lock,0) )
    {
        if( !pthread_create(&thread, NULL, &MGenAutoguider::connectionThreadWrapper, this) )
        {
            { DEBUGF(INDI::Logger::DBG_SESSION, "%s: connection thread %p started successfully.", __FUNCTION__, thread); usleep(100000); }
            sleep(2);
        }
    }

    return connectionStatus.is_active;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool MGenAutoguider::Disconnect()
{
    if( connectionStatus.is_active )
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: initiating disconnection.", __FUNCTION__); usleep(100000); }

        connectionStatus.is_active = false;
        sleep(5);
    }

    return !connectionStatus.is_active;
}

/**************************************************************************************
** INDI is asking us for our default device name
***************************************************************************************/
const char * MGenAutoguider::getDefaultName()
{
    return "MGen Autoguider";
}

/**************************************************************************************
** Mode management
***************************************************************************************/

int MGenAutoguider::setOpModeBaudRate(struct ftdi_context * const ftdi, enum MGenAutoguider::OpMode const mode)
{
    int baudrate = 0;

    switch( mode )
    {
        case OPM_COMPATIBLE:
            baudrate = 9600;
            break;

        case OPM_APPLICATION:
            baudrate = 250000;
            break;

        case OPM_UNKNOWN:
        case OPM_BOOT:
        default:
            baudrate = 9600;
            break;
    }

    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: switching device to baudrate %d", __FUNCTION__, baudrate); usleep(100000); }

    int res = 0;

    if((res = ftdi_set_baudrate(ftdi, baudrate)) < 0)
    {
        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed updating device connection using %d bauds (%d: %s)", __FUNCTION__, baudrate, res, ftdi_get_error_string(ftdi)); usleep(100000); }
        return 1;
    }

    if((res = ftdi_set_line_property(ftdi, BITS_8, STOP_BIT_1, NONE)) < 0)
    {
        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed setting device line properties to 8-N-1 (%d: %s)", __FUNCTION__, res, ftdi_get_error_string(ftdi)); usleep(100000); }
        return 1;
    }

    /* Purge I/O buffers */
    if((res = ftdi_usb_purge_buffers(ftdi)) < 0 )
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed purging I/O buffers (%d: %s)", __FUNCTION__, res, ftdi_get_error_string(ftdi)); usleep(100000); }
        return 1;
    }

    /* Set latency to minimal 2ms */
    if((res = ftdi_set_latency_timer(ftdi, 2)) < 0 )
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed setting latency timer (%d: %s)", __FUNCTION__, res, ftdi_get_error_string(ftdi)); usleep(100000); }
        return 1;
    }

    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: successfully switched device to baudrate %d.", __FUNCTION__, baudrate); usleep(100000); }
    return 0;
}

/**************************************************************************************
 * Connection thread
 **************************************************************************************/
void* MGenAutoguider::connectionThreadWrapper( void* arg )
{
    MGenAutoguider * const _this = reinterpret_cast<MGenAutoguider*>(arg);
    INDI::Logger::getInstance().print(_this->getDeviceName(), INDI::Logger::DBG_SESSION, __FILE__, __LINE__, "%s: connection thread wrapper", __FUNCTION__);

    if(_this)
        _this->connectionThread();

    INDI::Logger::getInstance().print(_this->getDeviceName(), INDI::Logger::DBG_SESSION, __FILE__, __LINE__, "%s: connection thread terminated", __FUNCTION__);
    return _this;
}

void MGenAutoguider::connectionThread()
{
    int fd = 0;
    unsigned char buffer[256];
    size_t const buffer_len = sizeof(buffer)/sizeof(buffer[0]);
    int bytes_read = 0;

    struct ftdi_version_info version = ftdi_get_library_version();
    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: connection thread starting, using FTDI '%s' v%d.%d.%d snapshot '%s'", __FUNCTION__, version.version_str, version.major, version.minor, version.micro, version.snapshot_str); usleep(100000); }

    int  const vid = 0x0403, pid = 0x6001;

    int res = 0;
    struct ftdi_context *ftdi = ftdi_new();
    if( !ftdi )
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: ftdi context initialization failed", __FUNCTION__); usleep(100000); }
        connectionStatus.is_active = false;
    }
    else if((res = ftdi_set_interface(ftdi, INTERFACE_ANY)) < 0)
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed setting FTDI interface to ANY (%d: %s)", __FUNCTION__, res, ftdi_get_error_string(ftdi)); usleep(100000); }
        connectionStatus.is_active = false;
    }
    else if((res = ftdi_usb_open(ftdi, vid, pid)) < 0)
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device 0x%04X:0x%04X not found (%d: %s)", __FUNCTION__, vid, pid, res, ftdi_get_error_string(ftdi)); usleep(100000); }

        if((res = ftdi_set_interface(ftdi, INTERFACE_ANY)) < 0)
        {
            { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed setting FTDI interface to ANY (%d: %s)", __FUNCTION__, res, ftdi_get_error_string(ftdi)); usleep(100000); }
        }
        else
        {
            struct ftdi_device_list *devlist;
            if((res = ftdi_usb_find_all(ftdi, &devlist, 0, 0)) < 0)
            {
                { DEBUGF(INDI::Logger::DBG_SESSION, "%s: no FTDI device found (%d: %s)", __FUNCTION__, res, ftdi_get_error_string(ftdi)); usleep(100000); }
            }
            else
            {
                if(devlist) for( struct ftdi_device_list const * dev_index = devlist; dev_index; dev_index = dev_index->next )
                {
                    struct libusb_device_descriptor desc = {0};

                    if(libusb_get_device_descriptor(dev_index->dev, &desc) < 0)
                    {
                        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device %p returned by libftdi is unreadable", __FUNCTION__, dev_index->dev); usleep(100000); }
                        continue;
                    }

                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: detected device 0x%04X:0x%04X", __FUNCTION__, desc.idVendor, desc.idProduct); usleep(100000); }
                }
                else { DEBUGF(INDI::Logger::DBG_SESSION, "%s: no FTDI device enumerated", __FUNCTION__); usleep(100000); }

                ftdi_list_free(&devlist);
            }
        }

        connectionStatus.is_active = false;
    }
    else if(setOpModeBaudRate(ftdi, OPM_COMPATIBLE) < 0)
    {
        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed setting up device line", __FUNCTION__); usleep(100000); }
        connectionStatus.is_active = false;
    }
    else
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device 0x%04X:0x%04X connected successfully", __FUNCTION__, vid, pid); usleep(100000); }
        connectionStatus.is_active = true;
    }

    /* TODO: EXPERIMENTAL: turn the autoguider on if not connected yet - chapter 6.2.2 */

    while( connectionStatus.is_active )
    {
        switch( connectionStatus.mode )
        {
            /* Unknown mode, try to connect in COMPATIBLE mode first */
            case OPM_UNKNOWN:
                { DEBUGF(INDI::Logger::DBG_SESSION, "%s: running device identification", __FUNCTION__); usleep(100000); }

                /* Run an identification */
                buffer[0] = getOpCode(MGCP_QUERY_DEVICE);
                buffer[1] = 0x01; /* Length of parameters */
                buffer[2] = 0x01; /* Query device ID sub-comand */
                if(ftdi_write_data(ftdi, buffer, 3) < 0)
                {
                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed writing identification request to device", __FUNCTION__); usleep(100000); }
                    connectionStatus.is_active = false;
                    continue;
                }

                { DEBUGF(INDI::Logger::DBG_SESSION, "%s: reading identification results", __FUNCTION__); usleep(100000); }
                if(( bytes_read = ftdi_read_data(ftdi, buffer, 5) ) < 0)
                {
                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed reading identification from device", __FUNCTION__); usleep(100000); }
                    connectionStatus.is_active = false;
                    continue;
                }

                /* Verify opcode answer in case we got something */
                if(verifyOpCode(MGCP_QUERY_DEVICE, buffer, bytes_read))
                {
                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device acknowledged identification, analyzing '%02X%02X%02X'", __FUNCTION__, (unsigned char) buffer[2], (unsigned char) buffer[3], (unsigned char) buffer[4]); usleep(100000); }

                    unsigned char const app_mode_answer[] = { 0x01, 0x80, 0x02 };
                    size_t const app_mode_len = sizeof(app_mode_answer)/sizeof(app_mode_answer[0]);
                    unsigned char const boot_mode_answer[] = { 0x01, 0x80, 0x01 };
                    size_t const boot_mode_len = sizeof(boot_mode_answer)/sizeof(boot_mode_answer[0]);

                    /* Check integrity of answer */
                    if( memcmp(&buffer[2], &app_mode_answer, app_mode_len) && memcmp(&buffer[2], &boot_mode_answer, boot_mode_len) )
                    {
                        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device identification returned unknown mode (%d bytes read)", __FUNCTION__, bytes_read); usleep(100000); }
                        connectionStatus.is_active = false;
                        continue;
                    }

                    /* Compatible mode, switch to normal mode as if we were in boot mode */
                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: identified boot/compatible mode", __FUNCTION__); usleep(100000); }
                    connectionStatus.mode = OPM_COMPATIBLE;
                    continue;
                }

                { DEBUGF(INDI::Logger::DBG_SESSION, "%s: identification failed, try to communicate in applicative mode", __FUNCTION__); usleep(100000); }
                connectionStatus.mode = OPM_APPLICATION;

                if(setOpModeBaudRate(ftdi, connectionStatus.mode))
                {
                    /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed adjusting device line", __FUNCTION__); usleep(100000); }
                    connectionStatus.is_active = false;
                    continue;
                }

                if(heartbeat(ftdi))
                {
                    connectionStatus.is_active = false;
                    continue;
                }

                break;

            /*case OPM_BOOT:*/
            case OPM_COMPATIBLE:
                { DEBUGF(INDI::Logger::DBG_SESSION, "%s: switching from compatible to normal mode", __FUNCTION__); usleep(100000); }

                /* Switch to applicative mode */
                buffer[0] = getOpCode(MGCP_ENTER_NORMAL_MODE);
                if(ftdi_write_data(ftdi, buffer, 1) < 0)
                {
                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected while entering applicative mode", __FUNCTION__); usleep(100000); }
                    connectionStatus.is_active = false;
                    continue;
                }

                sleep(1);

                connectionStatus.mode = OPM_APPLICATION;

                if(setOpModeBaudRate(ftdi, connectionStatus.mode))
                {
                    /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed updating device connection", __FUNCTION__); usleep(100000); }
                    connectionStatus.is_active = false;
                    continue;
                }

                { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device is in now expected to be in applicative mode", __FUNCTION__); usleep(100000); }

                if(heartbeat(ftdi))
                {
                    connectionStatus.is_active = false;
                    continue;
                }

                break;

            case OPM_APPLICATION:
                /* If we didn't get the firmware version, ask */
                if( !connectionStatus.version.uploaded_firmware )
                {
                    buffer[0] = getOpCode(MGCMD_GET_FW_VERSION);
                    if(ftdi_write_data(ftdi, buffer, 1) < 0)
                    {
                        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected while writing GET_FW_VERSION", __FUNCTION__); usleep(100000); }
                        connectionStatus.is_active = false;
                        continue;
                    }

                    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: reading version result.", __FUNCTION__); usleep(100000); }
                    if((bytes_read = ftdi_read_data(ftdi, buffer, getOpCodeAnswerLength(MGCMD_GET_FW_VERSION))) < 0)
                    {
                        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected while reading version", __FUNCTION__); usleep(100000); }
                        connectionStatus.is_active = false;
                        continue;
                    }

                    if(verifyOpCode(MGCMD_GET_FW_VERSION, buffer, bytes_read))
                    {
                        connectionStatus.version.uploaded_firmware = (buffer[2]<<8) | buffer[1];

                        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: received version 0x%04X", __FUNCTION__, connectionStatus.version.uploaded_firmware); usleep(100000); }

                        break;
                    }
                    else { DEBUGF(INDI::Logger::DBG_SESSION, "%s: no version ack (%d bytes read, ack 0x%02X)", __FUNCTION__, bytes_read, bytes_read?buffer[0]:0x00); usleep(100000); }
                }

                /* Heartbeat */
                if(heartbeat(ftdi))
                {
                    connectionStatus.is_active = false;
                    continue;
                }

                /* OK, wait a few seconds and retry heartbeat */
                for( int i = 0; i < 10; i++ )
                    if( connectionStatus.is_active )
                        usleep(500000);

                break;
        }
    }


    if(ftdi)
    {
        ftdi_usb_close(ftdi);
        ftdi_free(ftdi);
    }

    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: disconnected successfully.", __FUNCTION__); usleep(100000); }
    //return 0;
}

/**************************************************************************************
 * Helpers
 **************************************************************************************/
#if 0

struct MessageBuffer
{
public:
    MGenAutoguider guider;
    char buffer[1024];

public:
    MessageBuffer( MGenAutoguider& guider, enum MGenAutoguider::CommandByte commandByte )
    : guider(guider)
    {
        memset(buffer,0,sizeof(buffer)/sizeof(buffer[0]));
        buffer[0] = guider.getOpCode(commandByte);
    }
    ~MessageBuffer();
};

int MGenAutoguider::queryDevice( enum CommandByte commandByte, std::vector<char>& payload, int * io_len )
{
    if( !io_len || !buffer )
        return 1;

    buffer[0] = getOpCode(commandByte);
    buffer[1] = *io_len;

    switch( tty_write(fd, buffer, 2 + *io_len, io_len) )
    {
        case TTY_OK:
            /* OK, read back result next */
            break;

        default: return 1;
    }

    int const expected_length = getOpCodeAnswerLength(commandByte);

    if( 0 < expected_length ) switch( tty_read(fd, buffer, expected_length, 1, io_len) )
    {
        case TTY_OK:
            /* Did we receive an ack? */
            if( ~getOpCode(commandByte) == buffer[0] )
                break;
            /* no-break */

        default: return 1;
    }

    return 0;
}
#endif

unsigned char MGenAutoguider::getOpCode(enum CommandByte commandByte)
{
    switch(connectionStatus.mode)
    {
        case OPM_UNKNOWN:
        case OPM_COMPATIBLE:
            /* From spec, no particular version to support */
            switch( commandByte )
            {
                case MGCP_QUERY_DEVICE:             return 0xAA;
                case MGCP_ENTER_NORMAL_MODE:        return 0x42;

                default: break;
            }
            break;

        case OPM_BOOT:
            /* From spec, no particular version to support */
            switch( commandByte )
            {
                case MGCMD_NOP0:                    return 0x00;
                case MGCMD_NOP1:                    return 0xFF;
                case MGCMDB_GET_VERSION:            return 0x14;
                case MGCMDB_GET_FW_VERSION:         return 0x2D;
                case MGCMDB_GET_CAMERA_VERSIONS:    return 0x2E;
                case MGCMDB_RUN_FIRMWARE:           return 0xE1;
                case MGCMDB_POWER_OFF:              return 0xE2;

                default: break;
            }
            break;

        case OPM_APPLICATION:
            switch( commandByte )
            {
                case MGCMD_NOP0:                    return 0x00;
                case MGCMD_NOP1:                    return 0xFF;
                case MGCMD_GET_FW_VERSION:          return 0x03;

                default: break;
            }
            break;

        default:
            { DEBUGF(INDI::Logger::DBG_SESSION, "%s: invalid mode '%s' for opcode '%s'", __FUNCTION__, getOpModeString(connectionStatus.mode), getOpCodeString(commandByte)); usleep(100000); }
            break;
    }

    /* Return NOP0 as fallback */
    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: mode '%s' doesn't support opcode '%s', returning NOP0", __FUNCTION__, getOpModeString(connectionStatus.mode), getOpCodeString(commandByte)); usleep(100000); }
    return 0x00;
}

bool MGenAutoguider::verifyOpCode(enum CommandByte commandByte, unsigned char const *buffer, int bytes_read)
{
    switch( this->connectionStatus.mode )
    {
        case OPM_UNKNOWN:
        case OPM_COMPATIBLE:
            switch( commandByte )
            {
                case MGCP_QUERY_DEVICE:
                    return buffer[0] == (unsigned char) ~getOpCode(commandByte) && 5 == bytes_read;

                case MGCP_ENTER_NORMAL_MODE:
                    return 0 == bytes_read;

                default: break;
            }
            break;

        case OPM_BOOT:
            /* Spec chapter 4 */
            switch( commandByte )
            {
                case MGCMD_NOP0:
                case MGCMD_NOP1:
                    return buffer[0] == (unsigned char) ~getOpCode(commandByte) && 1 == bytes_read;

                case MGCMDB_GET_VERSION:
                    return buffer[0] == (unsigned char) getOpCode(commandByte) && 3 == bytes_read;

                case MGCMDB_GET_FW_VERSION:
                    return buffer[0] == (unsigned char) getOpCode(commandByte) && ( 1 == bytes_read || 3 == bytes_read );

                case MGCMDB_GET_CAMERA_VERSIONS:
                    return buffer[0] == (unsigned char) getOpCode(commandByte) && ( 1 == bytes_read || 2 == bytes_read );

                case MGCMDB_RUN_FIRMWARE:
                case MGCMDB_POWER_OFF:
                    return 0 == bytes_read;

                default: break;
            }
            break;

        case OPM_APPLICATION:
            switch( commandByte )
            {
                case MGCMD_NOP0:
                case MGCMD_NOP1:
                    return buffer[0] == getOpCode(commandByte) && 1 == bytes_read;

                case MGCMD_GET_FW_VERSION:
                    return buffer[0] == getOpCode(commandByte) && ( 1 == bytes_read || 3 == bytes_read );

                default: break;
            }

            break;

        default:
            { DEBUGF(INDI::Logger::DBG_SESSION, "%s: invalid mode '%s' for opcode '%s'", __FUNCTION__, getOpModeString(connectionStatus.mode), getOpCodeString(commandByte)); usleep(100000); }
            return false;
    }

    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: mode '%s' doesn't support opcode '%s'", __FUNCTION__, getOpModeString(connectionStatus.mode), getOpCodeString(commandByte)); usleep(100000); }
    return false;
}

int const MGenAutoguider::getOpCodeAnswerLength( enum CommandByte command ) const
{
    /* TODO: version dependency */
    switch( command )
    {
        case MGCMD_NOP0: return 1;
        case MGCMD_NOP1: return 1;
        case MGCP_QUERY_DEVICE: return 1+4;
        case MGCP_ENTER_NORMAL_MODE: return 0;
        case MGCMDB_GET_VERSION: return 1+2;
        case MGCMDB_GET_FW_VERSION: return 1+1+2;
        case MGCMDB_GET_CAMERA_VERSIONS: return -1;
        case MGCMDB_RUN_FIRMWARE: return 0;
        case MGCMDB_POWER_OFF: return 0;
        case MGCMD_ENTER_BOOT_MODE: return 0;
        case MGCMD_GET_FW_VERSION: return 1+2;
        case MGCMD_READ_ADCS: return 1+5*2;
        case MGCMD_GET_LAST_FRAME: return 1;
        case MGCMD_GET_LAST_FRAME_FLAGS: return -1;
        case MGCMD_IO_FUNCTIONS: return -1;
        case MGIO_INSERT_BUTTON: return -1;
        case MGIO_GET_LED_STATES: return -1;
        case MGIO_READ_DISPLAY: return -1;
        case MGCMD_RD_FUNCTIONS: return -1;
        case MGCMD_EXPO_FUNCTIONS: return -1;
        case MGEXP_SET_EXTERNAL: return -1;
        case MGEXP_SET_EXTERNAL_OFF: return -1;
        case MGEXP_SET_EXTERNAL_ON: return -1;
        default: return -1;
    };
}

char const * const MGenAutoguider::getOpCodeString( enum CommandByte command ) const
{
    switch( command )
    {
        case MGCMD_NOP0: return "MGCMD_NOP0";
        case MGCMD_NOP1: return "MGCMD_NOP1";
        case MGCMDB_GET_VERSION: return "MGCMDB_GET_VERSION";
        case MGCMDB_GET_FW_VERSION: return "MGCMDB_GET_FW_VERSION";
        case MGCMDB_GET_CAMERA_VERSIONS: return "MGCMDB_GET_CAMERA_VERSIONS";
        case MGCMDB_RUN_FIRMWARE: return "MGCMDB_RUN_FIRMWARE";
        case MGCMDB_POWER_OFF: return "MGCMDB_POWER_OFF";
        case MGCMD_ENTER_BOOT_MODE: return "MGCMD_ENTER_BOOT_MODE";
        case MGCMD_GET_FW_VERSION: return "MGCMD_GET_FW_VERSION";
        case MGCMD_READ_ADCS: return "MGCMD_READ_ADCS";
        case MGCMD_GET_LAST_FRAME: return "MGCMD_GET_LAST_FRAME";
        case MGCMD_IO_FUNCTIONS: return "MGCMD_IO_FUNCTIONS";
        case MGIO_INSERT_BUTTON: return "MGIO_INSERT_BUTTON";
        case MGIO_GET_LED_STATES: return "MGIO_GET_LED_STATES";
        case MGIO_READ_DISPLAY: return "MGIO_READ_DISPLAY";
        case MGCMD_RD_FUNCTIONS: return "MGCMD_RD_FUNCTIONS";
        case MGCMD_EXPO_FUNCTIONS: return "MGCMD_EXPO_FUNCTIONS";
        case MGEXP_SET_EXTERNAL: return "MGEXP_SET_EXTERNAL";
        case MGEXP_SET_EXTERNAL_OFF: return "MGEXP_SET_EXTERNAL_OFF";
        case MGEXP_SET_EXTERNAL_ON: return "MGEXP_SET_EXTERNAL_ON";
        default: return "???";
    };
}

char const * const MGenAutoguider::getOpModeString(enum OpMode mode) const
{
    switch(mode)
    {
        case OPM_UNKNOWN:       return "UNKNOWN";
        case OPM_COMPATIBLE:    return "COMPATIBLE";
        case OPM_BOOT:          return "BOOT";
        case OPM_APPLICATION:   return "APPLICATION";
        default: return "???";
    }
}

/********************/

int MGenAutoguider::heartbeat(struct ftdi_context * const ftdi)
{
    unsigned char buffer[1] = {getOpCode(MGCMD_NOP1)};
    int bytes_read = 0;

    /* Heartbeat */
    if(ftdi_write_data(ftdi, buffer, 1) < 0)
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected while writing heartbeat", __FUNCTION__); usleep(100000); }
        return 1;
    }

    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: reading heartbeat result.", __FUNCTION__); usleep(100000); }
    if((bytes_read = ftdi_read_data(ftdi, buffer, 1)) < 0)
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected while reading heartbeat", __FUNCTION__); usleep(100000); }
        return 1;
    }

    if(verifyOpCode(MGCMD_NOP1, buffer, bytes_read))
    {
        { DEBUGF(INDI::Logger::DBG_SESSION, "%s: received heartbeat ack", __FUNCTION__); usleep(100000); }
        return 0;
    }

    { DEBUGF(INDI::Logger::DBG_SESSION, "%s: no heartbeat ack (%d bytes read)", __FUNCTION__, bytes_read); usleep(100000); }
    return 1;
}
