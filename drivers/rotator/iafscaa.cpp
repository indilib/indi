/*
    iOptron iAFS Focuser Rotator INDI driver

    Copyright (C) 2024 Jasem Mutlaq
    Copyright (C) 2025 Joe Zhou

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
*/

#include "iafscaa.h"
#include "indicom.h"

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <memory>
#include <connectionplugins/connectionserial.h>

#define iEAFFOCUS_TIMEOUT 10
#define TEMPERATURE_THRESHOLD 0.1
#define ROTATOR_TAB "Rotator"

std::unique_ptr<iAFSRotator> iafsRotator(new iAFSRotator());

/************************************************************************************
*
* ***********************************************************************************/
iAFSRotator::iAFSRotator(): RotatorInterface(this)
{
    setVersion(1, 1);
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_REVERSE);
 // Rotator capabilities
    RI::SetCapability(ROTATOR_CAN_ABORT | ROTATOR_CAN_SYNC | ROTATOR_CAN_REVERSE);

}

/************************************************************************************
*
* ***********************************************************************************/

iAFSRotator::~iAFSRotator()
{
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::initProperties()
{
    INDI::Focuser::initProperties();

    setDefaultPollingPeriod(1500);
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%2.2f", 0., 50., 0., 50.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    SetZeroSP[0].fill("SETZERO", "Sync Focuser Position To 0", ISS_OFF);
    SetZeroSP.fill(getDeviceName(), "Zero Position", "Zero Position", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    /* Relative and absolute movement */
    FocusRelPosNP[0].setMin(0.);
    FocusRelPosNP[0].setMax(5000.);
    FocusRelPosNP[0].setValue(0.);
    FocusRelPosNP[0].setStep(10.);

    FocusAbsPosNP[0].setMin(0.);
    FocusAbsPosNP[0].setMax(99999.);
    FocusAbsPosNP[0].setValue(0.);
    FocusAbsPosNP[0].setStep(10.);

    // Rotator Properties
    INDI::RotatorInterface::initProperties(ROTATOR_TAB);

 // Firmware
    FirmwareTP[0].fill("VERSION", "Version", "NA");
    FirmwareTP.fill(getDeviceName(), "FIRMWARE_INFO", "Firmware", ROTATOR_TAB, IP_RO, 60,
                    IPS_IDLE);
    StatusTP[0].fill("STATUS", "CAA Status", "NA");
    StatusTP.fill(getDeviceName(), "Status_INFO", "Status", ROTATOR_TAB, IP_RO, 60,
                    IPS_IDLE);

 // Rotator size
    RotatorSize[0].fill("CAASIZE", "CAA Size (inch)", "%.1f", 2.0, 4.0, 1.0, 0);
    RotatorSize.fill(getDeviceName(), "ROTATOR_SIZE", "CAA Size", ROTATOR_TAB, IP_RW,
                    60, IPS_IDLE);
    addAuxControls();
    setDriverInterface(getDriverInterface() | ROTATOR_INTERFACE);
    return true;
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::updateProperties()
{
    INDI::Focuser::updateProperties();
    if (isConnected())
    {
        defineProperty(TemperatureNP);
        defineProperty(SetZeroSP);
 // Rotator Properties
        INDI::RotatorInterface::updateProperties();
	defineProperty(FirmwareTP);
	defineProperty(RotatorSize);
        defineProperty(StatusTP);
	GetFocusParams();
    }
    else
    {
        deleteProperty(TemperatureNP);
        deleteProperty(SetZeroSP);
 // Rotator Properties
        INDI::RotatorInterface::updateProperties();
 	deleteProperty(FirmwareTP);
	deleteProperty(RotatorSize);
        deleteProperty(StatusTP);
    }

    return true;

}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::Handshake()
{

    if (Ack())
    {
        DEBUG(INDI::Logger::DBG_SESSION, "iAFSCAA Focuse Rotater is online. Getting parameters...");
        getFirmware();
        return true;
    }
    return false;
}

/************************************************************************************
*
* ***********************************************************************************/

const char *iAFSRotator::getDefaultName()
{
    return "iAFSRotator";
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::Ack()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char resp[16];
    int ieafpos, ieafmodel, ieaflast;
    sleep(2);
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":DeviceInfo#", 12, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Init send getdeviceinfo  error: %s.", errstr);
        return false;
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT * 2, &nbytes_read)) != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "Init read deviceinfo error: %s.", errstr);
        return false;
    }
    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';
    sscanf(resp, "%6d%2d%4d", &ieafpos, &ieafmodel, &ieaflast);
    // iAFSCAA Focuser Rotator  02=iEAF 03=iAFS 04=iAFS no CAA   05=iAFS+CAA
    if (ieafmodel == 5)
    {
        return true;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "Ack Response: %s", resp);
        return false;
    }

    return true;
}

