#if 0
	Temma INDI driver
	Copyright (C) 2004 Francois Meyer (dulle @ free.fr)
	Remi Petitdemange for the temma protocol
  Reference site is http://dulle.free.fr/alidade/indi.php

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

#include <config.h>

#include <libnova.h>

#include "indicom.h"
#include "indidevapi.h"
#include "eventloop.h"
#include "indicom.h"
#include "temmadriver.h"


	char errormes[][128]={
		"I/O Timeout",
		"Error reading from io port",
		"Error writing to io port",
		"Unrecognized message"
	};

char answer[32];
int fd;
int read_ret, write_ret;
unsigned char buffer[256];
unsigned char buffer2[256];


#if 1
/* Initlization routine */
static void mountInit()
{
	static int inited;		/* set once mountInit is called */

	if (inited)
		return;

	/* setting default comm port */	
	PortT->text = realloc(PortT->text, 10);
	TemmaNoteT[0].text = realloc( TemmaNoteT[0].text, 64);
	TemmaNoteT[1].text = realloc( TemmaNoteT[1].text, 64);
	
	if (!PortT->text || !TemmaNoteT[0].text || !TemmaNoteT[1].text){
		fprintf(stderr,"Memory allocation error");
		return;
	}
	strcpy(PortT->text, "/dev/ttyS0");
	strcpy(TemmaNoteT[0].text, "Experimental Driver");
	strcpy(TemmaNoteT[1].text, "http://dulle.free.fr/alidade/indi.php");

	inited = 1;
}
#endif

void ISGetProperties (const char *dev) {
	if (dev && strcmp (mydev, dev))
		return;

	mountInit();

	IDDefSwitch (&powSw, NULL);
	IDDefNumber (&eqTemma, NULL);
	IDDefNumber (&eqNum, NULL);
	IDDefSwitch (&OnCoordSetSw, NULL);
	IDDefSwitch (&abortSlewSw, NULL);
	IDDefText   (&TemmaNoteTP, NULL);
	IDDefSwitch (&RAmotorSw, NULL);
	IDDefSwitch (&trackmodeSw, NULL);
	IDDefText (&Port, NULL);
	IDDefText (&TemmaVersion, NULL);
	IDDefNumber (&Time, NULL);
	IDDefNumber (&SDTime, NULL);
	IDDefNumber (&cometNum, NULL);
	IDDefNumber (&geoNum, NULL);
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

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) 
{

	INDI_UNUSED(names);
	INDI_UNUSED(n);

	if (strcmp (dev, mydev))
		return;

	if (!strcmp (name, Port.name)) {
		IUSaveText(PortT, texts[0]);
		Port.s = IPS_OK;
		IDSetText (&Port, NULL);
	}
	return;
}

/* client is sending us a new value for a Numeric vector property */
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n) {
	/* ignore if not ours */
	if (strcmp (dev, mydev))
		return;

	if (!strcmp (name, eqNum.name)) {
		/* new equatorial target coords */
	        /*double newra = 0, newdec = 0;*/
		int i, nset;

		/* Check power, if it is off, then return */
		if (power[0].s != ISS_ON)
		{
			eqNum.s = IPS_IDLE;
			IDSetNumber(&eqNum, "Power is off");
			return;
		}

		for (nset = i = 0; i < n; i++) {
			/* Find numbers with the passed names in the eqNum property */
			INumber *eqp = IUFindNumber (&eqNum, names[i]);

			/* If the number found is Right ascension (eq[0]) then process it */
			if (eqp == &eq[0])
			{
				currentRA = (values[i]);
				nset += currentRA >= 0 && currentRA <= 24;
			}
			/* Otherwise, if the number found is Declination (eq[1]) then process it */ 
			else if (eqp == &eq[1]) {
				currentDec = (values[i]);
				nset += currentDec >= -90 && currentDec <= 90;
			}
		} /* end for */

		/* Did we process the two numbers? */
		if (nset == 2) {
		        /*char r[32], d[32];*/

			/* Set the mount state to BUSY */
			eqNum.s = IPS_BUSY;

			if (SLEWSW==ISS_ON || TRACKSW==ISS_ON){
				IDSetNumber(&eqNum, "Moving to RA Dec %f %f", currentRA,
						currentDec);
				do_TemmaGOTO();
			}

			if (SYNCSW==ISS_ON){
				IDSetNumber(&eqNum, "Syncing to RA Dec %f %f", currentRA, currentDec);
				set_TemmaCurrentpos();
			}
			eqNum.s = IPS_OK;
			IDSetNumber(&eqNum, "Synced");
		}
		/* We didn't process the two number correctly, report an error */
		else {
			/* Set property state to idle */
			eqNum.s = IPS_IDLE;

			IDSetNumber(&eqNum, "RA or Dec absent or bogus.");
		}
	return;
	}
}

