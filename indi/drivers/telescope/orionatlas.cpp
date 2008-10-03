/*
    OrionAtlas (EQ-G/EQ-6 with SkyScan/SynScan controller)
    Copyright (C) 2005 Bruce Bockius (bruceb@WhiteAntelopeSoftware.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "orionatlas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>

#define mydev       "OrionAtlas"
#define ATLAS_DEBUG 1
#define POLLMS 5000

#define lat geo[0].value
#define lon geo[1].value

OrionAtlas *telescope = NULL;


/* There is _one_ binary for all LX200 drivers, but each binary is renamed
** to its device name (i.e. lx200gps, lx200_16..etc). The main function will
** fetch from std args the binary name and ISInit will create the apporpiate
** device afterwards. If the binary name does not match any known devices,
** we simply create a generic device
*/
extern char* me;

#define COMM_GROUP  "Communication"
#define BASIC_GROUP "Main Control"
#define SETUP_GROUP "Setup"

#define ATLAS_MIN_RA 0.0
#define ATLAS_MAX_RA 24.0
#define ATLAS_MIN_DEC -90.0
#define ATLAS_MAX_DEC 90.0

#define ATLAS_MIN_AZ 0.0
#define ATLAS_MAX_AZ 360.0
#define ATLAS_MIN_ALT -90.0
#define ATLAS_MAX_ALT 90.0

static void ISPoll(void *);

//static ISwitch abortSlewS[]      = {{"ABORT", "Abort", ISS_OFF, 0, 0}};

/* Fundamental group */
static ISwitch PowerS[] = {{"CONNECT","Connect",ISS_OFF,0,0},{"DISCONNECT","Disconnect",ISS_ON,0,0},{"RECONNECT","Reconnect",ISS_OFF,0,0}};
static ISwitchVectorProperty PowerSw = { mydev, "CONNECTION" , "Connection", COMM_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, PowerS, NARRAY(PowerS), "", 0};
static IText PortT[] = {{"PORT", "Port", 0, 0, 0, 0}};
static ITextVectorProperty Port = { mydev, "DEVICE_PORT", "Ports", COMM_GROUP, IP_RW, 0, IPS_OK, PortT, NARRAY(PortT), "", 0};

/* Movement group */
//static ISwitchVectorProperty abortSlewSw     = { mydev, "ABORT_MOTION", "Abort Slew/Track", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_IDLE, abortSlewS, NARRAY(abortSlewS), "", 0};
static INumber eq[] = {
    {"RA",  "RA (hh:mm.m)", "%010.5m",  ATLAS_MIN_RA, ATLAS_MAX_RA, 0., 0., 0, 0, 0},
    {"DEC", "Dec (dd:mm.m)", "%010.5m", ATLAS_MIN_DEC, ATLAS_MAX_DEC, 0., 0., 0, 0, 0}};
// Azimuth/Altitude
static INumber aa[] = {
    {"XAZ",  "Az (ddd:mm.m)", "%010.5m", ATLAS_MIN_AZ, ATLAS_MAX_AZ, 0.0, 0.0, 0, 0, 0},
    {"XALT", "Alt (dd:mm.m)", "%010.5m", ATLAS_MIN_ALT, ATLAS_MAX_ALT, 0.0, 0.0, 0, 0, 0}};
static INumberVectorProperty eqNum = {
    mydev, "EQUATORIAL_EOD_COORD", "Eq. Coordinates", BASIC_GROUP, IP_RW, 0, IPS_OK, eq, NARRAY(eq), "", 0};

static INumberVectorProperty aaNum = {
    mydev, "XHORIZONTAL_COORD", "Horz. Coordinates", BASIC_GROUP, IP_RW, 0, IPS_OK, aa, NARRAY(aa), "", 0};

static ISwitch OnCoordSetS[] = {{"TRACK", "Track", ISS_ON, 0, 0}}; // switch does nothing, but some clients won't let you track w/o it.
static ISwitchVectorProperty OnCoordSetSw = {
    mydev, "ON_COORD_SET", "On Set", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_OK, OnCoordSetS, NARRAY(OnCoordSetS), "", 0};
