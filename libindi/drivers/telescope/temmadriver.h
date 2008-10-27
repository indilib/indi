/*
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
 
 */

#ifndef TEMMADRIVER_H
#define TEMMADRIVER_H
#define VERSION "Temma indi driver Ver 0.0, fm-2004/10/09" 
#if 0
bit definition for M message : slew speed and parameters
#endif
#define HS 0x01   /* high speed */
#define RR 0x02		/* RA right */
#define RL 0x04		/* RA left */
#define DU 0x08		/* DEC up */
#define DD 0x10   /* DEC down */
#define EN 0x20		/* ENC on */
#define BB 0x40		/* Always set */
#if 0
(HS|RR|EN|BB)
#endif
#define HRAD	(M_PI/12)
#define DEGRAD	(M_PI/180)
#define TEMMA_TIMEOUT	1		/* FD timeout in seconds */

	/* error codes */
#define	SUCCESS (2) /* return value for successful read */
#define ETIMEOUT (-2) 	/* timeout */
#define EREAD (-3) 			 /* error reading from port */	
#define EWRITE (-4) 		 /* error writing to port */
#define ECOMMAND (-5) /* unexpected answer */
	/* Definitions */

#define	mydev	 "Temma"	/* Device name */
#define MAIN_GROUP "Main Control"	/* Group name */
#define COMM_GROUP "Communication"	/* Group name */
#define MOVE_GROUP	"Movement Control"
#define DATETIME_GROUP	"Date/Time/Location"

#define SLEWSW	OnCoordSetS[0].s
#define SYNCSW	OnCoordSetS[1].s
#define TRACKSW	OnCoordSetS[2].s
#define POWSW 	(power[0].s==ISS_ON)

#define	SLEWRATE 1			/* slew rate, degrees/s */
#define	POLLMS	 1000		/* poll period, ms */

#define 	latitude	geo[0].value	/* scope's current rads. */
#define 	longitude	geo[1].value	/* scope's current rads. */
#define 	currentUTC	UTC[0].value	/* scope's current rads. */
#define 	currentLST	STime[0].value	/* scope's current rads. */
#define 	currentRA	eq[0].value	/* scope's current RA, rads. */
#define 	currentDec	eq[1].value	/* scope's current Dec, rads. */
#define 	temmaRA	eqtem[0].value	/* scope's current RA, rads. */
#define 	temmaDec	eqtem[1].value	/* scope's current Dec, rads. */


int openPort(const char *portID);
int portRead(char *buf, int nbytes, int timeout);
int portWrite(char * buf);
int TemmareadOut(int timeout);
static void mountInit(void);
void ISGetProperties (const char *dev);
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
double calcLST(char *strlst);
int TemmaConnect(const char *device) ;
int TemmaDisconnect(void) ;
int  set_CometTracking(int RArate, int DECrate);
int TemmaabortSlew(void) ;
int do_TemmaGOTO(void) ;
int extractRA(char *buf);
int extractDEC(char *buf);
int get_TemmaCurrentpos(char *buffer);
int set_TemmaCurrentpos(void) ;
int do_TemmaSLEW(char mode);
int get_TemmaVERSION(char *buffer);
int get_TemmaGOTOstatus(char *buffer);
int get_TemmaBOTHcorrspeed(char *buffer);
int get_TemmaDECcorrspeed(char *buffer);
int set_TemmaDECcorrspeed(char *buffer);
int get_TemmaRAcorrspeed(char *buffer);
int set_TemmaRAcorrspeed(char *buffer);
int get_TemmaLatitude(char *buffer);
int set_TemmaLatitude(char *buffer);
int get_TemmaLST(char *buffer);
int set_TemmaLST(char *buffer);
int  set_TemmaStandbyState(int on);
int  get_TemmaStandbyState(unsigned char *buffer);
int  get_TemmaCometTracking(char *buffer);
int  set_TemmaCometTracking(char *buffer);
int  set_TemmaSolarRate(void);
int  set_TemmaStellarRate(void);
int  switch_Temmamountside(void);

static void disconnectMount (void);
static void connectMount (void);
static void readMountcurrentpos (void *);

/***************************/
/* RA motor control switch */
/***************************/
static ISwitch RAmotor[] = {
	{"RUN", "On", ISS_OFF, 0, 0}, 
	{"STOP", "Off", ISS_ON, 0, 0} };

static ISwitchVectorProperty RAmotorSw = { 
	mydev, "RA motor", "RA motor", MOVE_GROUP, IP_RW, ISR_ATMOST1, 
	0, IPS_IDLE, RAmotor, NARRAY(RAmotor), "", 0};

/*****************************/
/* Track mode control switch */
/*****************************/
static ISwitch trackmode[] = {
	{"Sidereal", "Sidereal", ISS_ON, 0, 0}, 
	{"Lunar", "Lunar", ISS_OFF, 0, 0}, 
	{"Comet", "Comet", ISS_OFF, 0, 0}, };

static ISwitchVectorProperty trackmodeSw = { 
	mydev, "Tracking mode", "Tracking mode", MOVE_GROUP, IP_RW, ISR_1OFMANY, 
	0, IPS_IDLE, trackmode, NARRAY(trackmode), "", 0};

/*****************************/
/* Power control switch 		 */
/*****************************/
static ISwitch power[] = {
	{"CONNECT", "On", ISS_OFF, 0, 0}, {"DISCONNECT", "Off", ISS_ON, 0, 0}, };