/* client is sending us a new value for a Switch property */
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
	ISwitch *sp;

	/* ignore if not ours */
	if (strcmp (dev, mydev))
		return;

	if (!strcmp (name, powSw.name)) {
		sp = IUFindSwitch (&powSw, names[0]);
		if (!sp)
			return;

		fprintf(stderr,"new state %s\n",names[0]);

		sp->s = states[0];
		if (!strcmp(names[0],"CONNECT")) {
			connectMount();
		}		
		if (!strcmp(names[0],"DISCONNECT")) {
			disconnectMount();
		}		
	}

	if (!strcmp (name, abortSlewSw.name)) {
		if (POWSW){
			TemmaabortSlew(); 
			IDSetSwitch (&abortSlewSw, "Abort slew");
		}
		else {
			IDSetSwitch (&abortSlewSw, "Power is off");
		}
	}

	if (!strcmp (name, RAmotorSw.name)) {
		if (POWSW){
			sp = IUFindSwitch (&RAmotorSw, names[0]);
			if (!sp)
				return;

			fprintf(stderr,"new state %s\n",names[0]);

			sp->s = states[0];

			RAmotorSw.s = IPS_BUSY;
			if (!strcmp(names[0],"RUN")) {
				set_TemmaStandbyState(0);
			}

			if (!strcmp(names[0],"STOP")) {
				set_TemmaStandbyState(1);
			}	

			if(get_TemmaStandbyState(buffer)){
				RAmotorSw.s = IPS_IDLE;
				IDSetSwitch (&RAmotorSw, "Error writing to port");
				return;
			}
		}
		else {
			IDSetSwitch (&RAmotorSw, "Power is off");
		}
	}

	if (!strcmp (name, OnCoordSetSw.name)) {
		if (POWSW){
			IUResetSwitch(&OnCoordSetSw);
			sp = IUFindSwitch (&OnCoordSetSw, names[0]);
			if (!sp)
				return;

			fprintf(stderr,"new state %s\n",names[0]);

			sp->s = states[0];

			IUResetSwitch(&OnCoordSetSw);
			IUUpdateSwitch(&OnCoordSetSw, states, names, n);
			/*			    currentSet = getOnSwitch(&OnCoordSetSw); */
			OnCoordSetSw.s = IPS_OK;
			IDSetSwitch(&OnCoordSetSw, NULL);
		}
		else {
			IDSetSwitch (&OnCoordSetSw, "Power is off");
		}
	}
}

double calcLST(char *strlst){
	double jd,gmst,lst;

	jd = ln_get_julian_from_sys();
	gmst = ln_get_mean_sidereal_time(jd);

	lst=(gmst-longitude)/15;	

	lst=(lst/24-(int)(lst/24))*24;

	if(lst>=24) 
		lst-=24;
	sprintf(strlst,"%.2d%.2d%.2d", (int)lst,
			((int)(lst*60))%60,
			((int)(lst*3600))%60);
	currentLST=lst;
	IDSetNumber (&SDTime, NULL);
	IDSetNumber (&Time, NULL);
	
	return 0;
}

#if 1
/* update the mount over time */
void readMountcurrentpos (void *p)
{
	INDI_UNUSED(p);

	char result[32];

	if(POWSW){
		get_TemmaCurrentpos(result);
		calcLST(result);

		/* This is a temporary workaround to allow
		   clients with only one eq property to work */
		currentRA=temmaRA;
		currentDec=temmaDec;

		/* again */
		IEAddTimer (POLLMS, readMountcurrentpos, NULL);
	}
}
#endif

