#include "rbfocus.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

static std::unique_ptr<RBFOCUS> rbfocus(new RBFOCUS());

void ISGetProperties(const char * dev)
{
    rbfocus->ISGetProperties(dev);
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    rbfocus->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    rbfocus->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    rbfocus->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
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

void ISSnoopDevice(XMLEle * root)
{
    rbfocus->ISSnoopDevice(root);
}

RBFOCUS::RBFOCUS()
{
    // Absolute, Abort, and Sync
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);
    setVersion(1, 0);
}

bool RBFOCUS::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    focuserHoldSP[HOLD_ON].fill("HOLD_ON", "Hold Enabled", ISS_OFF);
    focuserHoldSP[HOLD_OFF].fill("HOLD_OFF", "Hold Disabled", ISS_OFF);
    focuserHoldSP.fill(getDeviceName(), "Focuser Hold", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    dirSP[NORMAL].fill("NORMAL", "Normal", ISS_OFF);
    dirSP[REVERSED].fill("REVERSED", "Reverse", ISS_OFF);
    dirSP.fill(getDeviceName(), "Direction", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);


    // Relative and absolute movement
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(50000.);
    FocusRelPosNP[0].setValue(0.);
    FocusRelPosNP[0].setStep(1000);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(100000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);


    return true;
}

bool RBFOCUS::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        defineProperty(TemperatureNP);
        defineProperty(focuserHoldSP);
        defineProperty(dirSP);
        LOG_INFO("Focuser ready.");
    }
    else
    {
        deleteProperty(TemperatureNP.getName());
        deleteProperty(focuserHoldSP.getName());
        deleteProperty(dirSP.getName());
    }

    return true;
}

bool RBFOCUS::Handshake()
{
    if (Ack())
    {

        LOG_INFO("RBF is online.");
        readVersion();
        MaxPos();
        readHold();
        readDir();
        return true;
    }

    LOG_INFO("Error retrieving data from RBFocuser, please ensure RBFocus controller is powered and the port is correct.");
    return false;
}

const char * RBFOCUS::getDefaultName()
{
    return "RB Focuser";
}

bool RBFOCUS::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[5] = {0};

    tcflush(PortFD, TCIOFLUSH);

    int numChecks = 0;
    bool success = false;
    while (numChecks < 3 && !success)
    {
        numChecks++;
        //wait 1 second between each test.
        sleep(1);

        bool transmissionSuccess = (rc = tty_write(PortFD, "#", 1, &nbytes_written)) == TTY_OK;
        if(!transmissionSuccess)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Handshake Attempt %i, tty transmission error: %s.", numChecks, errstr);
        }

        bool responseSuccess = (rc = tty_read(PortFD, resp, 4, DRIVER_TIMEOUT, &nbytes_read)) == TTY_OK;
        if(!responseSuccess)
        {
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Handshake Attempt %i, updatePosition response error: %s.", numChecks, errstr);
        }

        success = transmissionSuccess && responseSuccess;
    }

    if(!success)
    {
        LOG_INFO("Handshake failed after 3 attempts");
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    return !strcmp(resp, "OK!#");
}


bool RBFOCUS::readTemperature()
{
    char res[DRIVER_RES] = {0};

       if (sendCommand("Q#", res) == false)
           return false;

       int32_t temp = 0;
       int rc = sscanf(res, "C%d#", &temp);
       if (rc > 0)
           // Hundredth of a degree
           TemperatureNP[0].setValue(temp / 100.0);
       else
       {
           LOGF_ERROR("Unknown error: focuser temperature value (%s)", res);
           return false;
       }

       return true;
}

bool RBFOCUS::readVersion()
{
   return true;
}
bool RBFOCUS::readHold()
{
    char res[DRIVER_RES] = {0};

    if (sendCommand("V#", res) == false){
        return false;
}

        if(strcmp(res, "Enable")==0)
        {
            focuserHoldSP[HOLD_ON].setState(ISS_ON);

        }
        else if (strcmp(res, "Disable")==0)
        {
            focuserHoldSP[HOLD_OFF].setState(ISS_ON);


        }



    return true;
}
bool RBFOCUS::readDir()
{
    char res[DRIVER_RES] = {0};

    if (sendCommand("B#", res) == false){
        return false;
}

        if(strcmp(res, "Reversed")==0)
        {
            dirSP[REVERSED].setState(ISS_ON);

        }
        else if (strcmp(res, "Normal")==0)
        {
            dirSP[NORMAL].setState(ISS_ON);


        }



    return true;
}
bool RBFOCUS::readPosition()
{
    char res[DRIVER_RES] = {0};

    if (sendCommand("P#", res) == false)
        return false;

    int32_t pos;
    int rc = sscanf(res, "%d#", &pos);

    if (rc > 0)
        FocusAbsPosNP[0].setValue(pos);
    else
    {
        return false;
    }

    return true;
}

