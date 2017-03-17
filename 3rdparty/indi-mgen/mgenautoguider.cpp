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
    //IDLog("%s: get properties.", __FUNCTION__);
    mgenAutoguider->ISGetProperties(dev);
}

/**************************************************************************************
** Process new switch from client
***************************************************************************************/
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    //IDLog("%s: new switch '%s'.", __FUNCTION__, name);
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
                IUUpdateSwitch(&ui.buttons.properties[0], states, names, n);
                ISwitch * const key_switch = IUFindOnSwitch(&ui.buttons.properties[0]);
                MGIO_INSERT_BUTTON((MGIO_INSERT_BUTTON::Button)(int) key_switch->aux).ask(*device);
                key_switch->s = ISS_OFF;
                ui.buttons.properties[0].s = IPS_OK;
                IDSetSwitch(&ui.buttons.properties[0], NULL);
            }
            if(!strcmp(name, "MGEN_UI_BUTTONS2"))
            {
                IUUpdateSwitch(&ui.buttons.properties[1], states, names, n);
                ISwitch * const key_switch = IUFindOnSwitch(&ui.buttons.properties[1]);
                MGIO_INSERT_BUTTON((MGIO_INSERT_BUTTON::Button)(int) key_switch->aux).ask(*device);
                key_switch->s = ISS_OFF;
                ui.buttons.properties[1].s = IPS_OK;
                IDSetSwitch(&ui.buttons.properties[1], NULL);
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
    //IDLog("%s: get new text '%s'.", __FUNCTION__, name);
    mgenAutoguider->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
** Process new number from client
***************************************************************************************/
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //IDLog("%s: new number '%s'.", __FUNCTION__, name);
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
                IUUpdateNumber(&ui.framerate.property, values, names, n);
                ui.framerate.property.s = IPS_OK;
                IDSetNumber(&ui.framerate.property, NULL);
                RemoveTimer(ui.timer);
                ui.timer = SetTimer(0 < ui.framerate.number.value ? 1000/(long)ui.framerate.number.value : 1000);
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
    //IDLog("%s: get new blob '%s'.", __FUNCTION__, name);
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
    _D("initiating properties","");

    INDI::CCD::initProperties();

    addDebugControl();

    {
        char const TAB[] = "Main Control";
        IUFillText(&version.textFirmware,"MGEN_FIRMWARE_VERSION","Firmware version","n/a");
        IUFillTextVector(&version.propVersions, &version.textFirmware, 1, getDeviceName(), "Versions", "Versions", TAB, IP_RO, 60, IPS_IDLE);
    }

    {
        char const TAB[] = "Voltages";
        IUFillNumber(&voltage.numVoltages[0], "MGEN_LOGIC_VOLTAGE", "Logic [4.8V, 5.1V]", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumber(&voltage.numVoltages[1], "MGEN_INPUT_VOLTAGE", "Input [9.0V, 15.0V]", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumber(&voltage.numVoltages[2], "MGEN_REFERENCE_VOLTAGE", "Reference [1.1V, 1.3V]", "%+02.2f V", 0, 220, 0, 0);
        IUFillNumberVector(&voltage.propVoltages, voltage.numVoltages, sizeof(voltage.numVoltages)/sizeof(voltage.numVoltages[0]), getDeviceName(), "MGEN_VOLTAGES", "Voltages", TAB, IP_RO, 60, IPS_IDLE);
    }

    {
        char const TAB[] = "Remote UI";
        IUFillNumber(&ui.framerate.number, "MGEN_UI_FRAMERATE", "Frame rate", "%+02.2f fps", 0, 4, 1, 1); /* Warning: frame rate kills connection quickly */
        IUFillNumberVector(&ui.framerate.property, &ui.framerate.number, 1, getDeviceName(), "MGEN_UI_OPTIONS", "UI", TAB, IP_RW, 60, IPS_IDLE);

        IUFillSwitch(&ui.buttons.switches[0], "MGEN_UI_BUTTON_ESC", "ESC", ISS_OFF);
        ui.buttons.switches[0].aux = (void*) MGIO_INSERT_BUTTON::IOB_ESC;
        IUFillSwitch(&ui.buttons.switches[1], "MGEN_UI_BUTTON_SET", "SET", ISS_OFF);
        ui.buttons.switches[1].aux = (void*) MGIO_INSERT_BUTTON::IOB_SET;
        IUFillSwitch(&ui.buttons.switches[2], "MGEN_UI_BUTTON_UP", "UP", ISS_OFF);
        ui.buttons.switches[2].aux = (void*) MGIO_INSERT_BUTTON::IOB_UP;
        IUFillSwitch(&ui.buttons.switches[3], "MGEN_UI_BUTTON_LEFT", "LEFT", ISS_OFF);
        ui.buttons.switches[3].aux = (void*) MGIO_INSERT_BUTTON::IOB_LEFT;
        IUFillSwitch(&ui.buttons.switches[4], "MGEN_UI_BUTTON_RIGHT", "RIGHT", ISS_OFF);
        ui.buttons.switches[4].aux = (void*) MGIO_INSERT_BUTTON::IOB_RIGHT;
        IUFillSwitch(&ui.buttons.switches[5], "MGEN_UI_BUTTON_DOWN", "DOWN", ISS_OFF);
        ui.buttons.switches[5].aux = (void*) MGIO_INSERT_BUTTON::IOB_DOWN;
        /* ESC SET
         * UP LEFT RIGHT DOWN
         */
        IUFillSwitchVector(&ui.buttons.properties[0], &ui.buttons.switches[0], 2, getDeviceName(), "MGEN_UI_BUTTONS1", "UI Buttons", TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
        IUFillSwitchVector(&ui.buttons.properties[1], &ui.buttons.switches[2], 4, getDeviceName(), "MGEN_UI_BUTTONS2", "UI Buttons", TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
    }

    return true;
}

bool MGenAutoguider::updateProperties()
{
    INDI::CCD::updateProperties();

    if(isConnected())
    {
        _D("registering properties","");
        defineText(&version.propVersions);
        defineNumber(&voltage.propVoltages);
        defineNumber(&ui.framerate.property);
        defineSwitch(&ui.buttons.properties[0]);
        defineSwitch(&ui.buttons.properties[1]);
    }
    else
    {
        _D("removing properties","");
        deleteProperty(version.propVersions.name);
        deleteProperty(voltage.propVoltages.name);
        deleteProperty(ui.framerate.property.name);
        deleteProperty(ui.buttons.properties[0].name);
        deleteProperty(ui.buttons.properties[1].name);
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
        _S("ignoring connection request received while connecting or already connected","");
        return true;
    }

    _D("initiating connection.","");

    if(device)
        delete device;
    device = new MGenDevice();

    if(!device->Connect(0x0403, 0x6001)) try
    {
        /* Loop until connection is considered stable */
        while(device->isConnected())
        {
            switch(getOpMode())
            {
                /* Unknown mode, try to connect in COMPATIBLE mode first */
                case OPM_UNKNOWN:
                    _D("running device identification","");

                    /* Run an identification - failing this is not a problem, we'll try to communicate in applicative mode next */
                    if(CR_SUCCESS == MGCP_QUERY_DEVICE().ask(*device))
                    {
                        _D("identified boot/compatible mode","");
                        device->setOpMode(OPM_COMPATIBLE);
                        continue;
                    }

                    _D("identification failed, try to communicate as if in applicative mode","");
                    if(device->setOpMode(OPM_APPLICATION))
                    {
                        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                        _E("failed reconfiguring device serial line","");
                        device->disable();
                        continue;
                    }

                    /* Run a basic exchange */
                    if(CR_SUCCESS != MGCMD_NOP1().ask(*device))
                    {
                        if(device->TurnPowerOn())
                        {
                            _E("failed heartbeat after turning device on","");
                            device->disable();
                            continue;
                        }
                    }

                    break;

                /*case OPM_BOOT:*/
                case OPM_COMPATIBLE:
                    _D("switching from compatible to normal mode","");

                    /* Switch to applicative mode */
                    MGCP_ENTER_NORMAL_MODE().ask(*device);

                    if(device->setOpMode(OPM_APPLICATION))
                    {
                        /* TODO: Not good, the device doesn't support our settings - out of spec, bail out */
                        _E("failed reconfiguring device serial line","");
                        device->disable();
                        continue;
                    }

                    _D("device is in now expected to be in applicative mode","");

                    if(CR_SUCCESS != MGCMD_NOP1().ask(*device))
                    {
                        device->disable();
                        continue;
                    }

                    break;

                case OPM_APPLICATION:
                    if(getHeartbeat())
                    {
                        _S("considering device connected","");
                        /* FIXME: currently no way to tell which timer hit */
                        //heartbeat.timer = SetTimer(5000);
                        //version.timer = SetTimer(20000);
                        //voltage.timer = SetTimer(10000);
                        ui.timer = SetTimer(0 < ui.framerate.number.value ? 1000 / (long)ui.framerate.number.value : 1000);
                        return device->isConnected();
                    }
                    else if(device->isConnected())
                    {
                        _D("waiting for heartbeat","");
                        sleep(1);
                    }
                    break;
            }
        }
    }
    catch(IOError &e)
    {
        _E("device disconnected (%s)", e.what());
        device->disable();
    }

    /* We expect to have failed connecting at this point */
    return device->isConnected();
}

/**************************************************************************************
** Client is asking us to terminate connection to the device
***************************************************************************************/
bool MGenAutoguider::Disconnect()
{
    if( device->isConnected() )
    {
        _D("initiating disconnection.","");
        //RemoveTimer(heartbeat.timer);
        //RemoveTimer(version.timer);
        //RemoveTimer(voltage.timer);
        RemoveTimer(ui.timer);
        device->disable();
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
void MGenAutoguider::TimerHit()
{
    if(device->isConnected()) try
    {
        struct timespec tm = {0};
        if(clock_gettime(CLOCK_MONOTONIC, &tm))
            return;

        /* If we didn't get the firmware version, ask */
        if( !version.uploaded_firmware )
        {
            MGCMD_GET_FW_VERSION cmd;
            if(CR_SUCCESS == cmd.ask(*device))
            {
                version.uploaded_firmware = cmd.fw_version();
                sprintf(version.textFirmware.text,"%04X", version.uploaded_firmware);
                _D("received version 0x%04X", version.uploaded_firmware);
                IDSetText(&version.propVersions, NULL);
                /* FIXME: actually the timer for version will probably never fire */
                //RemoveTimer(version.timer);
            }
            else _E("failed retrieving firmware version","");

            version.timestamp = tm;
        }

        /* Heartbeat */
        if(heartbeat.timestamp.tv_sec + 5 < tm.tv_sec)
        {
            getHeartbeat();
            heartbeat.timestamp = tm;
        }

        /* Update ADC values */
        if(voltage.timestamp.tv_sec + 20 < tm.tv_sec)
        {
            MGCMD_READ_ADCS adcs;

            if(CR_SUCCESS == adcs.ask(*device))
            {
                voltage.numVoltages[0].value = voltage.logic = adcs.logic_voltage();
                _D("received logic voltage %fV (spec is between 4.8V and 5.1V)", voltage.logic);
                voltage.numVoltages[1].value = voltage.input = adcs.input_voltage();
                _D("received input voltage %fV (spec is between 9V and 15V)", voltage.input);
                voltage.numVoltages[2].value = voltage.reference = adcs.refer_voltage();
                _D("received reference voltage %fV (spec is around 1.23V)", voltage.reference);

                /* FIXME: my device has input at 15.07... */
                if(4.8f <= voltage.logic && voltage.logic <= 5.1f)
                    if(9.0f <= voltage.input && voltage.input <= 15.0f)
                        if(1.1 <= voltage.reference && voltage.reference <= 1.3)
                            voltage.propVoltages.s = IPS_OK;
                        else voltage.propVoltages.s = IPS_ALERT;
                    else voltage.propVoltages.s = IPS_ALERT;
                else voltage.propVoltages.s = IPS_ALERT;

                IDSetNumber(&voltage.propVoltages, NULL);
            }
            else _E("failed retrieving voltages","");

            voltage.timestamp = tm;
        }

        /* Update UI frame */
        if(0 < ui.framerate.number.value && (ui.timestamp.tv_sec < tm.tv_sec || ui.timestamp.tv_nsec + 1000000000/(long)ui.framerate.number.value < tm.tv_nsec))
        {
            MGIO_READ_DISPLAY_FRAME read_frame;

            if(CR_SUCCESS == read_frame.ask(*device))
            {
                MGIO_READ_DISPLAY_FRAME::ByteFrame frame;
                read_frame.get_frame(frame);
                memcpy(PrimaryCCD.getFrameBuffer(),frame.data(),frame.size());
                ExposureComplete(&PrimaryCCD);
            }
            else _E("failed reading remote UI frame","");

            ui.timestamp = tm;
        }

        /* Send buttons */
        /*
        if(!device->button_queue.empty())
        {
            MGIO_INSERT_BUTTON((MGIO_INSERT_BUTTON::Button) device->popButton()).ask(*device);
        }
        */

        ui.timer = SetTimer(0 < ui.framerate.number.value ? 1000 / (long)ui.framerate.number.value : 1000);
    }
    catch(IOError &e)
    {
        _S("device disconnected (%s)", e.what());
        device->disable();
        /* FIXME: tell client we disconnected */
    }
}

/**************************************************************************************
 * Helpers
 **************************************************************************************/

bool MGenAutoguider::getHeartbeat()
{
    if(CR_SUCCESS != MGCMD_NOP1().ask(*device))
    {
        heartbeat.no_ack_count++;
        _E("%d times no ack to heartbeat (NOP1 command)", heartbeat.no_ack_count);
        if(5 < heartbeat.no_ack_count)
        {
            device->disable();
            /* FIXME: tell client we disconnected */
        }
        return false;
    }
    else heartbeat.no_ack_count = 0;

    return true;
}
