#if 0
    Induino general propose driver. Allow using arduino boards
    as general I/O 	
    Copyright 2012 (c) Nacho Mas (mas.ignacio at gmail.com)


    Base on Tutorial Four
    Demonstration of libindi v0.7 capabilities.

    Copyright (C) 2010 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

#include "indiduino.h"
#include "connectionplugins/connectionserial.h"

#include "config.h"
#include "firmata.h"

#include <indicontroller.h>

#include <memory>
#include <sys/stat.h>

/* Our indiduino auto pointer */
std::unique_ptr<indiduino> indiduino_prt(new indiduino());

const char *indiduino_id = "indiduino";

/**************************************************************************************
** Send client definitions of all properties.
***************************************************************************************/
/**************************************************************************************
**
***************************************************************************************/
void ISGetProperties(const char *dev)
{
    indiduino_prt->ISGetProperties(dev);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    indiduino_prt->ISNewSwitch(dev, name, states, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    indiduino_prt->ISNewText(dev, name, texts, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    indiduino_prt->ISNewNumber(dev, name, values, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    indiduino_prt->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

/**************************************************************************************
**
***************************************************************************************/
void ISSnoopDevice(XMLEle *root)
{
    indiduino_prt->ISSnoopDevice(root);
}

/**************************************************************************************
** Initialize firmata debug
***************************************************************************************/

void firmata_debug(const char *file, int line, const char *message, ...)
{
    va_list ap;
    char msg[257];
    msg[256] = '\0';
    va_start(ap, message);
    vsnprintf(msg, 257, message, ap);
    va_end(ap);

    INDI::Logger::getInstance().print(indiduino_prt->getDeviceName(), INDI::Logger::DBG_DEBUG, file, line, msg);
}


/**************************************************************************************
**
***************************************************************************************/
indiduino::indiduino()
{
    LOG_DEBUG("Indiduino driver start...");
    firmata_debug_cb = firmata_debug;
    setVersion(DUINO_VERSION_MAJOR, DUINO_VERSION_MINOR);
    controller = new INDI::Controller(this);
    controller->setJoystickCallback(joystickHelper);
    controller->setButtonCallback(buttonHelper);
    controller->setAxisCallback(axisHelper);
}

/**************************************************************************************
**
***************************************************************************************/
indiduino::~indiduino()
{
    delete (controller);
}

bool indiduino::ISSnoopDevice(XMLEle *root)
{
    controller->ISSnoopDevice(root);
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

void indiduino::TimerHit()
{
    if (isConnected() == false)
        return;

    sf->OnIdle();

    std::vector<INDI::Property *> *pAll = getProperties();

    for (unsigned int i = 0; i < pAll->size(); i++)
    {
        const char *name = pAll->at(i)->getName();
        INDI_PROPERTY_TYPE type = pAll->at(i)->getType();

        //DIGITAL INPUT
        if (type == INDI_LIGHT)
        {
            ILightVectorProperty *lvp = getLight(name);
            if (lvp->aux != (void *)indiduino_id)
                continue;

            for (int i = 0; i < lvp->nlp; i++)
            {
                ILight *lqp = &lvp->lp[i];
                if (!lqp)
                    return;

                IO *pin_config = (IO *)lqp->aux;
                if (pin_config == nullptr)
                    continue;
                if (pin_config->IOType == DI)
                {
                    int pin = pin_config->pin;
                    if (sf->pin_info[pin].mode == FIRMATA_MODE_INPUT)
                    {
                        if (sf->pin_info[pin].value == 1)
                        {
                            //LOGF_DEBUG("%s.%s on pin %u change to  ON",lvp->name,lqp->name,pin);
                            //IDSetLight (lvp, "%s.%s change to ON\n",lvp->name,lqp->name);
                            lqp->s = IPS_OK;
                        }
                        else
                        {
                            //LOGF_DEBUG("%s.%s on pin %u change to  OFF",lvp->name,lqp->name,pin);
                            //IDSetLight (lvp, "%s.%s change to OFF\n",lvp->name,lqp->name);
                            lqp->s = IPS_IDLE;
                        }
                    }
                }
            }
            IDSetLight(lvp, nullptr);
        }

        //read back DIGITAL OUTPUT values as reported by the board (FIRMATA_PIN_STATE_RESPONSE)
        if (type == INDI_SWITCH)
        {
            int n_on = 0;
            ISwitchVectorProperty *svp = getSwitch(name);

            if (svp->aux != (void *)indiduino_id)
                continue;

            for (int i = 0; i < svp->nsp; i++)
            {
                ISwitch *sqp = &svp->sp[i];
                if (!sqp)
                    return;

                IO *pin_config = (IO *)sqp->aux;
                if (pin_config == nullptr)
                    continue;
                if (pin_config->IOType == DO)
                {
                    int pin = pin_config->pin;
                    if (sf->pin_info[pin].mode == FIRMATA_MODE_OUTPUT)
                    {
                        if (sf->pin_info[pin].value == 1)
                        {
                            sqp->s = ISS_ON;
                            n_on++;
                        }
                        else
                        {
                            sqp->s = ISS_OFF;
                        }
                    }
                }
            }
            if (svp->r == ISR_1OFMANY) // make sure that 1 switch is on
            {
                for (int i = 0; i < svp->nsp; i++)
                {
                    ISwitch *sqp = &svp->sp[i];

                    if ((IO *)sqp->aux != nullptr)
                       continue;
                    if (n_on > 0)
                    {
                        sqp->s = ISS_OFF;
                    }
                    else
                    {
                        sqp->s = ISS_ON;
                        n_on++;
                    }
                }
            }

            IDSetSwitch(svp, nullptr);
        }

        //ANALOG
        if (type == INDI_NUMBER)
        {
            INumberVectorProperty *nvp = getNumber(name);

            if (nvp->aux != (void *)indiduino_id)
                continue;

            for (int i = 0; i < nvp->nnp; i++)
            {
                INumber *eqp = &nvp->np[i];
                if (!eqp)
                    return;

                IO *pin_config = (IO *)eqp->aux0;
                if (pin_config == nullptr)
                    continue;

                if (pin_config->IOType == AI)
                {
                    int pin = pin_config->pin;
                    if (sf->pin_info[pin].mode == FIRMATA_MODE_ANALOG)
                    {
                        eqp->value = pin_config->MulScale * (double)(sf->pin_info[pin].value) + pin_config->AddScale;
                        //LOGF_DEBUG("%f",eqp->value);
                        IDSetNumber(nvp, nullptr);
                    }
                }
                if (pin_config->IOType == AO) // read back ANALOG OUTPUT values as reported by the board (FIRMATA_PIN_STATE_RESPONSE)
                {
                    int pin = pin_config->pin;
                    if (sf->pin_info[pin].mode == FIRMATA_MODE_PWM)
                    {
                        eqp->value = ((double)(sf->pin_info[pin].value) - pin_config->AddScale) / pin_config->MulScale;
                        //LOGF_DEBUG("%f",eqp->value);
                        IDSetNumber(nvp, nullptr);
                    }
                }
            }
        }

        //TEXT
        if (type == INDI_TEXT)
        {
            ITextVectorProperty *tvp = getText(name);
            if (tvp->aux != (void *)indiduino_id)
                continue;

            for (int i=0;i<tvp->ntp;i++) {
                IText *eqp = &tvp->tp[i];
                if (!eqp)
                    return;

                if (eqp->aux0 == nullptr) continue;
                strcpy(eqp->text,(char*)eqp->aux0);
                //LOGF_DEBUG("%s.%s TEXT: %s ",tvp->name,eqp->name,eqp->text);
                IDSetText(tvp, nullptr);
            }
        }
    }

    time_t sec_since_reply = sf->secondsSinceVersionReply();
    if (sec_since_reply > 30)
    {
        LOG_ERROR("No reply from the device for 30 sec, disconnecting");
        setConnected(false, IPS_ALERT);
        Disconnect();
        return;
    }
    if (sec_since_reply > 10)
    {
        LOG_DEBUG("Sending keepalive message");
        sf->askFirmwareVersion();
    }

    SetTimer(POLLMS);
}

/**************************************************************************************
** Initialize all properties & set default values.
**************************************************************************************/
bool indiduino::initProperties()
{
    // This is the default driver skeleton file location
    // Convention is: drivername_sk_xml
    // Default location is /usr/share/indi

    struct stat st;

    strcpy(skelFileName, DEFAULT_SKELETON_FILE);

    char *skel = getenv("INDISKEL");
    if (skel)
    {
        LOGF_INFO("Building from %s skeleton", skel);
        strcpy(skelFileName, skel);
        buildSkeleton(skel);
    }
    else if (stat(skelFileName, &st) == 0)
    {
        LOGF_INFO("Building from %s skeleton", skelFileName);
        buildSkeleton(skelFileName);
    }
    else
    {
        LOG_WARN(
            "No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.");
    }
    // Optional: Add aux controls for configuration, debug & simulation that get added in the Options tab
    //           of the driver.
    //addAuxControls();

    controller->initProperties();

    DefaultDevice::initProperties();

    setDefaultPollingPeriod(500);

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake(); });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_57600);
    // Arduino default port
    serialConnection->setDefaultPort("/dev/ttyACM0");
    registerConnection(serialConnection);

    addDebugControl();
    return true;
}

bool indiduino::Handshake()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        return true;
    }

    sf = new Firmata(serialConnection->getPortFD());
    if (!sf->portOpen) {
        delete sf;
        sf = NULL;
        return false;
    }

    return true;
}