static ISwitch UpdateS[] = {{"UPDATE1", "On", ISS_ON, 0, 0},{"UPDATE0","Off", ISS_OFF,0,0}};
static ISwitch MovementRADecS[] = {{"XRAPLUS", "RA+", ISS_OFF, 0, 0}, {"XRAMINUS", "RA-", ISS_OFF, 0, 0}, {"XDECPLUS", "Dec+", ISS_OFF, 0, 0},            {"XDECMINUS", "Dec-", ISS_OFF, 0, 0}};
static ISwitch MovementAzAltS[] = {{"XAZPLUS", "Az+", ISS_OFF, 0, 0}, {"XAZMINUS", "Az-", ISS_OFF, 0, 0}, {"XALTPLUS", "Alt+", ISS_OFF, 0, 0},            {"XALTMINUS", "Alt-", ISS_OFF, 0, 0}};
static ISwitchVectorProperty MovementRADecSw = { mydev, "XRADECMOVEMENT", "Nudge", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_OK, MovementRADecS,             NARRAY(MovementRADecS), "", 0};
static ISwitchVectorProperty MovementAzAltSw = { mydev, "XAZALTMOVEMENT", "Nudge", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_OK, MovementAzAltS,             NARRAY(MovementAzAltS), "", 0};
static ISwitchVectorProperty UpdateSw = { mydev, "XUPDATE", "Update Coords", BASIC_GROUP, IP_RW, ISR_1OFMANY, 0, IPS_OK, UpdateS, NARRAY(UpdateS), "", 0};

/* Telescope Parameters group */
// Setup group
static INumber steps[] = {
    {"XRASTEP", "RA Step", "%010.6m", 0.0, 5.0, 0.0, 0.0, 0, 0, 0},
    {"XDECSTEP", "Dec Step", "%010.6m", 0.0, 5.0, 0.0, 0.0, 0, 0, 0},
    {"XAZSTEP", "Az Step", "%010.6m", 0.0, 5.0, 0.0, 0.0, 0, 0, 0},
    {"XALTSTEP", "Alt Step", "%010.6m", 0.0, 5.0, 0.0, 0.0, 0, 0, 0}};
// geographic position
static INumber geo[] = {
    {"LAT", "Lat (dd:mm.m)", "%010.5m", -90.0, 90.0, 0.0, 0.0, 0, 0, 0},
    {"LONG", "Lon (ddd:mm.m)", "%010.5m", -180.0, 360.0, 0.0, 0.0, 0, 0, 0}};
static INumberVectorProperty geoNum = {
    mydev, "GEOGRAPHIC_COORD", "Scope Location", SETUP_GROUP, IP_RW, 0, IPS_OK, geo, NARRAY(geo), "", 0};
static INumberVectorProperty stepNum = {
    mydev, "XSTEPS", "Nudge Steps", SETUP_GROUP, IP_RW, 0, IPS_OK, steps, NARRAY(steps), "", 0};

/* send client definitions of all properties */
void ISInit()
{
    static int isInit=0;

    if (isInit) return;

    isInit = 1;

    PortT[0].text = strcpy(new char[32], "/dev/ttyUSB0");
    steps[0].value=1.0/60.0;
    steps[1].value=1.0/60.0;
    steps[2].value=1.0/60.0;
    steps[3].value=1.0/60.0;

    telescope = new OrionAtlas();

    IEAddTimer (POLLMS, ISPoll, NULL);
}

void ISGetProperties (const char *dev)
{ ISInit(); telescope->ISGetProperties(dev);}
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{ ISInit(); telescope->ISNewSwitch(dev, name, states, names, n);}
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{ ISInit(); telescope->ISNewText(dev, name, texts, names, n);}
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{ ISInit(); telescope->ISNewNumber(dev, name, values, names, n);}
void ISPoll (void *p) { telescope->ISPoll();    IEAddTimer (POLLMS, ISPoll, NULL); p=p;}
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

OrionAtlas::OrionAtlas()
{
    geo[0].value=0;
    geo[1].value=0;
    lat=lon=-1000;

    TelConnectFlag=0;
    Updating=true;
    // Children call parent routines, this is the default
    IDLog("Initialized Orion Atlas EQ-G device, driver ver 0.101\n");
    if (ATLAS_DEBUG) IDLog("Driver in DEBUG mode.");
}