/************************************************************************************
*
* ***********************************************************************************/
bool iAFSRotator::getFirmware()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16] = {0};
    char joefirmwareinfo[16] = {0};
    char iafsfirm[7] = {0};
    char caafirm[7] = {0};
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FW1#", 5, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "get firmware error: %s.", errstr);
    }
    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "get firmware  error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);
    resp[nbytes_read] = '\0';
    sscanf(resp, "%12s", joefirmwareinfo);
    rc = sscanf(resp, "%6s%6s", iafsfirm, caafirm);
    FirmwareTP[0].setText(caafirm);
    FirmwareTP.apply();
    return true;

}
/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::updateInfo()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[16] = {0};
    char joedeviceinfo[16] = {0};
    int ieafpos, ieafmove, ieaftemp, ieafdir;

    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateInfo error: %s.", errstr);
    }
    sleep(5);
    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateInfo  error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';

    sscanf(resp, "%14s", joedeviceinfo);
    rc = sscanf(resp, "%7d%1d%5d%1d", &ieafpos, &ieafmove, &ieaftemp, &ieafdir);
//    LOGF_INFO("******Focuser Response: %s", resp);

    if (rc != 4)
    {
        LOGF_ERROR("Could not parse response %s", resp);
        return false;
    }

    m_isMoving = ieafmove == 1;
    auto temperature = ieaftemp / 100.0 - 273.15;
    m_Reversed = (ieafdir == 0);
    auto currentlyReversed = FocusReverseSP[INDI_ENABLED].getState() == ISS_ON;

    // Only update if there is change
    if (std::abs(temperature - TemperatureNP[0].getValue()) > TEMPERATURE_THRESHOLD)
    {
        TemperatureNP[0].setValue(temperature);
        TemperatureNP.apply();
    }

    // Only update if there is change

    if (m_Reversed != currentlyReversed)
    {
        FocusReverseSP[INDI_ENABLED].setState(m_Reversed ? ISS_ON : ISS_OFF);
        FocusReverseSP[INDI_DISABLED].setState(m_Reversed ? ISS_OFF : ISS_ON);
        FocusReverseSP.setState(IPS_OK);
        FocusReverseSP.apply();
    }

    // If absolute position is different, let's update
    if (ieafpos != FocusAbsPosNP[0].getValue())
    {
        FocusAbsPosNP[0].setValue(ieafpos);
        // Check if we are busy or not.
        if ((m_isMoving == false && FocusAbsPosNP.getState() == IPS_BUSY) || (m_isMoving && FocusAbsPosNP.getState() != IPS_BUSY))
        {
            FocusAbsPosNP.setState(m_isMoving ? IPS_BUSY : IPS_OK);
            FocusRelPosNP.setState(m_isMoving ? IPS_BUSY : IPS_OK);
            FocusRelPosNP.apply();
        }
        FocusAbsPosNP.apply();
    }
    // Update status if required
    else  if ((m_isMoving == false && FocusAbsPosNP.getState() == IPS_BUSY) || (m_isMoving
              && FocusAbsPosNP.getState() != IPS_BUSY))
    {
        FocusAbsPosNP.setState(m_isMoving ? IPS_BUSY : IPS_OK);
        FocusRelPosNP.setState(m_isMoving ? IPS_BUSY : IPS_OK);
        FocusRelPosNP.apply();
        FocusAbsPosNP.apply();
    }

    return true;
}