bool indiduino::updateProperties()
{
    if (isConnected())
    {
        // Mapping the controller according to the properties previously read from the XML file
        // We only map controls for pin of type AO and SERVO
        for (int numiopin = 0; numiopin < MAX_IO_PIN; numiopin++)
        {
            if (iopin[numiopin].IOType == SERVO)
            {
                if (iopin[numiopin].SwitchButton)
                {
                    char buffer[33];
                    sprintf(buffer, "%d", numiopin);
                    controller->mapController(buffer, iopin[numiopin].defVectorName,
                                              INDI::Controller::CONTROLLER_BUTTON, iopin[numiopin].SwitchButton);
                }
            }
            else if (iopin[numiopin].IOType == AO)
            {
                if (iopin[numiopin].UpButton && iopin[numiopin].DownButton)
                {
                    char upBuffer[33];
                    sprintf(upBuffer, "%d", numiopin);
                    // To distinguish the up button from the down button, we add MAX_IO_PIN to the message
                    char downBuffer[33];
                    sprintf(downBuffer, "%d", numiopin + MAX_IO_PIN);
                    controller->mapController(upBuffer, iopin[numiopin].defVectorName,
                                              INDI::Controller::CONTROLLER_BUTTON, iopin[numiopin].UpButton);
                    controller->mapController(downBuffer, iopin[numiopin].defVectorName,
                                              INDI::Controller::CONTROLLER_BUTTON, iopin[numiopin].DownButton);
                }
            }
        }
    }
    controller->updateProperties();
    return true;
}

