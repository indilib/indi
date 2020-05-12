
#include "sestosenso2.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>
#include <connectionplugins/connectionserial.h>
#include <sys/ioctl.h>

static std::unique_ptr<SestoSenso2> sesto(new SestoSenso2());


void ISGetProperties(const char *dev)
{
    sesto->ISGetProperties(dev);

}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    sesto->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    sesto->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    sesto->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    sesto->ISSnoopDevice(root);
}

SestoSenso2::SestoSenso2()
{
    setVersion(0, 1);
    // Can move in Absolute & Relative motions, can AbortFocuser motion.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT);

}

bool SestoSenso2::initProperties()
{

    INDI::Focuser::initProperties();

    setConnectionParams();

    // Firmware Information
    IUFillText(&FirmwareT[0], "VERSION", "Version", "");
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "FOCUS_FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Motor temp.", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&SpeedN[0], "SPEED", "RPM", "%0.0f", 0, 7000., 1, 0);
    IUFillNumberVector(&SpeedNP, SpeedN, 1, getDeviceName(), "FOCUS_SPEED", "Motor speed", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Focuser calibration
    IUFillText(&CalibrationMessageT[0], "CALIBRATION", "Calibration stage", "");
    IUFillTextVector(&CalibrationMessageTP, CalibrationMessageT, 1, getDeviceName(), "CALIBRATION_MESSAGE", "Calibration", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillSwitch(&CalibrationS[CALIBRATION_START], "CALIBRATION_START", "Start", ISS_OFF);
    IUFillSwitch(&CalibrationS[CALIBRATION_NEXT], "CALIBRATION_NEXT", "Next", ISS_OFF);
    IUFillSwitchVector(&CalibrationSP, CalibrationS, 2, getDeviceName(), "FOCUS_CALIBRATION", "Calibration", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&FastMoveS[FASTMOVE_IN], "FASTMOVE_IN", "Move In", ISS_OFF);
    IUFillSwitch(&FastMoveS[FASTMOVE_OUT], "FASTMOVE_OUT", "Move out", ISS_OFF);
    IUFillSwitch(&FastMoveS[FASTMOVE_STOP], "FASTMOVE_STOP", "Stop", ISS_OFF);

    IUFillSwitchVector(&FastMoveSP, FastMoveS, 3, getDeviceName(), "FAST_MOVE", "Calibration Move", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    //
    // Override the default Max. Position to make it Read-Only
    IUFillNumberVector(&FocusMaxPosNP, FocusMaxPosN, 1, getDeviceName(), "FOCUS_MAX", "Max. Position", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Relative and absolute movement
    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = 50000.;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = 1000;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = 200000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    FocusMaxPosN[0].value = 2097152;

    addAuxControls();

    setDefaultPollingPeriod(500);

    return true;
}

bool SestoSenso2::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        if (updateTemperature())
            defineNumber(&TemperatureNP);
        defineNumber(&SpeedNP);
        defineText(&FirmwareTP);
        IUSaveText(&CalibrationMessageT[0], "Press START to begin the Calibration");
        defineText(&CalibrationMessageTP);
        defineSwitch(&CalibrationSP);

        if (getStartupValues())
            LOG_INFO("SestoSenso parameters updated, focuser ready for use.");
        else
            LOG_WARN("Failed to inquire parameters. Check logs.");
    }
    else
    {
        if (TemperatureNP.s == IPS_OK)
            deleteProperty(TemperatureNP.name);
        deleteProperty(FirmwareTP.name);
        deleteProperty(CalibrationMessageTP.name);
        deleteProperty(CalibrationSP.name);
        deleteProperty(SpeedNP.name);
    }

    return true;
}

bool SestoSenso2::Handshake()
{
    if (Ack())
    {
        LOG_INFO("SestoSenso is online. Getting focus parameters...");
        return true;
    }

    LOG_INFO("Error retrieving data from SestoSenso, please ensure SestoSenso controller is powered and the port is correct.");
    return false;
}

bool SestoSenso2::Disconnect()
{
    if (isSimulation() == false)
        command->goHome();

    return INDI::Focuser::Disconnect();
}

const char *SestoSenso2::getDefaultName()
{
    return "Sesto Senso 2";
}

bool SestoSenso2::updateTemperature()
{
    char res[SESTO_LEN] = {0};
    double temperature = 0;

    if (isSimulation())
        strncpy(res, "23.45", SESTO_LEN);
    else if (command->getMotorTemp(res) == CMD_FALSE)
        return false;

    try
    {
        temperature = std::stod(res);
    }
    catch(...)
    {
        LOGF_WARN("Failed to process temperature response: %s (%d bytes)", res, strlen(res));
        return false;
    }

    if (temperature > 90)
        return false;

    TemperatureN[0].value = temperature;
    TemperatureNP.s = IPS_OK;

    return true;
}


bool SestoSenso2::updateMaxLimit()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
        return true;

    if (command->getMaxPosition(res) == CMD_FALSE)
        return false;

    int maxLimit = 0;

    sscanf(res, "%d", &maxLimit);

    if (maxLimit > 0)
    {
        FocusMaxPosN[0].max = maxLimit;
        if (FocusMaxPosN[0].value > maxLimit)
            FocusMaxPosN[0].value = maxLimit;

        FocusAbsPosN[0].min   = 0;
        FocusAbsPosN[0].max   = maxLimit;
        FocusAbsPosN[0].value = 0;
        FocusAbsPosN[0].step  = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

        FocusRelPosN[0].min   = 0.;
        FocusRelPosN[0].max   = FocusAbsPosN[0].step * 10;
        FocusRelPosN[0].value = 0;
        FocusRelPosN[0].step  = FocusAbsPosN[0].step;

        IUUpdateMinMax(&FocusAbsPosNP);
        IUUpdateMinMax(&FocusRelPosNP);

        FocusMaxPosNP.s = IPS_OK;
        IUUpdateMinMax(&FocusMaxPosNP);
        return true;
    }


    FocusMaxPosNP.s = IPS_ALERT;
    return false;
}

