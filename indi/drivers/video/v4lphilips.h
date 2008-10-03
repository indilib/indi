/*
    Phlips webcam INDI driver
    Copyright (C) 2003-2005 by Jasem Mutlaq

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

    2005.04.29  JM: There is no need for this file for Video 4 Linux 2. It is kept for V4L 1 compatibility.

*/

#ifndef V4LPHILIPS_H
#define V4LPHILIPS_H



#ifndef HAVE_LINUX_VIDEODEV2_H
#include "webcam/v4l1_pwc.h"
#endif 

#include "v4ldriver.h"

class V4L_Philips : public V4L_Driver
{
  public:
   V4L_Philips();
  ~V4L_Philips();

    /* INDI Functions that must be called from indidrivermain */
    void ISGetProperties (const char *dev);
    void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

    void initCamBase();
    void initProperties(const char *dev);
    void connectCamera(void);

    private:
    
    /* Switches */
    ISwitch BackLightS[2];
    ISwitch AntiFlickerS[2];
    ISwitch NoiseReductionS[4];
    ISwitch CamSettingS[3];
    ISwitch WhiteBalanceModeS[5];


    /* Nmubers */
    INumber WhiteBalanceN[2];
    INumber ShutterSpeedN[1];

    /* Switch Vectors */
    ISwitchVectorProperty BackLightSP;
    ISwitchVectorProperty AntiFlickerSP;
    ISwitchVectorProperty NoiseReductionSP;
    ISwitchVectorProperty CamSettingSP;
    ISwitchVectorProperty WhiteBalanceModeSP;

    /* Number Vectors */
    INumberVectorProperty WhiteBalanceNP;
    INumberVectorProperty ShutterSpeedNP;

    #ifndef HAVE_LINUX_VIDEODEV2_H
    V4L1_PWC * v4l_pwc;
    void updateV4L1Controls();
    void getBasicData(void);
    #endif

};

#endif