/**************************************************************************************
** Define Basic properties to clients.
***************************************************************************************/
void indiduino::ISGetProperties(const char *dev)
{
    static int configLoaded = 0;

    // Ask the default driver first to send properties.
    INDI::DefaultDevice::ISGetProperties(dev);

    configLoaded = 1;

    controller->ISGetProperties(dev);
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool indiduino::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // Ignore if not ours
    if (strcmp(dev, getDeviceName()))
        return false;

    ITextVectorProperty *tProp = getText(name);
    // Device Port Text
    if (!strcmp(tProp->name, "DEVICE_PORT"))
    {
        if (IUUpdateText(tProp, texts, names, n) < 0)
            return false;

        tProp->s = IPS_OK;
        tProp->s = IPS_IDLE;
        IDSetText(tProp, "Port updated.");

        return true;
    }

    controller->ISNewText(dev, name, texts, names, n);

    return false;
}

/**************************************************************************************
**
***************************************************************************************/
bool indiduino::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Ignore if not ours
    if (strcmp(dev, getDeviceName()))
        return false;

    INumberVectorProperty *nvp = getNumber(name);
    if (!nvp)
        return false;

    if (isConnected() == false)
    {
        nvp->s = IPS_ALERT;
        IDSetNumber(nvp, "Cannot change property while device is disconnected.");
        return false;
    }

    bool change = false;
    for (int i = 0; i < n; i++)
    {
        INumber *eqp = IUFindNumber(nvp, names[i]);

        if (!eqp)
            return false;

        IO *pin_config = (IO *)eqp->aux0;
        if (pin_config == nullptr)
            continue;
        if ((pin_config->IOType == AO) || (pin_config->IOType == SERVO))
        {
            int pin = pin_config->pin;
            IUUpdateNumber(nvp, values, names, n);
            LOGF_DEBUG("Setting output %s.%s on pin %u to %f", nvp->name, eqp->name, pin, eqp->value);
            sf->setPwmPin(pin, (int)(pin_config->MulScale * eqp->value + pin_config->AddScale));
            IDSetNumber(nvp, "%s.%s change to %f", nvp->name, eqp->name, eqp->value);
            nvp->s = IPS_IDLE;
            change = true;
        }
        if (pin_config->IOType == AI)
        {
            IUUpdateNumber(nvp, values, names, n);
            nvp->s = IPS_IDLE;
            change = true;
        }
    }

    if (change)
    {
        IDSetNumber(nvp, nullptr);
        return true;
    }
    else
    {
        return false;
    }
}