void OrionAtlas::ISGetProperties(const char *dev)
{

    if (dev && strcmp (mydev, dev))
        return;

    // COMM_GROUP
    IDDefSwitch (&PowerSw, NULL);
    IDDefText   (&Port, NULL);

    // BASIC_GROUP
    IDDefNumber (&eqNum, NULL);
    IDDefNumber (&aaNum, NULL);
    IDDefSwitch (&UpdateSw, NULL);
//    IDDefSwitch (&abortSlewSw, NULL);
    IDDefSwitch (&OnCoordSetSw,NULL);
    IDDefSwitch (&MovementRADecSw, NULL);
    IDDefSwitch (&MovementAzAltSw, NULL);

  // SETUP_GROUP
    IDDefNumber (&geoNum, NULL);
    IDDefNumber (&stepNum, NULL);

  /* Send the basic data to the new client if the previous client(s) are already connected. */
    if (PowerSw.s == IPS_OK) {
        if (ATLAS_DEBUG) log("Initial call to getBasicData()\n");
        getBasicData();
    }
}

void OrionAtlas::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    IText *tp;

    // suppress warning
    n=n;
    // ignore if not ours //
    if (strcmp (dev, mydev))
        return;

    if (!strcmp(name, Port.name) ) {
        Port.s = IPS_OK;

        tp = IUFindText( &Port, names[0] );
        if (!tp)
            return;

        tp->text = new char[strlen(texts[0])+1];
        strcpy(tp->text, texts[0]);
        IDSetText (&Port, NULL);
        return;
    }
    else {
        if (ATLAS_DEBUG) log("ISNewText('%s')\n",name);
    }
}

void OrionAtlas::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    double newRA=0, newDEC=0, newAlt=0, newAz=0;

        // ignore if not ours //
    if (strcmp (dev, mydev))
        return;

    if (!strcmp (name, eqNum.name)) {
        int i=0, nset=0;
        if (checkPower(&eqNum)) return;

        for (nset = i = 0; i < n; i++) {
            INumber *eqp = IUFindNumber (&eqNum, names[i]);
            if (eqp == &eq[0]) {
                newRA = values[i];
                nset += newRA >= 0 && newRA <= 24.0;
            } else if (eqp == &eq[1]) {
                newDEC = values[i];
                nset += newDEC >= -90.0 && newDEC <= 90.0;
            }
        }
        if (nset==2) { // both coords were valid.  Slew.
            MoveScope(RADEC,newRA,newDEC);
        }
    }
    else if (!strcmp(name, aaNum.name)) {
        int i=0, nset=0;
        if (checkPower(&eqNum)) return;

        for (nset = i = 0; i < n; i++) {
            INumber *aap = IUFindNumber (&aaNum, names[i]);
            if (aap == &aa[1]) {
                newAlt = values[i];
                nset += newAlt >= -90.0 && newAlt <= 90.0;
            } else if (aap == &aa[0]) {
                newAz = values[i];
                nset += newAz >= 0.0 && newAz <=360.0;
            }
        }
        if (nset==2) { // both coords were valid.  Slew.
            MoveScope(AZALT,newAz,newAlt);
        }
    }
    else if (!strcmp(name, geoNum.name)) {
        if (ATLAS_DEBUG) log("NewNumber(geoName)\n");
        int i=0;
        for (i=0;i<n;i++) {
            INumber *geop = IUFindNumber(&geoNum, names[i]);
            geop->value=values[i];
        }
        IDSetNumber(&geoNum,NULL);
    }
    else if (!strcmp(name, stepNum.name)) {
        if (ATLAS_DEBUG) log("NewNumber(stepNum)\n");
        for (int i=0;i<n;i++) {
            INumber *stepp = IUFindNumber(&stepNum, names[i]);
            stepp->value=values[i];
        }
        IDSetNumber(&stepNum,NULL);
    }
    else {
        if (ATLAS_DEBUG) log("ISNewNumber('%s')\n",name);
    }
}

