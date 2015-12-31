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

#include "synscanmount.h"
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


// We declare an auto pointer to Synscan.
std::unique_ptr<SynscanMount> synscan(new SynscanMount());

void ISPoll(void *p);

void ISGetProperties(const char *dev)
{
        synscan->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        synscan->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        synscan->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        synscan->ISNewNumber(dev, name, values, names, num);
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

SynscanMount::SynscanMount()
{    
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION ,SYNSCAN_SLEW_RATES);
    SetParkDataType(PARK_RA_DEC_ENCODER);
    strcpy(LastParkRead,"");
    SlewRate=5;
    HasFailed=true;
    FirstConnect=true;
}

SynscanMount::~SynscanMount()
{
    //dtor
}

bool SynscanMount::Connect()
{
    bool rc=false;

    if(isConnected()) return true;

    //rc=Connect(PortT[0].text, atoi(IUFindOnSwitch(&BaudRateSP)->name));
    rc=INDI::Telescope::Connect();

    if(rc) {
    }
    return rc;
}


const char * SynscanMount::getDefaultName()
{
    return "SynScan";
}

bool SynscanMount::initProperties()
{
    bool r;



//fprintf(stderr,"Synscan initProperties\n");
    //  call base class
    r=INDI::Telescope::initProperties();

    //SetTelescopeCapability(TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION ,SYNSCAN_SLEW_RATES);
    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_ABORT | TELESCOPE_CAN_SYNC | TELESCOPE_HAS_TIME | TELESCOPE_HAS_LOCATION ,SYNSCAN_SLEW_RATES);
    SetParkDataType(PARK_RA_DEC_ENCODER);


    //  probably want to debug this
    addDebugControl();

            DEBUG(INDI::Logger::DBG_SESSION, "InitProperties");
        // Add alignment properties
        InitAlignmentProperties(this);
        // Force the alignment system to always be on
        getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE")->sp[0].s = ISS_ON;



    return r;
}
void SynscanMount::ISGetProperties (const char *dev)
{
    /* First we let our parent populate */
    INDI::Telescope::ISGetProperties(dev);


    return;
}
bool SynscanMount::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // It is for us
        ProcessAlignmentNumberProperties(this, name, values, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool SynscanMount::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // It is for us
        ProcessAlignmentSwitchProperties(this, name, states, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool SynscanMount::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        // It is for us
        ProcessAlignmentBLOBProperties(this, name, sizes, blobsizes, blobs, formats, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool SynscanMount::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        ProcessAlignmentTextProperties(this, name, texts, names, n);
    }
    // Pass it up the chain
    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool SynscanMount::updateProperties()
{
    INDI::Telescope::updateProperties();
            DEBUG(INDI::Logger::DBG_SESSION, "Update Properties");

    if (isConnected())
    {


    }
    else
    {

    }

    return true;
}


bool SynscanMount::AnalyzeHandset()
{
	bool r;
	bool rc;
	int caps;
	//  call the parent
	//r=INDI::Telescope::Connect();
	//if( !r) {
	//	return r;
	//}
	//  get the basics
	caps=GetTelescopeCapability();

	IDMessage(getDeviceName(),"Detecting Synscan Handset Capabilities");
	rc=ReadLocation();
	if(rc) {
	    CanSetLocation=true;
	    //caps |= TELESCOPE_HAS_LOCATION;
            rc=ReadTime();
            //if(rc) caps |= TELESCOPE_HAS_TIME;
	} else {
	    //CanSetLocation=false;
        }

	int tmp,tmp1,tmp2;
	int bytesWritten,bytesRead;
	char str[20];

    	bytesRead=0;
	memset(str,0,20);
    	tty_write(PortFD,"J",1,&bytesWritten);
    	tty_read(PortFD,str,2,2,&bytesRead);
	tmp=str[0];

    	bytesRead=0;
	memset(str,0,20);
    	tty_write(PortFD,"m",1,&bytesWritten);
    	tty_read(PortFD,str,2,2,&bytesRead);
	tmp=str[0];
	//fprintf(stderr,"Model %d\n",tmp);
	MountModel=tmp;
	IDMessage(getDeviceName(),"Mount Model %d",tmp);

    	bytesRead=0;
	memset(str,0,20);
    	tty_write(PortFD,"V",1,&bytesWritten);
    	tty_read(PortFD,str,3,2,&bytesRead);
	tmp=str[0];
        tmp1=str[1];
        tmp2=str[2];
	//fprintf(stderr,"version %d %d %d\n",tmp,tmp1,tmp2);
	
	FirmwareVersion=tmp2;
	FirmwareVersion/=100;
	FirmwareVersion+=tmp1;
	FirmwareVersion/=100;
	FirmwareVersion+=tmp;
	//fprintf(stderr,"FirmwareVersion %6.4f\n",FirmwareVersion);
	IDMessage(getDeviceName(),"Handset Firmware Version %lf",FirmwareVersion);

	SetTelescopeCapability(caps,SYNSCAN_SLEW_RATES);

        if (InitPark()) {
            //fprintf(stderr,"InitPark returns true\n");
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(90);
        } else {
            //fprintf(stderr,"InitPark returns false\n");
            SetAxis1Park(0);
            SetAxis2Park(90);
            SetAxis1ParkDefault(0);
            SetAxis2ParkDefault(90);
        }

    //SetApproximateMountAlignmentFromMountType(EQUATORIAL);
    //SetApproximateMountAlignment(NORTH_CELESTIAL_POLE);


	return r;
}


bool SynscanMount::ReadScopeStatus()
{
    char str[20];
    int bytesWritten, bytesRead;
    int numread;
    double ra,dec;
    long unsigned int n1,n2;

    //IDLog("SynScan Read Status\n");
    //tty_write(PortFD,(unsigned char *)"Ka",2, &bytesWritten);  //  test for an echo

    tty_write(PortFD,"Ka",2, &bytesWritten);  //  test for an echo
    tty_read(PortFD,str,2,2, &bytesRead);  //  Read 2 bytes of response
    if(str[1] != '#')
    {
        //  this is not a correct echo
        if (isDebug())
            IDLog("ReadStatus Echo Fail. %s\n", str);
        IDMessage(getDeviceName(),"Mount Not Responding");
	HasFailed=true;
        return false;
    }

/*
//  With 3.37 firmware, on the older line of eq6 mounts
//  The handset does not always initialize the communication with the motors correctly
//  We can check for this condition by querying the motors for firmware version
//  and if it returns zero, it means we need to power cycle the handset
//  and try again after it restarts again

    if(HasFailed) {
        int v1,v2;
	//fprintf(stderr,"Calling passthru command to get motor firmware versions\n");
        v1=PassthruCommand(0xfe,0x11,1,0,2);
        v2=PassthruCommand(0xfe,0x10,1,0,2);
        fprintf(stderr,"Motor firmware versions %d %d\n",v1,v2);
        if((v1==0)||(v2==0)) {
            IDMessage(getDeviceName(),"Cannot proceed");
            IDMessage(getDeviceName(),"Handset is responding, but Motors are Not Responding");
            return false;
	}
        //  if we get here, both motors are responding again
        //  so the problem is solved
	HasFailed=false;
    }
*/

    if(FirstConnect) {
	//fprintf(stderr,"sending fake radec\n");
	//NewRaDec(0,0);
	//return true;
	//  the first time we come thru, we need to figure out a few things about our handset
        //  ie can it do date and time etc
	AnalyzeHandset();
	FirstConnect=false;
	return true;
    } else {
	//  on subsequent passes, we just need to read the time
        if(HasTime()) {
            ReadTime();
        }
        if(HasLocation()) {
	    //  this flag is set when we get a new lat/long from the host
            //  so we should go thru the read routine once now, so things update
            //  correctly in the client displays
            if(ReadLatLong) {
                ReadLocation();
            }
        }
    }

    if(TrackState==SCOPE_SLEWING)
    {
        //  We have a slew in progress
        //  lets see if it's complete
        //  This only works for ra/dec goto commands
        //  The goto complete flag doesn't trip for ALT/AZ commands
        memset(str,0,3);
        tty_write(PortFD,"L",1, &bytesWritten);
        numread=tty_read(PortFD,str,2,3, &bytesRead);
        if(str[0]!=48)
        {
            //  Nothing to do here
        } else
        {
            if(TrackState==SCOPE_PARKING) TrackState=SCOPE_PARKED;
            else TrackState=SCOPE_TRACKING;
        }
    }
    if(TrackState==SCOPE_PARKING)
    {
	//IDMessage(getDeviceName(),"Firmware %lf",FirmwareVersion);
	if(FirmwareVersion == 4.103500) {
	    //  With this firmware the correct way
	    //  is to check the slewing flat
            memset(str,0,3);
            tty_write(PortFD,"L",1, &bytesWritten);
            numread=tty_read(PortFD,str,2,3, &bytesRead);
            if(str[0]!=48)
            {
                //  Nothing to do here
            } else {
		if(NumPark++ < 2) {
		    Park();
                } else {
		    TrackState=SCOPE_PARKED;
	 	    //IDMessage(getDeviceName(),"Telescope is Parked");
		    SetParked(true);
		}
            }

	} else {
	
        //  ok, lets try read where we are
        //  and see if we have reached the park position
	//  newer firmware versions dont read it back the same way
	//  so we watch now to see if we get the same read twice in a row
	//  to confirm that it has stopped moving
        memset(str,0,20);
        tty_write(PortFD,"z",1, &bytesWritten);
        numread=tty_read(PortFD,str,18,2, &bytesRead);

        //IDMessage(getDeviceName(),"Park Read %s %d",str,StopCount);

        if(strncmp((char *)str,LastParkRead,18)==0)
        {
	    //  We find that often after it stops from park 
            //  it's off the park position by a small amount
            //  issuing another park command gets a small movement and then
	    if(++StopCount > 2) {
	        if(NumPark++ < 2) {
		    StopCount=0;
		    //IDMessage(getDeviceName(),"Sending park again");
		    Park();
                } else {
                    TrackState=SCOPE_PARKED;
                    //ParkSP.s=IPS_OK;
                    //IDSetSwitch(&ParkSP,NULL);
                    //IDMessage(getDeviceName(),"Telescope is Parked.");
                    SetParked(true);
                }
   	    } else {
		//StopCount=0;
            }
        } else {
	    StopCount=0;
	}
	strcpy(LastParkRead,str);
	}

    }

    memset(str,0,20);
    tty_write(PortFD,"e",1, &bytesWritten);
    numread=tty_read(PortFD,str,18,1, &bytesRead);
    if (bytesRead!=18)
	//if(str[17] != '#')
    {
        IDLog("read status bytes didn't get a full read\n");
        return false;
    }

    if (isDebug())
        IDLog("Bytes read is (%s)\n", str);

    // bytes read is as expected
    //sscanf((char *)str,"%x",&n1);
    //sscanf((char *)&str[9],"%x",&n2);

    // JM read as unsigned long int, otherwise we get negative RA!
    sscanf(str, "%lx,%lx#", &n1, &n2);

    ra=(double)n1/0x100000000*24.0;
    dec=(double)n2/0x100000000*360.0;
    currentRA=ra;
    currentDEC=dec;


//  Before we pass this back to a client
//  run it thru the alignment matrix to correct the data

    if(GetAlignmentDatabase().size() > 1) {

    	double RightAscension,Declination;
    	ln_equ_posn eq;
    	TelescopeDirectionVector TDV;

    	eq.ra=ra*360/24;
    	eq.dec=dec;

    	TDV=TelescopeDirectionVectorFromEquatorialCoordinates(eq);
    	if (TransformTelescopeToCelestial( TDV, RightAscension, Declination)) {
	    if(RightAscension < 0) RightAscension+=24.0;
	    //fprintf(stderr,"new values %6.4f %6.4f %6.4f  %6.4f Deltas %3.0lf %3.0lf\n",ra,dec,RightAscension,Declination,(ra-RightAscension)*60,(dec-Declination)*60);

    	} else {
	    //fprintf(stderr,"Conversion failed\n");
            RightAscension=ra;
            Declination=dec;       
    	}

    	NewRaDec(RightAscension,Declination);
    } else {
    	NewRaDec(ra,dec);
    }
    return true;
}

bool SynscanMount::Goto(double ra,double dec)
{
    char str[20];
    int n1,n2;
    int numread, bytesWritten, bytesRead;
//fprintf(stderr,"Enter Goto  %4.4lf %4.4lf\n",ra,dec);
    DEBUGF(INDI::Logger::DBG_SESSION,"Enter Goto %g %g",ra,dec);

    ln_equ_posn eq;
    ln_lnlat_posn here;
    ln_hrz_posn altaz;
    double RightAscension,Declination;
    TelescopeDirectionVector TDV;

    eq.ra=ra*360/24;
    eq.dec=dec;
    here.lat=LocationN[LOCATION_LATITUDE].value;
    here.lng=LocationN[LOCATION_LONGITUDE].value;

    ln_get_hrz_from_equ(&eq,&here,ln_get_julian_from_sys(),&altaz);

    if(GetAlignmentDatabase().size() > 1) {
	//  if the alignment system has been turned off
	//  this transformation will fail, and we fall thru
	//  to using raw co-ordinates from the mount
	if(TransformCelestialToTelescope(ra, dec, 0.0, TDV)) {
    	    EquatorialCoordinatesFromTelescopeDirectionVector(TDV,eq);
//fprintf(stderr,"****  Transform was successful\n");
	    RightAscension=eq.ra*24.0/360;
	    Declination=eq.dec;
	    if(RightAscension < 0) RightAscension+=24.0;
//fprintf(stderr,"New co-ords %4.4lf %4.4lf\n",RightAscension,Declination);
    	    DEBUGF(INDI::Logger::DBG_SESSION,"Transformed Co-ordinates %g %g\n",RightAscension,Declination);
        } else {
//fprintf(stderr,"**** Transform failed\n");
            DEBUGF(INDI::Logger::DBG_SESSION,"Transform failed, using raw co-ordinates %g %g\n",ra,dec);

	    RightAscension=ra;
	    Declination=dec;
        }
    } else {
    	RightAscension=ra;
    	Declination=dec;
    }

    //  not fleshed in yet
    tty_write(PortFD,"Ka",2, &bytesWritten);  //  test for an echo
    tty_read(PortFD,str,2,2, &bytesRead);  //  Read 2 bytes of response
    if(str[1] != '#') {
        //  this is not a correct echo
        //  so we are not talking to a mount properly
        return false;
    }
    //  Ok, mount is alive and well
    //  so, lets format up a goto command
    n1=RightAscension*0x1000000/24;
    n2=Declination*0x1000000/360;
    n1=n1<<8;
    n2=n2<<8;
    sprintf((char *)str,"r%08X,%08X",n1,n2);
    tty_write(PortFD,str,18, &bytesWritten);
    TrackState=SCOPE_SLEWING;
    numread=tty_read(PortFD,str,1,60, &bytesRead);
    if (bytesRead!=1||str[0]!='#')
    {
        if (isDebug())
            IDLog("Timeout waiting for scope to complete slewing.");
        return false;
    }

    return true;
}

bool SynscanMount::Sync(double ra, double dec)
{
    ln_equ_posn eq;
    AlignmentDatabaseEntry NewEntry;
    ln_lnlat_posn here;

    DEBUGF(INDI::Logger::DBG_SESSION,"Sync %g %g -> %g %g\n",currentRA,currentDEC,ra,dec);

    here.lat=LocationN[LOCATION_LATITUDE].value;
    here.lng=LocationN[LOCATION_LONGITUDE].value;

    //  this is where we think we are pointed now
    eq.ra=currentRA*360.0/24.0;	//  this is wanted in degrees, not hours
    eq.dec=currentDEC;

    //  And this is where the client says we are pointed
    NewEntry.ObservationJulianDate = ln_get_julian_from_sys();
    NewEntry.RightAscension=ra;
    NewEntry.Declination=dec;
    NewEntry.TelescopeDirection=TelescopeDirectionVectorFromEquatorialCoordinates(eq);
    NewEntry.PrivateDataSize=0;
    if (!CheckForDuplicateSyncPoint(NewEntry))
    {
        GetAlignmentDatabase().push_back(NewEntry);
        // Tell the client about size change
        UpdateSize();
        // Tell the math plugin to reinitialise
        Initialise(this);
        return true;
    }
    return false;
}

bool SynscanMount::Park()
{
    char str[20];
    int numread, bytesWritten, bytesRead;

    strcpy(LastParkRead,"");
    memset(str,0,3);
    tty_write(PortFD,"Ka",2, &bytesWritten);  //  test for an echo
    tty_read(PortFD,str,2,2, &bytesRead);  //  Read 2 bytes of response
    if(str[1] != '#')
    {
        //  this is not a correct echo
        //  so we are not talking to a mount properly
        return false;
    }
    //  Now we stop tracking
    tty_write(PortFD,"T0",2, &bytesWritten);
    numread=tty_read(PortFD,str,1,60, &bytesRead);
    if (bytesRead!=1||str[0]!='#')
    {
        if (isDebug())
            IDLog("Timeout waiting for scope to stop tracking.");
        return false;
    }

    //sprintf((char *)str,"b%08X,%08X",0x0,0x40000000);
    tty_write(PortFD,"b00000000,40000000",18, &bytesWritten);
    numread=tty_read(PortFD,str,1,60, &bytesRead);
    if (bytesRead!=1||str[0]!='#')
    {
        if (isDebug())
            IDLog("Timeout waiting for scope to respond to park.");
        return false;
    }

    TrackState=SCOPE_PARKING;
    if(NumPark==0) IDMessage(getDeviceName(),"Parking Mount...");
    StopCount=0;
    return true;
}

bool SynscanMount::UnPark()
{
    SetParked(false);
    NumPark=0;
    return true;
}

void SynscanMount::SetCurrentPark()
{
    IDMessage(getDeviceName(),"Setting Default Park Position");
    IDMessage(getDeviceName(),"Arbitrary park positions not yet supported.");
    SetAxis1Park(0);
    SetAxis2Park(90);
}

void SynscanMount::SetDefaultPark()
{
    // By default az to north, and alt to pole
    IDMessage(getDeviceName(),"Setting Park Data to Default.");
    SetAxis1Park(0);
    SetAxis2Park(90);
}

bool  SynscanMount::Abort()
{
    char str[20];
    int bytesWritten, bytesRead;

    // Hmmm twice only stops it
    tty_write(PortFD,"M",1, &bytesWritten);
    tty_read(PortFD,str,1,1, &bytesRead);  //  Read 1 bytes of response


    tty_write(PortFD,"M",1, &bytesWritten);
    tty_read(PortFD,str,1,1, &bytesRead);  //  Read 1 bytes of response

    return true;
}


bool SynscanMount::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    //fprintf(stderr,"MoveNS %d rate %d dir %d\n",command,SlewRate,dir);
    if(command) {
        //fprintf(stderr,"Stop Motion N/S\n");
        PassthruCommand(37,17,2,0,0);
    } else {
        int tt;
	tt=SlewRate;
        tt=tt<<16;
        if(dir) {
            //fprintf(stderr,"Start Motion South\n");
            PassthruCommand(37,17,2,tt,0);
        } else {
            //fprintf(stderr,"Start Motion North\n");
            PassthruCommand(36,17,2,tt,0);
        }
    }

    return true;
}

bool SynscanMount::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    //fprintf(stderr,"MoveWE %d rate %d dir %d\n",command,SlewRate,dir);

    if(command) {
        //fprintf(stderr,"Stop Motion E/W\n");
        PassthruCommand(37,16,2,0,0);
    } else {
        int tt;
	tt=SlewRate;
        tt=tt<<16;
        if(dir) {
            //fprintf(stderr,"Start Motion East\n");
            PassthruCommand(37,16,2,tt,0);
        } else {
            //fprintf(stderr,"Start Motion West\n");
            PassthruCommand(36,16,2,tt,0);
        }
    }

    return true;
}