/**************************************************************************************
**
***************************************************************************************/
bool indiduino::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    for (int i = 0; i < n; i++)
    {
        if (states[i] == ISS_ON)
            LOGF_DEBUG("State %d is on", i);
        else if (states[i] == ISS_OFF)
            LOGF_DEBUG("State %d is off", i);
    }
    // ignore if not ours //
    if (strcmp(dev, getDeviceName()))
        return false;

    if (INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n) == true)
        return true;

    ISwitchVectorProperty *svp = getSwitch(name);

    if (isConnected() == false)
    {
        svp->s = IPS_ALERT;
        IDSetSwitch(svp, "Cannot change property while device is disconnected.");
        return false;
    }

    if (!svp)
        return false;

    for (int i = 0; i < svp->nsp; i++)
    {
        ISwitch *sqp   = &svp->sp[i];
        IO *pin_config = (IO *)sqp->aux;
        if (pin_config == nullptr)
            continue;
        if (pin_config->IOType == DO)
        {
            int pin = pin_config->pin;
            IUUpdateSwitch(svp, states, names, n);
            if (sqp->s == ISS_ON)
            {
                LOGF_DEBUG("Switching ON %s.%s on pin %u", svp->name, sqp->name, pin);
                sf->writeDigitalPin(pin, ARDUINO_HIGH);
                IDSetSwitch(svp, "%s.%s ON", svp->name, sqp->name);
            }
            else
            {
                LOGF_DEBUG("Switching OFF %s.%s on pin %u", svp->name, sqp->name, pin);
                sf->writeDigitalPin(pin, ARDUINO_LOW);
                IDSetSwitch(svp, "%s.%s OFF", svp->name, sqp->name);
            }
        }
        else if (pin_config->IOType == SERVO)
        {
            int pin = pin_config->pin;
            IUUpdateSwitch(svp, states, names, n);
            if (sqp->s == ISS_ON)
            {
                LOGF_DEBUG("Switching ON %s.%s on pin %u", svp->name, sqp->name, pin);
                sf->setPwmPin(pin, (int)pin_config->OnAngle);
                IDSetSwitch(svp, "%s.%s ON", svp->name, sqp->name);
            }
            else
            {
                LOGF_DEBUG("Switching OFF %s.%s on pin %u", svp->name, sqp->name, pin);
                sf->setPwmPin(pin, (int)pin_config->OffAngle);
                IDSetSwitch(svp, "%s.%s OFF", svp->name, sqp->name);
            }
        }
    }
    controller->ISNewSwitch(dev, name, states, names, n);

    IUUpdateSwitch(svp, states, names, n);
    return true;
}