bool SestoSenso2::updatePosition()
{
    char res[SESTO_LEN] = {0};
    if (isSimulation())
        snprintf(res, SESTO_LEN, "%d", static_cast<uint32_t>(FocusAbsPosN[0].value));
    else if (command->getAbsolutePosition(res) == CMD_FALSE)
        return false;

    try
    {
        FocusAbsPosN[0].value = std::stoi(res);
        FocusAbsPosNP.s = IPS_OK;
        return true;
    }
    catch(...)
    {
        LOGF_WARN("Failed to process position response: %s (%d bytes)", res, strlen(res));
        FocusAbsPosNP.s = IPS_ALERT;
        return false;
    }
}

bool SestoSenso2::setupRunPreset()
{
    char res[SESTO_LEN] = {0};
    if (command->loadSlowPreset(res) == CMD_FALSE)
    {
        return false;
    }
    return true;
}

bool SestoSenso2::isMotionComplete()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
    {
        int32_t nextPos = FocusAbsPosN[0].value;
        int32_t targPos = static_cast<int32_t>(targetPos);

        if (targPos > nextPos)
            nextPos += 250;
        else if (targPos < nextPos)
            nextPos -= 250;

        if (abs(nextPos - targPos) < 250)
            nextPos = targetPos;
        else if (nextPos < 0)
            nextPos = 0;
        else if (nextPos > FocusAbsPosN[0].max)
            nextPos = FocusAbsPosN[0].max;

        snprintf(res, SESTO_LEN, "%d", nextPos);
    }
    else
    {
        if(command->getCurrentSpeed(res) == CMD_OK)
        {
            try
            {
                uint32_t newSpeed = std::stoi(res);
                SpeedN[0].value = newSpeed;
                SpeedNP.s = IPS_OK;
            }
            catch (...)
            {
                LOGF_WARN("Failed to get motor speed response: %s (%d bytes)", res, strlen(res));
            }

            if(!strcmp(res, "0"))
                return true;

            *res = {0};
            if(command->getAbsolutePosition(res) == CMD_OK)
            {
                try
                {
                    uint32_t newPos = std::stoi(res);
                    FocusAbsPosN[0].value = newPos;
                }
                catch (...)
                {
                    LOGF_WARN("Failed to process motion response: %s (%d bytes)", res, strlen(res));
                }
            }
        }

    }

    return false;
}

