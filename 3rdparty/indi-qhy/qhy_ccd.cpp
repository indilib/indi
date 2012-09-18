/*******************************************************************************
  Copyright(c) 2012 Jasem Mutlaq. All rights reserved.

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

#include "qhy_ccd.h"
#include "qhy5_driver.h"
#include <memory>

#define POLLMS 250

// We declare an auto pointer to QHYCCDPtr.
std::auto_ptr<QHYCCD> QHYCCDPtr(0);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(QHYCCDPtr.get() == 0) QHYCCDPtr.reset(new QHYCCD());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        QHYCCDPtr->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        QHYCCDPtr->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        QHYCCDPtr->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        QHYCCDPtr->ISNewNumber(dev, name, values, names, num);
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

QHYCCD::QHYCCD()
{
    //ctor

    HasSt4Port = true;

    driver = new QHY5Driver();

}

QHYCCD::~QHYCCD()
{
    //dtor
    driver->Disconnect();

    delete(driver);
}


const char * QHYCCD::getDefaultName()
{
    return (char *)"QHY CCD";
}

bool QHYCCD::initProperties()
{
    INDI::CCD::initProperties();

    IUFillNumber(&GainN[0],"GAIN","Gain","%0.f", 1., 100, 1., 1.);
    IUFillNumberVector(&GainNP,GainN,1,getDeviceName(),"CCD_GAIN","Gain",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

}

void QHYCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    addAuxControls();

    //setSimulation(true);

    setDebug(true);
}

bool QHYCCD::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {

        int width, height, gain;

        driver->GetDefaultParam(&width, &height, &gain);

        GainN[0].value = gain;

        defineNumber(&GainNP);

        SetCCDParams(width, height, 8, 5.2, 5.2);

        updateCCDFrame(0,0, width, height);

        PrimaryCCD.setBPP(8);

    }


    return true;
}

bool QHYCCD::Connect()
{
    bool rc;
    //IDLog("Calling sx connect\n");

    if (isSimulation())
        driver->setSimulation(true);
    else
        driver->setSimulation(false);

    if (isDebug())
        driver->setDebug(true);
    else
        driver->setDebug(false);

    rc=driver->Connect();
    return rc;
    
}

bool QHYCCD::Disconnect()
{
    bool rc;
    rc=driver->Disconnect();

    return rc;
}


bool QHYCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (!strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, "GAIN"))
        {
            IUUpdateNumber(&GainNP, values, names, n);

            int pixw, pixh;
            driver->SetParams(PrimaryCCD.getSubW(), PrimaryCCD.getSubH(), PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), GainN[0].value, &pixw, &pixh);

            IDSetNumber(&GainNP, "Gain updated.");

            return true;
        }

    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

float QHYCCD::CalcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec/1000);
    timesince=timesince/1000;


    timeleft=ExposureRequest-timesince;
    return timeleft;
}

float QHYCCD::CalcPulseTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now,NULL);

    timesince=(double)(now.tv_sec * 1000.0 + now.tv_usec/1000) - (double)(PulseStart.tv_sec * 1000.0 + PulseStart.tv_usec/1000);
    timesince=timesince/1000;


    timeleft=PulseRequest-timesince;
    return timeleft;
}


int QHYCCD::StartExposure(float n)
{
    ExposureRequest=n;
    gettimeofday(&ExpStart,NULL);
    InExposure=true;


    if (isDebug())
        IDLog("Calling start exposure...\n");
    int rc = driver->StartExposure(n);
    if (isDebug())
        IDLog("Result from start exposure is (%d)\n", rc);

    //  Relatively long exposure
     //  lets do it on our own timers
     int tval;
     tval=n*1000;
     tval=tval-50;
     if(tval < 1)
         tval=1;
     if(tval > 250)
         tval=250;

     //IDLog("Cleared all fields, setting timer to %d\n", tval);

     SetTimer(tval);

    return 0;
}

/*bool QHYCCD::AbortGuideExposure()
{
    if(InGuideExposure) {
        InGuideExposure=false;
        return true;
    }
    return false;
}*/


void QHYCCD::TimerHit()
{
    float timeleft;
    int timerID=-1;

    //IDLog("QHYCCD Timer \n");

    //  If this is a relatively long exposure
    //  and its nearing the end, but not quite there yet
    //  We want to flush the accumulators

    if(InExposure)
    {
        timeleft=CalcTimeLeft();

        if(timeleft < 1.0)
        {
            if(timeleft > 0.25)
            {
                //  a quarter of a second or more
                //  just set a tighter timer
                timerID = SetTimer(250);
            } else
            {
                if(timeleft >0.07)
                {
                    //  use an even tighter timer
                    timerID = SetTimer(50);
                } else
                {
                    //  it's real close now, so spin on it
                    while(timeleft > 0)
                    {
                        int slv;
                        slv=100000*timeleft;
                        //IDLog("usleep %d\n",slv);
                        usleep(slv);
                        timeleft=CalcTimeLeft();
                    }

                    if (isDebug())
                        IDLog("Exposure done, calling ReadCameraFrame\n");

                    ReadCameraFrame();



                }
            }
        } else
        {
            if(!InPulse)
                timerID = SetTimer(250);
        }
    }


    if(InPulse)
    {
        timeleft=CalcPulseTimeLeft();

        if(timeleft < 1.0)
        {
            if(timeleft > 0.25)
            {
                //  a quarter of a second or more
                //  just set a tighter timer
                if (timerID == -1)
                    SetTimer(250);
            } else
            {
                if(timeleft >0.07)
                {
                    //  use an even tighter timer
                    if (timerID == -1)
                        SetTimer(50);
                } else
                {
                    //  it's real close now, so spin on it
                    while(timeleft > 0)
                    {
                        int slv;
                        slv=100000*timeleft;
                        //IDLog("usleep %d\n",slv);
                        usleep(slv);
                        timeleft=CalcPulseTimeLeft();
                    }


                    InPulse = false;
                    driver->Pulse(QHY_NORTH | QHY_EAST, 0);

                    if (isDebug())
                        IDLog("Stopping guide.");

                    // If we have an exposure, keep going
                    if (InExposure && timerID == -1)
                        SetTimer(250);

                }
            }
        } else
           SetTimer(250);
    }
}