bool indiduino::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                          char *formats[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()))
        return false;

    IBLOBVectorProperty *bvp = getBLOB(name);

    if (!bvp)
        return false;

    if (isConnected() == false)
    {
        bvp->s = IPS_ALERT;
        IDSetBLOB(bvp, "Cannot change property while device is disconnected.");
        return false;
    }

    if (!strcmp(bvp->name, "BLOB Test"))
    {
        IUUpdateBLOB(bvp, sizes, blobsizes, blobs, formats, names, n);

        IBLOB *bp = IUFindBLOB(bvp, names[0]);

        if (!bp)
            return false;

        LOGF_DEBUG("Recieved BLOB with name %s, format %s, and size %d, and bloblen %d", bp->name, bp->format, bp->size,
              bp->bloblen);

        char *blobBuffer = new char[bp->bloblen + 1];
        strncpy(blobBuffer, ((char *)bp->blob), bp->bloblen);
        blobBuffer[bp->bloblen] = '\0';

        LOGF_DEBUG("BLOB Content:\n##################################\n%s\n##################################",
              blobBuffer);

        delete[] blobBuffer;
    }

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool indiduino::Connect()
{
    //This way it tries to connect using the Serial connection method with autosearch capability.
    this->serialConnection->Connect();

    if (sf)
    {
        if (sf->initState() != 0)
        {
            LOG_ERROR("Failed to get Erduino state");
            IDSetSwitch(getSwitch("CONNECTION"), "Fail to get Arduino state");
            delete sf;
            this->serialConnection->Disconnect();
            return false;
        }
        LOG_INFO("Arduino board connected.");
        LOGF_INFO("FIRMATA version:%s", sf->firmata_name);
        IDSetSwitch(getSwitch("CONNECTION"), "CONNECTED. FIRMATA version:%s", sf->firmata_name);
        if (!setPinModesFromSKEL())
        {
            LOG_ERROR("Failed to map Arduino pins, check skeleton file syntax.");
            IDSetSwitch(getSwitch("CONNECTION"), "Failed to map Arduino pins, check skeleton file syntax.");
            delete sf;
            this->serialConnection->Disconnect();
            return false;
        }
        else
        {
            SetTimer(POLLMS);
            return true;
        }
    }
    return false;
}

bool indiduino::Disconnect()
{
    delete sf;
    this->serialConnection->Disconnect();
    LOG_INFO("Arduino board disconnected.");
    IDSetSwitch(getSwitch("CONNECTION"), "DISCONNECTED");
    return true;
}

const char *indiduino::getDefaultName()
{
    return "Arduino";
}

