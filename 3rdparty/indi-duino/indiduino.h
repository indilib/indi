/*
    Induino general propose driver. Allow using arduino boards
    as general I/O 	
    Copyright 2012 (c) Nacho Mas (mas.ignacio at gmail.com)

    Base on INDIDUINO Four
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

*/

#ifndef INDIDUINO_FOUR_H
#define INDIDUINO_FOUR_H

#include <defaultdevice.h>
#include <indicom.h>

/* Firmata */
#include "firmata.h"

/* NAMES in the xml skeleton file to 
   used to define I/O arduino mapping*/

#define PROP_NAME_AOUTPUT "AOUTPUT"
#define PROP_NAME_DOUTPUT "DOUTPUT"
#define PROP_NAME_AINPUT  "AINPUT_"
#define PROP_NAME_DINPUT  "DINPUT_"


class indiduino : public INDI::DefaultDevice
{
 public:
 indiduino();
 ~indiduino();

 virtual void ISGetProperties (const char *dev);
 virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
 virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
 virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
 virtual bool ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
 virtual void ISPoll ();

private:
 const char *getDefaultName();
 virtual bool initProperties(); 
 virtual bool Connect();
 virtual bool Disconnect();
 void setPinModesFromSK(); 
 bool is_connected(void);
 Firmata* sf;

};

#endif