void QHYCCD::ReadCameraFrame()
{
    int binw = PrimaryCCD.getBinX();
    int binh = PrimaryCCD.getBinY();
    char *ccdBuffer = PrimaryCCD.getFrameBuffer();

    int impixw=PrimaryCCD.getSubW();
    int impixh=PrimaryCCD.getSubH();

    driver->ReadExposure();

    IDLog("binw : %d - binh: %d - impixw: %d, impixh: %d\n", binw, binh, impixw, impixh);

    if (binw == 1)
    {

        IDLog("No binning, copying the whole buffer\n");
        for(int i = 0; i < impixh; i ++)
        //memcpy(ccdBuffer + (i * impixw) + 1, qhy5_get_row(i), impixw);
        memcpy(ccdBuffer + (i * impixw), driver->GetRow(i), impixw);
    }
    else
    {

        int new_wd = impixw/binw;
        int new_ht = impixh/binh;
        //float kx = (float) impixw/ (float)new_wd;
        //float ky = (float) impixh / (float)new_ht;
        int kx = binw;
        int ky = binh;

        IDLog("scaling to new_wd: %d - new_ht: %d\n", new_wd, new_ht);

        char *d_ptr = ccdBuffer;

        for( int j = 0;j < new_ht ;j++ )
        {
            char *s_ptr = driver->GetRow(j*ky);
            for( int i = 0;i < new_wd;i++ )
            {
                *d_ptr = *(s_ptr + (i * kx));
                d_ptr++;
            }
        }
    }

    ExposureComplete();

}

bool QHYCCD::updateCCDFrame(int x, int y, int w, int h)
{
    int nbuf, pixw, pixh;

    driver->SetParams(w, h, x, y, GainN[0].value, &pixw, &pixh);

    nbuf = (pixw * pixh) / (PrimaryCCD.getBinX() * PrimaryCCD.getBinY());
    PrimaryCCD.setFrameBufferSize(nbuf);

    IDLog("Setting primary CCD buffer size to %d\n", nbuf);

    PrimaryCCD.setFrame(x, y, pixw, pixh);

}

bool QHYCCD::updateCCDBin(int hor, int ver)
{
    INDI_UNUSED(ver);

    if (hor == 3)
    {
        IDMessage(getDeviceName(), "3x3 binning is not supported.");
        PrimaryCCD.setBin(1,1);
        return false;
    }

    PrimaryCCD.setBin(hor, hor);

    return true;
}

bool QHYCCD::GuideNorth(float ms)
{
    if (HasSt4Port == false)
        return false;

    guideDirection = QHY_NORTH;

    driver->Pulse(guideDirection, ms);


    if (isDebug())
        IDLog("Starting NORTH guide\n");

    if (ms <= POLLMS)
    {

        usleep(ms*1000);


        driver->Pulse(QHY_NORTH | QHY_EAST, 0);
        return true;
    }

    PulseRequest=ms/1000.0;
    gettimeofday(&PulseStart,NULL);
    InPulse=true;

    if (!InExposure )
        SetTimer(ms-50);

    return true;
}

bool QHYCCD::GuideSouth(float ms)
{
    if (HasSt4Port == false)
        return false;

    guideDirection = QHY_SOUTH;

    driver->Pulse(guideDirection, ms);


    if (isDebug())
        IDLog("Starting SOUTH guide\n");

    if (ms <= POLLMS)
    {

        usleep(ms*1000);


        driver->Pulse(QHY_NORTH | QHY_EAST, 0);
        return true;
    }

    PulseRequest=ms/1000.0;
    gettimeofday(&PulseStart,NULL);
    InPulse=true;

    if (!InExposure )
        SetTimer(ms-50);

    return true;

}

bool QHYCCD::GuideEast(float ms)
{
    if (HasSt4Port == false)
        return false;

    guideDirection = QHY_EAST;

    driver->Pulse(guideDirection, ms);


    if (isDebug())
        IDLog("Starting NORTH guide\n");

    if (ms <= POLLMS)
    {

        usleep(ms*1000);


        driver->Pulse(QHY_NORTH | QHY_EAST, 0);
        return true;
    }

    PulseRequest=ms/1000.0;
    gettimeofday(&PulseStart,NULL);
    InPulse=true;

    if (!InExposure )
        SetTimer(ms-50);

    return true;
}

bool QHYCCD::GuideWest(float ms)
{
    if (HasSt4Port == false)
        return false;

    guideDirection = QHY_WEST;

    driver->Pulse(guideDirection, ms);


    if (isDebug())
        IDLog("Starting WEST guide\n");

    if (ms <= POLLMS)
    {

        usleep(ms*1000);


        driver->Pulse(QHY_NORTH | QHY_EAST, 0);
        return true;
    }

    PulseRequest=ms/1000.0;
    gettimeofday(&PulseStart,NULL);
    InPulse=true;

    if (!InExposure )
        SetTimer(ms-50);

    return true;

}
