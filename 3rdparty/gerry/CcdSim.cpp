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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

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
    ShowStarField=true;

    HasSt4Port=true;
    HasGuideHead=false;


    RA=9.95;
    Dec=68.9;

    //  sxvh9
    bias=1500;
    maxnoise=20;
    maxval=65000;
    limitingmag=11.5;
    saturationmag=2;
    focallength=1280;   //  focal length of the telescope in millimeters
    CenterOffsetDec=0;    //  An oag is offset this much from center of scope position (arcminutes);
    skyglow=40;

    seeing=3.5;         //  fwhm of our stars
    ImageScalex=1.0;    //  preset with a valid non-zero
    ImageScaley=1.0;
    //focallength=175;   //  focal length of the telescope in millimeters
    time(&RunStart);

    //  Our PEPeriod is 8 minutes
    //  and we have a 22 arcsecond swing
    PEPeriod=8*60;
    PEMax=11;
    GuideRate=7;    //  guide rate is 7 arcseconds per second
    TimeFactor=1;

}

bool CcdSim::SetupParms()
{
    SetCCDParams(SimulatorSettingsN[0].value,SimulatorSettingsN[1].value,16,SimulatorSettingsN[2].value,SimulatorSettingsN[3].value);
    //  Kwiq
    maxnoise=SimulatorSettingsN[10].value;
    skyglow=SimulatorSettingsN[11].value;
    maxval=SimulatorSettingsN[4].value;
    bias=SimulatorSettingsN[5].value;
    limitingmag=SimulatorSettingsN[7].value;
    saturationmag=SimulatorSettingsN[6].value;
    focallength=SimulatorSettingsN[8].value;   //  focal length of the telescope in millimeters
    CenterOffsetDec=SimulatorSettingsN[12].value;    //  An oag is offset this much from center of scope position (arcminutes);
    seeing=SimulatorSettingsN[9].value;        //  we get real fat stars in this one


    if(RawFrame != NULL) delete RawFrame;
    RawFrameSize=XRes*YRes;                 //  this is pixel count
    RawFrameSize=RawFrameSize*2;            //  Each pixel is 2 bytes
    RawFrameSize+=512;                      //  leave a little extra at the end
    RawFrame=new char[RawFrameSize];

    return true;
}

bool CcdSim::Connect()
{

    SetupParms();

    if(HasGuideHead) {
        SetGuidHeadParams(500,290,8,9.8,12.6);
        RawGuideSize=GXRes*GYRes;                 //  this is pixel count
        RawGuideSize+=512;                      //  leave a little extra at the end
        RawGuiderFrame=new char[RawGuideSize];
    }

    SetTimer(1000);     //  start the timer
    return true;
}

CcdSim::~CcdSim()
{
    //dtor
}

char * CcdSim::getDefaultName()
{
    fprintf(stderr,"Arrived in getDefaultName and deviceName returns '%s'\n",deviceName());
    //if(strlen(deviceName())==0) {
        return (char *)"CcdSimulator";
    //} else {
    //    char n[500];
    //    strcpy(n,deviceName());
    //    return n;
    //}
}

