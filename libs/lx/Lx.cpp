
#include "Lx.h"

#include "indicom.h"

#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>

// from indicom.cpp for tty_connect
#define PARITY_NONE 0
#define PARITY_EVEN 1
#define PARITY_ODD  2

void Lx::setCamerafd(int fd)
{
    camerafd = fd;
}

bool Lx::isEnabled()
{
    return (LxEnableSP[1].getState() == ISS_ON);
}

bool Lx::initProperties(INDI::DefaultDevice *device)
{
    //IDLog("Initializing Long Exposure Properties\n");
    dev         = device;
    device_name = dev->getDeviceName();
    LxEnableSP[0].fill("Disable", "", ISS_ON);
    LxEnableSP[1].fill("Enable", "", ISS_OFF);
    LxEnableSP.fill(device_name, "Activate", "", LX_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);
    LxModeSP[LXSERIAL].fill("Serial", "", ISS_ON);
    //LxModeSP[LXPARALLEL].fill("Parallel", "", ISS_OFF);
    LxModeSP[LXLED].fill("SPC900 LED", "", ISS_OFF);
    //LxModeSP[LXGPIO].fill("GPIO (Arm/RPI)", "", ISS_OFF);
    //  LxModeSP[4].fill("IndiDuino Switcher", "", ISS_OFF); // Snooping is not enough
    LxModeSP.fill(device_name, "LX Mode", "", LX_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);
    LxPortTP[0].fill("Port", "", "/dev/ttyUSB0");
    LxPortTP.fill(device_name, "Lx port", "", LX_TAB, IP_RW, 0, IPS_IDLE);
    LxSerialOptionSP[0].fill("Use DTR (pin 4)", "", ISS_OFF);
    LxSerialOptionSP[1].fill("Use RTS (pin 7)", "", ISS_ON);
    LxSerialOptionSP[2].fill("Use Serial command", "", ISS_OFF);
    LxSerialOptionSP.fill(device_name, "Serial Options", "",
                       LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    LxParallelOptionSP[0].fill("Use Data 0 (pin 2)", "", ISS_OFF);
    LxParallelOptionSP[1].fill("Use Data 1 (pin 3)", "", ISS_ON); // Steve's Chambers Schematics
    LxParallelOptionSP[2].fill("Use Data 2 (pin 4)", "", ISS_OFF);
    LxParallelOptionSP[3].fill("Use Data 3 (pin 5)", "", ISS_OFF);
    LxParallelOptionSP[4].fill("Use Data 4 (pin 6)", "", ISS_OFF);
    LxParallelOptionSP[5].fill("Use Data 5 (pin 7)", "", ISS_OFF);
    LxParallelOptionSP[6].fill("Use Data 6 (pin 8)", "", ISS_OFF);
    LxParallelOptionSP[7].fill("Use Data 7 (pin 9)", "", ISS_OFF);
    LxParallelOptionSP[8].fill("Use Parallel command", "", ISS_OFF);
    LxParallelOptionSP.fill(device_name,
                       "Parallel Options", "", LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    LxStartStopCmdTP[0].fill("Start command", "", ":O1");
    LxStartStopCmdTP[1].fill("Stop command", "", ":O0");
    LxStartStopCmdTP.fill(device_name, "Start/Stop commands",
                     "", LX_TAB, IP_RW, 0, IPS_IDLE);
    LxLogicalLevelSP[0].fill("Low to High", "", ISS_ON);
    LxLogicalLevelSP[1].fill("High to Low", "", ISS_OFF);
    LxLogicalLevelSP.fill(device_name, "Start Transition", "",
                       LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    LxSerialSpeedSP[0].fill("1200", "", ISS_OFF);
    LxSerialSpeedSP[1].fill("2400", "", ISS_OFF);
    LxSerialSpeedSP[2].fill("4800", "", ISS_OFF);
    LxSerialSpeedSP[3].fill("9600", "", ISS_ON);
    LxSerialSpeedSP[4].fill("19200", "", ISS_OFF);
    LxSerialSpeedSP[5].fill("38400", "", ISS_OFF);
    LxSerialSpeedSP[6].fill("57600", "", ISS_OFF);
    LxSerialSpeedSP[7].fill("115200", "", ISS_OFF);
    LxSerialSpeedSP[8].fill("230400", "", ISS_OFF);
    LxSerialSpeedSP.fill(device_name, "Serial speed", "",
                       LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    LxSerialSizeSP[0].fill("5", "", ISS_OFF);
    LxSerialSizeSP[1].fill("6", "", ISS_OFF);
    LxSerialSizeSP[2].fill("7", "", ISS_OFF);
    LxSerialSizeSP[3].fill("8", "", ISS_ON);
    LxSerialSizeSP.fill(device_name, "Serial size", "", LX_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    LxSerialParitySP[0].fill("None", "", ISS_ON);
    LxSerialParitySP[1].fill("Even", "", ISS_OFF);
    LxSerialParitySP[2].fill("Odd", "", ISS_OFF);
    LxSerialParitySP.fill(device_name, "Serial parity", "",
                       LX_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    LxSerialStopSP[0].fill("1", "", ISS_ON);
    LxSerialStopSP[1].fill("2", "", ISS_OFF);
    LxSerialStopSP.fill(device_name, "Serial stop", "", LX_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    LxSerialAddeolSP[0].fill("None", "", ISS_OFF);
    LxSerialAddeolSP[1].fill("CR (OxD, \\r)", "", ISS_ON);
    LxSerialAddeolSP[2].fill("LF (0xA, \\n)", "", ISS_OFF);
    LxSerialAddeolSP[3].fill("CR+LF", "", ISS_OFF);
    LxSerialAddeolSP.fill(device_name, "Add EOL", "", LX_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    FlashStrobeSP     = nullptr;
    FlashStrobeStopSP = nullptr;
    ledmethod         = PWCIOCTL;
    return true;
}

bool Lx::updateProperties()
{
    if (dev->isConnected())
    {
        INDI::Property *pfound;
        dev->defineProperty(LxEnableSP);
        dev->defineProperty(LxModeSP);
        dev->defineProperty(LxPortTP);
        dev->defineProperty(LxSerialOptionSP);
        //dev->defineProperty(LxParallelOptionSP);
        dev->defineProperty(LxStartStopCmdTP);
        dev->defineProperty(LxLogicalLevelSP);
        dev->defineProperty(LxSerialSpeedSP);
        dev->defineProperty(LxSerialSizeSP);
        dev->defineProperty(LxSerialParitySP);
        dev->defineProperty(LxSerialStopSP);
        dev->defineProperty(LxSerialAddeolSP);
        pfound = findbyLabel(dev, (char *)"Strobe");
        if (pfound)
        {
            FlashStrobeSP     = dev->getSwitch(pfound->getName());
            pfound            = findbyLabel(dev, (char *)"Stop Strobe");
            FlashStrobeStopSP = dev->getSwitch(pfound->getName());
        }
    }
    else
    {
        dev->deleteProperty(LxEnableSP.getName());
        dev->deleteProperty(LxModeSP.getName());
        dev->deleteProperty(LxPortTP.getName());
        dev->deleteProperty(LxSerialOptionSP.getName());
        //dev->deleteProperty(LxParallelOptionSP.getName());
        dev->deleteProperty(LxStartStopCmdTP.getName());
        dev->deleteProperty(LxLogicalLevelSP.getName());
        dev->deleteProperty(LxSerialSpeedSP.getName());
        dev->deleteProperty(LxSerialSizeSP.getName());
        dev->deleteProperty(LxSerialParitySP.getName());
        dev->deleteProperty(LxSerialStopSP.getName());
        dev->deleteProperty(LxSerialAddeolSP.getName());
        FlashStrobeSP     = nullptr;
        FlashStrobeStopSP = nullptr;
    }
    return true;
}

bool Lx::ISNewSwitch(const char *devname, const char *name, ISState *states, char *names[], int n)
{
    /* ignore if not ours */
    if (devname && strcmp(device_name, devname))
        return true;

    if (LxEnableSP.isNameMatch(name))
    {
        LxEnableSP.reset();
        LxEnableSP.update(states, names, n);
        LxEnableSP.setState(IPS_OK);

        LxEnableSP.apply("%s long exposure on device %s", (LxEnableSP[0].getState() == ISS_ON ? "Disabling" : "Enabling"),
                    device_name);
        return true;
    }

    if (LxModeSP.isNameMatch(name))
    {
        unsigned int index, oldindex;
        oldindex = LxModeSP.findOnSwitchIndex();
        LxModeSP.reset();
        LxModeSP.update(states, names, n);
        LxModeSP.setState(IPS_OK);
        index      = LxModeSP.findOnSwitchIndex();
        if (index == LXLED)
            if (!checkPWC())
            {
                LxModeSP.reset();
                LxModeSP.setState(IPS_ALERT);
                LxModeSP[oldindex].setState(ISS_ON);
                LxModeSP.apply("Can not set Lx Mode to %s", LxModeSP[index].getName());
                return false;
            }
        LxModeSP.apply("Setting Lx Mode to %s", LxModeSP[index].getName());
        return true;
    }

    if (LxSerialOptionSP.isNameMatch(name))
    {
        unsigned int index;
        LxSerialOptionSP.reset();
        LxSerialOptionSP.update(states, names, n);
        LxSerialOptionSP.setState(IPS_OK);
        index              = LxSerialOptionSP.findOnSwitchIndex();
        LxSerialOptionSP.apply("Setting Lx Serial option: %s", LxSerialOptionSP[index].getName());
        return true;
    }

    if (LxParallelOptionSP.isNameMatch(name))
    {
        unsigned int index;
        LxParallelOptionSP.reset();
        LxParallelOptionSP.update(states, names, n);
        LxParallelOptionSP.setState(IPS_OK);
        index                = LxParallelOptionSP.findOnSwitchIndex();
        LxParallelOptionSP.apply("Setting Lx Parallel option: %s", LxParallelOptionSP[index].getName());
        return true;
    }

    if (LxLogicalLevelSP.isNameMatch(name))
    {
        unsigned int index;
        LxLogicalLevelSP.reset();
        LxLogicalLevelSP.update(states, names, n);
        LxLogicalLevelSP.setState(IPS_OK);
        index              = LxLogicalLevelSP.findOnSwitchIndex();
        LxLogicalLevelSP.apply("Setting Lx logical levels for start transition: %s",
                    LxLogicalLevelSP[index].getName());
        return true;
    }

    if (LxSerialSpeedSP.isNameMatch(name))
    {
        unsigned int index;
        LxSerialSpeedSP.reset();
        LxSerialSpeedSP.update(states, names, n);
        LxSerialSpeedSP.setState(IPS_OK);
        index             = LxSerialSpeedSP.findOnSwitchIndex();
        LxSerialSpeedSP.apply("Setting Lx serial speed: %s", LxSerialSpeedSP[index].getName());
        return true;
    }

    if (LxSerialSizeSP.isNameMatch(name))
    {
        unsigned int index;
        LxSerialSizeSP.reset();
        LxSerialSizeSP.update(states, names, n);
        LxSerialSizeSP.setState(IPS_OK);
        index            = LxSerialSizeSP.findOnSwitchIndex();
        LxSerialSizeSP.apply("Setting Lx serial word size: %s", LxSerialSizeSP[index].getName());
        return true;
    }

    if (LxSerialParitySP.isNameMatch(name))
    {
        unsigned int index;
        LxSerialParitySP.reset();
        LxSerialParitySP.update(states, names, n);
        LxSerialParitySP.setState(IPS_OK);
        index              = LxSerialParitySP.findOnSwitchIndex();
        LxSerialParitySP.apply("Setting Lx serial parity: %s", LxSerialParitySP[index].getName());
        return true;
    }

    if (LxSerialStopSP.isNameMatch(name))
    {
        unsigned int index;
        LxSerialStopSP.reset();
        LxSerialStopSP.update(states, names, n);
        LxSerialStopSP.setState(IPS_OK);
        index            = LxSerialStopSP.findOnSwitchIndex();
        LxSerialStopSP.apply("Setting Lx serial stop bits: %s", LxSerialStopSP[index].getName());
        return true;
    }

    if (LxSerialAddeolSP.isNameMatch(name))
    {
        unsigned int index;
        LxSerialAddeolSP.reset();
        LxSerialAddeolSP.update(states, names, n);
        LxSerialAddeolSP.setState(IPS_OK);
        index              = LxSerialAddeolSP.findOnSwitchIndex();
        LxSerialAddeolSP.apply("Setting Lx End of Line: %s", LxSerialAddeolSP[index].getName());
        return true;
    }

    return true; // not ours, don't care
}

bool Lx::ISNewText(const char *devname, const char *name, char *texts[], char *names[], int n)
{
    IText *tp;
    /* ignore if not ours */
    if (devname && strcmp(device_name, devname))
        return true;

    if (LxPortTP.isNameMatch(name))
    {
        LxPortTP.setState(IPS_OK);
        tp         = LxPortTP.findWidgetByName(names[0]);
        if (!tp)
            return false;

        IUSaveText(tp, texts[0]);
        LxPortTP.apply("Setting Lx port to %s", tp->text);
        return true;
    }

    if (LxStartStopCmdTP.isNameMatch(name))
    {
        LxStartStopCmdTP.setState(IPS_OK);
        for (int i = 0; i < n; i++)
        {
            tp = LxStartStopCmdTP.findWidgetByName(names[i]);
            if (!tp)
                return false;
            IUSaveText(tp, texts[i]);
        }
        LxStartStopCmdTP.apply("Setting Lx Start/stop commands");
        return true;
    }

    return true; // not ours, don't care
}

unsigned int Lx::getLxmode()
{
    return LxModeSP.findOnSwitchIndex();
}

bool Lx::startLx()
{
    unsigned int index;
    IDMessage(device_name, "Starting Long Exposure");
    index = LxModeSP.findOnSwitchIndex();
    switch (index)
    {
        case LXSERIAL:
            return startLxSerial();
        case LXLED:
            return startLxPWC();
        default:
            return false;
    }
    return false;
}

int Lx::stopLx()
{
    unsigned int index = 0;

    IDMessage(device_name, "Stopping Long Exposure");
    index = LxModeSP.findOnSwitchIndex();
    switch (index)
    {
        case LXSERIAL:
            return stopLxSerial();
        case LXLED:
            return stopLxPWC();
        default:
            return -1;
    }

    return 0;
}

// Serial Stuff

void Lx::closeserial(int fd)
{
    tcsetattr(fd, TCSANOW, &oldterminfo);
    if (close(fd) < 0)
        perror("closeserial()");
}

int Lx::openserial(const char *devicename)
{
    int fd;
    struct termios attr;

    if ((fd = open(devicename, O_RDWR)) == -1)
    {
        IDLog("openserial(): open()");
        return -1;
    }
    if (tcgetattr(fd, &oldterminfo) == -1)
    {
        IDLog("openserial(): tcgetattr()");
        return -1;
    }
    attr = oldterminfo;
    //attr.c_cflag |= CRTSCTS | CLOCAL;
    attr.c_cflag |= CLOCAL;
    attr.c_oflag = 0;
    if (tcflush(fd, TCIOFLUSH) == -1)
    {
        IDLog("openserial(): tcflush()");
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &attr) == -1)
    {
        IDLog("initserial(): tcsetattr()");
        return -1;
    }
    return fd;
}

int Lx::setRTS(int fd, int level)
{
//    int status;
    int mcr = 0;
    // does not work for RTS
    //if (ioctl(fd, TIOCMGET, &status) == -1) {
    //    IDLog("setRTS(): TIOCMGET");
    //    return 0;
    //}
    //if (level)
    //    status |= TIOCM_RTS;
    //else
    //    status &= ~TIOCM_RTS;
    //if (ioctl(fd, TIOCMSET, &status) == -1) {
    //    IDLog("setRTS(): TIOCMSET");
    //    return 0;
    //}
    mcr = TIOCM_RTS;
    if (level)
    {
        if (ioctl(fd, TIOCMBIS, &mcr) == -1)
        {
            IDLog("setRTS(): TIOCMBIS");
            return 0;
        }
    }
    else
    {
        if (ioctl(fd, TIOCMBIC, &mcr) == -1)
        {
            IDLog("setRTS(): TIOCMBIC");
            return 0;
        }
    }
    return 1;
}

int Lx::setDTR(int fd, int level)
{
    int status;

    if (ioctl(fd, TIOCMGET, &status) == -1)
    {
        IDLog("setDTR(): TIOCMGET");
        return 0;
    }
    if (level)
        status |= TIOCM_DTR;
    else
        status &= ~TIOCM_DTR;
    if (ioctl(fd, TIOCMSET, &status) == -1)
    {
        IDLog("setDTR(): TIOCMSET");
        return 0;
    }
    return 1;
}

void Lx::getSerialOptions(unsigned int *speed, unsigned int *wordsize, unsigned int *parity, unsigned int *stops)
{
    unsigned int index;
    unsigned int speedopts[]  = { 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400 };
    unsigned int sizeopts[]   = { 5, 6, 7, 8 };
    unsigned int parityopts[] = { PARITY_NONE, PARITY_EVEN, PARITY_ODD };
    unsigned int stopopts[]   = { 1, 2 };
    index                     = LxSerialSpeedSP.findOnSwitchIndex();
    *speed                    = speedopts[index];
    index                     = LxSerialSizeSP.findOnSwitchIndex();
    *wordsize                 = sizeopts[index];
    index                     = LxSerialParitySP.findOnSwitchIndex();
    *parity                   = parityopts[index];
    index                     = LxSerialStopSP.findOnSwitchIndex();
    *stops                    = stopopts[index];
}

const char *Lx::getSerialEOL()
{
    unsigned int index;
    index = LxSerialAddeolSP.findOnSwitchIndex();
    switch (index)
    {
        case 0:
            return "";
        case 1:
            return "\r";
        case 2:
            return "\n";
        case 3:
            return "\r\n";
    }
    return nullptr;
}

bool Lx::startLxSerial()
{
    unsigned int speed = 0, wordsize = 0, parity = 0, stops = 0;
    const char *eol = nullptr;
    unsigned int index = LxSerialOptionSP.findOnSwitchIndex();
    int ret = 0;

    switch (index)
    {
        case 0:
            serialfd = openserial(LxPortTP[0].getText());
            if (serialfd < 0)
                return false;
            if (LxLogicalLevelSP[0].getState() == ISS_ON)
                setDTR(serialfd, 1);
            else
                setDTR(serialfd, 0);
            break;
        case 1:
            serialfd = openserial(LxPortTP[0].getText());
            if (serialfd < 0)
                return false;
            if (LxLogicalLevelSP[0].getState() == ISS_ON)
                setRTS(serialfd, 1);
            else
                setRTS(serialfd, 0);
            break;
        case 2:
            getSerialOptions(&speed, &wordsize, &parity, &stops);
            eol = getSerialEOL();
            tty_connect(LxPortTP[0].getText(), speed, wordsize, parity, stops, &serialfd);
            if (serialfd < 0)
                return false;

            ret = write(serialfd, LxStartStopCmdTP[0].getText(), strlen(LxStartStopCmdTP[0].getText()));
            ret = write(serialfd, eol, strlen(eol));
            break;
    }
    return true;
}

int Lx::stopLxSerial()
{
    int ret = 0;
    const char *eol = nullptr;
    unsigned int index = LxSerialOptionSP.findOnSwitchIndex();

    switch (index)
    {
        case 0:
            if (LxLogicalLevelSP[0].getState() == ISS_ON)
                setDTR(serialfd, 0);
            else
                setDTR(serialfd, 1);
            break;
        case 1:
            if (LxLogicalLevelSP[0].getState() == ISS_ON)
                setRTS(serialfd, 0);
            else
                setRTS(serialfd, 1);
            break;
        case 2:
            ret = write(serialfd, LxStartStopCmdTP[1].getText(), strlen(LxStartStopCmdTP[1].getText()));
            eol = getSerialEOL();
            ret = write(serialfd, eol, strlen(eol));
            break;
    }
    close(serialfd);
    return 0;
}

INDI::Property *Lx::findbyLabel(INDI::DefaultDevice *dev, char *label)
{
    for(const auto &oneProperty: *dev->getProperties())
        if (!strcmp(oneProperty->getLabel(), label))
            return oneProperty;
    return nullptr;
}

// PWC Stuff
bool Lx::checkPWC()
{
    if (FlashStrobeSP && FlashStrobeStopSP)
    {
        IDMessage(device_name, "Using Flash control for led Lx Mode");
        ledmethod = FLASHLED;
        return true;
    }
    if (ioctl(camerafd, VIDIOCPWCPROBE, &probe) != 0)
    {
        IDMessage(device_name, "ERROR: device does not support PWC ioctl");
        return false;
    }
    if (probe.type < 730)
    {
        IDMessage(device_name, "ERROR: camera type %d does not support led control", probe.type);
        return false;
    }
    IDMessage(device_name, "Using PWC ioctl for led Lx Mode");
    return true;
}

void Lx::pwcsetLed(int on, int off)
{
    struct pwc_leds leds;
    leds.led_on  = on;
    leds.led_off = off;
    if (ioctl(camerafd, VIDIOCPWCSLED, &leds))
    {
        IDLog("ioctl: can't set Led.\n");
    }
}

void Lx::pwcsetflashon()
{
    ISState states[2]    = { ISS_ON, ISS_OFF };
    const char *names[2] = { FlashStrobeSP->sp[0].name, FlashStrobeStopSP->sp[0].name };
    dev->ISNewSwitch(device_name, FlashStrobeSP->name, &(states[0]), (char **)names, 1);
    //dev->ISNewSwitch(device_name, FlashStrobeStopSP->name, &(states[1]), (char **)(names + 1), 1);
    FlashStrobeSP->s = IPS_OK;
    IDSetSwitch(FlashStrobeSP, nullptr);
    FlashStrobeStopSP->s = IPS_IDLE;
    IDSetSwitch(FlashStrobeStopSP, nullptr);
}

void Lx::pwcsetflashoff()
{
    ISState states[2]    = { ISS_OFF, ISS_ON };
    const char *names[2] = { FlashStrobeSP->sp[0].name, FlashStrobeStopSP->sp[0].name };
    //dev->ISNewSwitch(device_name, FlashStrobeSP->name, &(states[0]), (char **)names, 1);
    dev->ISNewSwitch(device_name, FlashStrobeStopSP->name, &(states[1]), (char **)(names + 1), 1);
    FlashStrobeStopSP->s = IPS_OK;
    IDSetSwitch(FlashStrobeStopSP, nullptr);
    FlashStrobeSP->s = IPS_IDLE;
    IDSetSwitch(FlashStrobeSP, nullptr);
}

bool Lx::startLxPWC()
{
    switch (ledmethod)
    {
        case PWCIOCTL:
            if (LxLogicalLevelSP[0].getState() == ISS_ON)
                pwcsetLed(25500, 0);
            else
                pwcsetLed(0, 25500);
            return true;
        case FLASHLED:
            if (LxLogicalLevelSP[0].getState() == ISS_ON)
                pwcsetflashon();
            else
                pwcsetflashoff();
            return true;
    }

    return false;
}

int Lx::stopLxPWC()
{
    switch (ledmethod)
    {
        case PWCIOCTL:
            if (LxLogicalLevelSP[0].getState() == ISS_ON)
                pwcsetLed(0, 25500);
            else
                pwcsetLed(25500, 0);
            return 0;
        case FLASHLED:
            if (LxLogicalLevelSP[0].getState() == ISS_ON)
                pwcsetflashoff();
            else
                pwcsetflashon();
            return 0;
    }

    return -1;
}