bool SynscanMount::SetSlewRate(int s)
{
    SlewRate=s+1;
    //fprintf(stderr,"Slew rate %d\n",SlewRate);
    return true;
}


int SynscanMount::PassthruCommand(int cmd,int target,int msgsize,int data,int numReturn)
{
    char test[20];
    int bytesRead,bytesWritten;
    char a,b,c;
    int tt;

    tt=data;
    a=tt%256;
    tt=tt>>8;
    b=tt%256;
    tt=tt>>8;
    c=tt%256;
    
//  format up a passthru command
    memset(test,0,20);
    test[0]=80;  // passhtru
    test[1]=msgsize;   // set message size
    test[2]=target;    // set the target 
    test[3]=cmd;       // set the command
    test[4]=c;         // set data bytes
    test[5]=b;
    test[6]=a;
    test[7]=numReturn;

    tty_write(PortFD,test,8,&bytesWritten);
    memset(test,0,20);
    //fprintf(stderr,"Start Passthru %d %d %d %d\n",cmd,target,msgsize,data);
    tty_read(PortFD,test,numReturn+1,2,&bytesRead);
    //fprintf(stderr,"Got %d bytes\n",bytesRead);
    //for(int x=0; x<bytesRead; x++) {
    //    fprintf(stderr,"%x ",test[x]);
    //}
    //fprintf(stderr,"\n");
    if(numReturn > 0) {
        int retval=0;
        retval=test[0];
	if(numReturn >1) {
             retval=retval<<8;
	     retval+=test[1];
        }
	if(numReturn >2) {
             retval=retval<<8;
	     retval+=test[2];
        }
        return retval;
    }

    return 0;
}

