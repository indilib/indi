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

#include "mgc.h"

#define ERROR_MSG_LENGTH 250
#undef _L
#define _L(msg, ...) INDI::Logger::getInstance().print(getDeviceName(), INDI::Logger::DBG_SESSION, __FILE__, __LINE__, "%s: " msg, __FUNCTION__, __VA_ARGS__)

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

    SetCCDParams(128,64,8,5.0f,5.0f);
    PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes()*PrimaryCCD.getYRes()*PrimaryCCD.getBPP()/8,true);
}

MGenAutoguider::~MGenAutoguider()
{
    Disconnect();
}

bool MGenAutoguider::initProperties()
{
    _L("initiating properties","");

    INDI::CCD::initProperties();

    addDebugControl();

    {
        IUFillText(&VersionFirmwareT,"MGEN_FIRMWARE_VERSION","Firmware version","n/a");
        IUFillTextVector(&VersionTP, &VersionFirmwareT, 1, getDeviceName(), "Versions", "Versions", "Main Control", IP_RO, 60, IPS_IDLE);
        registerProperty(&VersionTP, INDI_TEXT);
    }

    {
        IUFillNumber(&VoltageN[0], "MGEN_LOGIC_VOLTAGE", "Logic voltage", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumber(&VoltageN[1], "MGEN_INPUT_VOLTAGE", "Input voltage", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumber(&VoltageN[2], "MGEN_REFERENCE_VOLTAGE", "Reference voltage", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumberVector(&VoltageNP, VoltageN, sizeof(VoltageN)/sizeof(VoltageN[0]), getDeviceName(), "MGEN_VOLTAGES", "Voltages", "Main Control", IP_RO, 60, IPS_IDLE);
        registerProperty(&VoltageNP, INDI_NUMBER);
    }

    {
        char const TAB[] = "Remote UI";
        IUFillNumber(&UIFramerateN, "MGEN_UI_FRAMERATE", "Frame rate", "%+02.2f fps", 0, 10, 1, 1);
        IUFillNumberVector(&UIFramerateNP, &UIFramerateN, 1, getDeviceName(), "MGEN_UI_OPTIONS", "UI", TAB, IP_RW, 60, IPS_IDLE);
        registerProperty(&UIFramerateNP, INDI_NUMBER);

        /* ESC  ---  SET
         * ---  UP   ---
         * LEFT ---  RIGHT
         * ---  DOWN ---
         */
        IUFillSwitch(&UIButtonS[0], "MGEN_UI_BUTTON_ESC", "ESC", ISS_OFF);
        UIButtonS[0].aux = (void*) ::MGIO_INSERT_BUTTON::IOB_ESC;
        IUFillSwitch(&UIButtonS[1], "MGEN_UI_BUTTON_SET", "SET", ISS_OFF);
        UIButtonS[1].aux = (void*) ::MGIO_INSERT_BUTTON::IOB_SET;
        IUFillSwitch(&UIButtonS[2], "MGEN_UI_BUTTON_UP", "UP", ISS_OFF);
        UIButtonS[2].aux = (void*) ::MGIO_INSERT_BUTTON::IOB_UP;
        IUFillSwitch(&UIButtonS[3], "MGEN_UI_BUTTON_LEFT", "LEFT", ISS_OFF);
        UIButtonS[3].aux = (void*) ::MGIO_INSERT_BUTTON::IOB_LEFT;
        IUFillSwitch(&UIButtonS[4], "MGEN_UI_BUTTON_RIGHT", "RIGHT", ISS_OFF);
        UIButtonS[4].aux = (void*) ::MGIO_INSERT_BUTTON::IOB_RIGHT;
        IUFillSwitch(&UIButtonS[5], "MGEN_UI_BUTTON_DOWN", "DOWN", ISS_OFF);
        UIButtonS[5].aux = (void*) ::MGIO_INSERT_BUTTON::IOB_DOWN;
        IUFillSwitchVector(&UIButtonSP[0], &UIButtonS[0], 2, getDeviceName(), "MGEN_UI_BUTTONS1", "UI Buttons", TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
        registerProperty(&UIButtonSP[0],INDI_SWITCH);
        IUFillSwitchVector(&UIButtonSP[1], &UIButtonS[2], 4, getDeviceName(), "MGEN_UI_BUTTONS2", "UI Buttons", TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
        registerProperty(&UIButtonSP[1],INDI_SWITCH);
    }

    /*{
        IUFillBLOB(&UIFrameB,"UI Frame","Image","raw");
        IUFillBLOBVector(&UIFrameBP, &UIFrameB, 1, getDeviceName(), "UI Frame", "UI Frame", "Main Control", IP_RO, 60, IPS_IDLE);
        registerProperty(&UIFrameBP, INDI_BLOB);
    }*/

    return true;
}

bool MGenAutoguider::ISNewNumber(char const * dev, char const * name, double values[], char * names[], int n)
{
    if(connectionStatus.isActive())
    {
        if(!strcmp(dev, getDeviceName()))
        {
            if(!strcmp(name, "MGEN_UI_OPTIONS"))
            {
                IUUpdateNumber(&UIFramerateNP, values, names, n);
                UIFramerateNP.s = IPS_OK;
                IDSetNumber(&UIFramerateNP, NULL);
                return true;
            }
        }
    }

    return CCD::ISNewNumber(dev, name, values, names, n);
}

bool MGenAutoguider::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(connectionStatus.isActive())
    {
        if(!strcmp(dev, getDeviceName()))
        {
            if(!strcmp(name, "MGEN_UI_BUTTONS1"))
            {
                IUUpdateSwitch(&UIButtonSP[0], states, names, n);
                ISwitch * const key_switch = IUFindOnSwitch(&UIButtonSP[0]);
                connectionStatus.pushButton((unsigned int)key_switch->aux);
                _L("sending key %d to remote UI",(unsigned int)key_switch->aux);
                key_switch->s = ISS_OFF;
                UIButtonSP[0].s = IPS_OK;
                IDSetSwitch(&UIButtonSP[0], NULL);
            }
            if(!strcmp(name, "MGEN_UI_BUTTONS2"))
            {
                IUUpdateSwitch(&UIButtonSP[1], states, names, n);
                ISwitch * const key_switch = IUFindOnSwitch(&UIButtonSP[1]);
                connectionStatus.pushButton((unsigned int)key_switch->aux);
                _L("sending key %d to remote UI",(unsigned int)key_switch->aux);
                key_switch->s = ISS_OFF;
                UIButtonSP[1].s = IPS_OK;
                IDSetSwitch(&UIButtonSP[1], NULL);
            }
        }
    }

    return CCD::ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool MGenAutoguider::Connect()
{
    if(connectionStatus.isActive())
    {
        _L("ignoring connection request received while already connected.","");
        return true;
    }

    _L("initiating connection.","");

    unsigned long int thread = 0;
    //struct threadData arg = { NULL, MGenAutoguider::MGCMD_NOP0 };
    pthread_mutex_t lock;

    connectionStatus = DeviceState();
    connectionStatus.enable();

    //if( !pthread_mutex_init(&lock,0) )
    {
        if( !pthread_create(&thread, NULL, &MGenAutoguider::connectionThreadWrapper, this) )
        {
            _L("connection thread %p started successfully.", thread);
            sleep(10);
        }
    }

    return connectionStatus.isActive();
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool MGenAutoguider::Disconnect()
{
    if( connectionStatus.isActive() )
    {
        _L("initiating disconnection.","");

        connectionStatus.disable();
        sleep(10);
    }

    return !connectionStatus.isActive();
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

    _L("switching device to baudrate %d", baudrate);

    int res = 0;

    if((res = ftdi_set_baudrate(ftdi, baudrate)) < 0)
    {
        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
        _L("failed updating device connection using %d bauds (%d: %s)", baudrate, res, ftdi_get_error_string(ftdi));
        return 1;
    }

    if((res = ftdi_set_line_property(ftdi, BITS_8, STOP_BIT_1, NONE)) < 0)
    {
        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
        _L("failed setting device line properties to 8-N-1 (%d: %s)", res, ftdi_get_error_string(ftdi));
        return 1;
    }

    /* Purge I/O buffers */
    if((res = ftdi_usb_purge_buffers(ftdi)) < 0 )
    {
        _L("failed purging I/O buffers (%d: %s)", res, ftdi_get_error_string(ftdi));
        return 1;
    }

    /* Set latency to minimal 2ms */
    if((res = ftdi_set_latency_timer(ftdi, 2)) < 0 )
    {
        _L("failed setting latency timer (%d: %s)", res, ftdi_get_error_string(ftdi));
        return 1;
    }

    _L("successfully switched device to baudrate %d.", baudrate);
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
    _L("connection thread starting, using FTDI '%s' v%d.%d.%d snapshot '%s'", version.version_str, version.major, version.minor, version.micro, version.snapshot_str);

    int  const vid = 0x0403, pid = 0x6001;

    int res = 0;
    struct ftdi_context *ftdi = ftdi_new();
    if( !ftdi )
    {
        _L("ftdi context initialization failed","");
        connectionStatus.disable();
    }
    else if((res = ftdi_set_interface(ftdi, INTERFACE_ANY)) < 0)
    {
        _L("failed setting FTDI interface to ANY (%d: %s)", res, ftdi_get_error_string(ftdi));
        connectionStatus.disable();
    }
    else if((res = ftdi_usb_open(ftdi, vid, pid)) < 0)
    {
        _L("device 0x%04X:0x%04X not found (%d: %s)", vid, pid, res, ftdi_get_error_string(ftdi));

        if((res = ftdi_set_interface(ftdi, INTERFACE_ANY)) < 0)
        {
            _L("failed setting FTDI interface to ANY (%d: %s)", res, ftdi_get_error_string(ftdi));
        }
        else
        {
            struct ftdi_device_list *devlist;
            if((res = ftdi_usb_find_all(ftdi, &devlist, 0, 0)) < 0)
            {
                _L("no FTDI device found (%d: %s)", res, ftdi_get_error_string(ftdi));
            }
            else
            {
                if(devlist) for( struct ftdi_device_list const * dev_index = devlist; dev_index; dev_index = dev_index->next )
                {
                    struct libusb_device_descriptor desc = {0};

                    if(libusb_get_device_descriptor(dev_index->dev, &desc) < 0)
                    {
                        _L("device %p returned by libftdi is unreadable", dev_index->dev);
                        continue;
                    }

                    _L("detected device 0x%04X:0x%04X", desc.idVendor, desc.idProduct);
                }
                else _L("no FTDI device enumerated","");

                ftdi_list_free(&devlist);
            }
        }

        connectionStatus.disable();
    }
    else if(setOpModeBaudRate(ftdi, OPM_COMPATIBLE) < 0)
    {
        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
        _L("failed setting up device line","");
        connectionStatus.disable();
    }
    else
    {
        _L("device 0x%04X:0x%04X connected successfully", vid, pid);
    }

    try
    {
        while( connectionStatus.isActive() )
        {
            switch( connectionStatus.getOpMode() )
            {
                /* Unknown mode, try to connect in COMPATIBLE mode first */
                case OPM_UNKNOWN:
                    _L("running device identification","");

                    /* Run an identification - failing this is not a problem, we'll try to communicate in applicative mode next */
                    if(MGC::CR_SUCCESS == ::MGCP_QUERY_DEVICE(*this).ask(ftdi))
                    {
                        _L("identified boot/compatible mode","");
                        connectionStatus.setOpMode(OPM_COMPATIBLE);
                        continue;
                    }

                    _L("identification failed, try to communicate as if in applicative mode","");
                    connectionStatus.setOpMode(OPM_APPLICATION);

                    if(setOpModeBaudRate(ftdi, connectionStatus.getOpMode()))
                    {
                        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                        _L("failed adjusting device line","");
                        connectionStatus.disable();
                        continue;
                    }

                    /* Run a basic exchange */
                    if(MGC::CR_SUCCESS != ::MGCMD_NOP1(*this).ask(ftdi))
                    {
                        if(connectionStatus.tried_turn_on)
                        {
                            _L("failed heartbeat after turn on, bailing out","");
                            connectionStatus.disable();
                            continue;
                        }

                        /* Perhaps the device is not turned on, so try to press ESC for a short time */
                        _L("no answer from device, trying to turn it on","");

                        unsigned char cbus_dir = 1<<1, cbus_val = 1<<1; /* Spec uses unitialized variables here */

                        if(ftdi_set_bitmode(ftdi, (cbus_dir<<4)+cbus_val, 0x20) < 0)
                        {
                            _L("failed depressing ESC to turn device on","");
                            connectionStatus.disable();
                            continue;
                        }

                        usleep(250000);
                        cbus_val &= ~(1<<1);

                        if(ftdi_set_bitmode(ftdi, (cbus_dir<<4)+cbus_val, 0x20) < 0)
                        {
                            _L("failed releasing ESC to turn device on","");
                            connectionStatus.disable();
                            continue;
                        }

                        /* Wait for the device to turn on */
                        sleep(5);

                        _L("turned device on, retrying identification","");
                        connectionStatus.setOpMode(OPM_UNKNOWN);
                        connectionStatus.tried_turn_on = true;
                    }

                    break;

                /*case OPM_BOOT:*/
                case OPM_COMPATIBLE:
                    _L("switching from compatible to normal mode","");

                    /* Switch to applicative mode */
                    ::MGCP_ENTER_NORMAL_MODE(*this).ask(ftdi);

                    connectionStatus.setOpMode(OPM_APPLICATION);

                    if(setOpModeBaudRate(ftdi, connectionStatus.getOpMode()))
                    {
                        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                        _L("failed updating device connection","");
                        connectionStatus.disable();
                        continue;
                    }

                    _L("device is in now expected to be in applicative mode","");

                    if(MGC::CR_SUCCESS != ::MGCMD_NOP1(*this).ask(ftdi))
                    {
                        connectionStatus.disable();
                        continue;
                    }

                    break;

                case OPM_APPLICATION:
                    /* If we didn't get the firmware version, ask */
                    if( !connectionStatus.version.uploaded_firmware )
                    {
                        ::MGCMD_GET_FW_VERSION cmd(*this);
                        if(MGC::CR_SUCCESS == cmd.ask(ftdi))
                        {
                            connectionStatus.version.uploaded_firmware = cmd.fw_version();
                            sprintf(VersionFirmwareT.text,"%04X", connectionStatus.version.uploaded_firmware);
                            _L("received version 0x%04X", connectionStatus.version.uploaded_firmware);
                            IDSetText(&VersionTP, NULL);
                            break;
                        }
                        else _L("failed retrieving firmware version","");
                    }

                    /* Heartbeat */
                    if(connectionStatus.heartbeat.timestamp + 5 < time(0))
                    {
                        if(MGC::CR_SUCCESS != ::MGCMD_NOP1(*this).ask(ftdi))
                        {
                            connectionStatus.no_ack_count++;
                            _L("%d times no ack to NOP1", connectionStatus.no_ack_count);
                            if(5 < connectionStatus.no_ack_count)
                            {
                                connectionStatus.disable();
                                continue;
                            }
                        }
                        else connectionStatus.no_ack_count = 0;

                        connectionStatus.heartbeat.timestamp = time(0);
                    }

                    /* Update ADC values */
                    if(connectionStatus.voltage.timestamp + 20 < time(0))
                    {
                        ::MGCMD_READ_ADCS adcs(*this);

                        if(MGC::CR_SUCCESS == adcs.ask(ftdi))
                        {
                            VoltageN[0].value = connectionStatus.voltage.logic = adcs.logic_voltage();
                            _L("received logic voltage %fV (spec is between 4.8V and 5.1V)", connectionStatus.voltage.logic);
                            VoltageN[1].value = connectionStatus.voltage.input = adcs.input_voltage();
                            _L("received input voltage %fV (spec is between 9V and 15V)", connectionStatus.voltage.input);
                            VoltageN[2].value = connectionStatus.voltage.reference = adcs.refer_voltage();
                            _L("received reference voltage %fV (spec is around 1.23V)", connectionStatus.voltage.reference);

                            /* FIXME: my device has input at 15.07... */
                            if(4.8f <= connectionStatus.voltage.logic && connectionStatus.voltage.logic <= 5.1f)
                                if(9.0f <= connectionStatus.voltage.input && connectionStatus.voltage.input <= 15.0f)
                                    if(1.1 <= connectionStatus.voltage.reference && connectionStatus.voltage.reference <= 1.3)
                                        VoltageNP.s = IPS_OK;
                                    else VoltageNP.s = IPS_ALERT;
                                else VoltageNP.s = IPS_ALERT;
                            else VoltageNP.s = IPS_ALERT;

                            IDSetNumber(&VoltageNP, NULL);
                        }

                        connectionStatus.voltage.timestamp = time(0);
                    }

                    /* Update UI frame */
                    if(0 < UIFramerateN.value && connectionStatus.ui_frame.timestamp + 1/UIFramerateN.value < time(0))
                    {
                        //::MGCMD_IO_FUNCTIONS io_func(*this);

                        //if(MGC::CR_SUCCESS == io_func.ask(ftdi))
                        {
                            ::MGIO_READ_DISPLAY_FRAME read_frame(*this);

                            if(MGC::CR_SUCCESS == read_frame.ask(ftdi))
                            {
                                ::MGIO_READ_DISPLAY_FRAME::ByteFrame frame;
                                read_frame.get_frame(frame);
                                //UIFrameB.blob = read_frame.get_frame(frame).data();
                                //UIFrameB.bloblen = frame.size();
                                //IDSetBLOB(&UIFrameBP, NULL);
                                memcpy(PrimaryCCD.getFrameBuffer(),frame.data(),frame.size());
                                /*for(unsigned int i = 0; i < 64; i++)
                                    for(unsigned int j = 0; j < 128; j++)
                                        PrimaryCCD.getFrameBuffer()[i*128+j] = (i/16)+(j/16);*/
                                //PrimaryCCD.binFrame();
                                ExposureComplete(&PrimaryCCD);
                            }
                            else _L("failed reading frame","");
                        }
                        //else _L("failed initiating IO function","");

                        connectionStatus.ui_frame.timestamp = time(0);
                    }

                    /* Send buttons */
                    if(connectionStatus.hasButtons())
                    {
                        ::MGIO_INSERT_BUTTON(*this, (::MGIO_INSERT_BUTTON::Button) connectionStatus.popButton()).ask(ftdi);
                    }

#if 0
                    /* OK, wait a few seconds and retry heartbeat */
                    for( int i = 0; i < 10; i++ )
                        if( connectionStatus.isActive() )
                            usleep(500000);
#endif

                    break;
            }
        }
    }
    catch(MGC::IOError &e)
    {
        _L("device disconnected (%s)", e.what());
        connectionStatus.disable();
    }

    if(ftdi)
    {
        ftdi_usb_close(ftdi);
        ftdi_free(ftdi);
    }

    _L("disconnected successfully.","");
    //return 0;
}

/**************************************************************************************
 * Helpers
 **************************************************************************************/

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
#if 0
int MGenAutoguider::heartbeat(struct ftdi_context * const ftdi)
{
    unsigned char buffer[1] = {getOpCode(MGCMD_NOP1)};
    int bytes_read = 0;

    /* Heartbeat */
    if(ftdi_write_data(ftdi, buffer, 1) < 0)
    {
        _L("device disconnected while writing heartbeat","");
        return 1;
    }

    _L("reading heartbeat result.","");
    if((bytes_read = ftdi_read_data(ftdi, buffer, 1)) < 0)
    {
        _L("device disconnected while reading heartbeat","");
        return 1;
    }

    if(verifyOpCode(MGCMD_NOP1, buffer, bytes_read))
    {
        _L("received heartbeat ack","");
        return 0;
    }

    _L("no heartbeat ack (%d bytes read)", bytes_read);
    return 1;
}
#endif