bool indiduino::setPinModesFromSKEL()
{
    char errmsg[MAXRBUF];
    FILE *fp       = nullptr;
    LilXML *lp     = newLilXML();
    XMLEle *fproot = nullptr;
    XMLEle *ep = nullptr, *ioep = nullptr, *xmlp = nullptr;
    int numiopin = 0;

    fp = fopen(skelFileName, "r");

    if (fp == nullptr)
    {
        LOGF_ERROR("Unable to build skeleton. Error loading file %s: %s", skelFileName, strerror(errno));
        return false;
    }

    fproot = readXMLFile(fp, lp, errmsg);

    if (fproot == nullptr)
    {
        LOGF_ERROR("Unable to parse skeleton XML: %s", errmsg);
        return false;
    }

    LOG_INFO("Setting pins behaviour from <indiduino> tags");
    std::vector<INDI::Property *> *pAll = getProperties();

    for (unsigned int i = 0; i < pAll->size(); i++)
    {
        const char *name = pAll->at(i)->getName();
        INDI_PROPERTY_TYPE type = pAll->at(i)->getType();

        if (ep == nullptr)
        {
            ep = nextXMLEle(fproot, 1);
        }
        else
        {
            ep = nextXMLEle(fproot, 0);
        }
        if (ep == nullptr)
        {
            break;
        }
        ioep = nullptr;
        if (type == INDI_SWITCH)
        {
            ISwitchVectorProperty *svp = getSwitch(name);
            ioep = nullptr;
            for (int i = 0; i < svp->nsp; i++)
            {
                ISwitch *sqp = &svp->sp[i];
                if (ioep == nullptr)
                {
                    ioep = nextXMLEle(ep, 1);
                }
                else
                {
                    ioep = nextXMLEle(ep, 0);
                }
                xmlp = findXMLEle(ioep, "indiduino");
                if (xmlp != nullptr)
                {
                    if (!readInduinoXml(xmlp, numiopin))
                    {
                        LOG_ERROR("Malforme <indiduino> XML");
                        return false;
                    }
                    svp->aux                      = (void *)indiduino_id;
                    sqp->aux                      = (void *)&iopin[numiopin];
                    iopin[numiopin].defVectorName = svp->name;
                    iopin[numiopin].defName       = sqp->name;
                    int pin                       = iopin[numiopin].pin;
                    if (iopin[numiopin].IOType == DO)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as DIGITAL OUTPUT", svp->name, sqp->name, pin);
                        sf->setPinMode(pin, FIRMATA_MODE_OUTPUT);
                    }
                    else if (iopin[numiopin].IOType == SERVO)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as SERVO", svp->name, sqp->name, pin);
                        sf->setPinMode(pin, FIRMATA_MODE_SERVO);
                        // Setting Servo pin to default startup angle
                        sf->setPwmPin(
                            pin, (int)(iopin[numiopin].MulScale * iopin[numiopin].OnAngle + iopin[numiopin].AddScale));
                    }
                    LOGF_DEBUG("numiopin:%u", numiopin);
                    numiopin++;
                }
            }
        }
        if (type == INDI_TEXT)
        {
            ITextVectorProperty *tvp = getText(name);

            ioep = nullptr;
            for (int i = 0; i < tvp->ntp; i++)
            {
                IText *tqp = &tvp->tp[i];

                if (ioep == nullptr)
                {
                    ioep = nextXMLEle(ep, 1);
                }
                else
                {
                    ioep = nextXMLEle(ep, 0);
                }
                xmlp = findXMLEle(ioep, "indiduino");
                if (xmlp != nullptr)
                {
                    if (!readInduinoXml(xmlp, 0))
                    {
                        LOG_ERROR("Malforme <indiduino> XML");
                        return false;
                    }
                    tvp->aux                      = (void *)indiduino_id;
                    tqp->aux0                     = (void *)&sf->string_buffer;
                    iopin[numiopin].defVectorName = tvp->name;
                    iopin[numiopin].defName       = tqp->name;
                    LOGF_DEBUG("%s.%s ARDUINO TEXT", tvp->name, tqp->name);
                    LOGF_DEBUG("numiopin:%u", numiopin);
                }
            }
        }
        if (type == INDI_LIGHT)
        {
            ILightVectorProperty *lvp = getLight(name);

            ioep = nullptr;
            for (int i = 0; i < lvp->nlp; i++)
            {
                ILight *lqp = &lvp->lp[i];
                if (ioep == nullptr)
                {
                    ioep = nextXMLEle(ep, 1);
                }
                else
                {
                    ioep = nextXMLEle(ep, 0);
                }
                xmlp = findXMLEle(ioep, "indiduino");
                if (xmlp != nullptr)
                {
                    if (!readInduinoXml(xmlp, numiopin))
                    {
                        LOG_ERROR("Malforme <indiduino> XML");
                        return false;
                    }
                    lvp->aux                      = (void *)indiduino_id;
                    lqp->aux                      = (void *)&iopin[numiopin];
                    iopin[numiopin].defVectorName = lvp->name;
                    iopin[numiopin].defName       = lqp->name;
                    int pin                       = iopin[numiopin].pin;
                    LOGF_DEBUG("%s.%s  pin %u set as DIGITAL INPUT", lvp->name, lqp->name, pin);
                    sf->setPinMode(pin, FIRMATA_MODE_INPUT);
                    LOGF_DEBUG("numiopin:%u", numiopin);
                    numiopin++;
                }
            }
        }
        if (type == INDI_NUMBER)
        {
            INumberVectorProperty *nvp = getNumber(name);

            ioep = nullptr;
            for (int i = 0; i < nvp->nnp; i++)
            {
                INumber *eqp = &nvp->np[i];
                if (ioep == nullptr)
                {
                    ioep = nextXMLEle(ep, 1);
                }
                else
                {
                    ioep = nextXMLEle(ep, 0);
                }
                xmlp = findXMLEle(ioep, "indiduino");
                if (xmlp != nullptr)
                {
                    if (!readInduinoXml(xmlp, numiopin))
                    {
                        LOG_ERROR("Malforme <indiduino> XML");
                        return false;
                    }
                    nvp->aux                      = (void *)indiduino_id;
                    eqp->aux0                     = (void *)&iopin[numiopin];
                    iopin[numiopin].defVectorName = nvp->name;
                    iopin[numiopin].defName       = eqp->name;
                    int pin                       = iopin[numiopin].pin;
                    if (iopin[numiopin].IOType == AO)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as ANALOG OUTPUT", nvp->name, eqp->name, pin);
                        sf->setPinMode(pin, FIRMATA_MODE_PWM);
                    }
                    else if (iopin[numiopin].IOType == AI)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as ANALOG INPUT", nvp->name, eqp->name, pin);
                        sf->setPinMode(pin, FIRMATA_MODE_ANALOG);
                    }
                    else if (iopin[numiopin].IOType == SERVO)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as SERVO", nvp->name, eqp->name, pin);
                        sf->setPinMode(pin, FIRMATA_MODE_SERVO);
                    }
                    LOGF_DEBUG("numiopin:%u", numiopin);
                    numiopin++;
                }
            }
        }
    }
    sf->setSamplingInterval(POLLMS / 2);
    sf->reportAnalogPorts(1);
    sf->reportDigitalPorts(1);
    return true;
}