/************************************************************************************
*
* ***********************************************************************************/
bool iAFSRotator::updateInforotator()
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;
    char errstr[MAXRBUF];
    char resp[25] = {0};
    char joedeviceinfo[25] = {0};
    int ieafposdelta, ieafmove, ieafcurrpos, ieafdir,ieafsize;
    double ieafcurrangle;
    double fieafsize;
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":RI#", 4, &nbytes_written)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateInfoRotator error: %s.", errstr);
    }

    if ( (rc = tty_read_section(PortFD, resp, '#', iEAFFOCUS_TIMEOUT, &nbytes_read)) != TTY_OK)
    {
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "updateInfoRotator  error: %s.", errstr);
        return false;
    }

    tcflush(PortFD, TCIOFLUSH);

    resp[nbytes_read] = '\0';

    sscanf(resp, "%23s", joedeviceinfo);
    rc = sscanf(resp, "%10d%1d%9d%1d%2d", &ieafposdelta, &ieafmove, &ieafcurrpos, &ieafdir,&ieafsize);


    if (rc != 5)
    {
        LOGF_ERROR("Could not parse response %s", resp);
        return false;
    }

    mr_isMoving = ieafmove == 1;
    StatusTP[0].setText(mr_isMoving ? "Move" : "Stop");
    StatusTP.apply();
    mr_Reversed = (ieafdir == 0);
//    ieafcaadelta = ieafposdelta;
    fieafsize = ieafsize/10.0;
    if (fieafsize != RotatorSize[0].getValue())
    {
        RotatorSize[0].setValue(fieafsize);
	RotatorSize.setState(IPS_OK);
        RotatorSize.apply();
    }

    auto currentlyReversed = ReverseRotatorSP[INDI_ENABLED].getState() == ISS_ON;

    // Only update if there is change
    if (mr_Reversed != currentlyReversed)
    {
        ReverseRotatorSP[INDI_ENABLED].setState(mr_Reversed ? ISS_ON : ISS_OFF);
        ReverseRotatorSP[INDI_DISABLED].setState(mr_Reversed ? ISS_OFF : ISS_ON);
        ReverseRotatorSP.setState(IPS_OK);
        ReverseRotatorSP.apply();
    }

    // If absolute position is different, let's update
    ieafcurrangle=ieafcurrpos/360000.0;
    if ( ieafcurrangle>=360.0)
    {
	 ieafcurrangle= ieafcurrangle-360;
    }
    if ( ieafcurrangle<0.0)
    {
         ieafcurrangle= ieafcurrangle+360;
    }

//    LOGF_INFO("*******Rotator curr angle  : %f", ieafcurrangle);
    if (ieafcurrangle !=  GotoRotatorNP[0].getValue())
    {
         GotoRotatorNP[0].setValue(ieafcurrangle);
	 GotoRotatorNP.apply();
        // Check if we are busy or not.

        if ((mr_isMoving == false &&  GotoRotatorNP.getState() == IPS_BUSY) || (mr_isMoving &&  GotoRotatorNP.getState() != IPS_BUSY))
        {
             GotoRotatorNP.setState(m_isMoving ? IPS_BUSY : IPS_OK);
	     GotoRotatorNP.apply();
       }
    }
    // Update status if required
    else  if ((mr_isMoving == false &&  GotoRotatorNP.getState() == IPS_BUSY) || (mr_isMoving
              &&  GotoRotatorNP.getState() != IPS_BUSY))
    {
         GotoRotatorNP.setState(m_isMoving ? IPS_BUSY : IPS_OK);
         GotoRotatorNP.apply();
    }

    return true;
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::SetFocuserMaxPosition(uint32_t ticks)
{
    INDI_UNUSED(ticks);
    return false;
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::MoveMyFocuser(uint32_t position)
{
    int nbytes_written = 0, rc = -1;
    char cmd[12] = {0};

    snprintf(cmd, 12, ":FM%07u#", position);

    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setPosition error: %s.", errstr);
        return false;
    }
    return true;
}


/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::ReverseFocuser(bool enabled)
{
    int nbytes_written = 0, rc = -1;

    // If there is no change, return.
    if (enabled == m_Reversed)
        return true;

    // Change Direction
//DEBUG(INDI::Logger::DBG_SESSION, "iAFSCAA Focuse Reverse change dir FR command...");

    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FR#", 4, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "change Direction error: %s.", errstr);
        return false;
    }

    return true;
}

/************************************************************************************
*
* ***********************************************************************************/