static ISwitchVectorProperty powSw = { 
	mydev, "CONNECTION", "Connection", MAIN_GROUP, IP_RW, ISR_ATMOST1, 
	0, IPS_IDLE, power, NARRAY(power), "", 0};

/*******************************/
/* Temma version text property */
/*******************************/
static IText TemmaVersionT[]			= {{"VERSION", "Version", 0, 0, 0, 0}};
static ITextVectorProperty TemmaVersion		= { 
	mydev, "VERSION", "Temma version", COMM_GROUP, IP_RO, 0,
	IPS_OK, TemmaVersionT, NARRAY(TemmaVersionT), "", 0};


/*******************************/
/* Driver notes text property */
/*******************************/
static IText TemmaNoteT[]			= {{"Note", "", 0, 0, 0, 0}, {"Feedback", "", 0, 0,0 ,0}};
static ITextVectorProperty TemmaNoteTP		= { mydev, "Temma Driver", "", MAIN_GROUP, IP_RO, 0, IPS_OK, TemmaNoteT, NARRAY(TemmaNoteT), "", 0};
		
/*******************************/
/* Port names text property */
/*******************************/
static IText PortT[]			= {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty Port		= { 
	mydev, "DEVICE_PORT", "Ports", COMM_GROUP, 
	IP_RW, 0, IPS_OK, PortT, NARRAY(PortT), "", 0};

/*******************************/
/* EQUATORIAL_COORD RW property */
/*******************************/
static INumber eq[] = {
	{"RA"		,"RA H:M:S" , "%10.6m", 0, 24, 0, 0, 0, 0, 0},	
	{"DEC", "Dec D:M:S", "%10.6m", -90, 90, 0, 0, 0, 0, 0}
};
static INumberVectorProperty eqNum = { 
	mydev, "EQUATORIAL_EOD_COORD", "Equatorial JNow", 
	MAIN_GROUP , IP_RW, 0, IPS_IDLE, eq, NARRAY(eq), "", 0};

/*******************************/
/* MOUNT_COORD RO property */
/*******************************/
static INumber eqtem[] = {
  {"RA", "RA H:M:S", "%10.6m", 0, 24, 0, 0, 0, 0, 0},
  {"DEC", "Dec D:M:S", "%10.6m", -90, 90, 0, 0, 0, 0, 0}
};
static INumberVectorProperty eqTemma = { 
	mydev, "Temma", "Mount coordinates", 
	MAIN_GROUP , IP_RO, 0, IPS_IDLE, eqtem, NARRAY(eqtem), "", 0};


/* Traccking correction parameters (i.e. to track comets) */
static INumber comet[] = {
	{"RAcorrspeed","Comet RA motion arcmin/day","%+5d",-21541,21541,0,0,0,0,0},	
	{"DECcor rspeed", "Comet DEC motion arcmin/day", "%+4d", -600, 600,0, 0., 0, 0, 0}
};

static INumberVectorProperty cometNum = { 
	mydev, "COMET_TRACKING", "Comet tracking parameters", 
	MOVE_GROUP, IP_RW, 0, IPS_IDLE, comet, NARRAY(comet), "", 0};

/* Date & Time */
static INumber UTC[] = {
	{"UTC", "UTC", "%10.6m" , 0.,0.,0.,0., 0, 0, 0}};
INumberVectorProperty Time = { 
	mydev, "TIME_UTC", "UTC Time", DATETIME_GROUP, 
	IP_RW, 0, IPS_IDLE, UTC, NARRAY(UTC), "", 0};

static INumber STime[] = {
	{"LST", "Sidereal time", "%10.6m" , 
		0.,0.,0.,0., 0, 0, 0}};

INumberVectorProperty SDTime = { 
	mydev, "TIME_LST", "Sidereal Time", 
	DATETIME_GROUP, IP_RW, 0, IPS_IDLE, 
	STime, NARRAY(STime), "", 0};

static ISwitch OnCoordSetS[] = {
	{"SLEW", "Goto", ISS_OFF, 0, 0 }, 
	{"SYNC", "Sync", ISS_ON, 0 , 0},
	{"TRACK", "Track", ISS_OFF, 0, 0 }}; 

static ISwitchVectorProperty OnCoordSetSw = { 
	mydev, "ON_COORD_SET", "On Set", 
	MAIN_GROUP, IP_RW, ISR_1OFMANY, 0,
	IPS_IDLE, OnCoordSetS, NARRAY(OnCoordSetS), "", 0};



static ISwitch abortSlewS[] = {
	{"ABORT", "Abort", ISS_OFF, 0, 0 }};
static ISwitchVectorProperty abortSlewSw = { 
	mydev, "ABORT_MOTION", "******* ABORT GOTO *********",
	MAIN_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, 
	abortSlewS, NARRAY(abortSlewS), "", 0};

/* geographic location */
static INumber geo[] = {
	{"LAT", "Lat. D:M:S +N", "%10.6m", -90., 90., 0., 0., 0, 0, 0},
	{"LONG", "Long. D:M:S +E", "%10.6m", 0., 360., 0., 0., 0, 0, 0} };

static INumberVectorProperty geoNum = {
	mydev, "GEOGRAPHIC_COORD", "Geographic Location", 
	DATETIME_GROUP, IP_RW, 0., IPS_IDLE,
	geo, NARRAY(geo), "", 0};

#endif
