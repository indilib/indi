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
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

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
    DEBUGF(INDI::Logger::DBG_SESSION, "%s: initiating connection.", __FUNCTION__);

    unsigned long int thread = 0;
    //struct threadData arg = { NULL, MGenAutoguider::MGCMD_NOP0 };
    pthread_mutex_t lock;

    if( !pthread_mutex_init(&lock,0) )
    {
        if( !pthread_create(&thread, NULL, &MGenAutoguider::connectionThreadWrapper, this) )
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "%s: connection thread %p started successfully.", __FUNCTION__, thread);
        }
    }

    return true;
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool MGenAutoguider::Disconnect()
{
    DEBUGF(INDI::Logger::DBG_SESSION, "%s: initiating disconnection.", __FUNCTION__);

    connectionStatus.is_active = false;
    sleep(1);

    return true;
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

int MGenAutoguider::getOpModeBaudRate( enum OpMode mode ) const
{
    switch( mode )
    {
        case OPM_COMPATIBLE: return 9600;
        case OPM_BOOT: return 9600;
        case OPM_APPLICATION: return 250000;
        case OPM_UNKNOWN:
	default:
            return 0;
    }
}

/**************************************************************************************
 * Connection thread
 **************************************************************************************/
void* MGenAutoguider::connectionThreadWrapper( void* arg )
{
    MGenAutoguider * const _this = reinterpret_cast<MGenAutoguider*>(arg);
    INDI::Logger::getInstance().print(_this->getDeviceName(), INDI::Logger::DBG_SESSION, __FILE__, __LINE__, "%s: connection thread wrapper.", __FUNCTION__);

    if(_this)
        _this->connectionThread();

    INDI::Logger::getInstance().print(_this->getDeviceName(), INDI::Logger::DBG_SESSION, __FILE__, __LINE__, "%s: connection thread terminated.", __FUNCTION__);
    return _this;
}

void MGenAutoguider::connectionThread()
{
    int fd = 0;
    char buffer[256];
    size_t const buffer_len = sizeof(buffer)/sizeof(buffer[0]);
    int bytes_read = 0;

    DEBUGF(INDI::Logger::DBG_SESSION, "%s: connection thread starting.", __FUNCTION__);
    connectionStatus.is_active = true;

    tty_set_debug(1);

    while( connectionStatus.is_active )
    {
        switch( connectionStatus.mode )
        {
            /* Unknown mode, try to connect in COMPATIBLE mode first */
            case OPM_UNKNOWN:
                DEBUGF(INDI::Logger::DBG_SESSION, "%s: initiating device connection on '%s'", __FUNCTION__, dev_path);
                switch( tty_connect(dev_path, getOpModeBaudRate(MGenAutoguider::OPM_COMPATIBLE), 8, 0, 1, &fd) )
                {
                    case TTY_OK:
                        /* OK, device descriptor is usable, flush input next */
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: device connected on '%s' using %d-8-N-1 successfully", __FUNCTION__, dev_path,getOpModeBaudRate(MGenAutoguider::OPM_COMPATIBLE));
                        break;

                    case TTY_PARAM_ERROR:
                    default:
                        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed connecting device on '%s' using %d-8-N-1, bailing out", __FUNCTION__, dev_path,getOpModeBaudRate(MGenAutoguider::OPM_COMPATIBLE));
                        connectionStatus.is_active = false;
                        continue;
                }

                /* Flush input and wait for timeout */
                memset(buffer,0,buffer_len);
#if 1
                /* TODO: add this implementation to indicom */
                tcflush(fd, TCIFLUSH); /* Leave out errors to next operations */
#else
                switch( tty_read(fd, buffer, buffer_len, 1, &bytes_read) );
                {
                    case TTY_TIME_OUT:
                    case TTY_READ_ERROR:
                        /* OK, there is a device connected here, and we maybe read stuff, so try to run an identification next */
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: flushed device data from '%s', identifying", __FUNCTION__, dev_path);
                        break;

                    case TTY_OK:
                        /* TODO: Not good, we've read the whole buffer, so perhaps we're in the middle of something - reboot the device? */
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: too much data from '%s', bailing out", __FUNCTION__, dev_path);
                        connectionStatus.is_active = false;
                        continue;

                    /* case TTY_ERRNO: */
                    default:
                        /* TODO: Not good, we've lost the device */
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected from '%s', bailing out", __FUNCTION__, dev_path);
                        connectionStatus.is_active = false;
                        continue;
                }
#endif

                DEBUGF(INDI::Logger::DBG_SESSION, "%s: running device identification.", __FUNCTION__);

                /* Run an identification */
                buffer[0] = getOpCode(MGCP_QUERY_DEVICE);
                buffer[1] = 0x01; /* Length of parameters */
                buffer[2] = 0x01; /* Query device ID sub-comand */
                switch( tty_write(fd, buffer, 3, &bytes_read) )
                {
                    case TTY_OK:
                        /* OK, read back result next */
                        break;

                    default:
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected from '%s', bailing out", __FUNCTION__, dev_path);
                        connectionStatus.is_active = false;
                        continue;
                }

                DEBUGF(INDI::Logger::DBG_SESSION, "%s: reading identification results.", __FUNCTION__);
                switch( tty_read(fd, buffer, 5, 1, &bytes_read) )
                {
                    case TTY_OK:
                        /* Verify opcode answer */
                        if( verifyOpCode(MGCP_QUERY_DEVICE, buffer, bytes_read) )
                        {
                            DEBUGF(INDI::Logger::DBG_SESSION, "%s: device acknowledged identification, analyzing '%02X%02X%02X'.", __FUNCTION__, (unsigned char) buffer[2], (unsigned char) buffer[3], (unsigned char) buffer[4]);

                            /* Check whether we are in applicative mode */
                            unsigned char const app_mode_answer[] = { 0x01, 0x80, 0x02 };
                            if( !memcmp(&buffer[2], &app_mode_answer, sizeof(app_mode_answer)/sizeof(app_mode_answer[0])) )
                            {
                                /* Applicative mode, next switch to higher baudrate */
                                DEBUGF(INDI::Logger::DBG_SESSION, "%s: identified applicative mode.", __FUNCTION__);
                                connectionStatus.mode = OPM_APPLICATION;
                                break;
                            }

                            /* Check whether we are in boot mode */
                            unsigned char const boot_mode_answer[] = { 0x01, 0x80, 0x01 };
                            if( !memcmp(&buffer[2], &boot_mode_answer, sizeof(boot_mode_answer)/sizeof(boot_mode_answer[0])) )
                            {
                                /* Boot mode, next switch to applicative mode */
                                DEBUGF(INDI::Logger::DBG_SESSION, "%s: identified boot mode.", __FUNCTION__);
                                connectionStatus.mode = OPM_BOOT;
                                break;

                            }
                        }
                        else
                        {
                            DEBUGF(INDI::Logger::DBG_SESSION, "%s: device identification failed (%d bytes read, opcode 0x%02X)", __FUNCTION__, bytes_read, (unsigned char) buffer[0]);
                        }
                        /* No-break */

                    case TTY_PARAM_ERROR:
                    case TTY_READ_ERROR:
                    case TTY_TIME_OUT:
                    default:
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected from '%s', bailing out", __FUNCTION__, dev_path);
                        connectionStatus.is_active = false;
                        continue;
                }

                DEBUGF(INDI::Logger::DBG_SESSION, "%s: done identifying.", __FUNCTION__);

                if( OPM_APPLICATION == connectionStatus.mode )
                {
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: switching tty to application baudrate.", __FUNCTION__);

                    /* Update tty applicative baudrate */
                    //struct termios tty_setting = {0};
                    //tcgetattr(fd, &tty_setting);
                    //cfsetspeed(&tty_setting,B2500000);
                    //tcsetattr(fd, TCSANOW, &tty_setting);
                    struct serial_struct tty_special = {0};
                    if( !ioctl(fd, TIOCGSERIAL, &tty_special) )
                    {
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: tty base baudrate is %d.", __FUNCTION__, tty_special.baud_base);

                        tty_special.flags = ( tty_special.flags & ~ASYNC_SPD_MASK ) | ASYNC_SPD_CUST;
                        tty_special.custom_divisor = ( tty_special.baud_base + (getOpModeBaudRate(OPM_APPLICATION)/2) ) / getOpModeBaudRate(OPM_APPLICATION);

                        if( !ioctl(fd,TIOCSSERIAL,&tty_special) )
                        {
                            if( !ioctl(fd, TIOCGSERIAL, &tty_special) )
                            {
                                DEBUGF(INDI::Logger::DBG_SESSION, "%s: tty configured with baudrate base %d custom divisor %d = %d.", __FUNCTION__, tty_special.baud_base, tty_special.custom_divisor, tty_special.baud_base / tty_special.custom_divisor);

                                if( getOpModeBaudRate(OPM_APPLICATION) == tty_special.baud_base / tty_special.custom_divisor )
                                {
                                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: successfully switched tty to baudrate %d.", __FUNCTION__, getOpModeBaudRate(OPM_APPLICATION));
                                }
                                else
                                {
                                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed configuring device connection on '%s' to %d bauds, bailing out", __FUNCTION__, dev_path,getOpModeBaudRate(MGenAutoguider::OPM_APPLICATION));
                                    connectionStatus.is_active = false;
                                    continue;
                                }
                            }
                        }
                        else
                        {
                            DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed updating device connection on '%s' using %d bauds, bailing out", __FUNCTION__, dev_path,getOpModeBaudRate(MGenAutoguider::OPM_APPLICATION));
                            connectionStatus.is_active = false;
                            continue;
                        }
                    }
                    else
                    {
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed reading device connection characteristics on '%s', bailing out", __FUNCTION__);
                        connectionStatus.is_active = false;
                        continue;
                    }
//                    tty_disconnect(fd);
//                    switch( tty_connect(dev_path, getOpModeBaudRate(MGenAutoguider::OPM_APPLICATION), 8, 0, 1, &fd) )
//                    {
//                        case TTY_OK:
//                            /* OK, device descriptor is in applicative mode */
//                            DEBUGF(INDI::Logger::DBG_SESSION, "%s: device connected on '%s' using %d-8-N-1 successfully", __FUNCTION__, dev_path,getOpModeBaudRate(MGenAutoguider::OPM_APPLICATION));
//                            break;
//
//                        case TTY_PARAM_ERROR:
//                        default:
//                            /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
//                            DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed connecting device on '%s' using %d-8-N-1, bailing out", __FUNCTION__, dev_path,getOpModeBaudRate(MGenAutoguider::OPM_APPLICATION));
//                            connectionStatus.is_active = false;
//                            continue;
//                    }
                }

                break;

            case OPM_BOOT:
                /* Switch to applicative mode */
                buffer[0] = getOpCode(MGCP_ENTER_NORMAL_MODE);
                switch( tty_write(fd, buffer, 1, &bytes_read) )
                {
                    case TTY_OK:
                        /* OK, wait 100ms and reconnect */
                        usleep(100000);
                        break;

                    default:
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected from '%s', bailing out", __FUNCTION__, dev_path);
                        connectionStatus.is_active = false;
                        continue;
                }

                /* Disconnect device and reconnect at applicative baudrate */
                tty_disconnect(fd);
                switch( tty_connect(dev_path, getOpModeBaudRate(MGenAutoguider::OPM_APPLICATION), 8, 0, 1, &fd) )
                {
                    case TTY_OK:
                        /* OK, device descriptor is in applicative mode */
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: device connected on '%s' using %d-8-N-1 successfully", __FUNCTION__, dev_path,getOpModeBaudRate(MGenAutoguider::OPM_APPLICATION));
                        break;

                    case TTY_PARAM_ERROR:
                    default:
                        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: failed connecting device on '%s' using %d-8-N-1, bailing out", __FUNCTION__, dev_path, getOpModeBaudRate(MGenAutoguider::OPM_APPLICATION));
                        connectionStatus.is_active = false;
                        continue;
                }

                break;

            case OPM_APPLICATION:
                sleep(5);

                /* Heartbeat */
                buffer[0] = getOpCode(MGCMD_NOP1);
                switch( tty_write(fd, buffer, 1, &bytes_read) )
                {
                    case TTY_OK:
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: reading heartbeat result.", __FUNCTION__);
                        switch( tty_read(fd, buffer, 1, 1, &bytes_read) )
                        {
                            case TTY_OK:
                                if( verifyOpCode(MGCMD_NOP0, buffer, bytes_read) )
                                {
                                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: received heartbeat ack.", __FUNCTION__);
                                    break;
                                }
                                /* No-break */

                            case TTY_PARAM_ERROR:
                            case TTY_READ_ERROR:
                            case TTY_TIME_OUT:
                            default:
                                DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected from '%s', bailing out", __FUNCTION__, dev_path);
                                /*connectionStatus.is_active = false;*/
                                continue;
                        }

                        /* OK, wait 1s and retry */
                        sleep(5);
                        break;

                    default:
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s: device disconnected from '%s', bailing out", __FUNCTION__, dev_path);
                        connectionStatus.is_active = false;
                        continue;
                }

                break;
        }
    }


    //tty_write
    /* OK, device is in COMPATIBLE mode, switch it to APPLICATIVE */

    if( 0 <= fd )
        tty_disconnect(fd);

    DEBUGF(INDI::Logger::DBG_SESSION, "%s: disconnected successfully.", __FUNCTION__);
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
    switch( this->connectionStatus.mode )
    {
        case OPM_UNKNOWN:
        case OPM_COMPATIBLE:
            /* From spec, no particular version to support */
            switch( commandByte )
            {
                case MGCP_QUERY_DEVICE:             return 0xAA;
                case MGCP_ENTER_NORMAL_MODE:        return 0x42;
                default:
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: mode OPM_COMPATIBLE doesn't support opcode '%s'.", __FUNCTION__, getOpCodeString(commandByte));
                    break;
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
                default:
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: mode OPM_BOOT doesn't support opcode '%s'.", __FUNCTION__, getOpCodeString(commandByte) );
                    break;
            }
            break;

        case OPM_APPLICATION:
            switch( commandByte )
            {
                case MGCMD_NOP0:                    return 0x00;
                case MGCMD_NOP1:                    return 0xFF;
                default:
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: mode OPM_APPLICATION doesn't support opcode '%s'.", __FUNCTION__, getOpCodeString(commandByte) );
                    break;
            }

            break;

        default:
            DEBUGF(INDI::Logger::DBG_SESSION, "%s: invalid mode OPM_UNKNOWN for opcode '%s'.", __FUNCTION__, getOpCodeString(commandByte) );
            break;
    }

    /* Return NOP0 as fallback */
    return 0x00;
}

bool MGenAutoguider::verifyOpCode(enum CommandByte commandByte, char const *buffer, int bytes_read)
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

                default:
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: mode OPM_COMPATIBLE doesn't support opcode '%s'.", __FUNCTION__, getOpCodeString(commandByte));
                    break;
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

                default:
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s: mode OPM_BOOT doesn't support opcode '%s'.", __FUNCTION__, getOpCodeString(commandByte) );
                    break;
            }
            break;

        case OPM_APPLICATION:
            switch( commandByte )
            {
            }

            break;

        default:
            DEBUGF(INDI::Logger::DBG_SESSION, "%s: invalid mode OPM_UNKNOWN for opcode '%s'.", __FUNCTION__, getOpCodeString(commandByte) );
            break;
    }

    /* Return NOP0 as fallback */
    return 0x00;
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