bool SestoSenso2::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        // Calibrate focuser
        if (!strcmp(name, CalibrationSP.name))
        {

            int current_switch = 0;

            CalibrationSP.s = IPS_BUSY;
            //IDSetSwitch(&CalibrationSP, nullptr);
            IUUpdateSwitch(&CalibrationSP, states, names, n);

            current_switch = IUFindOnSwitchIndex(&CalibrationSP);
            CalibrationS[current_switch].s = ISS_ON;
            IDSetSwitch(&CalibrationSP, nullptr);

            if (current_switch == CALIBRATION_START)
            {
                if (cStage == Idle || cStage == Complete )
                {
                    // Start the calibration process
                    LOG_INFO("Start Calibration");
                    CalibrationSP.s = IPS_BUSY;
                    IDSetSwitch(&CalibrationSP, nullptr);

                    //
                    // Init
                    //
                    if (command->initCalibration() == false)
                        return false;

                    IUSaveText(&CalibrationMessageT[0], "Set focus in MIN position and then press NEXT");
                    IDSetText(&CalibrationMessageTP, nullptr);

                    // Set next step
                    cStage = GoToMiddle;
                }
                else
                {
                    LOG_INFO("Already started calibration. Proceed to next step.");
                    IUSaveText(&CalibrationMessageT[0], "Already started. Proceed to NEXT.");
                    IDSetText(&CalibrationMessageTP, nullptr);
                }
            }
            else if (current_switch == CALIBRATION_NEXT)
            {
                if (cStage == GoToMiddle)
                {
                    if (command->storeAsMinPosition() == CMD_FALSE)
                        return false;
                    defineSwitch(&FastMoveSP);
                    IUSaveText(&CalibrationMessageT[0], "Press MOVE OUT to move focuser out (CAUTION!)");
                    IDSetText(&CalibrationMessageTP, nullptr);
                    cStage = GoMinimum;
                }
                else if (cStage == GoMinimum)
                {
                    char res[SESTO_LEN] = {0};
                    if (command->storeAsMaxPosition(res) == CMD_FALSE)
                        return false;

                    IUSaveText(&CalibrationMessageT[0], "Press NEXT to finish");
                    IDSetText(&CalibrationMessageTP, nullptr);
                    cStage = GoMaximum;
                }
                else if (cStage == GoMaximum)
                {
                    char res[SESTO_LEN] = {0};

                    if (command->getMaxPosition(res) == CMD_FALSE)
                        return false;

                    int maxLimit = 0;
                    sscanf(res, "%d", &maxLimit);
                    LOGF_INFO("MAX setting is %d", maxLimit);

                    FocusMaxPosN[0].max = maxLimit;
                    FocusMaxPosN[0].value = maxLimit;

                    FocusAbsPosN[0].min   = 0;
                    FocusAbsPosN[0].max   = maxLimit;
                    FocusAbsPosN[0].value = maxLimit;
                    FocusAbsPosN[0].step  = (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 50.0;

                    FocusRelPosN[0].min   = 0.;
                    FocusRelPosN[0].max   = FocusAbsPosN[0].step * 10;
                    FocusRelPosN[0].value = 0;
                    FocusRelPosN[0].step  = FocusAbsPosN[0].step;

                    IUUpdateMinMax(&FocusAbsPosNP);
                    IUUpdateMinMax(&FocusRelPosNP);
                    FocusMaxPosNP.s = IPS_OK;
                    IUUpdateMinMax(&FocusMaxPosNP);

                    IUSaveText(&CalibrationMessageT[0], "Calibration Completed.");
                    IDSetText(&CalibrationMessageTP, nullptr);

                    deleteProperty(FastMoveSP.name);
                    cStage = Complete;

                    LOG_INFO("Calibration completed");
                    CalibrationSP.s = IPS_OK;
                    IDSetSwitch(&CalibrationSP, nullptr);
                    CalibrationS[current_switch].s = ISS_OFF;
                    IDSetSwitch(&CalibrationSP, nullptr);
                }
                else
                {
                    IUSaveText(&CalibrationMessageT[0], "Calibration not in process");
                    IDSetText(&CalibrationMessageTP, nullptr);
                }

            }
            return true;
        }
        else if (!strcmp(name, FastMoveSP.name))
        {
            IUUpdateSwitch(&FastMoveSP, states, names, n);
            int current_switch = IUFindOnSwitchIndex(&FastMoveSP);

            switch (current_switch)
            {
            case FASTMOVE_IN:
                if (command->fastMoveIn() == CMD_FALSE)
                {
                    return false;
                }
                break;
            case FASTMOVE_OUT:
                if (command->goOutToFindMaxPos() == CMD_FALSE)
                {
                    return false;
                }
                IUSaveText(&CalibrationMessageT[0], "Press STOP focuser almost at MAX position.");
                IDSetText(&CalibrationMessageTP, nullptr);
                break;
            case FASTMOVE_STOP:
                if (command->stop() == CMD_FALSE)
                {
                    return false;
                }
                IUSaveText(&CalibrationMessageT[0], "Press NEXT to store max limit");
                IDSetText(&CalibrationMessageTP, nullptr);
                break;
            default:
                break;
            }

            FastMoveSP.s = IPS_BUSY;
            IDSetSwitch(&FastMoveSP, nullptr);
            return true;
        }

    }
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