void OrionAtlas::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    INDI_UNUSED(names);

    // ignore if not ours //
    if (strcmp(dev,mydev))
        return;

    // FIRST Switch ALWAYS for power
    if (!strcmp(name,PowerSw.name)) 
    {
        if (IUUpdateSwitch(&PowerSw,states,names,n) < 0)
		return;

        if (PowerS[1].s  == ISS_ON)
        	DisconnectTel();
	else
        	ConnectTel();
        return;
        }
    else if (!strcmp(name,UpdateSw.name)) {
        IUResetSwitch(&UpdateSw);
        IUUpdateSwitch(&UpdateSw,states,names,n);
        IDSetSwitch(&UpdateSw,NULL);
        Updating=!Updating;
    }
    else if (!strcmp(name,MovementRADecSw.name)) {
        if (!GetCoords(RADEC)) {
            IDMessage(mydev,"Invalid coordinates from scope - aborted nudge.");
        }
        log("before RA=%f  Dec=%f\n",returnRA,returnDec);
        for (int i=0; i<n; i++) {
            states[i]=ISS_OFF;
            ISwitch *mvp = IUFindSwitch(&MovementRADecSw, names[i]);
            if (mvp == &MovementRADecS[0]) {
                // RA+
                returnRA+=steps[0].value;
            } else if (mvp == &MovementRADecS[1]) {
                // RA-
                returnRA-=steps[0].value;
            } else if (mvp == &MovementRADecS[2]) {
                // Dec+
                returnDec+=steps[1].value;
            } else if (mvp == &MovementRADecS[3]) {
                // Dec-
                returnDec-=steps[1].value;
            }
        }
        log("after  RA=%f  Dec=%f\n",returnRA,returnDec);
        IUResetSwitch(&MovementRADecSw);
        IUUpdateSwitch(&MovementRADecSw,states,names,n);
        IDSetSwitch(&MovementRADecSw,NULL);
        MoveScope(RADEC,returnRA,returnDec);
        UpdateCoords();
    }
    else if (!strcmp(name,MovementAzAltSw.name)) {
        if (!GetCoords(AZALT)) {
            IDMessage(mydev,"Invalid coordinates from scope - aborted nudge.");
        }
        log("before     Az=%f  Alt=%f\n",returnAz,returnAlt);
        for (int i=0; i<n; i++) {
            states[i]=ISS_OFF;
            ISwitch *mvp = IUFindSwitch(&MovementAzAltSw, names[i]);
            if (mvp == &MovementAzAltS[0]) {
                // Az+
                returnAz+=steps[2].value;
            } else if (mvp == &MovementAzAltS[1]) {
                // Az-
                returnAz-=steps[2].value;
            } else if (mvp == &MovementAzAltS[2]) {
                // Alt+
                returnAlt+=steps[3].value;
            } else if (mvp == &MovementAzAltS[3]) {
                // Alt-
                returnAlt-=steps[3].value;
            }
        }
        log("commanded  Az=%f  Alt=%f\n",returnAz,returnAlt);
        IUResetSwitch(&MovementAzAltSw);
        IUUpdateSwitch(&MovementAzAltSw,states,names,n);
        IDSetSwitch(&MovementAzAltSw,NULL);
        MoveScope(AZALT,returnAz,returnAlt);
        UpdateCoords();
        log("after      Az=%f  Alt=%f\n",returnAz,returnAlt);
    }
    else {
        if (ATLAS_DEBUG) log("ISNewSwitch('%s')\n",name);
    }
}

int OrionAtlas::checkPower(ISwitchVectorProperty *sp)
{
    if (PowerSw.s != IPS_OK) {
        if (!strcmp(sp->label, ""))
            IDMessage (mydev, "Cannot change property %s while the telescope is offline.", sp->name);
        else
            IDMessage (mydev, "Cannot change property %s while the telescope is offline.", sp->label);
        sp->s = IPS_IDLE;
        IDSetSwitch(sp, NULL);
        return -1;
    }
    return 0;
}

int OrionAtlas::checkPower(INumberVectorProperty *np)
{
    if (PowerSw.s != IPS_OK) {
        if (!strcmp(np->label, ""))
            IDMessage (mydev, "Cannot change property %s while the telescope is offline.", np->name);
        else
            IDMessage (mydev, "Cannot change property %s while the telescope is offline.", np->label);
        np->s = IPS_IDLE;
        IDSetNumber(np, NULL);
        return -1;
    }
    return 0;
}

int OrionAtlas::checkPower(ITextVectorProperty *tp)
{
    if (PowerSw.s != IPS_OK) {
        if (!strcmp(tp->label, ""))
            IDMessage (mydev, "Cannot change property %s while the telescope is offline.", tp->name);
        else
            IDMessage (mydev, "Cannot change property %s while the telescope is offline.", tp->label);
        tp->s = IPS_IDLE;
        IDSetText(tp, NULL);
        return -1;
    }
    return 0;
}

