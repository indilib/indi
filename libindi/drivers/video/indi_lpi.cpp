/*
    Meade LPI Experimental driver
    Copyright (C) 2005 by Jasem Mutlaq

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

#include "v4ldriver.h"

class Meade_LPI : public V4L_Driver
{
  public:
    Meade_LPI();
   ~Meade_LPI();

  #ifdef HAVE_LINUX_VIDEODEV2_H
  void connectCamera(void);
  #endif

};

Meade_LPI::Meade_LPI() : V4L_Driver()
{
}

Meade_LPI::~Meade_LPI()
{
}

#ifdef HAVE_LINUX_VIDEODEV2_H
void Meade_LPI::connectCamera()
{
  char errmsg[ERRMSGSIZ];

  switch (PowerS[0].s)
  {
     case ISS_ON:
      if (v4l_base->connectCam(PortT[0].text, errmsg, V4L2_PIX_FMT_SBGGR8, 352, 288) < 0)
      {
	  PowerSP.s = IPS_IDLE;
	  PowerS[0].s = ISS_OFF;
	  PowerS[1].s = ISS_ON;
	  IDSetSwitch(&PowerSP, "Error: unable to open device");
	  IDLog("Error: %s\n", errmsg);
	  return;
      }
      
      /* Sucess! */
      PowerS[0].s = ISS_ON;
      PowerS[1].s = ISS_OFF;
      PowerSP.s = IPS_OK;
      IDSetSwitch(&PowerSP, "Meade LPI is online. Retrieving basic data.");

      v4l_base->registerCallback(newFrame, this);
      
      V4LFrame->compressedFrame = (unsigned char *) malloc (sizeof(unsigned char) * 1);
      
      IDLog("Meade LPI is online. Retrieving basic data.\n");
      getBasicData();
      
      break;
      
    case ISS_OFF:
      PowerS[0].s = ISS_OFF;
      PowerS[1].s = ISS_ON;
      PowerSP.s = IPS_IDLE;
      
      free(V4LFrame->compressedFrame);
      V4LFrame->compressedFrame = NULL;
      v4l_base->disconnectCam(true);
      
      IDSetSwitch(&PowerSP, "Meade LPI is offline.");
      
      break;
     }

}
#endif

Meade_LPI *MainCam = NULL;		/* Main and only camera */

/* send client definitions of all properties */
void ISInit()
{
  if (MainCam == NULL)
  {
    MainCam = new Meade_LPI();
    MainCam->initProperties("Meade LPI");
    MainCam->initCamBase();
  }
}
    
void ISGetProperties (const char *dev)
{ 
   ISInit();
  
  MainCam->ISGetProperties(dev);
}


void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	
  ISInit();
  
  MainCam->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
  
   ISInit();
   
   MainCam->ISNewText(dev, name, texts, names, n);
}


void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
      
  ISInit();
  
  MainCam->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) 
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
void ISSnoopDevice (XMLEle *root) 
{
  INDI_UNUSED(root);
}


