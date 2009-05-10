/*
    Kuwait National Radio Observatory
    INDI Driver for 24 bit AMCI Encoders
    Communication: RS485 Link, Binary

    Copyright (C) 2009 Jasem Mutlaq (mutlaqja@ikarustech.com)
    				   Bader Almithan (bq8000@hotmail.com)

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

#include "encoder.h"

/****************************************************************
**
**
*****************************************************************/
knroEncoder::knroEncoder(encoderType new_type)
{

  // Default value
  type = AZ_ENCODER;

  setType(new_type);

  init();



}

/****************************************************************
**
**
*****************************************************************/
knroEncoder::~knroEncoder()
{




}

/****************************************************************
**
**
*****************************************************************/
void knroEncoder::init()
{

  IUFillNumber(&EncoderAbsPosN[0], "Value" , "", "%g", 0., 500000., 0., 0.);
  IUFillText(&PortT[0], "PORT", "Port", "Dev1");

  if (type == AZ_ENCODER)
  {
  	IUFillNumberVector(&EncoderAbsPosNP, EncoderAbsPosN, NARRAY(EncoderAbsPosN), mydev, "Absolute Az", "", ENCODER_GROUP, IP_RO, 0, IPS_OK);
  
  IUFillTextVector(&PortTP, PortT, NARRAY(PortT), mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_IDLE);


  }
  else
  	IUFillNumberVector(&EncoderAbsPosNP, EncoderAbsPosN, NARRAY(EncoderAbsPosN), mydev, "Absolute Alt", "", ENCODER_GROUP, IP_RO, 0, IPS_OK);
 
}

void knroEncoder::ISGetProperties()
{
   IDDefNumber(&EncoderAbsPosNP, NULL);
}
