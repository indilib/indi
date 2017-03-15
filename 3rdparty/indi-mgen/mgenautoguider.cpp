/*
   INDI Developers Manual
   Tutorial #1

   "Hello INDI"

   We construct a most basic (and useless) device driver to illustate INDI.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simpledevice->cpp
    \brief Construct a basic INDI device with only one property to connect and disconnect.
    \author Jasem Mutlaq

    \example mgenautoguider.cpp
    A very minimal device! It also allows you to connect/disconnect and performs no other functions.
*/

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>
#include <queue>

#include "indidevapi.h"
#include "indilogger.h"
#include "indiccd.h"

#include "mgen.h"
#include "mgenautoguider.h"
#include "mgen_device.h"
#include "mgc.h"

using namespace std;
std::unique_ptr<MGenAutoguider> mgenAutoguider(new MGenAutoguider());
MGenAutoguider & MGenAutoguider::instance() { return *mgenAutoguider; }


/**************************************************************************************
** Return properties of device->
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

bool MGenAutoguider::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(device && device->isConnected())
    {
        if(!strcmp(dev, getDeviceName()))
        {
            if(!strcmp(name, "MGEN_UI_BUTTONS1"))
            {
                IUUpdateSwitch(&ui.UIButtonSP[0], states, names, n);
                ISwitch * const key_switch = IUFindOnSwitch(&ui.UIButtonSP[0]);
                device->pushButton((unsigned int)key_switch->aux);
                key_switch->s = ISS_OFF;
                ui.UIButtonSP[0].s = IPS_OK;
                IDSetSwitch(&ui.UIButtonSP[0], NULL);
            }
            if(!strcmp(name, "MGEN_UI_BUTTONS2"))
            {
                IUUpdateSwitch(&ui.UIButtonSP[1], states, names, n);
                ISwitch * const key_switch = IUFindOnSwitch(&ui.UIButtonSP[1]);
                device->pushButton((unsigned int)key_switch->aux);
                key_switch->s = ISS_OFF;
                ui.UIButtonSP[1].s = IPS_OK;
                IDSetSwitch(&ui.UIButtonSP[1], NULL);
            }
        }
    }

    return CCD::ISNewSwitch(dev, name, states, names, n);
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

bool MGenAutoguider::ISNewNumber(char const * dev, char const * name, double values[], char * names[], int n)
{
    if(device && device->isConnected())
    {
        if(!strcmp(dev, getDeviceName()))
        {
            if(!strcmp(name, "MGEN_UI_OPTIONS"))
            {
                IUUpdateNumber(&ui.UIFramerateNP, values, names, n);
                ui.UIFramerateNP.s = IPS_OK;
                IDSetNumber(&ui.UIFramerateNP, NULL);
                return true;
            }
        }
    }

    return CCD::ISNewNumber(dev, name, values, names, n);
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

MGenAutoguider::MGenAutoguider():
    device(NULL),
    version({0}),
    voltage({0}),
    ui({0}),
    heartbeat({0})
{
    SetCCDParams(128,64,8,5.0f,5.0f);
    PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes()*PrimaryCCD.getYRes()*PrimaryCCD.getBPP()/8,true);
}

MGenAutoguider::~MGenAutoguider()
{
    Disconnect();
    if(device)
        delete device;
}

bool MGenAutoguider::initProperties()
{
    _L("initiating properties","");

    INDI::CCD::initProperties();

    addDebugControl();

    {
        char const TAB[] = "Main Control";
        IUFillText(&version.VersionFirmwareT,"MGEN_FIRMWARE_VERSION","Firmware version","n/a");
        IUFillTextVector(&version.VersionTP, &version.VersionFirmwareT, 1, getDeviceName(), "Versions", "Versions", TAB, IP_RO, 60, IPS_IDLE);
    }

    {
        char const TAB[] = "Voltages";
        IUFillNumber(&voltage.VoltageN[0], "MGEN_LOGIC_VOLTAGE", "Logic voltage", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumber(&voltage.VoltageN[1], "MGEN_INPUT_VOLTAGE", "Input voltage", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumber(&voltage.VoltageN[2], "MGEN_REFERENCE_VOLTAGE", "Reference voltage", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumberVector(&voltage.VoltageNP, voltage.VoltageN, sizeof(voltage.VoltageN)/sizeof(voltage.VoltageN[0]), getDeviceName(), "MGEN_VOLTAGES", "Voltages", TAB, IP_RO, 60, IPS_IDLE);
    }

    {
        char const TAB[] = "Remote UI";
        IUFillNumber(&ui.UIFramerateN, "MGEN_UI_FRAMERATE", "Frame rate", "%+02.2f fps", 0, 10, 1, 1);
        IUFillNumberVector(&ui.UIFramerateNP, &ui.UIFramerateN, 1, getDeviceName(), "MGEN_UI_OPTIONS", "UI", TAB, IP_RW, 60, IPS_IDLE);

        IUFillSwitch(&ui.UIButtonS[0], "MGEN_UI_BUTTON_ESC", "ESC", ISS_OFF);
        ui.UIButtonS[0].aux = (void*) MGIO_INSERT_BUTTON::IOB_ESC;
        IUFillSwitch(&ui.UIButtonS[1], "MGEN_UI_BUTTON_SET", "SET", ISS_OFF);
        ui.UIButtonS[1].aux = (void*) MGIO_INSERT_BUTTON::IOB_SET;
        IUFillSwitch(&ui.UIButtonS[2], "MGEN_UI_BUTTON_UP", "UP", ISS_OFF);
        ui.UIButtonS[2].aux = (void*) MGIO_INSERT_BUTTON::IOB_UP;
        IUFillSwitch(&ui.UIButtonS[3], "MGEN_UI_BUTTON_LEFT", "LEFT", ISS_OFF);
        ui.UIButtonS[3].aux = (void*) MGIO_INSERT_BUTTON::IOB_LEFT;
        IUFillSwitch(&ui.UIButtonS[4], "MGEN_UI_BUTTON_RIGHT", "RIGHT", ISS_OFF);
        ui.UIButtonS[4].aux = (void*) MGIO_INSERT_BUTTON::IOB_RIGHT;
        IUFillSwitch(&ui.UIButtonS[5], "MGEN_UI_BUTTON_DOWN", "DOWN", ISS_OFF);
        ui.UIButtonS[5].aux = (void*) MGIO_INSERT_BUTTON::IOB_DOWN;
        /* ESC SET
         * UP LEFT RIGHT DOWN
         */
        IUFillSwitchVector(&ui.UIButtonSP[0], &ui.UIButtonS[0], 2, getDeviceName(), "MGEN_UI_BUTTONS1", "UI Buttons", TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
        IUFillSwitchVector(&ui.UIButtonSP[1], &ui.UIButtonS[2], 4, getDeviceName(), "MGEN_UI_BUTTONS2", "UI Buttons", TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    }

    return true;
}