bool SynscanMount::ReadTime()
{
    char str[20];
    int bytesWritten, bytesRead;
    int numread;

    //  lets see if this hand controller responds to a time request
    bytesRead=0;
    tty_write(PortFD,"h",1,&bytesWritten);
    tty_read(PortFD,str,9,2,&bytesRead);
    //fprintf(stderr,"Read Time returns %d\n",bytesRead);
    if(str[8] == '#') {
	ln_zonedate localTime;
	ln_date utcTime;


        int offset, daylightflag;
        //fprintf(stderr,"Got a good terminator\n");
        localTime.hours=str[0];
        localTime.minutes=str[1];
        localTime.seconds=str[2];
        localTime.months=str[3];
        localTime.days=str[4];
        localTime.years=str[5];
        localTime.gmtoff=str[6];
        offset=str[6];
        daylightflag=str[7];  //  this is the daylight savings flag in the hand controller, needed if we did not set the time
	localTime.years+=2000;
	localTime.gmtoff*=3600;
	//  now convert to utc
	ln_zonedate_to_date(&localTime, &utcTime);

	//  now we have time from the hand controller, we need to set some variables
	int sec;
	char utc[100];
	char ofs[10];
        sec=(int) utcTime.seconds;
	sprintf(utc,"%04d-%02d-%dT%d:%02d:%02d",utcTime.years,utcTime.months,utcTime.days,utcTime.hours,utcTime.minutes,sec);
	if(daylightflag==1) offset=offset+1;
	sprintf(ofs,"%d",offset);

        IUSaveText(&TimeT[0], utc);
        IUSaveText(&TimeT[1], ofs);
        TimeTP.s = IPS_OK;
        IDSetText(&TimeTP, NULL);

        return true;
    }
    return false;
}