IPState SestoSenso2::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;
    char res[SESTO_LEN] = {0};

    if (isSimulation() == false)
    {
        if (command->go(targetTicks,res) == CMD_FALSE)
            return IPS_ALERT;
    }

    if (m_MotionProgressTimerID > 0)
        IERmTimer(m_MotionProgressTimerID);
    m_MotionProgressTimerID = IEAddTimer(10, &SestoSenso2::checkMotionProgressHelper, this);
    return IPS_BUSY;
}

IPState SestoSenso2::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int reversed = (IUFindOnSwitchIndex(&FocusReverseSP) == INDI_ENABLED) ? -1 : 1;
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * reversed;
    double newPosition = FocusAbsPosN[0].value + relativeTicks;

    bool rc = MoveAbsFocuser(newPosition);

    return (rc ? IPS_BUSY : IPS_ALERT);
}

bool SestoSenso2::AbortFocuser()
{
    if (m_MotionProgressTimerID > 0)
    {
        IERmTimer(m_MotionProgressTimerID);
        m_MotionProgressTimerID = -1;
    }

    if (isSimulation())
        return true;

    return command->abort();

}

void SestoSenso2::checkMotionProgressHelper(void *context)
{
    static_cast<SestoSenso2*>(context)->checkMotionProgressCallback();
}
//
// This timer function is initiated when a GT command has been issued
// A timer will call this function on a regular interval during the motion
// Modified the code to exit when motion is complete
//
void SestoSenso2::checkMotionProgressCallback()
{
    if (isMotionComplete())
    {
        FocusAbsPosNP.s = IPS_OK;
        FocusRelPosNP.s = IPS_OK;
        SpeedNP.s = IPS_OK;
        SpeedN[0].value = 0;
        IDSetNumber(&SpeedNP, nullptr);

        IDSetNumber(&FocusRelPosNP, nullptr);
        IDSetNumber(&FocusAbsPosNP, nullptr);


        lastPos = FocusAbsPosN[0].value;
        LOG_INFO("Focuser reached requested position.");
        return;
    }
    else
    {
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    SpeedNP.s = IPS_BUSY;
    IDSetNumber(&SpeedNP, nullptr);

    lastPos = FocusAbsPosN[0].value;

    IERmTimer(m_MotionProgressTimerID);
    m_MotionProgressTimerID = IEAddTimer(500, &SestoSenso2::checkMotionProgressHelper, this);
}

void SestoSenso2::TimerHit()
{
    if (!isConnected() || FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY || CalibrationSP.s == IPS_BUSY)
    {
        SetTimer(POLLMS);
        return;
    }

    bool rc = updatePosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 0)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    if (m_TemperatureCounter++ == SESTO_TEMPERATURE_FREQ)
    {
        rc = updateTemperature();
        if (rc)
        {
            if (fabs(lastTemperature - TemperatureN[0].value) >= 0.1)
            {
                IDSetNumber(&TemperatureNP, nullptr);
                lastTemperature = TemperatureN[0].value;
            }
        }
        m_TemperatureCounter = 0;   // Reset the counter
    }

    SetTimer(POLLMS);
}

