#include "pegasus_scopsoag.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define DMFC_TIMEOUT 3
#define FOCUS_SETTINGS_TAB "Settings"
#define TEMPERATURE_THRESHOLD 0.1

static std::unique_ptr<PegasusScopsOAG> scopsOAG(new PegasusScopsOAG());

PegasusScopsOAG::PegasusScopsOAG()
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE |
                      FOCUSER_CAN_REL_MOVE |
                      FOCUSER_CAN_ABORT |
                      FOCUSER_CAN_REVERSE |
                      FOCUSER_CAN_SYNC |
                      FOCUSER_HAS_BACKLASH);
}

bool PegasusScopsOAG::initProperties()
{
    INDI::Focuser::initProperties();

    // LED
    IUFillSwitch(&LEDS[LED_OFF], "Off", "", ISS_ON);
    IUFillSwitch(&LEDS[LED_ON], "On", "", ISS_OFF);
    IUFillSwitchVector(&LEDSP, LEDS, 2, getDeviceName(), "LED", "", FOCUS_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware Version
    IUFillText(&FirmwareVersionT[0], "Version", "Version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "Firmware", "Firmware", MAIN_CONTROL_TAB, IP_RO,
                     0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 100000;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    // Backlash compensation
    FocusBacklashN[0].min   = 1; // 0 is off.
    FocusBacklashN[0].max   = 1000;
    FocusBacklashN[0].value = 1;
    FocusBacklashN[0].step  = 1;

    //LED Default ON
    LEDS[LED_ON].s = ISS_ON;
    LEDS[LED_OFF].s = ISS_OFF;

    addDebugControl();
    setDefaultPollingPeriod(200);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_19200);

    return true;
}

bool PegasusScopsOAG::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {

        deleteProperty(FocusReverseSP.name);
        deleteProperty(FocusBacklashSP.name);
        deleteProperty(FocusBacklashNP.name);
        defineProperty(&LEDSP);
        defineProperty(&FirmwareVersionTP);
    }
    else
    {
        deleteProperty(LEDSP.name);
        deleteProperty(FirmwareVersionTP.name);
    }

    return true;
}

bool PegasusScopsOAG::Handshake()
{
    if (ack())
    {
        LOGF_INFO("%s is online. Getting focus parameters...", this->getDeviceName());
        return true;
    }

    LOGF_INFO(
        "Error retrieving data from %s, please ensure device is powered and the port is correct.", this->getDeviceName());
    return false;
}


const char *PegasusScopsOAG::getDefaultName()
{
    return "Pegasus ScopsOAG";
}

bool PegasusScopsOAG::ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[2] = {0};
    char res[16] = {0};

    cmd[0] = '#';
    cmd[1] = 0xA;

    LOGF_DEBUG("CMD <%#02X>", cmd[0]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 2, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Ack error: %s.", errstr);
        return false;
    }

    if ((rc = tty_read_section(PortFD, res, 0xA, DMFC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Ack error: %s.", errstr);
        return false;
    }

    // Get rid of 0xA
    res[nbytes_read - 1] = 0;


    if( res[nbytes_read - 2] == '\r') res[nbytes_read - 2] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);



    if((strstr(res, "OK_SCOPS") != nullptr))
        return true;

    return false;
}


bool PegasusScopsOAG::SyncFocuser(uint32_t ticks)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "W:%ud", ticks);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    // Set Position
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("sync error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();

    return true;
}

bool PegasusScopsOAG::move(uint32_t newPosition)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "M:%ud", newPosition);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    // Set Position
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("move error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();

    return true;
}

bool PegasusScopsOAG::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // LED
        if (!strcmp(name, LEDSP.name))
        {
            IUUpdateSwitch(&LEDSP, states, names, n);
            bool rc = setLedEnabled(LEDS[LED_ON].s == ISS_ON);
            LEDSP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&LEDSP, nullptr);
            return true;
        }
    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

void PegasusScopsOAG::ignoreResponse()
{
    int nbytes_read = 0;
    char res[64];
    tty_read_section(PortFD, res, 0xA, DMFC_TIMEOUT, &nbytes_read);
}

