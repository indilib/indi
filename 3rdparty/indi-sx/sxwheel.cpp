#include "sxwheel.h"

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
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include <string.h>

// We declare an auto pointer to sxwheel.
std::auto_ptr<SxWheel> sxwheel(0);

void ISInit()
{
   static int isInit =0;

   if (isInit == 1)
       return;

    isInit = 1;
    if(sxwheel.get() == 0) sxwheel.reset(new SxWheel());
    //IEAddTimer(POLLMS, ISPoll, NULL);

}

void ISGetProperties(const char *dev)
{
        ISInit();
        sxwheel->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        sxwheel->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        sxwheel->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        sxwheel->ISNewNumber(dev, name, values, names, num);
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



SxWheel::SxWheel()
{
    //ctor
    MinFilter=1;
    MaxFilter=10;
    CurrentFilter=1;
    wheelfd=0;
}

SxWheel::~SxWheel()
{
    //dtor
}

const char * SxWheel::getDefaultName()
{
    return (char *)"SxWheel";
}

bool SxWheel::Connect()
{
	char buf[256];
	int res;
	struct hidraw_report_descriptor rpt_desc;
	struct hidraw_devinfo info;


    //  ok, this _should_ be done by linking to udev libraries
    //  and traversing trees, but, those libraries are often
    //  not available on a small headless embedded system
    //  Soooo, we do a monsterous kludge here and just hunt
    //  thru the entire list hidraw devices in /dev, looking
    //  for the correct vendor name string
    DIR *dip;
    struct dirent *dit;

    IDLog("Checking for SX Filter Wheel\n");

    dip=opendir("/dev");
    if(dip == NULL) return false;

    while((dit=readdir(dip))!= 0) {
        if(strncmp(dit->d_name,"hidraw",6)==0) {
            //  ok, it's a hidraw device
            int fd;
            char nn[512];
            //  we need fully pathed name for this
            sprintf(nn,"/dev/%s",dit->d_name);
            //  open non blocking
            //fprintf(stderr,"Testing %s\n",nn);
            fd=open(nn, O_RDWR|O_NONBLOCK);
            if(fd > 0) {
                memset(&rpt_desc, 0x0, sizeof(rpt_desc));
                memset(&info, 0x0, sizeof(info));
                memset(buf, 0x0, sizeof(buf));

                /* Get Raw Name */
                res = ioctl(fd, HIDIOCGRAWNAME(256), buf);
                if(res > 0) {
                    fprintf(stderr,"Found %s\n",buf);
                    if(strncmp(buf,"SxFilterWh",10)== 0) {
                        //  This is a starlight xpress filter wheel
                        //  so lets run with this one
                        //fprintf(stderr,"We have found the sx filter wheel\n");
                        wheelfd=fd;
                        closedir(dip);
                        QueryFilter();
                        return true;
                    }
                }
            }
        }
    }
    closedir(dip);

    IDLog("Connection to SX Filter Wheel Failed\n");
    return false;
}

bool SxWheel::Disconnect()
{
    if(wheelfd != 0) close(wheelfd);
    wheelfd=0;
    return true;
}

bool SxWheel::initProperties()
{

    //setDeviceName("SX Wheel");
    INDI::FilterWheel::initProperties();
    //  Lets add one property that makes it easy to test things
    //  Label it led on/off
    return true;
}
void SxWheel::ISGetProperties (const char *dev)
{
    IDLog("fcusb::ISGetProperties %s\n",dev);
    //  First we let our parent class do it's thing
    INDI::FilterWheel::ISGetProperties(dev);

    //  Now we add anything that's specific to this filter wheel
    return;
}

int SxWheel::SendWheelMessage(int a,int b)
{
    //  messages consist of two chars
    char buf[3];
    int rc;

    if(wheelfd==0) {
        IDMessage(deviceName(),"Filter wheel not connected\n");
        return -1;
    }
    //  messages to the wheel are 3 bytes
    //  first byte is the hid page
    //  then the two we have built up
    buf[0]=0;
    buf[1]=a;
    buf[2]=b;
    //
    rc=write(wheelfd,buf,3);
    IDLog("Write to wheel returns %d\n",rc);
    if(rc==3) {
        ReadWheelMessage();
    }



    return 0;
}

int SxWheel::ReadWheelMessage()
{
    char buf[3];
    int loop=0;
    int rc=0;

    memset(buf,0,3);
    //  ok, this will often fail because the wheel is not ready yet
    //  and we dont want to use a blocking read, so, we kludge a bit
    while((loop++ < 50)&&(rc != 2)) {
        rc=read(wheelfd,buf,2);
        if(rc != 2) {
            usleep(100);
        }
    }
    IDLog("Read wheel message rc=%d\n",rc);
    if(rc ==2) {
        IDLog("Wheel Message %d %d\n", buf[0],buf[1]);
        CurrentFilter=buf[0];
        MaxFilter=buf[1];
    }

    return 0;
}

int SxWheel::QueryFilter()
{
    SendWheelMessage(0,0);
    return CurrentFilter;
}

int SxWheel::SelectFilter(int f)
{
    TargetFilter=f;
    IDLog("SxWheel Select Filter %d\n",f);
    SendWheelMessage(f,0);
    SetTimer(250);
    return 0;
}

void SxWheel::TimerHit()
{
    QueryFilter();
    if(CurrentFilter != TargetFilter) {
        SetTimer(250);
    } else {
        //  let everybody know that the filter is changed
        SelectFilterDone(CurrentFilter);
    }
}
