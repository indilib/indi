/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include "ccd_simulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <memory>


// We declare an auto pointer to ccdsim.
std::auto_ptr<CCDSim> ccdsim(0);

void ISPoll(void *p);


void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(ccdsim.get() == 0) ccdsim.reset(new CCDSim());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        ccdsim->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        ccdsim->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        ccdsim->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        ccdsim->ISNewNumber(dev, name, values, names, num);
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
    ISInit();
    ccdsim->ISSnoopDevice(root);
}


CCDSim::CCDSim()
{
    //ctor
    testvalue=0;
    AbortGuideFrame=false;
    AbortPrimaryFrame = false;
    ShowStarField=true;

    HasSt4Port=true;
    HasGuideHead=true;


    raPEC=9.95;
    decPEC=68.9;

    //  sxvh9
    bias=1500;
    maxnoise=20;
    maxval=65000;
    maxpix=0;
    minpix =65000;
    limitingmag=11.5;
    saturationmag=2;
    focallength=1280;   //  focal length of the telescope in millimeters
    guider_focallength=1280;
    OAGoffset=0;    //  An oag is offset this much from center of scope position (arcminutes);
    skyglow=40;

    seeing=3.5;         //  fwhm of our stars
    ImageScalex=1.0;    //  preset with a valid non-zero
    ImageScaley=1.0;
    time(&RunStart);

    //  Our PEPeriod is 8 minutes
    //  and we have a 22 arcsecond swing
    PEPeriod=8*60;
    PEMax=11;
    GuideRate=7;    //  guide rate is 7 arcseconds per second
    TimeFactor=1;

    SimulatorSettingsNV = new INumberVectorProperty;
    TimeFactorSV = new ISwitchVectorProperty;

    // Filter stuff
    MinFilter=1;
    MaxFilter=5;
    FilterSlotN[0].max = MaxFilter;

}

bool CCDSim::SetupParms()
{
    int nbuf;
    SetCCDParams(SimulatorSettingsN[0].value,SimulatorSettingsN[1].value,16,SimulatorSettingsN[2].value,SimulatorSettingsN[3].value);

    //  Kwiq
    maxnoise=SimulatorSettingsN[8].value;
    skyglow=SimulatorSettingsN[9].value;
    maxval=SimulatorSettingsN[4].value;
    bias=SimulatorSettingsN[5].value;
    limitingmag=SimulatorSettingsN[7].value;
    saturationmag=SimulatorSettingsN[6].value;
    OAGoffset=SimulatorSettingsN[10].value;    //  An oag is offset this much from center of scope position (arcminutes);

    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP()/8;
    nbuf += 512;
    PrimaryCCD.setFrameBufferSize(nbuf);

    GetFilterNames(FILTER_TAB);

    return true;
}

bool CCDSim::Connect()
{

    int nbuf;

    SetTimer(1000);     //  start the timer
    return true;
}

CCDSim::~CCDSim()
{
    //dtor
}

const char * CCDSim::getDefaultName()
{
        return (char *)"CCD Simulator";
}