void iAFSRotator::setZero()
{
    int nbytes_written = 0, rc = -1;
    // Set Zero
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, ":FZ#", 4, &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        DEBUGF(INDI::Logger::DBG_ERROR, "set Zero error: %s.", errstr);
        return;
    }
    return;
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (SetZeroSP.isNameMatch(name))
        {
            setZero();
            SetZeroSP.setState(IPS_OK);
            SetZeroSP.apply();
            return true;
        }
 	if (ReverseRotatorSP.isNameMatch(name))
        {
	    auto currentlyReversed = ReverseRotatorSP[INDI_ENABLED].getState() == ISS_OFF;
            ReverseRotator(currentlyReversed);
            ReverseRotatorSP.setState(IPS_OK);
            ReverseRotatorSP.apply();
            return true;
        }
	if (AbortRotatorSP.isNameMatch(name))
        {
	    AbortRotator();
	    GotoRotatorNP.setState(IPS_IDLE);
            GotoRotatorNP.apply();
            AbortRotatorSP.setState(IPS_OK);
            AbortRotatorSP.apply();
            return true;
        }


    }

    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

/************************************************************************************
*
* ***********************************************************************************/

void iAFSRotator::GetFocusParams ()
{
    updateInfo();
    updateInforotator();
}

/************************************************************************************
*
* ***********************************************************************************/

IPState iAFSRotator::MoveAbsFocuser(uint32_t targetTicks)
{
    uint32_t targetPos = targetTicks;

    bool rc = false;

    rc = MoveMyFocuser(targetPos);

    if (rc == false)
        return IPS_ALERT;

    FocusAbsPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

/************************************************************************************
*
* ***********************************************************************************/

IPState iAFSRotator::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int relativeTicks =  ((dir == FOCUS_INWARD) ? -ticks : ticks) * (m_Reversed ? -1 : 1);
    uint32_t newPosition = FocusAbsPosNP[0].getValue() + relativeTicks;
    newPosition = std::max(0u, std::min(static_cast<uint32_t>(FocusAbsPosNP[0].getMax()), newPosition));

    auto rc = MoveMyFocuser(newPosition);

    if (rc == false)
        return IPS_ALERT;

    FocusRelPosNP[0].setValue(ticks);
    FocusRelPosNP.setState(IPS_BUSY);

    return IPS_BUSY;
}

/************************************************************************************
*
* ***********************************************************************************/