static void connectMount () {
	fprintf(stderr,"opening mount port %s\n",PortT->text);
	if (power[0].s == ISS_ON){
		if (Port.s != IPS_OK){
			powSw.s = IPS_IDLE;
			power[0].s = ISS_OFF;

			IDSetSwitch (&powSw, "Port not set.");
			return;
		}
		else {
			if (TemmaConnect(PortT->text)==0){
				IDSetText (&Port, "Port is opened.");
				if( !get_TemmaVERSION(buffer)){
					powSw.s = IPS_OK;
					power[0].s = ISS_ON;
					power[1].s = ISS_OFF;
					snprintf(buffer2,sizeof( buffer2 ),"%s",buffer+4);

					IDSetText(&TemmaVersion , "Temma version set");
					TemmaVersionT->text = realloc(TemmaVersionT->text, strlen(buffer2)+1);
					if (!TemmaVersionT->text){
						fprintf(stderr,"Memory allocation error");
						return;
					}
					IDSetSwitch (&powSw, "Mount is ready");
					IDSetSwitch (&powSw, VERSION);

					strcpy(TemmaVersionT->text, buffer2);
					TemmaVersion.s = IPS_OK; 
					IDSetText (&TemmaVersion, NULL);
					IDLog("%s", buffer2);
					/* start timer to read mount coords */
					IEAddTimer (POLLMS, readMountcurrentpos, NULL);
				}
				else {
					powSw.s = IPS_IDLE;
					power[0].s = ISS_OFF;
					power[1].s = ISS_ON;
					IDSetText(&Port , "Com error");
					IDSetSwitch (&powSw, "Port not set.");
				}

				if(get_TemmaStandbyState(answer)){
					IDSetSwitch (&RAmotorSw, "Error writing to port");
					return;
				}
			}
			else {
				powSw.s = IPS_IDLE;
				power[0].s = ISS_OFF;
				power[1].s = ISS_ON;

				IDSetSwitch (&powSw, "Failed to open port.");
			}
		}
	}
}

static void disconnectMount () {
	fprintf(stderr,"closing mount port %s\n",PortT->text);
	if (power[1].s == ISS_ON){
		if (Port.s != IPS_OK){
			powSw.s = IPS_IDLE;
			power[0].s = ISS_OFF;

			IDSetSwitch (&powSw, "Port not set.");
			return;
		}
		else {
			TemmaDisconnect();
			powSw.s = IPS_IDLE;
			power[0].s = ISS_OFF;

			IDSetSwitch (&powSw, "Port is closed.");
		}
	}
	else {
		fprintf(stderr, "Already disconnected \n");
	}
}

int TemmaConnect(const char *device) {
	fprintf(stderr, "Connecting to device %s\n", device);

	if (openPort(device) < 0){
		fprintf(stderr, "Error connecting to device %s\n", device);
		return -1;
	}
	else{
		return 0;
	}
}

int TemmaDisconnect() {
	fprintf(stderr, "Disconnected.\n");
	close(fd);
	
	return 0;
}