int CcdSim::init_properties()
{
    //  Most hardware layers wont actually have indi properties defined
    //  but the simulators are a special case
    IndiCcd::init_properties();

    IUFillNumber(&EqN[0],"RA","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC","Dec (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNV,EqN,2,"ScopeSim","EQUATORIAL_EOD_COORD","Eq. Coordinates","Main Control",IP_RW,60,IPS_IDLE);
    //IUFillNumberVector(&EqNV,EqN,2,deviceName(),"EQUATORIAL_EOD_COORD","Eq. Coordinates","Main Control",IP_RW,60,IPS_IDLE);

    IUFillNumber(&SimulatorSettingsN[0],"SIM_XRES","CCD X resolution","%4.0f",0,2048,0,1280);
    IUFillNumber(&SimulatorSettingsN[1],"SIM_YRES","CCD Y resolution","%4.0f",0,2048,0,1024);
    IUFillNumber(&SimulatorSettingsN[2],"SIM_XSIZE","CCD X Pixel Size","%4.2f",0,60,0,5.2);
    IUFillNumber(&SimulatorSettingsN[3],"SIM_YSIZE","CCD Y Pixel Size","%4.2f",0,60,0,5.2);
    IUFillNumber(&SimulatorSettingsN[4],"SIM_MAXVAL","CCD Maximum ADU","%4.0f",0,65000,0,65000);
    IUFillNumber(&SimulatorSettingsN[5],"SIM_BIAS","CCD Bias","%4.0f",0,6000,0,1500);
    IUFillNumber(&SimulatorSettingsN[6],"SIM_SATURATION","Saturation Mag","%4.1f",0,20,0,1.0);
    IUFillNumber(&SimulatorSettingsN[7],"SIM_LIMITINGMAG","Limiting Mag","%4.1f",0,20,0,20);
    IUFillNumber(&SimulatorSettingsN[8],"SIM_FOCALLENGTH","Focal Length","%4.0f",0,60000,0,1000);
    IUFillNumber(&SimulatorSettingsN[9],"SIM_FWHM","FWHM (arcseconds)","%4.2f",0,60,0,3.5);
    IUFillNumber(&SimulatorSettingsN[10],"SIM_NOISE","CCD Noise","%4.0f",0,6000,0,50);
    IUFillNumber(&SimulatorSettingsN[11],"SIM_SKYGLOW","Sky Glow (magnitudes)","%4.1f",0,6000,0,19.5);
    IUFillNumber(&SimulatorSettingsN[12],"SIM_DECOFFSET","Dec Offset (arminutes)","%4.1f",0,6000,0,0);
    IUFillNumberVector(&SimulatorSettingsNV,SimulatorSettingsN,13,deviceName(),"SIMULATOR_SETTINGS","Simulator Settings","SimSettings",IP_RW,60,IPS_IDLE);

    IUFillText(&ConfigFileT[0],"SIM_CONFIG","Filename",deviceName());
    IUFillTextVector(&ConfigFileTV,ConfigFileT,1,deviceName(),"SIM_CONFIG_SAVE","Config File","Simulator Config",IP_RW,60,IPS_IDLE);

    IUFillSwitch(&ConfigSaveRestoreS[0],"SAVE","Save",ISS_OFF);
    IUFillSwitch(&ConfigSaveRestoreS[1],"LOAD","Load",ISS_OFF);
    IUFillSwitchVector(&ConfigSaveRestoreSV,ConfigSaveRestoreS,2,deviceName(),"ON_CONFIG_SAVE_RESTORE","Set","Simulator Config",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillSwitch(&TimeFactorS[0],"1X","Actual Time",ISS_ON);
    IUFillSwitch(&TimeFactorS[1],"10X","10x",ISS_OFF);
    IUFillSwitch(&TimeFactorS[2],"100X","100x",ISS_OFF);
    IUFillSwitchVector(&TimeFactorSV,TimeFactorS,3,deviceName(),"ON_TIME_FACTOR","Time Factor","Simulator Config",IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    int rc;
    char err[MAXRBUF];
    char FileName[300];
    MakeConfigName(FileName);
    rc=IUReadConfig(FileName,deviceName(),err);
    if(rc != 0) IDLog("Error reading config '%s'\n",err);

    return 0;
}

void CcdSim::ISGetProperties (const char *dev)
{
    //  First we let our parent populate

    IDLog("CcdSim IsGetProperties with %s\n",dev);
    IndiCcd::ISGetProperties(dev);

    IDDefNumber(&SimulatorSettingsNV, NULL);
    IDDefSwitch(&TimeFactorSV, NULL);
    IDDefText(&ConfigFileTV, NULL);
    IDDefSwitch(&ConfigSaveRestoreSV, NULL);

    IDSnoopDevice("ScopeSim","EQUATORIAL_EOD_COORD");

    return;
}

bool CcdSim::UpdateProperties()
{
    IDDefNumber(&SimulatorSettingsNV, NULL);

    IndiCcd::UpdateProperties();
    return true;
}


bool CcdSim::Disconnect()
{
    delete RawFrame;
    RawFrameSize=0;
    RawFrame=NULL;


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

    gettimeofday(&ExpStart,NULL);
    //  Leave the proper time showing for the draw routines
    DrawCcdFrame();
    //  Now compress the actual wait time
    ExposureRequest=n*TimeFactor;

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

    if(ShowStarField) {
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
        memset(RawFrame,0,RawFrameSize);


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
        pc=XRes/2/BinX;
        pd=0.0;
        pf=YRes/2/BinY;
        //  and we do a simple scale for x and y locations
        //  based on the focal length and pixel size
        //  focal length in mm, pixels in microns
        pa=focallength/PixelSizex*1000/BinX;
        pe=focallength/PixelSizey*1000/BinY;

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
        rad=RA*15.0;
        rar=rad*0.0174532925;
        //  offsetting the dec by the guide head offset
        float cameradec;
        cameradec=Dec+CenterOffsetDec/60;
        decr=cameradec*0.0174532925;

        //fprintf(stderr,"Dec %7.5f  cameradec %7.5f  CenterOffsetDec %4.4f\n",Dec,cameradec,CenterOffsetDec);
        //  now lets calculate the radius we need to fetch
        float radius;

        radius=sqrt((Scalex*Scalex*XRes/2.0*XRes/2.0)+(Scaley*Scaley*YRes/2.0*YRes/2.0));
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
        if(radius > 60) lookuplimit=14;

        //sprintf(gsccmd,"gsc -c %8.6f %+8.6f -r 120 -m 0 9.1",rad+PEOffset,Dec);
        sprintf(gsccmd,"gsc -c %8.6f %+8.6f -r %4.1f -m 0 %4.2f -n 3000",rad+PEOffset,cameradec,radius,lookuplimit);
        //fprintf(stderr,"gsccmd %s\n",gsccmd);
        pp=popen(gsccmd,"r");
        if(pp != NULL) {
            char line[256];
            while(fgets(line,256,pp)!=NULL) {
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


                        rc=DrawImageStar(mag,ccdx,ccdy);
                        drawn+=rc;
                        if(rc==1) {
                            //fprintf(stderr,"star %s scope %6.4f %6.4f star %6.4f %6.4f  ccd %6.2f %6.2f\n",id,rad,Dec,ra,dec,ccdx,ccdy);
                            //fprintf(stderr,"star %s ccd %6.2f %6.2f\n",id,ccdx,ccdy);
                        }
                    }
                //}
            }
            pclose(pp);
        } else {
            IDMessage(deviceName(),"Error looking up stars, is gsc installed with appropriate environment variables set ??");
            //fprintf(stderr,"Error doing gsc lookup\n");
        }
        if(drawn==0) {
            IDMessage(deviceName(),"Got no stars, is gsc installed with appropriate environment variables set ??");

        }
        //fprintf(stderr,"Got %d stars from %d lines drew %d\n",stars,lines,drawn);

        //  now we need to add background sky glow, with vignetting
        //  this is essentially the same math as drawing a dim star with
        //  fwhm equivalent to the full field of view

        float skyflux;
        //  calculate flux from our zero point and gain values
        skyflux=pow(10,((skyglow-z)*k/-2.5));
        //  ok, flux represents one second now
        //  scale up linearly for exposure time
        skyflux=skyflux*ExposureRequest*BinX*BinY;
        //IDLog("SkyFlux = %4.2f ExposureRequest %4.2f\n",skyflux,ExposureRequest);

        unsigned short *pt;
        pt=(unsigned short int *)RawFrame;
        for(int y=0; y<YRes/BinY; y++) {
            for(int x=0; x<XRes/BinX; x++) {
                float dc;   //  distance from center
                float fp;   //  flux this pixel;
                float sx,sy;
                float vig;

                sx=XRes/2/BinX;
                sx=sx-x;
                sy=YRes/2/BinY;
                sy=sy-y;

                vig=XRes/BinX;
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
                //  and put it back
                pt[0]=fp;
                pt++;
                //fp=fa*flux;
                //if(fp < 0) fp=0;
                //AddToPixel(x,y,fp);

            }
        }


        //  Now we add some bias and read noise
        for(x=0; x<XRes; x++) {
            for(y=0; y<YRes; y++) {
                int noise;

                noise=random();
                noise=noise%maxnoise; //

                AddToPixel(x,y,bias+noise);
            }
        }


    } else {
        testvalue++;
        if(testvalue > 255) testvalue=0;
        val=testvalue;

        for(int x=0; x<XRes*YRes; x++) {
            *ptr=val++;
            ptr++;
        }

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

int CcdSim::DrawImageStar(float mag,float x,float y)
{
    //float d;
    //float r;
    int sx,sy;
    int drew=0;
    int boxsizex=5;
    int boxsizey=5;
    float flux;

    if((x<0)||(x>XRes/BinX)||(y<0)||(y>YRes/BinY)) {
        //  this star is not on the ccd frame anyways
        return 0;
    }



    //  calculate flux from our zero point and gain values
    flux=pow(10,((mag-z)*k/-2.5));
    //  ok, flux represents one second now
    //  scale up linearly for exposure time
    flux=flux*ExposureRequest;


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
            fp=fa*flux*BinX*BinY;
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
            rc=AddToPixel(x+sx,y+sy,fp);
            if(rc != 0) drew=1;
        }
    }
    return drew;
}

int CcdSim::AddToPixel(int x,int y,int val)
{
    int drew=0;
    if(x >= 0) {
        if(x < XRes/BinX) {
            if(y >= 0) {
                if(y < YRes/BinY) {
                    unsigned short *pt;
                    int newval;
                    drew++;
                    pt=(unsigned short int *)RawFrame;
                    pt+=(y*XRes/BinX);
                    pt+=x;
                    newval=pt[0];
                    newval+=val;
                    if(newval > maxval) newval=maxval;
                    pt[0]=newval;
                }
            }
        }
    }
    return drew;
}

int CcdSim::GuideNorth(float v)
{
    float c;

    c=v*GuideRate;  //
    c=c/3600;
    Dec=Dec+c;

    return 0;
}
int CcdSim::GuideSouth(float v)
{
    float c;

    c=v*GuideRate;  //
    c=c/3600;
    Dec=Dec-c;

    return 0;
}

int CcdSim::GuideEast(float v)
{
    float c;

    c=v*GuideRate;
    c=c/3600.0/15.0;
    c=c/(cos(Dec*0.0174532925));
    RA=RA-c;

    return 0;
}
int CcdSim::GuideWest(float v)
{
    float c;

    c=v*GuideRate;  //
    c=c/3600.0/15.0;
    c=c/(cos(Dec*0.0174532925));
    RA=RA+c;

    return 0;
}

void CcdSim::ISSnoopDevice (XMLEle *root)
 {
     //fprintf(stderr,"CcdSim handling snoop\n");
     if(IUSnoopNumber(root,&EqNV)==0) {
        float newra,newdec;
        newra=EqN[0].value;
        newdec=EqN[1].value;
        if((newra != RA)||(newdec != Dec)) {
            fprintf(stderr,"RA %4.2f  Dec %4.2f Snooped RA %4.2f  Dec %4.2f\n",RA,Dec,newra,newdec);
            RA=newra;
            Dec=newdec;

        }
     } else {
        fprintf(stderr,"Snoop Failed\n");
     }
 }

bool CcdSim::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("IndiCcd::ISNewNumber %s\n",name);
    if(strcmp(dev,deviceName())==0) {
        //  This is for our device
        //  Now lets see if it's something we process here

        //IDLog("CcdSim::ISNewNumber %s\n",name);
        if(strcmp(name,"SIMULATOR_SETTINGS")==0) {
            //  We are being asked to set camera binning
            SimulatorSettingsNV.s=IPS_OK;

            for(int x=0; x<n; x++) {
                //fprintf(stderr,"name %s value %4.2f\n",names[x],values[x]);
                if(values[x] != 0) {
                    SimulatorSettingsN[x].value=values[x];
                } else {
                    //  We ignore zeros on most of our items
                    //  because they likely mean it was just a field not
                    //  actually filled in, but, for the dec offset it is
                    //  important to keep a zero value
                    if(strcmp(names[x],"SIM_DECOFFSET")==0) {
                        SimulatorSettingsN[x].value=values[x];
                    }
                }
            }

            //  Reset our parameters now
            SetupParms();
            IDSetNumber(&SimulatorSettingsNV,NULL);

            //IDLog("Frame set to %4.0f,%4.0f %4.0f x %4.0f\n",CcdFrameN[0].value,CcdFrameN[1].value,CcdFrameN[2].value,CcdFrameN[3].value);
            //seeing=SimulatorSettingsN[0].value;
            return true;
        }


    }
    //  if we didn't process it, continue up the chain, let somebody else
    //  give it a shot
    return IndiCcd::ISNewNumber(dev,name,values,names,n);
}

bool CcdSim::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  Ok, lets see if this is a property wer process
    IDLog("CcdSim got %d new text items name %s\n",n,name);
    //  first check if it's for our device
    if(strcmp(dev,deviceName())==0) {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,ConfigFileTV.name)==0) {
            //  This is our config name, so, lets process it

            int rc;
            IDLog("calling update text\n");
            ConfigFileTV.s=IPS_OK;
            rc=IUUpdateText(&ConfigFileTV,texts,names,n);
            IDLog("update text returns %d\n",rc);
            //  Update client display
            IDSetText(&ConfigFileTV,NULL);

            //  We processed this one, so, tell the world we did it
            return true;
        }

    }

    return IndiCcd::ISNewText(dev,name,texts,names,n);
}