bool indiduino::readInduinoXml(XMLEle *ioep, int npin)
{
    char *propertyTag;
    int pin;

    if (ioep == nullptr)
        return false;

    propertyTag = tagXMLEle(parentXMLEle(ioep));

    if (!strcmp(propertyTag, "defSwitch") || !strcmp(propertyTag, "defLight") || !strcmp(propertyTag, "defNumber"))
    {
        pin = atoi(findXMLAttValu(ioep, "pin"));

        if (pin >= 1 && pin <= 40)
        { //19 hardware pins.
            iopin[npin].pin = pin;
        }
        else
        {
            LOG_ERROR("induino: pin number is required. Check pin attrib value (1-19)");
            return false;
        }

        if (!strcmp(propertyTag, "defSwitch"))
        {
            if (!strcmp(findXMLAttValu(ioep, "type"), "servo"))
            {
                iopin[npin].IOType = SERVO;
                if (strcmp(findXMLAttValu(ioep, "onangle"), ""))
                {
                    iopin[npin].OnAngle = atof(findXMLAttValu(ioep, "onangle"));
                }
                else
                {
                    iopin[npin].OnAngle = 150;
                }
                if (strcmp(findXMLAttValu(ioep, "offangle"), ""))
                {
                    iopin[npin].OffAngle = atof(findXMLAttValu(ioep, "offangle"));
                }
                else
                {
                    iopin[npin].OffAngle = 10;
                }
                if (strcmp(findXMLAttValu(ioep, "button"), ""))
                {
                    iopin[npin].SwitchButton = const_cast<char *>(findXMLAttValu(ioep, "button"));
                    LOGF_DEBUG("found button %s", iopin[npin].SwitchButton);
                }
            }
            else
            {
                iopin[npin].IOType = DO;
            }
        }

        if (!strcmp(propertyTag, "defLight"))
        {
            iopin[npin].IOType = DI;
        }

        if (!strcmp(propertyTag, "defNumber"))
        {
            if (strcmp(findXMLAttValu(ioep, "mul"), ""))
            {
                iopin[npin].MulScale = atof(findXMLAttValu(ioep, "mul"));
            }
            else
            {
                iopin[npin].MulScale = 1;
            }
            if (strcmp(findXMLAttValu(ioep, "add"), ""))
            {
                iopin[npin].AddScale = atof(findXMLAttValu(ioep, "add"));
            }
            else
            {
                iopin[npin].AddScale = 0;
            }
            if (!strcmp(findXMLAttValu(ioep, "type"), "output"))
            {
                iopin[npin].IOType = AO;
            }
            else if (!strcmp(findXMLAttValu(ioep, "type"), "input"))
            {
                iopin[npin].IOType = AI;
            }
            else if (!strcmp(findXMLAttValu(ioep, "type"), "servo"))
            {
                iopin[npin].IOType = SERVO;
            }
            else
            {
                LOG_ERROR("induino: Setting type (input or output) is required for analogs");
                return false;
            }
            if (strcmp(findXMLAttValu(ioep, "downbutton"), ""))
            {
                iopin[npin].DownButton = const_cast<char *>(findXMLAttValu(ioep, "downbutton"));
            }
            if (strcmp(findXMLAttValu(ioep, "upbutton"), ""))
            {
                iopin[npin].UpButton = const_cast<char *>(findXMLAttValu(ioep, "upbutton"));
            }
            if (strcmp(findXMLAttValu(ioep, "buttonincvalue"), ""))
            {
                iopin[npin].buttonIncValue = atof(findXMLAttValu(ioep, "buttonincvalue"));
            }
            else
            {
                iopin[npin].buttonIncValue = 50;
            }
        }

        if (false)
        {
            LOGF_DEBUG("arduino IO Match:Property:(%s)", findXMLAttValu(parentXMLEle(ioep), "name"));
            LOGF_DEBUG("arduino IO pin:(%u)", iopin[npin].pin);
            LOGF_DEBUG("arduino IO type:(%u)", iopin[npin].IOType);
            LOGF_DEBUG("arduino IO scale: mul:%g add:%g", iopin[npin].MulScale, iopin[npin].AddScale);
        }
    }
    return true;
}