int set_CometTracking(int RArate, int DECrate){
#if 0
	Set Comet Tracking
	LM+/-99999,+/-9999
	RA : Adjust Sidereal time by seconds per Day	

	DEC : Adjust DEC tracking by Minutes Per Day			
	valeur DEC min=-600 /max=+600 (+/-10 deg / jour)

	Example:
	LM+120,+30 would slow the RA speed by 86164/86284 and 		
	the Dec would track at 30 Minutes a day.
	To stop tracking either send a LM0,0 (or a PS 
	sauf erreur on constate en faite l inverse en RA
	retour Vsideral => LM0,0 ou LM+0,+0
#endif
	char local_buffer[16];

	if (RArate<-21541){
	RArate=-21541;
	}

	if (RArate>21541){
	RArate=21541;
	}

	if (DECrate<-600){
	DECrate=-600;
	}

	if (DECrate>600){
	DECrate=600;
	}

	snprintf(local_buffer, sizeof( local_buffer ), "%+6d,%+5d", RArate, DECrate);
	set_TemmaCometTracking(local_buffer);
	
	return 0;
}

int TemmaabortSlew() {
	if (portWrite("PS") < 0)
		return -1;

	return 0;
}

int do_TemmaGOTO() {
	/* Temma Sync */
	char command[16];
	char sign;
	double dec;

	calcLST(buffer);
	set_TemmaLST(buffer);

	dec=fabs(currentDec);
	if (currentDec>0){
		sign='+';
	}
	else {
		sign='-';
	}

	snprintf(buffer, sizeof(buffer),"%.2d%.2d%.2d%c%.2d%.2d%.1d",
			(int)currentRA,(int)(currentRA*(double)60)%60,((int)(currentRA*(double)6000))%100,sign,
			(int)dec,(int)(dec*(double)60)%60,((int)(dec*(double)600))%10);
	fprintf(stderr,"Goto %s\n", buffer);

	snprintf(command,14,"P%s",buffer);
	buffer[14]=0;
	fprintf(stderr,"Goto command:%s\n", command);
	portWrite(command);

	portRead(buffer,-1,TEMMA_TIMEOUT);
	if(command[0]=='R'){
		return 0;
	}
	else
		return -1;
}

int extractRA(char *buf){
	int r,h,m,s;
	/*double dra;*/

	r=atoi(buf);
	h=r/10000;
	m=r/100-100*h;
	s=(r%100)*.6;
	temmaRA=((double)h+((double)m + (double)s/60)/60);
	IDSetNumber (&eqTemma, NULL);
	/*	fprintf(stderr,"extractRA: %s %d %d %d %d %lf\n",buf,r,h,m,s,dra);*/
	return 0;
}

int extractDEC(char *buf){
	int dec,d,m,s;
	/*double ddec;*/

	dec=atoi(buf+1);
	d=dec/1000;
	m=dec/10-100*d;
	s=(dec%10)*6;
	temmaDec=((double)d+((double)m + (double)s/60)/60);
	if (*buf=='-')	
		temmaDec*=-1;

	IDSetNumber (&eqTemma, NULL);


	/*	fprintf(stderr,"extractDEC: %s %d %d %d %d %lf\n",buf,dec,d,m,s,ddec);*/
	return 0;
}


int get_TemmaCurrentpos(char *local_buffer){
	char buf[16];

	if (portWrite("E") < 0)
		return -1;

	if(portRead(local_buffer,-1,TEMMA_TIMEOUT)==SUCCESS){
		if(strstr(local_buffer, "E")==local_buffer){
			strncpy(buf,local_buffer+1,6);
			buf[6]=0;
			extractRA(buf);

			strncpy(buf,local_buffer+7,6);
			buf[6]=0;
			extractDEC(buf);
			return 0;
		}
		else {
			return -1;
		}
	}
	
	return 0;
}

int set_TemmaCurrentpos(void) {
	/* Temma Sync */
	char buf[16], sign;
	double dec;

	calcLST(buf);
	set_TemmaLST(buf);
	portWrite("Z");
	calcLST(buf);
	set_TemmaLST(buf);

	dec=fabs(currentDec);
	if (currentDec>0){
		sign='+';
	}
	else {
		sign='-';
	}

	sprintf(buffer,"%.2d%.2d%.2d%c%.2d%.2d%.1d",
			(int)currentRA,(int)(currentRA*(double)60)%60,((int)(currentRA*(double)6000))%100,sign,
			(int)dec,(int)(dec*(double)60)%60,((int)(dec*(double)600))%10);
	fprintf(stderr,"sync to %s %f %f\n", buffer,currentRA,dec);

	snprintf(buf, sizeof(buf), "D%s",buffer);
	buf[13]=0;
	portWrite(buf);
	*buffer=0;

	portRead(buffer,-1,TEMMA_TIMEOUT);
	if(buffer[0]=='R'){
		return 0;
	}
	else{
		return -1;
	}
}

int do_TemmaSLEW(char mode){
  	/*char command[16];*/

	sprintf(buffer,"M%c",mode); /* see bit definition in Temmadriver.h */

	if (portWrite(buffer) < 0)
		return -1;
	
	return 0;
}

int get_TemmaVERSION(char *local_buffer){
	int err;

	if ((err=portWrite("v")) < 0){
		return err;
	}

	portRead(local_buffer,-1,TEMMA_TIMEOUT);
	if(strstr(local_buffer, "ver")==local_buffer){
		return 0;
	}
	else
		return EREAD;
}

int get_TemmaGOTOstatus(char *local_buffer){
	/* 
	   0 no ongoing goto
	   1 ongoing Goto
	   -1 error
	 */
	if (portWrite("s") < 0)
		return -1;

	portRead(local_buffer,-1,TEMMA_TIMEOUT);
	if(strstr(local_buffer, "s")==local_buffer){
		return 0;
	}
	else
		return -1;
}

int get_TemmaBOTHcorrspeed(char *local_buffer){
	if (portWrite("lg") < 0)
		return -1;
	portRead(local_buffer,-1,TEMMA_TIMEOUT);
	if(strstr(local_buffer, "lg")==local_buffer){
		return 0;
	}
	else
		return -1;
}

int get_TemmaDECcorrspeed(char *local_buffer){
	if (portWrite("lb") < 0)
		return -1;
	portRead(local_buffer,-1,TEMMA_TIMEOUT);
	if(strstr(local_buffer, "lb")==local_buffer){
		return 0;
	}
	else
		return -1;
}

int set_TemmaDECcorrspeed(char *local_buffer){
	char command[16];

	snprintf(command, 4, "LB%s",local_buffer);

	if (portWrite(command) < 0)
		return -1;
	return 0;
}

int get_TemmaRAcorrspeed(char *local_buffer){
	if (portWrite("la") < 0)
		return -1;

	portRead(local_buffer,-1,TEMMA_TIMEOUT);
	if(strstr(local_buffer, "la")==local_buffer){
		return 0;
	}
	else
		return -1;
}

int set_TemmaRAcorrspeed(char *local_buffer){
	char command[16];

	snprintf(command, 4,"LA%s",local_buffer);

	if (portWrite(command) < 0)
		return -1;
	return 0;
}

int get_TemmaLatitude(char *local_buffer){
	if (portWrite("i") < 0)
		return -1;
	portRead(local_buffer,-1,TEMMA_TIMEOUT);
	if(local_buffer[0]=='i'){
		return 0;
	}
	else 
		return -1;
}

int set_TemmaLatitude(char *local_buffer){
	char command[16];
	double lat;
	char sign;

	lat=fabs(latitude);
	if (latitude>0){
		sign='+';
	}
	else {
		sign='-';
	}

	sprintf(command,"I%c%.2d%.2d%.1d", sign,
			(int)lat,
			(int)(lat*(double)60)%60,
			((int)(lat*(double)600))%10);

	if (portWrite(command) < 0)
		return -1;
	return 0;
}

int get_TemmaLST(char *local_buffer){
	if (portWrite("g") < 0)
		return -1;

	portRead(local_buffer,-1,TEMMA_TIMEOUT);
	if(local_buffer[0]=='g'){
		return 0;
	}
	else 
		return -1;
}


int set_TemmaLST(char *local_buffer){
	char command[16];
	snprintf(command,7,"T%s",local_buffer);

	if (portWrite(command) < 0)
		return -1;
	return 0;
}


int get_TemmaCometTracking(char *local_buffer){
	if (portWrite("lm") < 0)
		return -1;
	portRead(local_buffer,-1,TEMMA_TIMEOUT);
	if(strstr(local_buffer, "lm")==local_buffer){
		return 0;
	}
	else 
		return -1;
}

int set_TemmaStandbyState(int on){
	if (on){
		return portWrite("STN-ON");
	}
	else{
		return portWrite("STN-OFF");
	}
	return 0;
}

int get_TemmaStandbyState(unsigned char *local_buffer){
	int nb;
	int status;

	if ((nb=portWrite("STN-COD")) < 0){
		IDSetSwitch (&RAmotorSw, "I/O error when asking RAmotor status");
		return -1;
	}

	if((status=portRead(local_buffer,-1,TEMMA_TIMEOUT)==SUCCESS)){
		if(strstr(local_buffer, "stn")==local_buffer){
			local_buffer[7]=0;
			if (strstr(local_buffer,"on")){ /* stanby on */
				RAmotorSw.s = IPS_OK;
				RAmotor[0].s = ISS_OFF;
				RAmotor[1].s = ISS_ON;
				IDSetSwitch (&RAmotorSw, "RA motor is off.");
			}
			else{
				if (strstr(local_buffer,"off")){ /* stanby off */ 
					RAmotorSw.s = IPS_OK;
					RAmotor[0].s = ISS_ON;
					RAmotor[1].s = ISS_OFF;
					IDSetSwitch (&RAmotorSw, "RA motor is on.");
				}
				else {
					RAmotorSw.s = IPS_OK;
					IDSetSwitch (&RAmotorSw, "I/O error when getting RAmotor status");
				}
			}
		}
		return 0;
	}
	if (status<=ETIMEOUT && status >=ECOMMAND){
		IDSetSwitch(&RAmotorSw, "%s", errormes[ETIMEOUT - status]);
	}
	
	return -1;
}

int set_TemmaCometTracking(char *local_buffer){
	char command[16];

	snprintf(command,15,"LM%s",local_buffer);
	if (portWrite(command) < 0)
		return -1;
	return 0;
}

int set_TemmaSolarRate(void){
	if (portWrite("LK") < 0)
		return -1;
	return 0;
}

int set_TemmaStellarRate(void){
	if (portWrite("LL") < 0)
		return -1;
	return 0;
}


int switch_Temmamountside(void){
	if (portWrite("PT") < 0)
		return -1;
	return 0;
}

/**********************************************************************
 * Comm
 **********************************************************************/

int openPort(const char *portID) {
	struct termios ttyOptions;

	if ( (fd = open(portID, O_RDWR)) == -1)
		return -1;
	memset(&ttyOptions, 0, sizeof(ttyOptions));
	tcgetattr(fd, &ttyOptions);

	/* 8 bit, enable read */
	ttyOptions.c_cflag |= CS8;
	/* parity */
	ttyOptions.c_cflag |= PARENB;
	ttyOptions.c_cflag &= ~PARODD;
	ttyOptions.c_cflag &= ~CSTOPB;
	ttyOptions.c_cflag |= CRTSCTS;

	/* set baud rate */
	cfsetispeed(&ttyOptions, B19200);
	cfsetospeed(&ttyOptions, B19200);

	/* set input/output flags */
	ttyOptions.c_iflag = IGNBRK;

	/* Read at least one byte */
	ttyOptions.c_cc[VMIN] = 1;
	ttyOptions.c_cc[VTIME] = 5;

	/* Misc. */
	ttyOptions.c_lflag = 0;
	ttyOptions.c_oflag = 0;

	/* set attributes */
	tcsetattr(fd, TCSANOW, &ttyOptions);

	/* flush the channel */
	tcflush(fd, TCIOFLUSH);
	return (fd);
}

int portWrite(char * buf) {
  int nbytes=strlen(buf); /*, totalBytesWritten;*/
	int bytesWritten = 0; 
	/*int retry=10;*/

	bytesWritten = write(fd, buf, nbytes);
	bytesWritten += write(fd, "\r\n", 2);
	/*fprintf(stderr,"portwrite :%d octets %s\n", bytesWritten, buf);*/

	if (bytesWritten!=nbytes+2){
		perror("write error: ");
		IDLog("Error writing to port");
		return EWRITE;
	}
	return (bytesWritten);
}

int portRead(char *buf, int nbytes, int timeout) {
	/*
	   A very basic finite state machine monitors
	   the bytes read ; 
	   state 0 : read regular bytes
	   state 1 : just read a \n, waiting for a \r
	   state 2 : read a \n and a \r, command is over.

	   Not sure it is useful here but I use a more
	   sophisticated version of this with a GPS receiver
	   and it is robust and reliable

	   We return a null terminated string.
	 */

  int bytesRead = 0, state=0, /*i=0,*/ current=0;
  /*int totalBytesRead = 0;*/
	int err;

	if ( (err = TemmareadOut(timeout)) ){
		switch (err){
			case ETIMEOUT:
				IDLog("Error: timeout while reading");
				return err;
		}
	}

	while (read(fd,buf+bytesRead,1)==1){
		/*		fprintf(stderr,"%c",buf[bytesRead]); */
		fflush(NULL);
		switch (state) {
			case 0:
				if(buf[bytesRead]==13)
					state=1;
				break;

			case 1:
				if(buf[bytesRead]==10)
					state=2;
				else
					if(buf[bytesRead]==13)
						state=1;
					else
						state=0;
				break;
		}

		++current;

		if (state==2){
			/*process(buf);*/
			buf[bytesRead+1]=0;
			state=current=0;
			return SUCCESS;
		}

		bytesRead=current;
	}
	return state;
}

int TemmareadOut(int timeout) {
	struct timeval tv;
	fd_set readout;
	int retval;

	FD_ZERO(&readout);
	FD_SET(fd, &readout);

	/* wait for 'timeout' seconds */
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	/* Wait till we have a change in the fd status */
	retval = select (fd+1, &readout, NULL, NULL, &tv);

	/* Return 0 on successful fd change */
	if (retval > 0)
		return 0;
	/* Return -1 due to an error */
	else if (retval == EREAD)
		return retval;
	/* Return -2 if time expires before anything interesting happens */
	else {
		return ETIMEOUT;
	}
}