bool CCDSim::initProperties()
{
    //  Most hardware layers wont actually have indi properties defined
    //  but the simulators are a special case
    INDI::CCD::initProperties();

    IUFillNumber(&SimulatorSettingsN[0],"SIM_XRES","CCD X resolution","%4.0f",0,2048,0,1280);
    IUFillNumber(&SimulatorSettingsN[1],"SIM_YRES","CCD Y resolution","%4.0f",0,2048,0,1024);
    IUFillNumber(&SimulatorSettingsN[2],"SIM_XSIZE","CCD X Pixel Size","%4.2f",0,60,0,5.2);
    IUFillNumber(&SimulatorSettingsN[3],"SIM_YSIZE","CCD Y Pixel Size","%4.2f",0,60,0,5.2);
    IUFillNumber(&SimulatorSettingsN[4],"SIM_MAXVAL","CCD Maximum ADU","%4.0f",0,65000,0,65000);
    IUFillNumber(&SimulatorSettingsN[5],"SIM_BIAS","CCD Bias","%4.0f",0,6000,0,1500);
    IUFillNumber(&SimulatorSettingsN[6],"SIM_SATURATION","Saturation Mag","%4.1f",0,20,0,1.0);
    IUFillNumber(&SimulatorSettingsN[7],"SIM_LIMITINGMAG","Limiting Mag","%4.1f",0,20,0,20);
    IUFillNumber(&SimulatorSettingsN[8],"SIM_NOISE","CCD Noise","%4.0f",0,6000,0,50);
    IUFillNumber(&SimulatorSettingsN[9],"SIM_SKYGLOW","Sky Glow (magnitudes)","%4.1f",0,6000,0,19.5);
    IUFillNumber(&SimulatorSettingsN[10],"SIM_OAGOFFSET","Oag Offset (arcminutes)","%4.1f",0,6000,0,0);
    IUFillNumberVector(SimulatorSettingsNV,SimulatorSettingsN,11,getDeviceName(),"SIMULATOR_SETTINGS","Simulator Settings","Simulator Config",IP_RW,60,IPS_IDLE);


    IUFillSwitch(&TimeFactorS[0],"1X","Actual Time",ISS_ON);
    IUFillSwitch(&TimeFactorS[1],"10X","10x",ISS_OFF);
    IUFillSwitch(&TimeFactorS[2],"100X","100x",ISS_OFF);
    IUFillSwitchVector(TimeFactorSV,TimeFactorS,3,getDeviceName(),"ON_TIME_FACTOR","Time Factor","Simulator Config",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillNumber(&FWHMN[0],"SIM_FWHM","FWHM (arcseconds)","%4.2f",0,60,0,7.5);
    IUFillNumberVector(&FWHMNP,FWHMN,1,"Focuser Simulator", "FWHM","FWHM",OPTIONS_TAB,IP_RO,60,IPS_IDLE);

    IUFillNumber(&ScopeParametersN[0],"TELESCOPE_APERTURE","Aperture (mm)","%g",50,4000,0,0.0);
    IUFillNumber(&ScopeParametersN[1],"TELESCOPE_FOCAL_LENGTH","Focal Length (mm)","%g",100,10000,0,0.0 );
    IUFillNumber(&ScopeParametersN[2],"GUIDER_APERTURE","Guider Aperture (mm)","%g",50,4000,0,0.0);
    IUFillNumber(&ScopeParametersN[3],"GUIDER_FOCAL_LENGTH","Guider Focal Length (mm)","%g",100,10000,0,0.0 );
    IUFillNumberVector(&ScopeParametersNP,ScopeParametersN,4,"Telescope Simulator","TELESCOPE_INFO","Scope Properties",OPTIONS_TAB,IP_RW,60,IPS_OK);

    IUFillNumber(&EqPECN[0],"RA_PEC","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqPECN[1],"DEC_PEC","decPEC (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqPECNP,EqPECN,2,ActiveDeviceT[0].text,"EQUATORIAL_PEC","EQ PEC","Main Control",IP_RW,60,IPS_IDLE);

    IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_PEC");

    initFilterProperties(getDeviceName(), FILTER_TAB);

    MinFilter=1;
    MaxFilter=5;
    FilterSlotN[0].max = MaxFilter;

    return true;
}

void CCDSim::ISGetProperties (const char *dev)
{
    //  First we let our parent populate

    //IDLog("CCDSim IsGetProperties with %s\n",dev);
    INDI::CCD::ISGetProperties(dev);

    defineNumber(SimulatorSettingsNV);
    defineSwitch(TimeFactorSV);

    return;
}

bool CCDSim::updateProperties()
{
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        SetupParms();

        if(HasGuideHead)
        {
            SetGuidHeadParams(500,290,16,9.8,12.6);
            GuideCCD.setFrameBufferSize(GuideCCD.getXRes() * GuideCCD.getYRes() * 2);
        }

        // Define the Filter Slot and name properties
        defineNumber(&FilterSlotNP);
        if (FilterNameT != NULL)
            defineText(FilterNameTP);
    }

    return true;
}


bool CCDSim::Disconnect()
{
    return true;
}

int CCDSim::StartExposure(float duration)
{
    //  for the simulator, we can just draw the frame now
    //  and it will get returned at the right time
    //  by the timer routines
    AbortPrimaryFrame=false;
    ExposureRequest=duration;

    PrimaryCCD.setExposureDuration(duration);
    gettimeofday(&ExpStart,NULL);
    //  Leave the proper time showing for the draw routines
    DrawCcdFrame(&PrimaryCCD);
    //  Now compress the actual wait time
    ExposureRequest=duration*TimeFactor;
    InExposure=true;
    return 0;
}

int CCDSim::StartGuideExposure(float n)
{
    GuideExposureRequest=n;
    AbortGuideFrame = false;
    GuideCCD.setExposureDuration(n);
    DrawCcdFrame(&GuideCCD);
    gettimeofday(&GuideExpStart,NULL);
    InGuideExposure=true;
    return 0;
}

bool CCDSim::AbortExposure()
{
    if (!InExposure)
        return true;

    AbortPrimaryFrame = true;

    return true;
}

bool CCDSim::AbortGuideExposure()
{
    //IDLog("Enter AbortGuideExposure\n");
    if(!InGuideExposure) return true;   //  no need to abort if we aren't doing one
    AbortGuideFrame=true;
    return true;
}

float CCDSim::CalcTimeLeft(timeval start,float req)
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

void CCDSim::TimerHit()
{
    int nexttimer=1000;

    if(isConnected() == false) return;  //  No need to reset timer if we are not connected anymore

    if(InExposure)
    {
        if (AbortPrimaryFrame)
        {
            InExposure = false;
            AbortPrimaryFrame = false;
        }
        else
        {
            float timeleft;
            timeleft=CalcTimeLeft(ExpStart,ExposureRequest);

            //IDLog("CCD Exposure left: %g - Requset: %g\n", timeleft, ExposureRequest);
            if (timeleft < 0)
                 timeleft = 0;

            PrimaryCCD.setExposureLeft(timeleft);

            if(timeleft < 1.0)
            {
                if(timeleft <= 0.001)
                {
                    InExposure=false;
                    ExposureComplete(&PrimaryCCD);
                } else
                {
                    nexttimer=timeleft*1000;    //  set a shorter timer
                }
            }
        }
    }

    if(InGuideExposure)
    {
        float timeleft;
        timeleft=CalcTimeLeft(GuideExpStart,GuideExposureRequest);


        //IDLog("GUIDE Exposure left: %g - Requset: %g\n", timeleft, GuideExposureRequest);

        if (timeleft < 0)
             timeleft = 0;

        //ImageExposureN[0].value = timeleft;
        //IDSetNumber(ImageExposureNP, NULL);
        GuideCCD.setExposureLeft(timeleft);

        if(timeleft < 1.0)
        {
            if(timeleft <= 0.001)
            {
                InGuideExposure=false;
                if(!AbortGuideFrame)
                {
                    //IDLog("Sending guider frame\n");
                    ExposureComplete(&GuideCCD);
                    if(InGuideExposure)
                    {    //  the call to complete triggered another exposure
                        timeleft=CalcTimeLeft(GuideExpStart,GuideExposureRequest);
                        if(timeleft <1.0)
                        {
                            nexttimer=timeleft*1000;
                        }
                    }
                } else
                {
                    IDLog("Not sending guide frame cuz of abort\n");
                }
                AbortGuideFrame=false;
            } else
            {
                nexttimer=timeleft*1000;    //  set a shorter timer
            }
        }
    }
    SetTimer(nexttimer);
    return;
}

int CCDSim::DrawCcdFrame(CCDChip *targetChip)
{
    //  Ok, lets just put a silly pattern into this
    //  CCd frame is 16 bit data
    unsigned short int *ptr;
    unsigned short int val;
    float ExposureTime;
    float targetFocalLength;

    ptr=(unsigned short int *) targetChip->getFrameBuffer();

    if (targetChip->getXRes() == 500)
    {
        targetFocalLength = guider_focallength;
        ExposureTime = GuideExposureRequest*4;
    }
    else
    {
        targetFocalLength = focallength;
        ExposureTime = ExposureRequest;
    }


    if(ShowStarField)
    {
        char gsccmd[250];
        FILE *pp;
        int stars=0;
        int lines=0;
        int drawn=0;
        int x,y;
        float PEOffset;
        float PESpot;
        double rad;  //  telescope ra in degrees
        double rar;  //  telescope ra in radians
        double decr; //  telescope dec in radians;


        double timesince;
        time_t now;
        time(&now);

        //  Lets figure out where we are on the pe curve
        timesince=difftime(now,RunStart);
        //  This is our spot in the curve
        PESpot=timesince/PEPeriod;
        //  Now convert to radians
        PESpot=PESpot*2.0*3.14159;

        PEOffset=PEMax*sin(PESpot);
        //fprintf(stderr,"PEOffset = %4.2f arcseconds timesince %4.2f\n",PEOffset,timesince);
        PEOffset=PEOffset/3600;     //  convert to degrees
        //PeOffset=PeOffset/15;       //  ra is in h:mm

        //  Start by clearing the frame buffer
        memset(targetChip->getFrameBuffer(),0,targetChip->getFrameBufferSize());


        //  Spin up a set of plate constants that will relate
        //  ra/dec of stars, to our fictitious ccd layout

        //  to account for various rotations etc
        //  we should spin up some plate constants here
        //  then we can use these constants to rotate and offset
        //  the standard co-ordinates on each star for drawing
        //  a ccd frame;
        double pa,pb,pc,pd,pe,pf;
        //  Since this is a simple ccd, correctly aligned
        //  for now we cheat
        //  no offset or rotation for and y axis means
        pb=0.0;
        pc=targetChip->getXRes()/2/targetChip->getBinX();
        pd=0.0;
        pf=targetChip->getYRes()/2/targetChip->getBinY();
        //  and we do a simple scale for x and y locations
        //  based on the focal length and pixel size
        //  focal length in mm, pixels in microns
        pa=targetFocalLength/targetChip->getPixelSizeX()*1000/targetChip->getBinX();
        pe=targetFocalLength/targetChip->getPixelSizeY()*1000/targetChip->getBinY();

        //IDLog("Pixels are %4.2f %4.2f  pa %6.4f  pe %6.4f\n",PixelSizex,PixelSizey,pa,pe);

        //  these numbers are now pixels per radian
        float Scalex;
        float Scaley;
        Scalex=pa*0.0174532925;    //  pixels per degree
        Scalex=Scalex/3600.0;           // pixels per arcsecond
        Scalex=1.0/Scalex;  //  arcseconds per pixel

        Scaley=pe*0.0174532925;    //  pixels per degree
        Scaley=Scaley/3600.0;           // pixels per arcsecond
        Scaley=1.0/Scaley;  //  arcseconds per pixel
        //qq=qq/3600; //  arcseconds per pixel

        //IDLog("Pixel scale is %4.2f x %4.2f\n",Scalex,Scaley);
        ImageScalex=Scalex;
        ImageScaley=Scaley;

        //  calc this now, we will use it a lot later
        rad=raPEC*15.0;
        rar=rad*0.0174532925;
        //  offsetting the dec by the guide head offset
        float cameradec;
        cameradec=decPEC+OAGoffset/60;
        decr=cameradec*0.0174532925;

        //fprintf(stderr,"decPEC %7.5f  cameradec %7.5f  CenterOffsetDec %4.4f\n",decPEC,cameradec,CenterOffsetDec);
        //  now lets calculate the radius we need to fetch
        float radius;

        radius=sqrt((Scalex*Scalex*targetChip->getXRes()/2.0*targetChip->getXRes()/2.0)+(Scaley*Scaley*targetChip->getYRes()/2.0*targetChip->getYRes()/2.0));
        //  we have radius in arcseconds now
        radius=radius/60;   //  convert to arcminutes
        //fprintf(stderr,"Lookup radius %4.2f\n",radius);
        //radius=radius*2;

        //  A saturationmag star saturates in one second
        //  and a limitingmag produces a one adu level in one second
        //  solve for zero point and system gain

        k=(saturationmag-limitingmag)/((-2.5*log(maxval))-(-2.5*log(1.0/2.0)));
        z=saturationmag-k*(-2.5*log(maxval));
        //z=z+saturationmag;

        //IDLog("K=%4.2f  Z=%4.2f\n",k,z);

        //  Should probably do some math here to figure out the dimmest
        //  star we can see on this exposure
        //  and only fetch to that magnitude
        //  for now, just use the limiting mag number with some room to spare
        float lookuplimit;

        lookuplimit=limitingmag;
        lookuplimit=lookuplimit;
        if(radius > 60) lookuplimit=11;

        //  if this is a light frame, we need a star field drawn
        if(targetChip->getFrameType()==CCDChip::LIGHT_FRAME)
        {
            //sprintf(gsccmd,"gsc -c %8.6f %+8.6f -r 120 -m 0 9.1",rad+PEOffset,decPEC);
            sprintf(gsccmd,"gsc -c %8.6f %+8.6f -r %4.1f -m 0 %4.2f -n 3000",rad+PEOffset,cameradec,radius,lookuplimit);
            //fprintf(stderr,"gsccmd %s\n",gsccmd);
            pp=popen(gsccmd,"r");
            if(pp != NULL) {
                char line[256];
                while(fgets(line,256,pp)!=NULL)
                {
                    //fprintf(stderr,"%s",line);

                    //  ok, lets parse this line for specifcs we want
                    char id[20];
                    char plate[6];
                    char ob[6];
                    float mag;
                    float mage;
                    float ra;
                    float dec;
                    float pose;
                    int band;
                    float dist;
                    int dir;
                    int c;
                    int rc;

                    rc=sscanf(line,"%10s %f %f %f %f %f %d %d %4s %2s %f %d",
                            id,&ra,&dec,&pose,&mag,&mage,&band,&c,plate,ob,&dist,&dir);
                    //fprintf(stderr,"Parsed %d items\n",rc);
                    if(rc==12) {
                        lines++;
                        //if(c==0) {
                        stars++;
                        //fprintf(stderr,"%s %8.4f %8.4f %5.2f %5.2f %d\n",id,ra,dec,mag,dist,dir);

                        //  Convert the ra/dec to standard co-ordinates
                        double sx;   //  standard co-ords
                        double sy;   //
                        double srar;        //  star ra in radians
                        double sdecr;       //  star dec in radians;
                        double ccdx;
                        double ccdy;

                        //fprintf(stderr,"line %s",line);
                        //fprintf(stderr,"parsed %6.5f %6.5f\n",ra,dec);

                        srar=ra*0.0174532925;
                        sdecr=dec*0.0174532925;
                        //  Handbook of astronomical image processing
                        //  page 253
                        //  equations 9.1 and 9.2
                        //  convert ra/dec to standard co-ordinates

                        sx=cos(decr)*sin(srar-rar)/( cos(decr)*cos(sdecr)*cos(srar-rar)+sin(decr)*sin(sdecr) );
                        sy=(sin(decr)*cos(sdecr)*cos(srar-rar)-cos(decr)*sin(sdecr))/( cos(decr)*cos(sdecr)*cos(srar-rar)+sin(decr)*sin(sdecr) );

                        //  now convert to microns
                        ccdx=pa*sx+pb*sy+pc;
                        ccdy=pd*sx+pe*sy+pf;


                        rc=DrawImageStar(targetChip, mag,ccdx,ccdy);
                        drawn+=rc;
                        if(rc==1)
                        {
                            //fprintf(stderr,"star %s scope %6.4f %6.4f star %6.4f %6.4f  ccd %6.2f %6.2f\n",id,rad,decPEC,ra,dec,ccdx,ccdy);
                            //fprintf(stderr,"star %s ccd %6.2f %6.2f\n",id,ccdx,ccdy);
                        }
                    }
                }
                pclose(pp);
            } else
            {
                IDMessage(getDeviceName(),"Error looking up stars, is gsc installed with appropriate environment variables set ??");
                //fprintf(stderr,"Error doing gsc lookup\n");
            }
            if(drawn==0)
            {
                IDMessage(getDeviceName(),"Got no stars, is gsc installed with appropriate environment variables set ??");

            }
        }
        //fprintf(stderr,"Got %d stars from %d lines drew %d\n",stars,lines,drawn);

        //  now we need to add background sky glow, with vignetting
        //  this is essentially the same math as drawing a dim star with
        //  fwhm equivalent to the full field of view

        CCDChip::CCD_FRAME ftype = targetChip->getFrameType();

        if((ftype==CCDChip::LIGHT_FRAME)||(ftype==CCDChip::FLAT_FRAME))
        {
            float skyflux;
            float glow;
            //  calculate flux from our zero point and gain values
            glow=skyglow;
            if(ftype==CCDChip::FLAT_FRAME)
            {
                //  Assume flats are done with a diffuser
                //  in broad daylight, so, the sky magnitude
                //  is much brighter than at night
                glow=skyglow/10;
            }

            //fprintf(stderr,"Using glow %4.2f\n",glow);

            skyflux=pow(10,((glow-z)*k/-2.5));
            //  ok, flux represents one second now
            //  scale up linearly for exposure time
            skyflux=skyflux*ExposureTime*targetChip->getBinX()*targetChip->getBinY();
           //IDLog("SkyFlux = %g ExposureRequest %g\n",skyflux,ExposureTime);

            unsigned short *pt;

            int nwidth  = targetChip->getXRes()/targetChip->getBinX();
            int nheight = targetChip->getYRes()/targetChip->getBinY();
            pt=(unsigned short int *)targetChip->getFrameBuffer();

            for(int y=0; y< nheight; y++)
            {
                for(int x=0; x< nwidth; x++)
                {
                    float dc;   //  distance from center
                    float fp;   //  flux this pixel;
                    float sx,sy;
                    float vig;

                    sx=targetChip->getXRes()/2/targetChip->getBinX();
                    sx=sx-x;
                    sy=targetChip->getYRes()/2/targetChip->getBinY();
                    sy=sy-y;

                    vig=targetChip->getXRes()/targetChip->getBinX();
                    vig=vig*ImageScalex;
                    //  need to make this account for actual pixel size
                    dc=sqrt(sx*sx*ImageScalex*ImageScalex+sy*sy*ImageScaley*ImageScaley);
                    //  now we have the distance from center, in arcseconds
                    //  now lets plot a gaussian falloff to the edges
                    //
                    float fa;
                    fa=exp(-2.0*0.7*(dc*dc)/vig/vig);

                    //  get the current value
                    fp=pt[0];

                    //  Add the sky glow
                    fp+=skyflux;

                    //  now scale it for the vignetting
                    fp=fa*fp;

                    //  clamp to limits
                    if(fp > maxval) fp=maxval;
                    if (fp > maxpix) maxpix = fp;
                    if (fp < minpix) minpix = fp;
                    //  and put it back
                    pt[0]=fp;
                    pt++;

                }
            }
        }


        //  Now we add some bias and read noise
        for(x=0; x<targetChip->getXRes(); x++) {
            for(y=0; y<targetChip->getYRes(); y++) {
                int noise;

                noise=random();
                noise=noise%maxnoise; //

                //IDLog("noise is %d\n", noise);
                AddToPixel(targetChip, x,y,bias+noise);
            }
        }


    } else {
        testvalue++;
        if(testvalue > 255) testvalue=0;
        val=testvalue;

        int nbuf    = targetChip->getXRes()*targetChip->getYRes();

        for(int x=0; x<nbuf; x++)
        {
            *ptr=val++;
            ptr++;
        }

    }
    return 0;
}

int CCDSim::DrawImageStar(CCDChip *targetChip, float mag,float x,float y)
{
    //float d;
    //float r;
    int sx,sy;
    int drew=0;
    int boxsizex=5;
    int boxsizey=5;
    float flux;
    float ExposureTime;

    if (targetChip->getXRes() == 500)
        ExposureTime = GuideExposureRequest*4;
    else
        ExposureTime = ExposureRequest;


    if((x<0)||(x>targetChip->getXRes()/targetChip->getBinX())||(y<0)||(y>targetChip->getYRes()/targetChip->getBinY()))
    {
        //  this star is not on the ccd frame anyways
        return 0;
    }



    //  calculate flux from our zero point and gain values
    flux=pow(10,((mag-z)*k/-2.5));

    //  ok, flux represents one second now
    //  scale up linearly for exposure time
    flux=flux*ExposureTime;

    float qx;
    //  we need a box size that gives a radius at least 3 times fwhm
    qx=seeing/ImageScalex;
    qx=qx*3;
    boxsizex=(int)qx;
    boxsizex++;
    qx=seeing/ImageScaley;
    qx=qx*3;
    boxsizey=(int)qx;
    boxsizey++;

    //IDLog("BoxSize %d %d\n",boxsizex,boxsizey);


    for(sy=-boxsizey; sy<=boxsizey; sy++) {
        for(sx=-boxsizey; sx<=boxsizey; sx++) {
            int rc;
            float dc;   //  distance from center
            float fp;   //  flux this pixel;

            //  need to make this account for actual pixel size
            dc=sqrt(sx*sx*ImageScalex*ImageScalex+sy*sy*ImageScaley*ImageScaley);
            //  now we have the distance from center, in arcseconds
            //  This should be gaussian, but, for now we'll just go with
            //  a simple linear function
            float fa;
            fa=exp(-2.0*0.7*(dc*dc)/seeing/seeing);
            fp=fa*flux*targetChip->getBinX()*targetChip->getBinY();


            if(fp < 0) fp=0;

            /*
            if(dc < boxsize) {
                dc=boxsize-dc;
                dc=dc/boxsize;
                fp=dc*flux;
            } else {
                fp=0;
            }
            */
            rc=AddToPixel(targetChip, x+sx,y+sy,fp);
            if(rc != 0) drew=1;
        }
    }
    return drew;
}

int CCDSim::AddToPixel(CCDChip *targetChip, int x,int y,int val)
{
    int drew=0;
    if(x >= 0) {
        if(x < targetChip->getXRes()/targetChip->getBinX()) {
            if(y >= 0) {
                if(y < targetChip->getYRes()/targetChip->getBinY()) {
                    unsigned short *pt;
                    int newval;
                    drew++;

                    pt=(unsigned short int *)targetChip->getFrameBuffer();

                    pt+=(y*targetChip->getXRes()/targetChip->getBinX());
                    pt+=x;
                    newval=pt[0];
                    newval+=val;
                    if(newval > maxval) newval=maxval;
                    if (newval > maxpix) maxpix = newval;
                    if (newval < minpix) minpix = newval;
                    pt[0]=newval;
                }
            }
        }
    }
    return drew;
}

bool CCDSim::GuideNorth(float v)
{
    float c;

    c=v/1000*GuideRate;  //
    c=c/3600;
    decPEC=decPEC+c;

    return true;
}
bool CCDSim::GuideSouth(float v)
{
    float c;

    c=v/1000*GuideRate;  //
    c=c/3600;
    decPEC=decPEC-c;

    return true;
}

bool CCDSim::GuideEast(float v)
{
    float c;

    c=v/1000*GuideRate;
    c=c/3600.0/15.0;
    c=c/(cos(decPEC*0.0174532925));
    raPEC=raPEC+c;

    return true;
}
bool CCDSim::GuideWest(float v)
{
    float c;

    c=v/1000*GuideRate;  //
    c=c/3600.0/15.0;
    c=c/(cos(decPEC*0.0174532925));
    raPEC=raPEC-c;

    return true;
}

bool CCDSim::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::CCD::ISNewNumber %s\n",name);
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here

        //IDLog("CCDSim::ISNewNumber %s\n",name);
        if(strcmp(name,"SIMULATOR_SETTINGS")==0)
        {
            IUUpdateNumber(SimulatorSettingsNV, values, names, n);
            SimulatorSettingsNV->s=IPS_OK;

            //  Reset our parameters now
            SetupParms();
            IDSetNumber(SimulatorSettingsNV,NULL);
            saveConfig();

            //IDLog("Frame set to %4.0f,%4.0f %4.0f x %4.0f\n",CcdFrameN[0].value,CcdFrameN[1].value,CcdFrameN[2].value,CcdFrameN[3].value);
            //seeing=SimulatorSettingsN[0].value;
            return true;
        }

        if (strcmp(name, FilterSlotNP.name)==0)
        {
            processFilterProperties(name, values, names, n);
            return true;
        }


    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return INDI::CCD::ISNewNumber(dev,name,values,names,n);
}