void iAFSRotator::TimerHit()
{
    if (!isConnected())
        return;

    updateInfo();
    updateInforotator();
    SetTimer(getPollingPeriod());
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::AbortFocuser()
{
    int nbytes_written;
    tcflush(PortFD, TCIOFLUSH);
    if (tty_write(PortFD, ":FQ#", 4, &nbytes_written) == TTY_OK)
    {
        FocusAbsPosNP.setState(IPS_IDLE);
        FocusRelPosNP.setState(IPS_IDLE);
        FocusAbsPosNP.apply();
        FocusRelPosNP.apply();
        return true;
    }
    else
        return false;
}


/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::AbortRotator()
{
    int nbytes_written;
    tcflush(PortFD, TCIOFLUSH);
    DEBUG(INDI::Logger::DBG_SESSION, "iAFSCAA Abort Moving...");
    if (tty_write(PortFD, ":RQ#", 4, &nbytes_written) == TTY_OK)
    {
        GotoRotatorNP.setState(IPS_IDLE);
        GotoRotatorNP.apply();
        return true;
    }
    else
        return false;
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::SyncRotator(double angle)
{
    int nbytes_written = 0, rc = -1;
    char cmd[14] = {0};
    int position=round(angle*3600*100);
    snprintf(cmd, 14, ":RY%09u#", position);
    // Set Position
    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setPosition error: %s.", errstr);
	return false;
    }
   GotoRotatorNP[0].setValue(angle);
   GotoRotatorNP.setState(IPS_IDLE);
   GotoRotatorNP.apply();
   return true;
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::ReverseRotator(bool enabled)
{
    int nbytes_written = 0, rc = -1;

    // If there is no change, return.
    if (enabled)
	{
        tcflush(PortFD, TCIOFLUSH);
        if ( (rc = tty_write(PortFD, ":RR0#", 5, &nbytes_written)) != TTY_OK)
                {
                char errstr[MAXRBUF];
                tty_error_msg(rc, errstr, MAXRBUF);
                DEBUGF(INDI::Logger::DBG_ERROR, "change Rotator Direction error: %s.", errstr);
                return false;
                }
        ReverseRotatorSP[INDI_ENABLED].setState(ISS_ON);
        ReverseRotatorSP[INDI_DISABLED].setState(ISS_OFF);
        ReverseRotatorSP.setState(IPS_OK);
        ReverseRotatorSP.apply();
        return true;
	}
    else {

        tcflush(PortFD, TCIOFLUSH);
        if ( (rc = tty_write(PortFD, ":RR1#", 5, &nbytes_written)) != TTY_OK)
                {
                char errstr[MAXRBUF];
                tty_error_msg(rc, errstr, MAXRBUF);
                DEBUGF(INDI::Logger::DBG_ERROR, "change Rotator Direction error: %s.", errstr);
                return false;
                }
        ReverseRotatorSP[INDI_ENABLED].setState(ISS_OFF);
        ReverseRotatorSP[INDI_DISABLED].setState(ISS_ON);
        ReverseRotatorSP.setState(IPS_OK);
        ReverseRotatorSP.apply();
	return true;
        }
}
/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::MoveMyRotator(double angle)
{
    int nbytes_written = 0, rc = -1;
    char cmd[14] = {0};
    int position=(angle*3600*100);
    snprintf(cmd, 14, ":RM%09u#", position);
    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setPosition error: %s.", errstr);
        return false;
    }

    GotoRotatorNP.setState(IPS_BUSY);

    tcflush(PortFD, TCIFLUSH);

    return true;
}

/************************************************************************************
 *
* ***********************************************************************************/
IPState iAFSRotator::MoveRotator(double angle)
{
    IPState state = MoveAbsRotatorAngle(angle);
    GotoRotatorNP.setState(state);
    GotoRotatorNP.apply();

    return state;

}


/************************************************************************************
*
* ***********************************************************************************/
IPState iAFSRotator::MoveAbsRotatorAngle(double angle)
{
    int nbytes_written = 0, rc = -1;
    char cmd[14] = {0};
    int position=(angle*3600*100);
    snprintf(cmd, 14, ":RM%09u#", position);
    tcflush(PortFD, TCIOFLUSH);

    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("setPosition error: %s.", errstr);
        return IPS_ALERT;
    }

    GotoRotatorNP.setState(IPS_BUSY);

    tcflush(PortFD, TCIFLUSH);

    return IPS_BUSY;

}

/************************************************************************************
*
* ***********************************************************************************/
bool iAFSRotator::setCaaSize(double caasize)
{
    int nbytes_written = 0, rc = -1;
    char cmd[14] = {0};
    int position=round(caasize*10);
    snprintf(cmd, 14, ":RS%02u#", position);
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, cmd, strlen(cmd), &nbytes_written)) != TTY_OK)
    {
        char errstr[MAXRBUF];
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("set CAA Size error: %s.", errstr);
        return false;
    }

    return true;
}

/************************************************************************************
*
* ***********************************************************************************/

bool iAFSRotator::ISNewNumber (const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
       if (GotoRotatorNP.isNameMatch(name))
        {

            GotoRotatorNP.update(values, names, n);
            MoveMyRotator(GotoRotatorNP[0].getValue());
            GotoRotatorNP.setState(IPS_OK);
            GotoRotatorNP.apply();
            return true;
        }

       if (SyncRotatorNP.isNameMatch(name))
        {

            SyncRotatorNP.update(values, names, n);
            if (SyncRotator(SyncRotatorNP[0].getValue()))
                SyncRotatorNP.setState(IPS_OK);
            else
                SyncRotatorNP.setState(IPS_ALERT);
            SyncRotatorNP.apply();
            return true;
        }

       if (RotatorSize.isNameMatch(name))
        {

            RotatorSize.update(values, names, n);
            if (setCaaSize(RotatorSize[0].getValue()))
                RotatorSize.setState(IPS_OK);
            else
                RotatorSize.setState(IPS_ALERT);
            RotatorSize.apply();
            return true;
        }
    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}