bool SynscanMount::ReadLocation()
{
    char str[20];
    int bytesWritten, bytesRead;
    int numread;

    //fprintf(stderr,"Read Location\n");

    tty_write(PortFD,"Ka",2, &bytesWritten);  //  test for an echo
    tty_read(PortFD,str,2,2, &bytesRead);  //  Read 2 bytes of response
    if(str[1] != '#')
    {
        fprintf(stderr,"bad echo in ReadLocation\n");
    } else {

	//  lets see if this hand controller responds to a location request
    	bytesRead=0;
    	tty_write(PortFD,"w",1,&bytesWritten);
    	tty_read(PortFD,str,9,2,&bytesRead);
    	if(str[8] == '#') {
		
	    double lat,lon;
	    //  lets parse this data now
            int a,b,c,d,e,f,g,h;
            a=str[0];
            b=str[1];
            c=str[2];
            d=str[3];
            e=str[4];
            f=str[5];
            g=str[6];
            h=str[7];
            //fprintf(stderr,"Pos %d:%d:%d  %d:%d:%d\n",a,b,c,e,f,g);

            double t1,t2,t3;

            t1=c;
            t2=b;
            t3=a;
            t1=t1/3600.0;
            t2=t2/60.0;
            lat=t1+t2+t3;

            t1=g;
            t2=f;
            t3=e;
            t1=t1/3600.0;
            t2=t2/60.0;
            lon=t1+t2+t3;

            if(d==1) lat=lat*-1;
            if(h==1) lon=360-lon;
            LocationN[LOCATION_LATITUDE].value=lat;
            LocationN[LOCATION_LONGITUDE].value=lon;
            IDSetNumber(&LocationNP, NULL);
            //  We dont need to keep reading this one on every cycle
            //  only need to read it when it's been changed
            ReadLatLong=false;
            return true;
        } else {
           fprintf(stderr,"Mount does not support setting location\n");
        }
    }
    return false;
}

