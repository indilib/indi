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
    2009-04-26: Creating encoder class (BA)

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

#include <indidevapi.h>
#include <indicom.h>

#include "knro_common.h"

class knroEncoder
{

public:

        enum encoderType { AZ_ENCODER, ALT_ENCODER };

	knroEncoder(encoderType new_type);
	~knroEncoder();

      void ISGetProperties();
      unsigned int get_abs_encoder_count() { return abs_encoder_count; }
      double get_current_angel() { return current_angle; }

      void setType(encoderType new_type) { type = new_type; }
      encoderType getType() { return type; }

     
private: 

        // INDI Properties

	// Encoder Absolute Position
        INumber EncoderAbsPosN[1];
	INumberVectorProperty EncoderAbsPosNP;
	
	// Encoder Port number
	ITextVectorProperty PortTP;
	IText PortT[1];

	// Functions
	void init();

	// Variable 
	unsigned int abs_encoder_count;
	double current_angle;
	encoderType type;


};