void indiduino::joystickHelper(const char *joystick_n, double mag, double angle, void *context)
{
    static_cast<indiduino *>(context)->processJoystick(joystick_n, mag, angle);
}

void indiduino::buttonHelper(const char *button_n, ISState state, void *context)
{
    static_cast<indiduino *>(context)->processButton(button_n, state);
}

void indiduino::axisHelper(const char *axis_n, double value, void *context)
{
    static_cast<indiduino *>(context)->processAxis(axis_n, value);
}

void indiduino::processAxis(const char *axis_n, double value)
{
    INDI_UNUSED(axis_n);
    INDI_UNUSED(value);
    // TO BE IMPLEMENTED
}

void indiduino::processJoystick(const char *joystick_n, double mag, double angle)
{
    INDI_UNUSED(joystick_n);
    INDI_UNUSED(mag);
    INDI_UNUSED(angle);
    // TO BE IMPLEMENTED
}

void indiduino::processButton(const char *button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    int numiopin;
    numiopin = atoi(button_n);

    bool isDownAO = false;
    // Recognizing a shifted numiopin means it is a button to decrease the value of a AO
    if (numiopin >= MAX_IO_PIN)
    {
        isDownAO = true;
        numiopin = numiopin - MAX_IO_PIN;
    }

    if (iopin[numiopin].IOType == AO)
    {
        char *names[1]             = { iopin[numiopin].defName };
        char *name                 = iopin[numiopin].defVectorName;
        INumberVectorProperty *nvp = getNumber(name);
        INumber *eqp               = IUFindNumber(nvp, names[0]);
        if (isDownAO)
        {
            double values[1] = { eqp->value - iopin[numiopin].buttonIncValue };
            ISNewNumber(getDeviceName(), name, values, names, 1);
        }
        else
        {
            double values[1] = { eqp->value + iopin[numiopin].buttonIncValue };
            ISNewNumber(getDeviceName(), name, values, names, 1);
        }
    }
    else if (iopin[numiopin].IOType == SERVO)
    {
        ISwitchVectorProperty *svp = getSwitch(iopin[numiopin].defVectorName);
        // Only considering the first switch, because the servo switches must be configured with only one switch
        ISwitch *sqp   = &svp->sp[0];
        char *names[1] = { iopin[numiopin].defName };
        if (sqp->s == ISS_ON)
        {
            ISState states[1] = { ISS_OFF };
            ISNewSwitch(getDeviceName(), iopin[numiopin].defVectorName, states, names, 1);
        }
        else
        {
            ISState states[1] = { ISS_ON };
            ISNewSwitch(getDeviceName(), iopin[numiopin].defVectorName, states, names, 1);
        }
    }
}