void OrionAtlas::getBasicData()
{
    if (ATLAS_DEBUG) log("getBasicData\n");
    UpdateCoords();
}

void OrionAtlas::ConnectTel()
{
    if (PowerSw.sp[0].s==ISS_ON||PowerSw.sp[2].s==ISS_ON) { // CONNECT or RECONNECT
        if (ConnectTel(Port.tp[0].text) < 0) 
	{
            PowerS[0].s = PowerS[2].s = ISS_OFF;
            PowerS[1].s = ISS_ON;
            IDSetSwitch (&PowerSw, "Error connecting to port %s", Port.tp[0].text);
            return;
        }
        PowerSw.s = IPS_OK;
        PowerS[0].s = ISS_ON;
        PowerS[1].s = PowerS[2].s= ISS_OFF;
        IDSetSwitch (&PowerSw, "Telescope is online. Updating coordinates.");
        if (ATLAS_DEBUG) log("Powered on scope, calling getBasicData()\n");
        getBasicData();
    }
    else if (PowerSw.sp[0].s==ISS_OFF) {
        IDSetSwitch (&PowerSw, "Telescope is offline.");
        DisconnectTel();
    }
}

int OrionAtlas::CheckConnectTel(void)
{
    return TelConnectFlag;
}

int OrionAtlas::ConnectTel(char *port)
{
    struct termios tty;
    unsigned char returnStr[128];
    int numRead;

    if (ATLAS_DEBUG) log( "Connecting to port: %s\n",port);

    if(TelConnectFlag != 0)
        return 0;

    /* Make the connection */

    TelPortFD = open(port,O_RDWR);
    if(TelPortFD == -1) {
        IDMessage(mydev,"Could not open the supplied port!");
        return -1;
    }

    tcgetattr(TelPortFD,&tty);
    cfsetospeed(&tty, (speed_t) B9600);
    cfsetispeed(&tty, (speed_t) B9600);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag =  IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 5;
    tty.c_iflag &= ~(IXON|IXOFF|IXANY);
    tty.c_cflag &= ~(PARENB | PARODD);
    tcsetattr(TelPortFD, TCSANOW, &tty);

    /* Flush the input (read) buffer */

    tcflush(TelPortFD,TCIOFLUSH);

    /* Test connection */

    writen(TelPortFD,(unsigned char*)"?",1);
    numRead=readn(TelPortFD,returnStr,1,2);
    returnStr[numRead] = '\0';

    if (numRead == 1 && returnStr[0]=='#') {
        TelConnectFlag = 1;
        IDMessage(mydev,"Successfully connected.");
        return (0);
    }
    else if (numRead>0) {
        IDMessage(mydev,"Connect failure: Did not detect an Orion Atlas EQ-G on this port!");
        return -2;
    }
    else {
        IDMessage(mydev,"Connect failure: Did not detect any device on this port!");
        return -3;
    }
}

void OrionAtlas::DisconnectTel(void)
{
    if(TelConnectFlag == 1)
        close(TelPortFD);
    TelConnectFlag = 0;
    IDMessage(mydev,"Telescope is offline.");
}