bool CcdSim::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    IDLog("Enter IsNewSwitch for %s\n",name);
    //for(int x=0; x<n; x++) {
    //    IDLog("Switch %s %d\n",names[x],states[x]);
    //}

    if(strcmp(dev,deviceName())==0) {
        //  This one is for us
        if(strcmp(name,"ON_CONFIG_SAVE_RESTORE")==0) {
            char err[MAXRBUF];
            FILE *fp;
            int rc;
            char FileName[300];

            //  client is telling us what to do with co-ordinate requests
            ConfigSaveRestoreSV.s=IPS_OK;
            IUUpdateSwitch(&ConfigSaveRestoreSV,states,names,n);
            //  Update client display
            IDSetSwitch(&ConfigSaveRestoreSV,NULL);

            if(ConfigSaveRestoreS[0].s==ISS_ON    ) {
                IDLog("CCDSim:: Save Config %s\n",ConfigFileT[0].text);
                MakeConfigName(FileName);
                fp=IUGetConfigFP(FileName,deviceName(),err);
                if(fp != NULL) {
                    IUSaveConfigTag(fp,0);
                    IUSaveConfigNumber(fp,&SimulatorSettingsNV);
                    IUSaveConfigTag(fp,1);
                    fclose(fp);
                    IDMessage(deviceName(),"Configuration Saved\n");
                } else {
                    IDMessage(deviceName(),"Load config failed\n");
                }
            }
            if(ConfigSaveRestoreS[1].s==ISS_ON    ) {
                IDLog("CCDSim:: Restore Config %s\n",ConfigFileT[0].text);
                MakeConfigName(FileName);
                rc=IUReadConfig(FileName,deviceName(),err);
                if(rc != 0) IDMessage(deviceName(),"Error reading config");
                SetupParms();
                UpdateProperties();
            }

            return true;
        }

        if(strcmp(name,"ON_TIME_FACTOR")==0) {

            //  client is telling us what to do with co-ordinate requests
            TimeFactorSV.s=IPS_OK;
            IUUpdateSwitch(&TimeFactorSV,states,names,n);
            //  Update client display
            IDSetSwitch(&TimeFactorSV,NULL);

            if(TimeFactorS[0].s==ISS_ON    ) {
                IDLog("CCDSim:: Time Factor 1\n");
                TimeFactor=1;
            }
            if(TimeFactorS[1].s==ISS_ON    ) {
                IDLog("CCDSim:: Time Factor 0.1\n");
                TimeFactor=0.1;
            }
            if(TimeFactorS[2].s==ISS_ON    ) {
                IDLog("CCDSim:: Time Factor 0.01\n");
                TimeFactor=0.01;
            }

            return true;
        }

    }

    //  Nobody has claimed this, so, ignore it
    return IndiCcd::ISNewSwitch(dev,name,states,names,n);
}

int CcdSim::MakeConfigName(char *buf)
{
    sprintf(buf,"%s/.indi/%s_config.xml",getenv("HOME"),ConfigFileT[0].text);
    return 0;
}