bool SestoSenso2::getStartupValues()
{
    setupRunPreset();
    bool rc1 = updatePosition();
    if (rc1)
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (updateMaxLimit() == false)
        LOG_WARN("Check you have the latest SestoSenso firmware. Focuser requires calibration.");

    return (rc1);
}


bool SestoSenso2::ReverseFocuser(bool enable)
{
    INDI_UNUSED(enable);
    return false;
}


bool SestoSenso2::Ack()
{
    char res[SESTO_LEN] = {0};

    if (isSimulation())
        strncpy(res, "1.0 Simulation", SESTO_LEN);
    else
    {
        if(initCommandSet() == false)
        {
            LOG_ERROR("Failed setting attributes on serial port and init command sets");
            return false;
        }
        if(command->getSerialNumber(res))
        {
            LOGF_INFO("Hello,I'm %s !", res);
        }
        else
        {
            return false;
        }
    }
    IUSaveText(&FirmwareT[0], res);

    return true;
}


void SestoSenso2::setConnectionParams()
{
    serialConnection->setDefaultBaudRate(serialConnection->B_115200);
    serialConnection->setWordSize(8);
}


bool SestoSenso2::initCommandSet()
{
    command = new CommandSet(PortFD, getDeviceName());

    struct termios tty_setting;
    if (tcgetattr(PortFD, &tty_setting) == -1)
    {
        LOG_ERROR("setTTYFlags: failed getting tty attributes.");
        return false;
    }
    tty_setting.c_lflag |= ICANON;
    if (tcsetattr(PortFD, TCSANOW, &tty_setting))
    {
        LOG_ERROR("setTTYFlags: failed setting attributes on serial port.");
        return false;
    }
    return true;
}



bool CommandSet::sendCmd(std::string cmd, std::string property, char *res)
{  
    LOGF_DEBUG("Sending comand: %s with property: %s", cmd.c_str(), property.c_str());
    tcflush(CommandSet::PortFD, TCIOFLUSH);
    std::string cmd_str;
    cmd_str.append(cmd);

    if(write(CommandSet::PortFD, cmd.c_str(), cmd_str.length()) == 0)
    {
        LOGF_ERROR("Device not response: cmd %s property %s", cmd.c_str(), property.c_str());
        return false;
    }
    if(property.empty() || res == nullptr)
        return true;

    char read_buf[SESTO_LEN] = {0};
    if(read(CommandSet::PortFD, &read_buf, sizeof(read_buf)) == 0)
    {
        LOGF_ERROR("Device not response: cmd %s property %s", cmd.c_str(), property.c_str());
        return false;
    }
    std::string response(read_buf);
    if(CommandSet::getValueFromResponse(response, property, res) == CMD_FALSE)
    {
        LOGF_ERROR("Communication error: cmd %s property %s resvalue: %s", cmd.c_str(), property.c_str(), res);
        return false;
    }
    LOGF_DEBUG("Received response: %s", res);
    tcflush(PortFD, TCIOFLUSH);

    return true;
}