// If System=RADEC, c1=RA, c2=Dec
//          =AZALT, c1=Az, c2=Alt
int OrionAtlas::MoveScope(int System, double c1, double c2)
{
    MovementRADecSw.s=MovementAzAltSw.s=eqNum.s=aaNum.s=OnCoordSetSw.s=IPS_BUSY;
    IDSetSwitch(&MovementRADecSw,NULL);
    IDSetSwitch(&MovementAzAltSw,NULL);
    IDSetSwitch(&OnCoordSetSw,NULL);
    IDSetNumber(&eqNum,NULL);
    IDSetNumber(&aaNum,NULL);
    union {
        signed short int ints[2];
        unsigned short int uints[2];
        unsigned char bytes[4];
    };

    char Command=' ';
    while (1) {
        if (System==RADEC) Command='R';
        else if (System==AZALT) Command='A';
        else {
            log("Invalid command '%c' to MoveScope!",Command);
            break;
        }
        unsigned char sendStr[5];
        writen(TelPortFD,(unsigned char*)"?",1);
        int numRead=readn(TelPortFD,sendStr,1,3);
        if (numRead!=1 || sendStr[0]!='#') {
            IDMessage(mydev,"Failure: Scope not ready for movement command!");
            break;
        }

        if (Command=='R') {
            IDMessage(mydev,"Beginning slew to RA=%f Dec=%f\n",c1,c2);
            // RA:
            uints[0]= (unsigned short int) (c1*65536.0/24.0);
            ints[1]=  (signed short int) (c2*65536.0/360.0);
            sendStr[2]=bytes[0];    sendStr[1]=bytes[1];
            sendStr[4]=bytes[2];    sendStr[3]=bytes[3];
        }
        else {
            //c2=c2-lat+90.0; // decorrect
            IDMessage(mydev,"Beginning slew to Az=%f Alt=%f\n",c1,c2);
            // Az
            uints[0]= (unsigned short int) (c1*65536.0/360.0);
            ints[1]= (signed short int) (c2*65536.0/360.0);
            sendStr[2]=bytes[0];    sendStr[1]=bytes[1];
            sendStr[4]=bytes[2];    sendStr[3]=bytes[3];
        }
        sendStr[0]=Command;
        if (ATLAS_DEBUG) log("Sending '%c' %X %X %X %X\n",Command,sendStr[1],sendStr[2],sendStr[3],sendStr[4]);
        writen(TelPortFD,sendStr,5);
        // it should send us an '@' when done slewing
        numRead=readn(TelPortFD,sendStr,1,60);
        if (numRead!=1||sendStr[0]!='@') {
            IDMessage(mydev,"Timeout waiting for scope to complete slewing.");
            break;
        }
        IDMessage(mydev,"Slewing complete.");

        MovementRADecSw.s=MovementAzAltSw.s=eqNum.s=aaNum.s=OnCoordSetSw.s=IPS_OK;
        IDSetSwitch(&MovementRADecSw,NULL);
        IDSetSwitch(&MovementAzAltSw,NULL);
        IDSetSwitch(&OnCoordSetSw,NULL);
        IDSetNumber(&eqNum,NULL);
        IDSetNumber(&aaNum,NULL);
        return 1;
    }
    // only here if break = error
    MovementRADecSw.s=MovementAzAltSw.s=eqNum.s=aaNum.s=OnCoordSetSw.s=IPS_ALERT;
    IDSetSwitch(&MovementRADecSw,NULL);
    IDSetSwitch(&MovementAzAltSw,NULL);
    IDSetSwitch(&OnCoordSetSw,NULL);
    IDSetNumber(&eqNum,NULL);
    IDSetNumber(&aaNum,NULL);
    return 0;
}

/* Read the telescope coordinates */
int OrionAtlas::GetCoords(int System)
{
    UpdateSw.s=IPS_BUSY;
    IDSetSwitch(&UpdateSw,NULL);
    unsigned char returnStr[4];
    union {
        signed short int ints[2];
        unsigned short int uints[2];
        unsigned char bytes[4];
    };

    while (1) {
        if (System&AZALT) {
            returnAz=returnAlt=-1000;
            if (System&RADEC) {returnRA=returnDec=-1000;}
            // is scope ready?
            writen(TelPortFD,(unsigned char*)"?",1);
            int numRead=readn(TelPortFD,returnStr,1,3);
            if (numRead!=1 || returnStr[0]!='#') {
                IDMessage(mydev,"Failure: Scope not ready for Z command");
                break;
            }
            // Send request for current Az/Alt coords
            writen(TelPortFD,(unsigned char*)"Z",1);
            numRead=readn(TelPortFD,returnStr,4,1);
            if (numRead!=4) break;
            // bytes as expected
            if (ATLAS_DEBUG) log("Received '%c' %02x %02x %02x %02x\n",'Z',returnStr[0],returnStr[1],returnStr[2],returnStr[3]);

            bytes[0]=returnStr[1];  bytes[1]=returnStr[0];
            bytes[2]=returnStr[3];  bytes[3]=returnStr[2];
            returnAz=uints[0]/65536.0*360.0;
            returnAlt=ints[1]/65536.0*360.0;
    //        returnAlt=returnAlt+lat-90.0;
        }

        if (System&RADEC) {
            returnRA=returnDec=-1000;
            // Check scope is ready
            writen(TelPortFD,(unsigned char*)"?",1);
            int numRead=readn(TelPortFD,returnStr,1,3);
            if (numRead!=1 || returnStr[0]!='#') {
                IDMessage(mydev,"Failure: Scope not ready for E command");
                break;
            }
            // Send get RA/Dec
            writen(TelPortFD,(unsigned char*)"E",1);
            numRead=readn(TelPortFD,returnStr,4,1);
            if (ATLAS_DEBUG) log("Received '%c' %02x %02x %02x %02x\n",'E',returnStr[0],returnStr[1],returnStr[2],returnStr[3]);
            if (numRead!=4) break;
            // bytes read is as expected

            bytes[0]=returnStr[1];  bytes[1]=returnStr[0];
            bytes[2]=returnStr[3];  bytes[3]=returnStr[2];
            returnRA=uints[0]/65536.0*24.0;
            returnDec=ints[1]/65536.0*360.0;
        }
        // Yep, data is valid.
        UpdateSw.s=IPS_OK;
        IDSetSwitch(&UpdateSw,NULL);
        return(1);
    }
    // only here if break -> error!
    UpdateSw.s=IPS_ALERT;
    IDSetSwitch(&UpdateSw,NULL);
    return 0;
}