bool PegasusScopsOAG::updateFocusParams()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char cmd[3] = {0};
    char res[64];
    cmd[0] = 'A';
    cmd[1] = 0xA;

    LOGF_DEBUG("CMD <%#02X>", cmd[0]);

    tcflush(PortFD, TCIOFLUSH);

    if ((rc = tty_write(PortFD, cmd, 2, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("GetFocusParams error: %s.", errstr);
        return false;
    }


    if ((rc = tty_read_section(PortFD, res, 0xA, DMFC_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("GetFocusParams error: %s.", errstr);
        return false;
    }

    res[nbytes_read - 1] = 0;

    // Check for '\r' at end of string and replace with nullptr (DMFC firmware version 2.8)
    if( res[nbytes_read - 2] == '\r') res[nbytes_read - 2] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    char *token = std::strtok(res, ":");


    // #1 Status
    if (token == nullptr || ((strstr(token, "OK_SCOPS") == nullptr)))
    {
        LOGF_ERROR("Invalid status response. %s", res);
        return false;
    }

    // #2 Version
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid version response.");
        return false;
    }

    if (FirmwareVersionT[0].text == nullptr || strcmp(FirmwareVersionT[0].text, token))
    {
        IUSaveText(&FirmwareVersionT[0], token);
        FirmwareVersionTP.s = IPS_OK;
        IDSetText(&FirmwareVersionTP, nullptr);
    }

    // #3 Motor Type
    token = std::strtok(nullptr, ":");

    // #4 Temperature
    token = std::strtok(nullptr, ":");

    // #5 Position
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid position response.");
        return false;
    }

    currentPosition = atoi(token);
    if (currentPosition != FocusAbsPosN[0].value)
    {
        FocusAbsPosN[0].value = currentPosition;
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    // #6 Moving Status
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid moving status response.");
        return false;
    }

    isMoving = (token[0] == '1');

    // #7 LED Status
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid LED response.");
        return false;
    }

    int ledStatus = atoi(token);
    if (ledStatus >= 0 && ledStatus <= 1)
    {
        IUResetSwitch(&LEDSP);
        LEDS[ledStatus].s = ISS_ON;
        LEDSP.s = IPS_OK;
        IDSetSwitch(&LEDSP, nullptr);
    }

    // #8 Reverse Status
    token = std::strtok(nullptr, ":");

    // #9 Encoder status
    token = std::strtok(nullptr, ":");

    // #10 Backlash
    token = std::strtok(nullptr, ":");

    if (token == nullptr)
    {
        LOG_ERROR("Invalid encoder response.");
        return false;
    }

    int backlash = atoi(token);
    // If backlash is zero then compensation is disabled
    if (backlash == 0 && FocusBacklashS[INDI_ENABLED].s == ISS_ON)
    {
        LOG_WARN("Backlash value is zero, disabling backlash switch...");

        FocusBacklashS[INDI_ENABLED].s = ISS_OFF;
        FocusBacklashS[INDI_DISABLED].s = ISS_ON;
        FocusBacklashSP.s = IPS_IDLE;
        IDSetSwitch(&FocusBacklashSP, nullptr);
    }
    else if (backlash > 0 && (FocusBacklashS[INDI_DISABLED].s == ISS_ON || backlash != FocusBacklashN[0].value))
    {
        if (backlash != FocusBacklashN[0].value)
        {
            FocusBacklashN[0].value = backlash;
            FocusBacklashNP.s = IPS_OK;
            IDSetNumber(&FocusBacklashNP, nullptr);
        }

        if (FocusBacklashS[INDI_DISABLED].s == ISS_ON)
        {
            FocusBacklashS[INDI_ENABLED].s = ISS_OFF;
            FocusBacklashS[INDI_DISABLED].s = ISS_ON;
            FocusBacklashSP.s = IPS_IDLE;
            IDSetSwitch(&FocusBacklashSP, nullptr);
        }
    }

    return true;
}


bool PegasusScopsOAG::setLedEnabled(bool enable)
{
    int nbytes_written = 0, rc = -1;
    char cmd[16] = {0};

    snprintf(cmd, 16, "L:%d", enable ? 2 : 1);
    cmd[strlen(cmd)] = 0xA;

    LOGF_DEBUG("CMD <%s>", cmd);

    tcflush(PortFD, TCIOFLUSH);

    // Led
    if ((rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Led error: %s.", errstr);
        return false;
    }

    this->ignoreResponse();
    return true;
}

IPState PegasusScopsOAG::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPosition = targetTicks;

    bool rc = move(targetPosition);

    if (!rc)
        return IPS_ALERT;

    FocusAbsPosNP.s = IPS_BUSY;

    return IPS_BUSY;
}

IPState PegasusScopsOAG::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    double newPosition = 0;
    bool rc = false;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    rc = move(newPosition);

    if (!rc)
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void PegasusScopsOAG::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = updateFocusParams();

    if (rc)
    {
        if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
        {
            if (isMoving == false)
            {
                FocusAbsPosNP.s = IPS_OK;
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusAbsPosNP, nullptr);
                IDSetNumber(&FocusRelPosNP, nullptr);
                LOG_INFO("Focuser reached requested position.");
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool PegasusScopsOAG::AbortFocuser()
{
    int nbytes_written;
    char cmd[2] = { 'H', 0xA };

    if (tty_write(PortFD, cmd, 2, &nbytes_written) == TTY_OK)
    {
        FocusAbsPosNP.s = IPS_IDLE;
        FocusRelPosNP.s = IPS_IDLE;
        IDSetNumber(&FocusAbsPosNP, nullptr);
        IDSetNumber(&FocusRelPosNP, nullptr);
        this->ignoreResponse();
        return true;
    }
    else
        return false;
}

bool PegasusScopsOAG::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);
    IUSaveConfigSwitch(fp, &LEDSP);

    return true;
}