bool RBFOCUS::isMoving()
{
    char res[DRIVER_RES] = {0};

    if (sendCommand("J#", res) == false)
        return false;

    if (strcmp(res, "M1:OK") == 0)
        return true;
    else if (strcmp(res, "M0:OK") == 0)
        return false;

    LOGF_ERROR("Unknown error: isMoving value (%s)", res);
    return false;

}

bool RBFOCUS::MaxPos(){
    char res[DRIVER_RES] = {0};

    if (sendCommand("X#", res) == false)
        return false;

    uint32_t mPos = 0;
    int rc = sscanf(res, "%u#", &mPos);
     if (rc >0){

        FocusMaxPosNP[0].setValue(mPos);
        RBFOCUS::SyncPresets(mPos);
       }else
     {
         LOGF_ERROR("Invalid Response: focuser hold value (%s)", res);
         return false;
     }

    return true;


}

bool RBFOCUS::SetFocuserMaxPosition(uint32_t mPos)
{
    char cmd[DRIVER_RES] = {0};

    snprintf(cmd, DRIVER_RES, "H%d#", mPos);

    if(sendCommand(cmd))
    {
        Focuser::SyncPresets(mPos);

        return true;
    }
    return false;
}
bool RBFOCUS::SyncFocuser(uint32_t ticks)
{
    char cmd[DRIVER_RES] = {0};
    snprintf(cmd, DRIVER_RES, "I%d#", ticks);
    return sendCommand(cmd);
}

IPState RBFOCUS::MoveAbsFocuser(uint32_t targetTicks)
{
    char cmd[DRIVER_RES] = {0};
    snprintf(cmd, DRIVER_RES, "T%d#", targetTicks);

    if (sendCommand(cmd) == false)
        return IPS_BUSY;

    targetPos = targetTicks;
        return IPS_BUSY;
}



void RBFOCUS::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    bool rc = readPosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosNP[0].getValue()) > 5)
        {
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
        }
    }

    rc = readTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureNP[0].getValue()) >= 0.5)
        {
            TemperatureNP.apply();
            lastTemperature = TemperatureNP[0].getValue();
        }
    }

    if (FocusAbsPosNP.getState() == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.setState(IPS_OK);
            FocusAbsPosNP.apply();
            lastPos = FocusAbsPosNP[0].getValue();
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool RBFOCUS::AbortFocuser()
{
    return sendCommand("L#");
}
bool RBFOCUS::setHold()
{
    return sendCommand("C#");
}
bool RBFOCUS::setDir()
{
    return sendCommand("D#");
}
bool RBFOCUS::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n){
    if (focuserHoldSP.isNameMatch(name))
    {
        int current_mode = focuserHoldSP.findOnSwitchIndex();

        focuserHoldSP.update(states, names, n);

        int target_mode = focuserHoldSP.findOnSwitchIndex();

        if (current_mode == target_mode)
        {
            focuserHoldSP.setState(IPS_OK);
            focuserHoldSP.apply();
        }

        bool rc = setHold();
        if (!rc)
        {
            focuserHoldSP.reset();
            focuserHoldSP[current_mode].setState(ISS_ON);
            focuserHoldSP.setState(IPS_ALERT);
            focuserHoldSP.apply();
            return false;
        }

        focuserHoldSP.setState(IPS_OK);
        focuserHoldSP.apply();
        return true;
    }

    if (dirSP.isNameMatch(name))
    {
        int current_mode = dirSP.findOnSwitchIndex();

        dirSP.update(states, names, n);

        int target_mode = dirSP.findOnSwitchIndex();

        if (current_mode == target_mode)
        {
            dirSP.setState(IPS_OK);
            dirSP.apply();
        }

        bool rc = setDir();
        if (!rc)
        {
            dirSP.reset();
            dirSP[current_mode].setState(ISS_ON);
            dirSP.setState(IPS_ALERT);
            dirSP.apply();
            return false;
        }

        dirSP.setState(IPS_OK);
        dirSP.apply();
        return true;
    }


    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}


bool RBFOCUS::sendCommand(const char * cmd, char * res)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    if ((rc = tty_write_string(PortFD, cmd, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if ((rc = tty_nread_section(PortFD, res, DRIVER_RES, DRIVER_DEL, DRIVER_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    // Remove the #
    res[nbytes_read - 1] = 0;

    LOGF_DEBUG("RES <%s>", res);

    tcflush(PortFD, TCIOFLUSH);

    return true;
}