bool MGenAutoguider::updateProperties()
{
    CCD::updateProperties();

    if(isConnected())
    {
        registerProperty(&version.VersionTP, INDI_TEXT);
        registerProperty(&voltage.VoltageNP, INDI_NUMBER);
        registerProperty(&ui.UIFramerateNP, INDI_NUMBER);
        registerProperty(&ui.UIButtonSP[0], INDI_SWITCH);
        registerProperty(&ui.UIButtonSP[1], INDI_SWITCH);
    }
    else
    {
        deleteProperty(version.VersionTP.name);
        deleteProperty(voltage.VoltageNP.name);
        deleteProperty(ui.UIFramerateNP.name);
        deleteProperty(ui.UIButtonSP[0].name);
        deleteProperty(ui.UIButtonSP[1].name);
    }

    return true;
}

/**************************************************************************************
** Client is asking us to establish connection to the device
***************************************************************************************/
bool MGenAutoguider::Connect()
{
    if(device && device->isConnected())
    {
        _L("ignoring connection request received while already connected.","");
        return true;
    }

    _L("initiating connection.","");

    unsigned long int thread = 0;

    if(device)
        delete device;
    device = new MGenDevice();
    device->enable();

    //if( !pthread_mutex_init(&lock,0) )
    {
        if( !pthread_create(&thread, NULL, &MGenAutoguider::connectionThreadWrapper, this) )
        {
            _L("connection thread %p started successfully.", thread);
            sleep(10);
        }
    }

    return device->isConnected();
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool MGenAutoguider::Disconnect()
{
    if( device->isConnected() )
    {
        _L("initiating disconnection.","");

        device->disable();
        sleep(10);
    }

    return !device->isConnected();
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

IOMode MGenAutoguider::getOpMode() const
{
    return device->getOpMode();
}

/**************************************************************************************
 * Connection thread
 **************************************************************************************/
void* MGenAutoguider::connectionThreadWrapper(void* arg)
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

    device->Connect(0x0403, 0x6001);

    try
    {
        while( device->isConnected() )
        {
            switch( getOpMode() )
            {
                /* Unknown mode, try to connect in COMPATIBLE mode first */
                case OPM_UNKNOWN:
                    _L("running device identification","");

                    /* Run an identification - failing this is not a problem, we'll try to communicate in applicative mode next */
                    if(CR_SUCCESS == MGCP_QUERY_DEVICE().ask(*device))
                    {
                        _L("identified boot/compatible mode","");
                        device->setOpMode(OPM_COMPATIBLE);
                        continue;
                    }

                    _L("identification failed, try to communicate as if in applicative mode","");
                    if(device->setOpMode(OPM_APPLICATION))
                    {
                        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                        _L("failed adjusting device line","");
                        device->disable();
                        continue;
                    }

                    /* Run a basic exchange */
                    if(CR_SUCCESS != MGCMD_NOP1().ask(*device))
                    {
                        if(device->TurnPowerOn())
                        {
                            _L("failed heartbeat after turn on, bailing out","");
                            device->disable();
                            continue;
                        }
                    }

                    break;

                /*case OPM_BOOT:*/
                case OPM_COMPATIBLE:
                    _L("switching from compatible to normal mode","");

                    /* Switch to applicative mode */
                    MGCP_ENTER_NORMAL_MODE().ask(*device);

                    if(device->setOpMode(OPM_APPLICATION))
                    {
                        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                        _L("failed updating device connection","");
                        device->disable();
                        continue;
                    }

                    _L("device is in now expected to be in applicative mode","");

                    if(CR_SUCCESS != MGCMD_NOP1().ask(*device))
                    {
                        device->disable();
                        continue;
                    }

                    break;

                case OPM_APPLICATION:
                    /* If we didn't get the firmware version, ask */
                    if( !version.uploaded_firmware )
                    {
                        MGCMD_GET_FW_VERSION cmd;
                        if(CR_SUCCESS == cmd.ask(*device))
                        {
                            version.uploaded_firmware = cmd.fw_version();
                            sprintf(version.VersionFirmwareT.text,"%04X", version.uploaded_firmware);
                            _L("received version 0x%04X", version.uploaded_firmware);
                            IDSetText(&version.VersionTP, NULL);
                            break;
                        }
                        else _L("failed retrieving firmware version","");
                    }

                    /* Heartbeat */
                    if(heartbeat.timestamp + 5 < time(0))
                    {
                        if(CR_SUCCESS != MGCMD_NOP1().ask(*device))
                        {
                            heartbeat.no_ack_count++;
                            _L("%d times no ack to NOP1", heartbeat.no_ack_count);
                            if(5 < heartbeat.no_ack_count)
                            {
                                device->disable();
                                continue;
                            }
                        }
                        else heartbeat.no_ack_count = 0;

                        heartbeat.timestamp = time(0);
                    }

                    /* Update ADC values */
                    if(voltage.timestamp + 20 < time(0))
                    {
                        MGCMD_READ_ADCS adcs;

                        if(CR_SUCCESS == adcs.ask(*device))
                        {
                            voltage.VoltageN[0].value = voltage.logic = adcs.logic_voltage();
                            _L("received logic voltage %fV (spec is between 4.8V and 5.1V)", voltage.logic);
                            voltage.VoltageN[1].value = voltage.input = adcs.input_voltage();
                            _L("received input voltage %fV (spec is between 9V and 15V)", voltage.input);
                            voltage.VoltageN[2].value = voltage.reference = adcs.refer_voltage();
                            _L("received reference voltage %fV (spec is around 1.23V)", voltage.reference);

                            /* FIXME: my device has input at 15.07... */
                            if(4.8f <= voltage.logic && voltage.logic <= 5.1f)
                                if(9.0f <= voltage.input && voltage.input <= 15.0f)
                                    if(1.1 <= voltage.reference && voltage.reference <= 1.3)
                                        voltage.VoltageNP.s = IPS_OK;
                                    else voltage.VoltageNP.s = IPS_ALERT;
                                else voltage.VoltageNP.s = IPS_ALERT;
                            else voltage.VoltageNP.s = IPS_ALERT;

                            IDSetNumber(&voltage.VoltageNP, NULL);
                        }

                        voltage.timestamp = time(0);
                    }

                    /* Update UI frame */
                    if(0 < ui.UIFramerateN.value && ui.timestamp + 1/ui.UIFramerateN.value < time(0))
                    {
                        MGIO_READ_DISPLAY_FRAME read_frame;

                        if(CR_SUCCESS == read_frame.ask(*device))
                        {
                            MGIO_READ_DISPLAY_FRAME::ByteFrame frame;
                            read_frame.get_frame(frame);
                            memcpy(PrimaryCCD.getFrameBuffer(),frame.data(),frame.size());
                            ExposureComplete(&PrimaryCCD);
                        }
                        else _L("failed reading frame","");

                        ui.timestamp = time(0);
                    }

                    /* Send buttons */
                    /*
                    if(!device->button_queue.empty())
                    {
                        MGIO_INSERT_BUTTON((MGIO_INSERT_BUTTON::Button) device->popButton()).ask(*device);
                    }
                    */

#if 0
                    /* OK, wait a few seconds and retry heartbeat */
                    for( int i = 0; i < 10; i++ )
                        if( device->isConnected() )
                            usleep(500000);
#else
                    usleep(10000); /* FIXME: validate against UI framerate */
#endif

                    break;
            }
        }
    }
    catch(IOError &e)
    {
        _L("device disconnected (%s)", e.what());
        device->disable();
    }

    _L("disconnected successfully.","");
    //return 0;
}

/**************************************************************************************
 * Helpers
 **************************************************************************************/