bool CCDSim::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    //IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,getDeviceName())==0) {
        //  This one is for us


        if(strcmp(name,"ON_TIME_FACTOR")==0) {

            //  client is telling us what to do with co-ordinate requests
            TimeFactorSV->s=IPS_OK;
            IUUpdateSwitch(TimeFactorSV,states,names,n);
            //  Update client display
            IDSetSwitch(TimeFactorSV,NULL);

            saveConfig();
            if(TimeFactorS[0].s==ISS_ON    ) {
                //IDLog("CCDSim:: Time Factor 1\n");
                TimeFactor=1;
            }
            if(TimeFactorS[1].s==ISS_ON    ) {
                //IDLog("CCDSim:: Time Factor 0.1\n");
                TimeFactor=0.1;
            }
            if(TimeFactorS[2].s==ISS_ON    ) {
                //IDLog("CCDSim:: Time Factor 0.01\n");
                TimeFactor=0.01;
            }

            return true;
        }

    }

    //  Nobody has claimed this, so, ignore it
    return INDI::CCD::ISNewSwitch(dev,name,states,names,n);
}

bool CCDSim::ISSnoopDevice (XMLEle *root)
{
     if (IUSnoopNumber(root,&FWHMNP)==0)
     {
           seeing = FWHMNP.np[0].value;

           if (isDebug())
                IDLog("CCD Simulator: New FWHM value of %g\n", seeing);
           return true;
     }

     if (IUSnoopNumber(root,&ScopeParametersNP)==0)
     {
           focallength = ScopeParametersNP.np[1].value;
           guider_focallength = ScopeParametersNP.np[3].value;
           if (isDebug())
           {
                IDLog("CCD Simulator: New focalLength value of %g\n", focallength);
                IDLog("CCD Simulator: New guider_focalLength value of %g\n", guider_focallength);
           }
           return true;
     }

     if(IUSnoopNumber(root,&EqPECNP)==0)
     {
        float newra,newdec;
        newra=EqPECN[0].value;
        newdec=EqPECN[1].value;
        if((newra != raPEC)||(newdec != decPEC))
        {
            if (isDebug())
                IDLog("raPEC %4.2f  decPEC %4.2f Snooped raPEC %4.2f  decPEC %4.2f\n",raPEC,decPEC,newra,newdec);
            raPEC=newra;
            decPEC=newdec;

            return true;

        }
     }

     return INDI::CCD::ISSnoopDevice(root);
}

bool CCDSim::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigNumber(fp,SimulatorSettingsNV);
    IUSaveConfigSwitch(fp, TimeFactorSV);

    return true;
}

bool CCDSim::SelectFilter(int f)
{
    CurrentFilter = f;
    SelectFilterDone(f);
    return true;
}

bool CCDSim::GetFilterNames(const char* groupName)
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];

    const char *filterDesignation[5] = { "Red", "Green", "Blue", "H_Alpha", "Luminosity" };

    if (FilterNameT != NULL)
        delete FilterNameT;

    FilterNameT = new IText[MaxFilter];

    for (int i=0; i < MaxFilter; i++)
    {
        snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i+1);
        snprintf(filterLabel, MAXINDILABEL, "Filter #%d", i+1);
        IUFillText(&FilterNameT[i], filterName, filterLabel, filterDesignation[i]);
    }

    IUFillTextVector(FilterNameTP, FilterNameT, MaxFilter, getDeviceName(), "FILTER_NAME", "Filter", groupName, IP_RW, 0, IPS_IDLE);

    return true;
}

int CCDSim::QueryFilter()
{
    return CurrentFilter;
}