bool SynscanMount::updateTime(ln_date *utc, double utc_offset)
{

    char str[20];
    int bytesWritten, bytesRead;
    int numread;

    //  start by formatting a time for the hand controller
    //  we are going to set controller to local time
    //

    //fprintf(stderr,"Update time\n");
    struct ln_zonedate ltm;
    ln_date_to_zonedate(utc, &ltm, utc_offset*3600.0);

    int yr;
    yr=ltm.years;
    yr=yr%100;

    str[0]='H';
    str[1]=ltm.hours;
    str[2]=ltm.minutes;
    str[3]=ltm.seconds;
    str[4]=ltm.months;
    str[5]=ltm.days;
    str[6]=yr;
    str[7]=utc_offset;  //  offset from utc so hand controller is running in local time
    str[8]=0;  //  and no daylight savings adjustments, it's already included in the offset
    //  lets write a time to the hand controller
    bytesRead=0;
    tty_write(PortFD,str,9,&bytesWritten);
    tty_read(PortFD,str,1,2,&bytesRead);
    if(str[0] != '#' ) {
        fprintf(stderr,"Invalid return from set time\n");
    } else {
        //fprintf(stderr,"Set time returns correctly\n");
    }

    return true;
}

bool SynscanMount::updateLocation(double latitude, double longitude, double elevation)
{ 
    char str[20];
    int bytesWritten, bytesRead;
    int numread;
    int s;
    bool IsWest=false;
    double tmp;

    ln_lnlat_posn p1;
    lnh_lnlat_posn p2;

//  call the alignment subsystem with our location
    UpdateLocation(latitude, longitude, elevation);

    LocationN[LOCATION_LATITUDE].value=latitude;
    LocationN[LOCATION_LONGITUDE].value=longitude;
    IDSetNumber(&LocationNP, NULL);

    if(!CanSetLocation) {

	    return true;

    } else {
    

    DEBUGF(INDI::Logger::DBG_SESSION,"Enter Update Location %4.6lf  %4.6lf\n",latitude,longitude);
    if(longitude > 180) {
        p1.lng=360.0-longitude;
        IsWest=true;
    } else {
        p1.lng=longitude;
    }
    p1.lat=latitude;
    ln_lnlat_to_hlnlat(&p1,&p2);

    str[0]='W';
    str[1]=p2.lat.degrees;
    str[2]=p2.lat.minutes;
    tmp=p2.lat.seconds+0.5;
    s=(int)tmp; //  put in an int that's rounded
    str[3]=s;
    if(p2.lat.neg==0) str[4]=0;
    else str[4]=1;

    str[5]=p2.lng.degrees;
    str[6]=p2.lng.minutes;
    s=(p2.lng.seconds+0.5); //  make an int, that's rounded
    str[7]=s;
    if(IsWest) str[8]=1;
    else str[8]=0;
    //  All formatted, now send to the hand controller;
    bytesRead=0;
    tty_write(PortFD,str,9,&bytesWritten);
    tty_read(PortFD,str,1,2,&bytesRead);
    if(str[0] != '#' ) {
        fprintf(stderr,"Invalid return from set location\n");
    } else {
        //fprintf(stderr,"Set location returns correctly\n");
    }
    //  want to read it on the next cycle, so we update the fields in the client
    ReadLatLong=true;

    return true;
    }
}        


