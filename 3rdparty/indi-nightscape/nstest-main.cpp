/* serial_read.c

   Read data via serial I/O

   This program is distributed under the GPL, version 2
*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <libftdi1/ftdi.h>
//#include <ftdi.h>
#include <ctype.h>

#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <math.h>
#include "kaf_constants.h"
#include "nsmsg.h"
#include "nsdownload.h"
#include "nsdebug.h"
#include "nschannel-u.h"
#ifdef HAVE_D2XX
#include "nschannel-ftd.h"
#endif
#include <stdarg.h>

static volatile int interrupted = 0;
//static volatile int readdone = 0;


void siginthandler(int s) {
	interrupted = 1;
}


void IDLog (const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
  va_end(args);
}

long long millis()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	long long t2 = (long long)now.tv_usec +  (long long)(now.tv_sec*1000000);	
	return t2 / 1000;
}



void usage(char * prog)
{
		fprintf(stderr, "usage: %s [-c camera] [-f fanspeed=1-3] [-n num exp] [-t temp(c)] [ -d tdiff(c)] [-e exposure(s)] [-b binning=1|2] [-z start,lines] increment [-i] dark [-k]\n", prog);
		exit(-1);	
}


int main(int argc, char **argv)
{
    int ftd = 1;
    //char eeprom [128];
    int  i;
    //int baudrate = 115200;
    //int interface = INTERFACE_ANY;
    int fanspeed = 1; 
    int deffanspeed = 1; 
    int curfanspeed = -1; 
    float temp = -7.00;
    float tempdif = 0.5;
    float  expdur = 1.00;
    long long start, last, now, lastfan, lasttemp;
    int camnum = 0;
    int binning = 1;
    //long imgszmax = 3448*2574*2 + DEFAULT_CHUNK_SIZE;
    int zonestart = 0;
    int zoneend =0;
    int lcount = 0;
    int scount = 0;
    int in_exp = 0;
    int old_busy_flag =0;
    int download = 0;
    //int imgseq = 1;
    int done_first = 0;
   	int nexp = 0;
    //unsigned char buf[chunksize];
		int threaded = 1;
    int increment = 0;
		char fbase [64];
		int laststat = 0;
		bool dark = false;
    //char fbase[64] = "";

    //bigbuf = malloc(3358*2536*2);
    signal(SIGINT, siginthandler);
    while ((i = getopt(argc, argv, "t:f:c:n:e:b:z:d:o:ik")) != -1)
    {
        switch (i)
        {
					case 't': // 0=ANY, 1=A, 2=B, 3=C, 4=D
						temp = strtof(optarg, NULL);
						break;
					case 'f':
						deffanspeed = strtoul(optarg, NULL,0);
						break;
					case 'c':
						camnum = strtoul(optarg, NULL, 0);
						break;
					case 'n':
						nexp = strtoul(optarg, NULL, 0);
						break;
					case 'e':
						expdur = strtof(optarg, NULL);
						break;
					case 'b':
						binning = strtoul(optarg, NULL, 0);
						break;
					case 'z':
						if (strstr(optarg, ",") == NULL) usage(argv[0]);
						zonestart = strtoul(strtok(optarg, ","), NULL, 0);
						zoneend = strtoul(strtok(NULL, ","), NULL, 0);
						break;
					case 'd':
						tempdif = strtof(optarg, NULL);
						break;
					case 'i':
			  		increment = 1;
			  		break;
				  case 'o':
				  	strncpy(fbase, optarg, 64);
				  	break;
				  case 'k':
				  	dark = true;
				  	break;
					default:
						usage(argv[0]);
						break;
        }
    }
    
   	NsChannel * cn;
#ifdef HAVE_D2XX
   	if (ftd) {
	   cn = new NsChannelFTD(camnum);
	  } else {
	  	cn = new NsChannelU(camnum);
	  } 
#else 
		ftd = 0;
	  cn = new NsChannelU(camnum);
#endif
		 if (cn->open() < 0) exit (-1);

		 Nsmsg * m = new Nsmsg(cn);
	     
   	 NsDownload * d = new NsDownload(cn);

    d->setFrameXBinning(binning);
    d->setFrameYBinning(binning);

 		d->setSetTemp(temp);

    d->setImgSize(m->getRawImgSize(zonestart,zoneend,binning));

		d->setExpDur(expdur);
		d->setIncrement(increment);
		d->setFbase(fbase);
		d->setNumExp(nexp);
		d->setImgWrite(true);

 		if(!m->inquiry()) exit(-1);
 
    start = millis();
    last = start;
    now = start;
    lastfan = start;
    lasttemp = start;
    long long sdiff, tdiff, fandiff;
    if (threaded) {
    	d->startThread();
   	} 

    while (!interrupted) {
			int busy_flag =0;
			if (!download) { 
				now = millis();		      
				sdiff = now - last;
				fandiff = now  - lastfan;
				tdiff = now - lasttemp;
			}
			if ((sdiff > 50) && !download) {
				busy_flag = m->rcvstat();
				if (old_busy_flag != busy_flag) {
					fprintf(stderr,"status change %d millis %lld\n", busy_flag, now - laststat);
					laststat = now;
				}
				if (in_exp && old_busy_flag == 2 && busy_flag == 0) {
					download = 1;
					if(threaded) d->doDownload();	

					fprintf(stderr,"downloading image..\n");
					if (!threaded) {
						d->nextImage();
						//d->initdownload();
					} 
				}
				old_busy_flag = busy_flag;
				scount++;
				last = now;
				sdiff = 0;
      }
			if (download) {
				if (!threaded) {
					download = d->downloader();
				}
				else download = d->inDownload();
				if (!download) {
					in_exp = 0;
					//
					if (!threaded) {
						d->writedownload(0,1);
						d->freeBuf();
					}
					if (d->getImgSeq() > nexp) interrupted = 1;
				}
  		}

			if (download) { 
				if (threaded) sleep(1);
			} else {
				usleep(2000);
			}
			lcount++;
      if (!interrupted && scount  == 1 && !done_first) {
					fprintf(stderr,"settemp %f\n", temp);
					m->sendtemp(temp, 1);
      } 
      if (!interrupted && ((scount == 1 && !done_first) || (tdiff > (10 * 1000))) &&  !in_exp) {
				done_first = 1;
				
				float acttemp = m->rcvtemp();
				d->setActTemp(acttemp);
				fprintf(stderr, "setpoint %f temp %f %d\n", temp,acttemp, m->getResp4());
				if  (fabs(acttemp - temp) < tempdif) {
 					fanspeed = 1;
				} else {
					fanspeed = deffanspeed;
				}
				if (fanspeed != curfanspeed) {
					fprintf(stderr,"fan speed %d\n", fanspeed);
					lastfan = now;
					fandiff = 0;
					m->sendfan(fanspeed);
					curfanspeed = fanspeed;
  			}
				lasttemp = now;
				tdiff = 0;
			}
			if (!interrupted && fandiff > (10 * 1000) &&!in_exp && fanspeed == 1) {
				fprintf(stderr,"now do exp\n");
				if (!threaded) {
					d->initdownload();
					d->purgedownload();
				}
				m->sendzone(zonestart,zoneend,binning);
				m->senddur(expdur, binning, dark);
				in_exp = 1;
				//if(threaded) d->doDownload();	
			}
    }
    d->setInterrupted();
    fprintf(stderr,"exiting\n");
		m->abort();
		m->sendfan(deffanspeed);
    cn->close();
}
