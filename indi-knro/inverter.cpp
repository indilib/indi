/*
    Kuwait National Radio Observatory
    INDI driver for Baldor V/Hz Inverter
    Communication: RS485 Link over ModBus

    Copyright (C) 2009 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    Change Log:

*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

#include <memory>

#include <indicom.h>

#include "inverter.h"


/****************************************************************
**
**
*****************************************************************/
knroInverter::knroInverter(inverterType new_type)
{

  // Default value
  type = AZ_INVERTER;

  set_type(new_type);

  init();



}

/****************************************************************
**
**
*****************************************************************/
knroInverter::~knroInverter()
{




}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::init()
{

    IUFillSwitch(&MotionControlS[0], "FORWARD", "Forward", ISS_OFF);
    IUFillSwitch(&MotionControlS[1], "REVERSE", "Reverse", ISS_OFF);
    IUFillSwitch(&MotionControlS[2], "STOP", "Stop", ISS_OFF);
    IUFillNumber(&InverterSpeedN[0], "SPEED", "Hz", "%10.6m",  0., 50., 1., 0.);    
    IUFillText(&PortT[0], "PORT", "Port", "Dev1");
    
  if (type == AZ_INVERTER)
  {
  	IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "AZ_INVERTER_PORT", "AZ Port", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  	IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), mydev, "AZ_MOTION_CONTROL", "Az Motion", INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    IUFillNumberVector(&InverterSpeedNP, InverterSpeedN, NARRAY(InverterSpeedN), mydev, "AZ_SPEED" , "Az Speed", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);	
  }
  else
  {
  	IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "ALT_INVERTER_PORT", "ALT Port", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);
  	IUFillSwitchVector(&MotionControlSP, MotionControlS, NARRAY(MotionControlS), mydev, "ALT_MOTION_CONTROL", "Alt Motion", INVERTER_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
  	IUFillNumberVector(&InverterSpeedNP, InverterSpeedN, NARRAY(InverterSpeedN), mydev, "ALT_SPEED" , "Az Speed", INVERTER_GROUP, IP_RW, 0, IPS_IDLE);  	
  }
 
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::move_forward()
{
	
	
	
	
	
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::move_reverse()
{
	
	
	
	
	
}

/****************************************************************
**
**
*****************************************************************/
void knroInverter::stop()
{
	
	
	
	
	
	
	
}
      
void knroInverter::ISGetProperties()
{
   IDDefSwitch(&MotionControlSP, NULL);
   IDDefNumber(&InverterSpeedNP, NULL);
   IDDefText(&PortTP, NULL);
}
