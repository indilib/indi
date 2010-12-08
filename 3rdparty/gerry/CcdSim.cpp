/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/
#include "CcdSim.h"

IndiDevice * _create_device()
{
    IndiDevice *ccd;
    IDLog("Create a Ccd Simulator\n");
    ccd=new CcdSim();
    return ccd;
}



CcdSim::CcdSim()
{
    //ctor
    testvalue=0;
    AbortGuideFrame=false;
}

CcdSim::~CcdSim()
{
    //dtor
}

char * CcdSim::getDefaultName()
{
    return (char *)"CcdSimulator";
}

int CcdSim::init_properties()
{
    //  Most hardware layers wont actually have indi properties defined
    //  but the simulators are a special case
    IndiCcd::init_properties();
    return 0;
}

void CcdSim::ISGetProperties (const char *dev)
{
    //  First we let our parent populate

    IDLog("CcdSim IsGetProperties with %s\n",dev);
    IndiCcd::ISGetProperties(dev);

    if(!Connected) {
        //  Only show simulator setup when not connected
    }

    return;
}

bool CcdSim::UpdateProperties()
{
    if(Connected) {
        //  We dont want our setup pages showing
        //  if connected
    }
    IndiCcd::UpdateProperties();
    return true;
}

bool CcdSim::Connect()
{
    SetCCDParams(1024,768,16,5.4,5.4);

    RawFrameSize=XRes*YRes;                 //  this is pixel count
    RawFrameSize=RawFrameSize*2;            //  Each pixel is 2 bytes
    RawFrameSize+=512;                      //  leave a little extra at the end
    RawFrame=new char[RawFrameSize];

    if(1==1) {
        SetGuidHeadParams(500,290,8,9.8,12.6);
        RawGuideSize=GXRes*GYRes;                 //  this is pixel count
        RawGuideSize+=512;                      //  leave a little extra at the end
        RawGuiderFrame=new char[RawGuideSize];
    }

    SetTimer(1000);     //  start the timer
    return true;
}

bool CcdSim::Disconnect()
{
    delete RawFrame;
    RawFrameSize=0;

    if(RawGuiderFrame != NULL) {
        delete RawGuiderFrame;
        RawGuideSize=0;
    }

    return true;
}

int CcdSim::StartExposure(float n)
{
    //  for the simulator, we can just draw the frame now
    //  and it will get returned at the right time
    //  by the timer routines
    ExposureRequest=n;

    if(InExposure) {
        //  We are already in an exposure, just change the time
        //  and be done with it.
        return 0;
    }
    DrawCcdFrame();

    gettimeofday(&ExpStart,NULL);
    InExposure=true;
    return 0;
}

int CcdSim::StartGuideExposure(float n)
{
    GuideExposureRequest=n;
    if(InGuideExposure) return 0;
    DrawGuiderFrame();
    gettimeofday(&GuideExpStart,NULL);
    InGuideExposure=true;
    return 0;
}

bool CcdSim::AbortGuideExposure()
{
    IDLog("Enter AbortGuideExposure\n");
    if(!InGuideExposure) return true;   //  no need to abort if we aren't doing one
    AbortGuideFrame=true;
    return true;
}
float CcdSim::CalcTimeLeft(timeval start,float req)
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(start.tv_sec * 1000.0 + start.tv_usec/1000);
    timesince=timesince/1000;
    timeleft=req-timesince;
    return timeleft;
}

void CcdSim::TimerHit()
{   int nexttimer=1000;

    if(!Connected) return;  //  No need to reset timer if we are not connected anymore

    if(InExposure) {
        float timeleft;
        timeleft=CalcTimeLeft(ExpStart,ExposureRequest);
        if(timeleft < 1.0) {
            if(timeleft <= 0.001) {
                InExposure=false;
                ExposureComplete();
            } else {
                nexttimer=timeleft*1000;    //  set a shorter timer
            }
        }
    }
    if(InGuideExposure) {
        float timeleft;
        timeleft=CalcTimeLeft(GuideExpStart,GuideExposureRequest);
        if(timeleft < 1.0) {
            if(timeleft <= 0.001) {
                InGuideExposure=false;
                if(!AbortGuideFrame) {
                    //IDLog("Sending guider frame\n");
                    GuideExposureComplete();
                    if(InGuideExposure) {    //  the call to complete triggered another exposure
                        timeleft=CalcTimeLeft(GuideExpStart,GuideExposureRequest);
                        if(timeleft <1.0) {
                            nexttimer=timeleft*1000;
                        }
                    }
                } else {
                    IDLog("Not sending guide frame cuz of abort\n");
                }
                AbortGuideFrame=false;
            } else {
                nexttimer=timeleft*1000;    //  set a shorter timer
            }
        }
    }
    SetTimer(nexttimer);
    return;
}

int CcdSim::DrawCcdFrame()
{
    //  Ok, lets just put a silly pattern into this
    //  CCd frame is 16 bit data
    unsigned short int *ptr;
    unsigned short int val;

    ptr=(unsigned short int *)RawFrame;
    testvalue++;
    if(testvalue > 255) testvalue=0;
    val=testvalue;

    for(int x=0; x<XRes*YRes; x++) {
        *ptr=val++;
        ptr++;
    }
    return 0;
}

int CcdSim::DrawGuiderFrame()
{
    unsigned char *ptr;
    unsigned char val;

    ptr=(unsigned char *)RawGuiderFrame;
    testvalue++;
    if(testvalue > 255) testvalue=0;
    val=testvalue;

    for(int x=0; x<GXRes*GYRes; x++) {
        *ptr=val++;
        ptr++;
    }
    return 0;
}