bool CommandSet::getValueFromResponse(std::string response, std::string property, char *value)
{
    std::size_t property_pos = response.find(property);
    if(property_pos == std::string::npos)
    {
        LOGF_ERROR("Invalid property: %s", property.c_str());
        return false;
    }
    int property_length = std::string(property).length();
    response = response.substr(property_pos + property_length);
    std::size_t found = response.find(",");
    if(found != std::string::npos)
    {
        response = response.substr(0,found);
    }
    else
    {
        found = response.find("}");
        response = response.substr(0,found);
    }
    response = removeChars(response,'\"');
    response = removeChars(response,',');
    response = removeChars(response,':');
    strcpy(value,response.c_str());

    return true;
}

std::string CommandSet::removeChars(std::string str, char ch)
{
    std::size_t found = str.find(ch);
    if(found != std::string::npos)
    {
        str = str.erase(found,1);
        str = removeChars(str, ch);
    }

    return str;
}

bool CommandSet::getSerialNumber(char *res)
{
    return CommandSet::sendCmd("{\"req\":{\"get\":{\"SN\":\"\"}}}", "SN", res);
}

bool CommandSet::abort()
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"MOT_ABORT\":\"\"}}}}");
}

bool CommandSet::go(uint32_t targetTicks, char *res)
{
    char cmd[SESTO_LEN] = {0};
    snprintf(cmd,sizeof(cmd),"{\"req\":{\"cmd\":{\"MOT1\" :{\"GOTO\":%u}}}}", targetTicks);
    return CommandSet::sendCmd(cmd,"GOTO",res);
}

bool CommandSet::stop()
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"MOT_STOP\":\"\"}}}}");
}

bool CommandSet::goHome()
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"GOHOME\":\"\"}}}}","GOHOME");
}

bool CommandSet::fastMoveOut()
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"F_OUTW\":\"\"}}}}");
}

bool CommandSet::fastMoveIn()
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\":{\"MOT1\" :{\"F_INW\":\"\"}}}}");
}

bool CommandSet::getMaxPosition(char *res)
{
    return CommandSet::sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "CAL_MAXPOS", res);
}

bool CommandSet::storeAsMaxPosition(char *res)
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\": {\"MOT1\": {\"CAL_FOCUSER\": \"StoreAsMaxPos\"}}}}"), res;
}

bool CommandSet::goOutToFindMaxPos()
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\": {\"MOT1\": {\"CAL_FOCUSER\": \"GoOutToFindMaxPos\"}}}}");
}

bool CommandSet::storeAsMinPosition()
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\": {\"MOT1\": {\"CAL_FOCUSER\": \"StoreAsMinPos\"}}}}");
}

bool CommandSet::initCalibration()
{
    return CommandSet::sendCmd("{\"req\":{\"cmd\": {\"MOT1\": {\"CAL_FOCUSER\": \"Init\"}}}}");
}

bool CommandSet::getAbsolutePosition(char *res)
{
    return CommandSet::sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "ABS_POS", res);
}

bool CommandSet::getCurrentSpeed(char *res)
{
    return CommandSet::sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "SPEED", res);
}

bool CommandSet::loadSlowPreset(char *res)
{
    return sendCmd("{\"req\":{\"cmd\":{\"RUNPRESET\":\"slow\"}}}","RUNPRESET",res);
}

bool CommandSet::getMotorTemp(char *res)
{
    return sendCmd("{\"req\":{\"get\":{\"MOT1\":\"\"}}}", "NTC_T", res);
}