// Get coords from scope and update INumbers
void OrionAtlas::UpdateCoords(int System)
{
    if (GetCoords(System)) {
        if (System&RADEC) {
            eqNum.np[0].value = returnRA;
            eqNum.np[1].value = returnDec;
            IDSetNumber(&eqNum, NULL);
        }
        if (System&AZALT) {
            aaNum.np[1].value = returnAlt;
            aaNum.np[0].value = returnAz;
            IDSetNumber(&aaNum, NULL);
        }
    }
}

int OrionAtlas::writen(int fd, unsigned char* ptr, int nbytes)
{
    int nleft, nwritten;
    nleft = nbytes;
    while (nleft > 0) {
        nwritten = write (fd, ptr, nleft);
        if (nwritten <=0 ) break;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (nbytes - nleft);
}

int OrionAtlas::readn(int fd, unsigned char* ptr, int nbytes, int sec)
{
    int status;
    int nleft, nread;
    nleft = nbytes;
    while (nleft > 0) {
        status = telstat(fd,sec,0);
        if (status <=  0 ) break;
        nread  = read (fd, ptr, nleft);

/*  Diagnostic */

/*    printf("readn: %d read\n", nread);  */

        if (nread <= 0)  break;
        nleft -= nread;
        ptr += nread;
    }
    return (nbytes - nleft);
}

/*
 * Examines the read status of a file descriptor.
 * The timeout (sec, usec) specifies a maximum interval to
 * wait for data to be available in the descriptor.
 * To effect a poll, the timeout (sec, usec) should be 0.
 * Returns non-negative value on data available.
 * 0 indicates that the time limit referred by timeout expired.
 * On failure, it returns -1 and errno is set to indicate the
 * error.
 */

int OrionAtlas::telstat(int fd,int sec,int usec)
{
    int ret;
    int width;
    struct timeval timeout;
    telfds readfds;

    memset((char *)&readfds,0,sizeof(readfds));
    FD_SET(fd, &readfds);
    width = fd+1;
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    ret = select(width,&readfds,(telfds *)0,(telfds *)0,&timeout);
    return(ret);
}

void OrionAtlas::ISPoll()
{
    // Called every 2 seconds.
    if (!TelConnectFlag) return;
    if (!Updating) return;
    // Check status of
    if (eqNum.s==IPS_IDLE||eqNum.s==IPS_OK) {
        GetCoords();
        eqNum.np[0].value=returnRA;
        eqNum.np[1].value=returnDec;
        aaNum.np[0].value=returnAz;
        aaNum.np[1].value=returnAlt;
        IDSetNumber(&eqNum,NULL);
        IDSetNumber(&aaNum,NULL);
    }
    else { // Slewing.  Don't get coords yet.
        if (ATLAS_DEBUG) log("   (still slewing)\n");
    }
}

void OrionAtlas::log(const char *fmt,...)
{
    if (fmt) {
        va_list ap;
        va_start (ap, fmt);
        fprintf(stderr, "%i: ", ((int) time(NULL)));
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
}
