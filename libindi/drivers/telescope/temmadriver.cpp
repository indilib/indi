/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "temmadriver.h"
#include "indicom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <memory>

#include <string.h>
#include <sys/stat.h>

using namespace INDI::AlignmentSubsystem;


// We declare an auto pointer to temma.
std::unique_ptr<TemmaMount> temma(new TemmaMount());

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
        temma->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        temma->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        temma->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        temma->ISNewNumber(dev, name, values, names, num);
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
   temma->ISSnoopDevice(root);
}

TemmaMount::TemmaMount()
{    
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION ,TEMMA_SLEW_RATES);
    //SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION);
    SetParkDataType(PARK_RA_DEC);
	GotoInProgress=false;
	ParkInProgress=false;
	TemmaInitialized=false;
	SlewRate=1;
}

TemmaMount::~TemmaMount()
{
    //dtor
}

bool TemmaMount::Connect()
{
    bool rc=false;

    if(isConnected()) return true;

    //rc=Connect(PortT[0].text, atoi(IUFindOnSwitch(&BaudRateSP)->name));
    rc=INDI::Telescope::Connect();

    if(rc) {
    }
 
    return rc;
}


const char * TemmaMount::getDefaultName()
{
    return "Temma";
}

bool TemmaMount::initProperties()
{
    bool r;

    //  call base class
    r=INDI::Telescope::initProperties();

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION, TEMMA_SLEW_RATES);
    //SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION);
    SetParkDataType(PARK_RA_DEC_ENCODER);
	initGuiderProperties(getDeviceName(), MOTION_TAB);

    IUFillSwitch(&SlewRateS[0], "SLEW_GUIDE", "Guide", ISS_OFF);
    IUFillSwitch(&SlewRateS[1], "SLEW_MAX", "Max", ISS_ON);
    IUFillSwitchVector(&SlewRateSP, SlewRateS, 2, getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /* How fast do we guide compared to sidereal rate */
    IUFillNumber(&GuideRateN[0], "GUIDE_RATE_WE", "W/E Rate", "%g", 0, 1, 0.1, 0.3);
    IUFillNumber(&GuideRateN[1], "GUIDE_RATE_NS", "N/S Rate", "%g", 0, 1, 0.1, 0.3);
    IUFillNumberVector(&GuideRateNP, GuideRateN, 2, getDeviceName(), "GUIDE_RATE", "Guiding Rate", MOTION_TAB, IP_RW, 0, IPS_IDLE);

    //  probably want to debug this
    //addDebugControl();

        //DEBUG(INDI::Logger::DBG_SESSION, "InitProperties");
        // Add alignment properties
        //InitAlignmentProperties(this);
        // Force the alignment system to always be on
        //getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")->sp[0].s = ISS_ON;

    //  lets set the baud rate property to 19200 at this point
    //  because that's the default for a temma mount
    //getSwitch("TELESCOPE_BAUD_RATE")->sp[1].s = ISS_ON;
	

    return r;
}
void TemmaMount::ISGetProperties (const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);

        // JM 2016-11-10: This is not used anywhere in the code now. Enable it again when you use it
        //defineNumber(&GuideRateNP);

    return;
}
bool TemmaMount::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {

		// It is for us
		//  call upstream for guider properties
        if(!strcmp(name,"GUIDE_RATE"))
		{
			IUUpdateNumber(&GuideRateNP, values, names, n);
			GuideRateNP.s = IPS_OK;
			IDSetNumber(&GuideRateNP, NULL);
			return true;
		}
		if (!strcmp(name,GuideNSNP.name) || !strcmp(name,GuideWENP.name))
		{
			processGuiderProperties(name, values, names, n);
			return true;
		}
		//  and check alignment properties
		ProcessAlignmentNumberProperties(this, name, values, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool TemmaMount::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // It is for us
        ProcessAlignmentSwitchProperties(this, name, states, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool TemmaMount::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // It is for us
        ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool TemmaMount::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        ProcessAlignmentTextProperties(this, name, texts, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool TemmaMount::updateProperties()
{
    INDI::Telescope::updateProperties();
            DEBUG(INDI::Logger::DBG_SESSION, "Update Properties");

    if (isConnected())
    {
        DEBUG(INDI::Logger::DBG_DEBUG,"Temma updating park stuff");
        if (InitPark())
        {
            DEBUG(INDI::Logger::DBG_DEBUG,"Success loading park data");
            // If loading parking data is successful, we just set the default parking values.
            SetAxis1ParkDefault(18);
            SetAxis2ParkDefault(60);
        }
        else
        {
            DEBUG(INDI::Logger::DBG_DEBUG,"Setting park data to default");
            // Otherwise, we set all parking data to default in case no parking data is found.
            SetAxis1Park(18);
            SetAxis2Park(60);
            SetAxis1ParkDefault(18);
            SetAxis2ParkDefault(60);
        }

        defineNumber(&GuideNSNP);
        defineNumber(&GuideWENP);

        // JM 2016-11-10: This is not used anywhere in the code now. Enable it again when you use it
        //defineNumber(&GuideRateNP);

    }
    else
    {

    }

    return true;
}

bool TemmaMount::ReadScopeStatus()
{
    char str[26];
    int bytesWritten, bytesRead;
    int numread;

    //DEBUG(INDI::Logger::DBG_DEBUG,"Temma::ReadScopeStatus() %d\n",PortFD);

    tty_write(PortFD,"E\r\n",3, &bytesWritten);  // Ask mount for current position
    memset(str,0,26);
    numread=TemmaRead(str,25);
    //DEBUG(INDI::Logger::DBG_DEBUG,"Temma read returns %d %s",numread,str);
    //  if the first byte is not our E, return an error
    if(str[0] != 'E' ) return false;

    int d,m,s;
    sscanf(&str[1],"%02d%02d%02d",&d,&m,&s);
    //DEBUG(INDI::Logger::DBG_DEBUG,"%d  %d  %d\n",d,m,s);
    currentRA=d*3600+m*60+s*.6;
    currentRA/=3600;

    sscanf(&str[8],"%02d%02d%01d",&d,&m,&s);
    //DEBUG(INDI::Logger::DBG_DEBUG,"%d  %d  %d\n",d,m,s);
    currentDEC=d*3600+m*60+s*6;
    currentDEC/=3600;

    NewRaDec(currentRA,currentDEC);

	if(GotoInProgress) {
		//  lets see if our goto has finished
		if(strstr(str,"F")) {
            DEBUG(INDI::Logger::DBG_DEBUG,"Goto finished");
			GotoInProgress=false;
			if(ParkInProgress) {
				SetParked(true);
				//  turn off the motor
                DEBUG(INDI::Logger::DBG_DEBUG,"Parked");
				SetTemmaMotorStatus(false);
				ParkInProgress=false;
			}
		} else {
            DEBUG(INDI::Logger::DBG_DEBUG,"Goto in Progress");
		}
	}

    //GetTemmaLst();

	if(SlewActive) {
		sprintf(str,"M \r\n");
		str[1]=Slewbits;
		tty_write(PortFD,str,4,&bytesWritten);
	}

    return true;
}

bool TemmaMount::TemmaSync(double ra,double d)
{
    char str[26];
    int bytesWritten, bytesRead;
    int numread;
	char sign;
	double dec;
    /*  sync involves jumping thru considerable hoops
	first we have to set local sideral time
	then we have to send a Z
	then we set local sideral time again
	and finally we send the co-ordinates we are syncing on
    */
    DEBUG(INDI::Logger::DBG_DEBUG,"Temma::Sync()");
	SetTemmaLst();
	tty_write(PortFD,"Z\r\n",3,&bytesWritten);
	SetTemmaLst();

	//  now lets format up our sync command
	if(d < 0) sign='-';
    else sign='+';
	dec=fabs(d);

	sprintf(str,"D%.2d%.2d%.2d%c%.2d%.2d%.1d\r\n",
			(int)ra,(int)(ra*(double)60)%60,((int)(ra*(double)6000))%100,sign,
			(int)dec,(int)(dec*(double)60)%60,((int)(dec*(double)600))%10);

    DEBUGF(INDI::Logger::DBG_DEBUG,"Sync command : %s",str);
	tty_write(PortFD,str,strlen(str),&bytesWritten);

	//  now read the response
	memset(str,0,26);
	TemmaRead(str,25);
    DEBUGF(INDI::Logger::DBG_DEBUG,"Sync response : %s",str);
	//  if the first character is an R, it's a correct response
	if(str[0] != 'R') return false;
	if(str[1] != '0') {
		// 0 = success
		// 1 = RA error
		// 2 = Dec error
		// 3 = To many digits
		return false;
    }

	return true;

}

bool TemmaMount::Sync(double ra, double dec)
{
	return TemmaSync(ra,dec);
}

bool TemmaMount::Goto(double ra,double d)
{
    char str[26];
    int bytesWritten, bytesRead;
    int numread;
	char sign;
	double dec;
	/*  goto involves hoops, but, not as many as a sync
		first set sideral time
		then issue the goto command
    */
    DEBUG(INDI::Logger::DBG_DEBUG,"Temma::Goto()");

	if(!MotorStatus) {
        DEBUG(INDI::Logger::DBG_DEBUG,"Goto turns on motors");
		SetTemmaMotorStatus(true);
	}

	SetTemmaLst();

	//  now lets format up our goto command
	if(d < 0) sign='-';
    else sign='+';
	dec=fabs(d);

	sprintf(str,"P%.2d%.2d%.2d%c%.2d%.2d%.1d\r\n",
			(int)ra,(int)(ra*(double)60)%60,((int)(ra*(double)6000))%100,sign,
			(int)dec,(int)(dec*(double)60)%60,((int)(dec*(double)600))%10);

    DEBUGF(INDI::Logger::DBG_DEBUG,"Goto command : %s",str);
	tty_write(PortFD,str,strlen(str),&bytesWritten);

	//  now read the response
	memset(str,0,26);
	TemmaRead(str,25);
    DEBUGF(INDI::Logger::DBG_DEBUG,"Goto response : %s",str);
	//  if the first character is an R, it's a correct response
	if(str[0] != 'R') return false;
	if(str[1] != '0') {
		// 0 = success
		// 1 = RA error
		// 2 = Dec error
		// 3 = To many digits
		GotoInProgress=false;
		return false;
    }
	GotoInProgress=true;
	return true;
}


bool TemmaMount::Park()
{

		double lst;
		double lha;
		double RightAscension;
		
		lha=rangeHA(GetAxis1Park());
	    lst=get_local_sideral_time(Longitude);
	    RightAscension=lst-(lha);	//  Get the park position
	    RightAscension=range24(RightAscension);
        DEBUGF(INDI::Logger::DBG_DEBUG,"head to Park position %4.2f %4.2f  %4.2f %4.2f",GetAxis1Park(),lha,RightAscension,GetAxis2Park());

		Goto(RightAscension,GetAxis2Park());

	//  if motors are in standby, turn em on
	//if(!MotorState) SetTemmaMotorStatus(true);
	//Goto(RightAscension,90);
	ParkInProgress=true;

    //DEBUG(INDI::Logger::DBG_DEBUG,"Temma::Park()\n");
    //SetTemmaMotorStatus(false);
    //GetTemmaMotorStatus();
    //SetParked(true);
    return true;
}

bool TemmaMount::UnPark()
{
    SetParked(false);
    //SetTemmaMotorStatus(true);
    GetTemmaMotorStatus();
    return true;
}

bool TemmaMount::SetCurrentPark()
{
	double lha;
	double lst;
    //IDMessage(getDeviceName(),"Setting Default Park Position");
    //IDMessage(getDeviceName(),"Arbitrary park positions not yet supported.");
    //SetAxis1Park(0);
    //SetAxis2Park(90);

	lst=get_local_sideral_time(Longitude);
	lha=lst-currentRA;
	lha=rangeHA(lha);
	//  base class wont store a negative number here
	lha=range24(lha);
	SetAxis1Park(lha);
	SetAxis2Park(currentDEC);

    return true;

}

bool TemmaMount::SetDefaultPark()
{
    // By default az to north, and alt to pole
    IDMessage(getDeviceName(),"Setting Park Data to Default.");
    SetAxis1Park(18);
    SetAxis2Park(90);

    return true;
}

bool  TemmaMount::Abort()
{
    char str[20];
    int bytesWritten;
    DEBUG(INDI::Logger::DBG_DEBUG,"Temma::Abort()");
    tty_write(PortFD,"PS\r\n",4, &bytesWritten);  // Send a stop
	//  Now lets confirm we stopped
    tty_write(PortFD,"s\r\n",3, &bytesWritten);  // check if it stopped

	memset(str,0,20);
	TemmaRead(str,20);
    DEBUGF(INDI::Logger::DBG_DEBUG,"Abort returns %s",str);

	GotoInProgress=false;
	ParkInProgress=false;

    return true;
}


bool TemmaMount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{

	int bytesWritten;
	char buf[10];
	
    DEBUGF(INDI::Logger::DBG_DEBUG,"Temma::MoveNS %d dir %d",command,dir);
	if(!MotorStatus) SetTemmaMotorStatus(true);
	if(!MotorStatus) return false;

	Slewbits=0;
	Slewbits |=64;	//  doc says always on

    DEBUGF(INDI::Logger::DBG_DEBUG,"Temma::MoveNS %d dir %d",command,dir);
	if(!command) {
		if(SlewRate!=0) Slewbits |=1;
		if(dir) {
            DEBUG(INDI::Logger::DBG_DEBUG,"Start slew Dec Up");
			Slewbits |=16;
		} else {
            DEBUG(INDI::Logger::DBG_DEBUG,"Start Slew Dec down");
			Slewbits |=8;
		}
		//sprintf(buf,"M \r\n");
		//buf[1]=Slewbits;
		//tty_write(PortFD,buf,4,&bytesWritten);
		SlewActive=true;
	} else {
		//  No direction bytes to turn it off
        DEBUG(INDI::Logger::DBG_DEBUG,"Abort slew e/w");
		//Abort();
		SlewActive=false;
	}
	sprintf(buf,"M \r\n");
	buf[1]=Slewbits;
	tty_write(PortFD,buf,4,&bytesWritten);

    return true;
}

bool TemmaMount::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
	int bytesWritten;
	char buf[10];
	
	if(!MotorStatus) SetTemmaMotorStatus(true);
	if(!MotorStatus) return false;

	Slewbits=0;
	Slewbits |=64;	//  doc says always on

    DEBUGF(INDI::Logger::DBG_DEBUG,"Temma::MoveWE %d dir %d",command,dir);
	if(!command) {
		if(SlewRate!=0) Slewbits |=1;
		if(dir) {
            DEBUG(INDI::Logger::DBG_DEBUG,"Start slew East");
			Slewbits |=2;
		} else {
            DEBUG(INDI::Logger::DBG_DEBUG,"Start Slew West");
			Slewbits |=4;
		}
		//sprintf(buf,"M \r\n");
		//buf[1]=Slewbits;
		//tty_write(PortFD,buf,4,&bytesWritten);
		SlewActive=true;
	} else {
		//  No direction bytes to turn it off
        DEBUG(INDI::Logger::DBG_DEBUG,"Abort slew e/w");
		//Abort();
		SlewActive=false;
	}
	sprintf(buf,"M \r\n");
	buf[1]=Slewbits;
	tty_write(PortFD,buf,4,&bytesWritten);

    return true;
}

bool TemmaMount::SetSlewRate(int s)
{
    DEBUGF(INDI::Logger::DBG_DEBUG,"Temma::Slew rate %d",s);
	SlewRate=s;
    return true;
}

IPState TemmaMount::GuideNorth(float ms)
{
	int bytesWritten;
	char buf[10];
	char bits;

    DEBUGF(INDI::Logger::DBG_DEBUG,"Guide North %4.0f",ms);
	if(!MotorStatus) return IPS_ALERT;
	if(SlewActive) return IPS_ALERT;

	bits=0;
	bits |= 64;	//  doc says always on
	bits |= 8;	//  going north
	sprintf(buf,"M \r\n");
	buf[1]=bits;
	tty_write(PortFD,buf,4,&bytesWritten);
	usleep(ms*1000);
	bits=64;
	buf[1]=bits;
	tty_write(PortFD,buf,4,&bytesWritten);

	return IPS_OK;
}

IPState TemmaMount::GuideSouth(float ms)
{
	int bytesWritten;
	char buf[10];
	char bits;
    DEBUGF(INDI::Logger::DBG_DEBUG,"Guide South %4.0f",ms);
	if(!MotorStatus) return IPS_ALERT;
	if(SlewActive) return IPS_ALERT;

	bits=0;
	bits |= 64;	//  doc says always on
	bits |= 16;	//  going south
	sprintf(buf,"M \r\n");
	buf[1]=bits;
	tty_write(PortFD,buf,4,&bytesWritten);
	usleep(ms*1000);
	bits=64;
	buf[1]=bits;
	tty_write(PortFD,buf,4,&bytesWritten);
	return IPS_OK;
}

IPState TemmaMount::GuideEast(float ms)
{
	int bytesWritten;
	char buf[10];
	char bits;
    DEBUGF(INDI::Logger::DBG_DEBUG,"Guide East %4.0f",ms);
	if(!MotorStatus) return IPS_ALERT;
	if(SlewActive) return IPS_ALERT;

	bits=0;
	bits |= 64;	//  doc says always on
	bits |=  2;	//  going east
	sprintf(buf,"M \r\n");
	buf[1]=bits;
	tty_write(PortFD,buf,4,&bytesWritten);
	usleep(ms*1000);
	bits=64;
	buf[1]=bits;
	tty_write(PortFD,buf,4,&bytesWritten);
	return IPS_OK;
}

IPState TemmaMount::GuideWest(float ms)
{
	int bytesWritten;
	char buf[10];
	char bits;
    DEBUGF(INDI::Logger::DBG_DEBUG,"Guide West %4.0f",ms);
	if(!MotorStatus) return IPS_ALERT;
	if(SlewActive) return IPS_ALERT;

	bits=0;
	bits |= 64;	//  doc says always on
	bits |=  4;	//  going west
	sprintf(buf,"M \r\n");
	buf[1]=bits;
	tty_write(PortFD,buf,4,&bytesWritten);
	usleep(ms*1000);
	bits=64;
	buf[1]=bits;
	tty_write(PortFD,buf,4,&bytesWritten);
	return IPS_OK;
}

bool TemmaMount::updateTime(ln_date *utc, double utc_offset)
{

    char str[20];
    int bytesWritten, bytesRead;
    int numread;

    DEBUG(INDI::Logger::DBG_DEBUG,"Temma::UpdateTime()");
    return true;
}

bool TemmaMount::updateLocation(double latitude, double longitude, double elevation)
{ 
    char str[20];
    int bytesWritten, bytesRead;
    int numread;
	bool DoFirstTimeStuff=false;
		double lst;

	Longitude=longitude;
	Lattitude=latitude;

DEBUG(INDI::Logger::DBG_DEBUG,"Temma::updateLocation");
//  A temma mount must have the LST and Lattitude set
//  Prior to these being set, reads will return garbage
	if(!TemmaInitialized) {
	    //GetTemmaLattitude();
	    SetTemmaLattitude(latitude);
	    //GetTemmaLattitude();
		SetTemmaLst();
		TemmaInitialized=true;
		DoFirstTimeStuff=true;

		//  We were NOT intialized, so, in case there is not park position set
		//  Sync to the position of bar vertical, telescope pointed at pole
		double RightAscension;
	    lst=get_local_sideral_time(Longitude);
	    RightAscension=lst-(-6);	//  Hour angle is negative 6 in this case
	    RightAscension=range24(RightAscension);
        DEBUGF(INDI::Logger::DBG_DEBUG,"Initial sync on %4.2f",RightAscension);

		TemmaSync(RightAscension,90);

	}
	lst=get_local_sideral_time(longitude);
	
    DEBUGF(INDI::Logger::DBG_DEBUG,"lst here is %4.1f",lst);
//  if the mount is parked, then we should sync it on our park position
	if(isParked()) {
		//  Here we have to sync on our park position
		double RightAscension;
	    lst=get_local_sideral_time(Longitude);
	    RightAscension=lst-(rangeHA(GetAxis1Park()));	//  Get the park position
	    RightAscension=range24(RightAscension);
        DEBUGF(INDI::Logger::DBG_DEBUG,"Sync to Park position %4.2f %4.2f  %4.2f",GetAxis1Park(),RightAscension,GetAxis2Park());
		TemmaSync(RightAscension,GetAxis2Park());
        DEBUG(INDI::Logger::DBG_DEBUG,"Turn motors off");
		SetTemmaMotorStatus(false);

	} else {
		sleep(1);
        DEBUG(INDI::Logger::DBG_DEBUG,"Mount is not parked");
		//SetTemmaMotorStatus(true);
	}
    
	
    return true;
}



        
ln_equ_posn TemmaMount::TelescopeToSky(double ra,double dec)
{
    double RightAscension,Declination;
    ln_equ_posn eq;

    if(GetAlignmentDatabase().size() > 1) {

    	TelescopeDirectionVector TDV;

        /*  Use this if we ar converting eq co-ords
	//  but it's broken for now
    	eq.ra=ra*360/24;
    	eq.dec=dec;
    	TDV=TelescopeDirectionVectorFromEquatorialCoordinates(eq);
	*/

	/* This code does a conversion from ra/dec to alt/az
	// before calling the alignment stuff
        ln_lnlat_posn here;
    	ln_hrz_posn altaz;

    	here.lat=LocationN[LOCATION_LATITUDE].value;
    	here.lng=LocationN[LOCATION_LONGITUDE].value;
    	eq.ra=ra*360.0/24.0;	//  this is wanted in degrees, not hours
    	eq.dec=dec;
    	ln_get_hrz_from_equ(&eq,&here,ln_get_julian_from_sys(),&altaz);
    	TDV=TelescopeDirectionVectorFromAltitudeAzimuth(altaz);
*/	

	/*  and here we convert from ra/dec to hour angle / dec before calling alignment stuff */
    	double lha,lst;
	lst=get_local_sideral_time(LocationN[LOCATION_LONGITUDE].value);
	lha=get_local_hour_angle(lst,ra);
	//  convert lha to degrees
	lha=lha*360/24;
	eq.ra=lha;
	eq.dec=dec;
    	TDV=TelescopeDirectionVectorFromLocalHourAngleDeclination(eq);
      
	
    	if (TransformTelescopeToCelestial( TDV, RightAscension, Declination)) {
	    //  if we get here, the conversion was successful
        //DEBUG(INDI::Logger::DBG_DEBUG,"new values %6.4f %6.4f %6.4f  %6.4f Deltas %3.0lf %3.0lf\n",ra,dec,RightAscension,Declination,(ra-RightAscension)*60,(dec-Declination)*60);
    	} else {
	    //if the conversion failed, return raw data
            RightAscension=ra;
            Declination=dec;       
    	}

    } else {
	//  With less than 2 align points
	// Just return raw data
    	RightAscension=ra;
	Declination=dec;
    }

    eq.ra=RightAscension;
    eq.dec=Declination;
    return eq;
}

ln_equ_posn TemmaMount::SkyToTelescope(double ra,double dec)
{

    ln_equ_posn eq;
    ln_lnlat_posn here;
    ln_hrz_posn altaz;
    TelescopeDirectionVector TDV;
    double RightAscension,Declination;

/*
*/


    if(GetAlignmentDatabase().size() > 1) {
	//  if the alignment system has been turned off
	//  this transformation will fail, and we fall thru
	//  to using raw co-ordinates from the mount
	if(TransformCelestialToTelescope(ra, dec, 0.0, TDV)) {

            /*  Initial attemp, using RA/DEC co-ordinates talking to alignment system
    	    EquatorialCoordinatesFromTelescopeDirectionVector(TDV,eq);
	    RightAscension=eq.ra*24.0/360;
	    Declination=eq.dec;
	    if(RightAscension < 0) RightAscension+=24.0;
    	    DEBUGF(INDI::Logger::DBG_SESSION,"Transformed Co-ordinates %g %g\n",RightAscension,Declination);
            */


	    //  nasty altaz kludge, use al/az co-ordinates to talk to alignment system
	    /*
            eq.ra=ra*360/24;
            eq.dec=dec;
            here.lat=LocationN[LOCATION_LATITUDE].value;
            here.lng=LocationN[LOCATION_LONGITUDE].value;
            ln_get_hrz_from_equ(&eq,&here,ln_get_julian_from_sys(),&altaz);
	    AltitudeAzimuthFromTelescopeDirectionVector(TDV,altaz);
	    //  now convert the resulting altaz into radec
    	    ln_get_equ_from_hrz(&altaz,&here,ln_get_julian_from_sys(),&eq);
	    RightAscension=eq.ra*24.0/360.0;
	    Declination=eq.dec;
            */
            

	    /* now lets convert from telescope to lha/dec */
	    double lst;
	    LocalHourAngleDeclinationFromTelescopeDirectionVector(TDV,eq);
	    //  and now we have to convert from lha back to RA
	    lst=get_local_sideral_time(LocationN[LOCATION_LONGITUDE].value);
	    eq.ra=eq.ra*24/360;
	    RightAscension=lst-eq.ra;
	    RightAscension=range24(RightAscension);
	    Declination=eq.dec;
	    

        } else {
            DEBUGF(INDI::Logger::DBG_SESSION,"Transform failed, using raw co-ordinates %g %g",ra,dec);
	    RightAscension=ra;
	    Declination=dec;
        }
    } else {
    	RightAscension=ra;
    	Declination=dec;
    }

    eq.ra=RightAscension;
    eq.dec=Declination;
    //eq.ra=ra;
    //eq.dec=dec;
    return eq;
}

bool TemmaMount::GetTemmaVersion()
{
    char str[50];
    int rc,numread,bytesWritten;

    rc=tty_write(PortFD,"v\r\n",3, &bytesWritten);  // Ask mount for current position
    //rc=tty_write(PortFD,"E\r\n",3, &bytesWritten);  // Ask mount for current position
    DEBUGF(INDI::Logger::DBG_DEBUG,"Wrote %d of 3 bytes rc %d",bytesWritten,rc);
    if(rc != TTY_OK)
        DEBUG(INDI::Logger::DBG_DEBUG,"got a write error");
    memset(str,0,50);
    numread=TemmaRead(str,50);
	if(numread == -1) {
        DEBUG(INDI::Logger::DBG_DEBUG,"get version returns bufer overflow");
		return false;
	}
	if(numread==0) {
        DEBUG(INDI::Logger::DBG_DEBUG,"get version returns nothing");
		return false;
	}
	

    DEBUGF(INDI::Logger::DBG_DEBUG,"Temma Version %d %s",numread,str);
    if(str[0] != 'v' ) return false;
    //if(str[0] != 'E' ) return false;

    return true;
}

bool TemmaMount::GetTemmaMotorStatus()
{
    char str[50];
    int rc,numread,bytesWritten;

    tty_write(PortFD,"STN-COD\r\n",9, &bytesWritten);  // Ask mount for current status
    memset(str,0,50);
    numread=TemmaRead(str,50);

    DEBUGF(INDI::Logger::DBG_DEBUG,"Temma motor %d: %s",numread,str);
    if(strstr(str,"off")) MotorStatus=true;
	else MotorStatus=false;
    return MotorStatus;

}

bool TemmaMount::SetTemmaMotorStatus(bool state)
{
    char str[50];
    int rc,numread,bytesWritten;

    if(!state) {
    	tty_write(PortFD,"STN-ON\r\n",8, &bytesWritten);  // motor on
    } else {
    	tty_write(PortFD,"STN-OFF\r\n",9, &bytesWritten);  // motor off
    }
    memset(str,0,50);
    numread=TemmaRead(str,50);

    DEBUGF(INDI::Logger::DBG_DEBUG,"Temma motor status return  %d: %s",numread,str);
    //if(strncmp(str,"stn-off") return false;
	GetTemmaMotorStatus();
    return true;

}


/*  bit of a hack, returns true if lst is a sane value, false if it is not sane */
int TemmaMount::GetTemmaLst()
{
    char str[50];
    int bytesWritten,numread;
    tty_write(PortFD,"g\r\n",3, &bytesWritten);  // get lst
    memset(str,0,50);
    numread=TemmaRead(str,50);
    DEBUGF(INDI::Logger::DBG_DEBUG,"TemmaLst : %d",numread);
    for(int x=0; x<numread; x++) {
        DEBUGF(INDI::Logger::DBG_DEBUG,"%02x ",(unsigned char)str[x]);
    }    
	//  if we got ascii characters back
	//  it's good
	for(int x=1; x<7; x++) {
		if((str[x] < 48)||(str[x] > 57)) return false;
	}
	// otherwise we read garbage
	return true;

}

bool TemmaMount::SetTemmaLst()
{
    char str[50];
    int bytesWritten,numread;
    double lst;
    DEBUGF(INDI::Logger::DBG_DEBUG,"Setting lst with %4.2f",Longitude);
    lst=get_local_sideral_time(Longitude);
    sprintf(str,"T%.2d%.2d%.2d\r\n", (int)lst,
			((int)(lst*60))%60,
			((int)(lst*3600))%60);
    DEBUGF(INDI::Logger::DBG_DEBUG,"SetLst : %s",str);
    tty_write(PortFD,str,strlen(str), &bytesWritten);  // get lst

    return true;
}


double TemmaMount::GetTemmaLattitude()
{
    char str[50];
    int bytesWritten,numread;
    tty_write(PortFD,"i\r\n",3, &bytesWritten);  // get lattitude
    memset(str,0,50);
    numread=TemmaRead(str,50);
    DEBUGF(INDI::Logger::DBG_DEBUG,"TemmaLattitude : %d",numread);
    for(int x=0; x<numread; x++) {
        DEBUGF(INDI::Logger::DBG_DEBUG,"%02x",(unsigned char)str[x]);
    }

    return true;
}

bool TemmaMount::SetTemmaLattitude(double lat)
{
    char str[50];
    char sign;
    double l;
    int d,m,s;
    int bytesWritten;

    if (lat>0){
        sign='+';
    } else {
        sign='-';
    }
    l=fabs(lat);
    d=(int)l;
    l=(l-d)*60;
    m=(int)l;
    l=(l-m)*6;
    s=(int)l;

    sprintf(str,"I%c%.2d%.2d%.1d\r\n", sign,d,m,s);
    DEBUGF(INDI::Logger::DBG_DEBUG,"%s",str);
    tty_write(PortFD,str,strlen(str),&bytesWritten);
   
    return true;
}

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

bool TemmaMount::Handshake()
{
    DEBUG(INDI::Logger::DBG_DEBUG,"Calling get version");
    bool rc=GetTemmaVersion();
    if (!rc)
        return false;

    rc=GetTemmaLst();
    if(rc)
    {
        DEBUG(INDI::Logger::DBG_DEBUG,"Temma is intialized");
        TemmaInitialized=true;
    }
    else
    {
        DEBUG(INDI::Logger::DBG_DEBUG,"Temma is not initialized");
        TemmaInitialized=false;
    }

    GetTemmaMotorStatus();
    return true;
}

int TemmaMount::TemmaRead(char *buf,int size)
{
    int bytesRead;
    int count=0;
    int ptr=0;
	int rc;
    while(ptr < size) {
        rc=tty_read(PortFD,&buf[ptr],1,2, &bytesRead);  //  Read 1 byte of response into the buffer with 5 second timeout
        if(bytesRead==1) {
	    //  we got a byte
            count++;
            if(count > 1) {
        //DEBUG(INDI::Logger::DBG_DEBUG,"%d ",buf[ptr]);
                if(buf[ptr]==10) {
                  if(buf[ptr-1]==13) {
                      //  we have the cr/lf from the response
                      //  Send it back to the caller
                      return count;
                  }
                }
            }
            ptr++;
            
        } else {
           DEBUGF(INDI::Logger::DBG_DEBUG,"We timed out reading bytes %d",count);
           return 0;
        }
    }
    //  if we get here, we got more than size bytes, and still dont have a cr/lf
    DEBUGF(INDI::Logger::DBG_DEBUG,"Read return error after %d bytes",bytesRead);
    return -1;
}
