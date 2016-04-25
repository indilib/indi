#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <algorithm>
#include <queue>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <indicom.h>
#include <inditelescope.h>
#include "NexStarAUXScope.h"



#define BUFFER_SIZE 10240
int MAX_CMD_LEN=32;
bool DEBUG = false;

//////////////////////////////////////////////////
/////// Utility functions
//////////////////////////////////////////////////

void msleep(unsigned ms){
    usleep(ms*1000);
}


void prnBytes(unsigned char *b,int n){
    fprintf(stderr,"[");
    for (int i=0; i<n; i++) fprintf(stderr,"%02x ", b[i]);
    fprintf(stderr,"]\n");
}

void dumpMsg(buffer buf){
    fprintf(stderr, "MSG: ");
    for (int i=0; i<buf.size(); i++){
        fprintf(stderr, "%02x ", buf[i]);
    }
    fprintf(stderr, "\n");
}

////////////////////////////////////////////////
//////  AUXCommand class
////////////////////////////////////////////////

AUXCommand::AUXCommand(buffer buf){
        parseBuf(buf);
}
    
AUXCommand::AUXCommand(AUXCommands c, AUXtargets s, AUXtargets d, buffer dat){
        cmd=c;
        src=s;
        dst=d;
        data=buffer(dat);
        len=3+data.size();
}

AUXCommand::AUXCommand(AUXCommands c, AUXtargets s, AUXtargets d){
        cmd=c;
        src=s;
        dst=d;
        data=buffer();
        len=3+data.size();
}



void AUXCommand::dumpCmd(){
    if (DEBUG) {
    fprintf(stderr,"(%02x) %02x -> %02x: ", cmd, src, dst);
    for (int i=0; i<data.size(); i++) fprintf(stderr,"%02x ", data[i]);
    fprintf(stderr,"\n");
    }
}


void AUXCommand::fillBuf(buffer &buf){
    buf.resize(len+3);
    buf[0]=0x3b;
    buf[1]=(unsigned char)len;
    buf[2]=(unsigned char)src;
    buf[3]=(unsigned char)dst;
    buf[4]=(unsigned char)cmd;
    for (int i=0; i<data.size(); i++){
        buf[i+5]=data[i];
    }
    buf.back()=checksum(buf);
    //dumpMsg(buf);
}

void AUXCommand::parseBuf(buffer buf){
    len=buf[1];
    src=(AUXtargets)buf[2];
    dst=(AUXtargets)buf[3];
    cmd=(AUXCommands)buf[4];
    data=buffer(buf.begin()+5,buf.end()-1);
    valid=(checksum(buf)==buf.back());
    if (not valid) {
        fprintf(stderr,"Checksum error: %02x vs. %02x", checksum(buf), buf.back());
        dumpMsg(buf);
    };
}


unsigned char AUXCommand::checksum(buffer buf){
    int l=buf[1];
    int cs=0;
    for (int i=1; i<l+2; i++){
        cs+=buf[i];
    }
    //fprintf(stderr,"CS: %x  %x  %x\n", cs, ~cs, ~cs+1);
    return (unsigned char)(((~cs)+1) & 0xFF);
    //return ((~sum([ord(c) for c in msg]) + 1) ) & 0xFF
}

// This is probably wrong. The controlers return fraction of the revolution.
// and it seems to be unsigned
//long AUXCommand::getPosition(){
//    if (data.size()==3) {
//        int a= (int)data[0] << 16 | (int)data[1]<<8 | (int)data[2];
//        if (data[0] & 0x80) { 
//            a |= 0xff000000; 
//        }
//        //fprintf(stderr,"Angle: %d %08x = %d => %f\n", sizeof(a), a, a, a/pow(2,24));
//        return a;
//    } else {
//        return 0;
//    }
//}

// One definition rule (ODR) constants
// AUX commands use 24bit integer as a representation of angle in units of 
// fractional revolutions. Thus 2^24 steps makes full revolution.
const long STEPS_PER_REVOLUTION = 16777216; 
const double STEPS_PER_DEGREE = STEPS_PER_REVOLUTION / 360.0;

long AUXCommand::getPosition(){
    if (data.size()==3) {
        unsigned int a= (unsigned int)data[0] << 16 | (unsigned int)data[1]<<8 | (unsigned int)data[2];
        //fprintf(stderr,"Angle: %d %08x = %d => %f\n", sizeof(a), a, a, a/pow(2,24));
        return (long)a % STEPS_PER_REVOLUTION;
    } else {
        return 0;
    }
}

void AUXCommand::setPosition(double p){
    setPosition(long(p * STEPS_PER_DEGREE));
}

void AUXCommand::setPosition(long p){
    //int a=(int)p; fprintf(stderr,"Angle: %08x = %d => %f\n", a, a, a/pow(2,24));
    data.resize(3);
    p = p % STEPS_PER_REVOLUTION;
    for (int i=2; i>-1; i--) {
        data[i]=(unsigned char)(p & 0xff);
        p >>= 8;
    }
    len=6;
}

void AUXCommand::setRate(unsigned char r){
    data.resize(1);
    len=4;
    data[0]=r;
}

/////////////////////////////////////////////////////
// NexStarAUXScope 
/////////////////////////////////////////////////////

NexStarAUXScope::NexStarAUXScope(char const *ip, int port){
    initScope(ip, port);
};

NexStarAUXScope::NexStarAUXScope(char const *ip){
    initScope(ip, NSEVO_DEFAULT_PORT);
};

NexStarAUXScope::NexStarAUXScope(int port){
    initScope(NSEVO_DEFAULT_IP, port);
};

NexStarAUXScope::NexStarAUXScope() {
    initScope(NSEVO_DEFAULT_IP, NSEVO_DEFAULT_PORT);
};

NexStarAUXScope::~NexStarAUXScope(){
    fprintf(stderr,"Bye!");
    closeConnection();
}

void NexStarAUXScope::initScope(char const *ip, int port){
    fprintf(stderr,"Scope IP %s:%d", ip, port);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET ;
    addr.sin_addr.s_addr=inet_addr(ip);
    addr.sin_port = htons(port);
    initScope();
};

void NexStarAUXScope::initScope() {
    // Max slew rate (steps per second)
    slewRate=2*pow(2,24)/360;
    tracking=false;
    slewingAlt=false;
    slewingAz=false;
    // Park position at the south horizon.
    Alt=targetAlt=Az=targetAz=0;
    Lat=Lon=Elv=0;
    sock=0;
};


bool NexStarAUXScope::Connect(){
    fprintf(stderr,"Connecting...");
    if (sock > 0) {
        // We are connected. Nothing to do!
        return true;
    }
    // Create client socket
    if( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
    {
      perror("Socket error");
      return false;
    }

    // Connect to the scope
    if(connect(sock, (struct sockaddr *)&addr, sizeof addr) < 0)
    {
      perror("Connect error");
      return false;
    }
    /*
    int flags;
    flags = fcntl(sock,F_GETFL,0);
    assert(flags != -1);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    fprintf(stderr, "Socket:%d\n", sock);
    */
    fprintf(stderr, "OK\n");
    msleep(500);
    readMsgs();
    processMsgs();
    return true;
}

bool NexStarAUXScope::Disconnect(){
    fprintf(stderr,"Disconnecting\n");
    closeConnection();
}

bool NexStarAUXScope::UpdateLocation(double lat, double lon, double elev){
    Lat=lat;
    Lon=lon;
    Elv=elev;
    return true;
}

void NexStarAUXScope::closeConnection(){
    if (sock > 0) {
        close(sock);
        sock=0;
    }
}


bool NexStarAUXScope::Abort(){
    Track(0,0);
    buffer b(1);
    b[0]=0;
    AUXCommand stopAlt(MC_MOVE_POS,APP,ALT,b);
    AUXCommand stopAz(MC_MOVE_POS,APP,AZM,b);
    sendCmd(&stopAlt);
    sendCmd(&stopAz);
    return true;
};

long NexStarAUXScope::GetALT(){
    return Alt % STEPS_PER_REVOLUTION;
};

long NexStarAUXScope::GetAZ(){
    return Az % STEPS_PER_REVOLUTION;
};

bool NexStarAUXScope::slewing(){
    return slewingAlt || slewingAz;
}

bool NexStarAUXScope::Slew(AUXtargets trg, int rate){
    AUXCommand cmd((rate<0) ? MC_MOVE_NEG : MC_MOVE_POS ,APP,trg);
    cmd.setRate((unsigned char)(abs(rate) & 0xFF));
    sendCmd(&cmd);
    readMsgs();
    return true;
}

bool NexStarAUXScope::SlewALT(int rate){
    slewingAlt=(rate != 0);
    return Slew(ALT,rate);
}

bool NexStarAUXScope::SlewAZ(int rate){
    slewingAz=(rate != 0);
    return Slew(AZM,rate);
}

bool NexStarAUXScope::GoToFast(long alt, long az, bool track){
    //DEBUG=true;
    targetAlt=alt;
    targetAz=az;
    tracking=track;
    slewingAlt=slewingAz=true;
    Track(0,0);
    AUXCommand altcmd(MC_GOTO_FAST,APP,ALT);
    AUXCommand azmcmd(MC_GOTO_FAST,APP,AZM);
    altcmd.setPosition(alt);
    azmcmd.setPosition(az);
    sendCmd(&altcmd);
    sendCmd(&azmcmd);
    readMsgs();
    //DEBUG=false;
    return true;
};

bool NexStarAUXScope::GoToSlow(long alt, long az, bool track){
    //DEBUG=false;
    targetAlt=alt;
    targetAz=az;
    tracking=track;
    slewingAlt=slewingAz=true;
    Track(0,0);
    AUXCommand altcmd(MC_GOTO_SLOW,APP,ALT);
    AUXCommand azmcmd(MC_GOTO_SLOW,APP,AZM);
    altcmd.setPosition(alt);
    azmcmd.setPosition(az);
    sendCmd(&altcmd);
    sendCmd(&azmcmd);
    readMsgs();
    //DEBUG=false;
    return true;
};

bool NexStarAUXScope::Track(long altRate, long azRate){
    // The scope rates are per minute?
    AltRate=altRate;
    AzRate=azRate;
    if (slewingAlt || slewingAz) {
        AltRate=0;
        AzRate=0;
    }
    tracking=true;
    //fprintf(stderr,"Set tracking rates: ALT: %ld   AZM: %ld\n", AltRate, AzRate);
    AUXCommand altcmd((altRate<0)? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE,APP,ALT);
    AUXCommand azmcmd((azRate<0)? MC_SET_NEG_GUIDERATE : MC_SET_POS_GUIDERATE,APP,AZM);
    altcmd.setPosition(long(abs(AltRate)));
    azmcmd.setPosition(long(abs(AzRate)));
    
    sendCmd(&altcmd);
    sendCmd(&azmcmd);
    readMsgs();
    return true;
};

void NexStarAUXScope::querryStatus(){
    AUXtargets trg[2]={ALT,AZM};
    AUXCommand *cmd;
    for (int i=0; i<2; i++){
        cmd=new AUXCommand(MC_GET_POSITION,APP,trg[i]);
        if (not sendCmd(cmd)) {
            fprintf(stderr,"Send failed!\n");
        }
        delete cmd;
    }
    if (slewingAlt) {
        cmd=new AUXCommand(MC_SLEW_DONE,APP,ALT);
        if (not sendCmd(cmd)) {
            fprintf(stderr,"Send failed!\n");
        }
        delete cmd;
    }
    if (slewingAz) {
        cmd=new AUXCommand(MC_SLEW_DONE,APP,AZM);
        if (not sendCmd(cmd)) {
            fprintf(stderr,"Send failed!\n");
        }
        delete cmd;
    }
    
    
}

void NexStarAUXScope::emulateGPS(AUXCommand *m) {
    if (m->dst != GPS) return;
    if (DEBUG) fprintf(stderr,"Got 0x%02x for GPS\n", m->cmd);

    switch (m->cmd) {
        case GET_VER: { 
            // fprintf(stderr,"GPS: GET_VER from 0x%02x\n", m->src);
            buffer dat(2);
            dat[0]=0x01;
            dat[1]=0x00;
            AUXCommand *cmd=new AUXCommand(GET_VER,GPS,m->src,dat);
            if (not sendCmd(cmd)) {
                fprintf(stderr,"GPS: Send failed!\n");
            }
            delete cmd;
            break;
            }
        case GPS_GET_LAT:
        case GPS_GET_LONG: {
            // fprintf(stderr,"GPS: Sending LAT/LONG Lat:%f Lon:%f\n", Lat, Lon);
            AUXCommand *cmd=new AUXCommand(m->cmd,GPS,m->src);
            if (m->cmd == GPS_GET_LAT )
                cmd->setPosition(Lat);
            else 
                cmd->setPosition(Lon);
            if (not sendCmd(cmd)) {
                fprintf(stderr,"GPS: Send failed!\n");
            }
            delete cmd;            
            break;
            }
        case GPS_GET_TIME: {
            // fprintf(stderr,"GPS: GET_TIME from 0x%02x\n", m->src);
            time_t gmt;
            struct tm * ptm;
            buffer dat(3);
            
            time(&gmt);
            ptm=gmtime(&gmt);
            dat[0]=unsigned(ptm->tm_hour);
            dat[1]=unsigned(ptm->tm_min);
            dat[2]=unsigned(ptm->tm_sec);
            AUXCommand *cmd=new AUXCommand(GPS_GET_TIME,GPS,m->src,dat);
            if (not sendCmd(cmd)) {
                fprintf(stderr,"GPS: Send failed!\n");
            }
            delete cmd;            
            break;
            }
        case GPS_GET_DATE: {
            // fprintf(stderr,"GPS: GET_DATE from 0x%02x\n", m->src);
            time_t gmt;
            struct tm * ptm;
            buffer dat(2);

            time(&gmt);
            ptm=gmtime(&gmt);
            dat[0]=unsigned(ptm->tm_mon+1);
            dat[1]=unsigned(ptm->tm_mday);
            AUXCommand *cmd=new AUXCommand(GPS_GET_DATE,GPS,m->src,dat);
            if (not sendCmd(cmd)) {
                fprintf(stderr,"GPS: Send failed!\n");
            }
            delete cmd;            
            break;
            }
        case GPS_GET_YEAR: {
            // fprintf(stderr,"GPS: GET_YEAR from 0x%02x", m->src);
            time_t gmt;
            struct tm * ptm;
            buffer dat(2);

            time(&gmt);
            ptm=gmtime(&gmt);
            dat[0]=unsigned(ptm->tm_year+1900)>>8;
            dat[1]=unsigned(ptm->tm_year+1900) & 0xFF;
            // fprintf(stderr," Sending: %d [%d,%d]\n",ptm->tm_year,dat[0],dat[1]);
            AUXCommand *cmd=new AUXCommand(GPS_GET_YEAR,GPS,m->src,dat);
            if (not sendCmd(cmd)) {
                fprintf(stderr,"GPS: Send failed!\n");
            }
            delete cmd;            
            break;
            }
        case GPS_LINKED: {
            // fprintf(stderr,"GPS: LINKED from 0x%02x\n", m->src);
            buffer dat(1);

            dat[0]=unsigned(1);
            AUXCommand *cmd=new AUXCommand(GPS_LINKED,GPS,m->src,dat);
            if (not sendCmd(cmd)) {
                fprintf(stderr,"GPS: Send failed!\n");
            }
            delete cmd;            
            break;                        
            }
        default :
            fprintf(stderr,"Got 0x%02x for GPS\n", m->cmd);
            break;
    }

}

void NexStarAUXScope::processMsgs(){
    if (DEBUG) fprintf(stderr,"Processing msgs: %d\n", iq.size());
    while (not iq.empty()) {
        AUXCommand *m=iq.front();
        if (DEBUG) { fprintf(stderr,"Recv: "); m->dumpCmd(); }
        if (m->dst == GPS) 
            emulateGPS(m);
        else 
        switch (m->cmd) {
            case MC_GET_POSITION:
                switch (m->src) {
                    case ALT:
                        Alt=m->getPosition();
                        break;
                    case AZM:
                        Az=m->getPosition();
                        break;
                    default: break;
                }
                break;
            case MC_SLEW_DONE:
                switch (m->src) {
                    case ALT:
                        slewingAlt=m->data[0]!=0xff;
                        break;
                    case AZM:
                        slewingAz=m->data[0]!=0xff;
                        break;
                    default: break;
                }
                break;
            default :
                break;
        }
        iq.pop();
        delete m;
    }
}


void NexStarAUXScope::readMsgs(){
    int n, i;
    unsigned char buf[BUFFER_SIZE];
    
    // We are not connected. Nothing to do.
    if (sock<=0) return;
    
    timeval tv;
    tv.tv_sec=0;
    tv.tv_usec=50000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
    while((n = recv(sock, buf, sizeof(buf),MSG_DONTWAIT|MSG_PEEK)) > 0) {
        if (DEBUG) {
            fprintf(stderr,"Got %d bytes: ", n);
            for (i=0; i<n; i++) fprintf(stderr,"%02x ", buf[i]);
            fprintf(stderr,"\n");
        }
        for (i=0; i<n; ){
            //if (DEBUG) fprintf(stderr,"%d ",i);
            if (buf[i]==0x3b) {
                int shft;
                shft=i+buf[i+1]+3;
                if (shft<=n) {
                    //if (DEBUG) prnBytes(buf+i,shft-i);
                    buffer b(buf+i, buf+shft);
                    //if (DEBUG) dumpMsg(b);
                    iq.push(new AUXCommand(b));
                    if (iq.empty()) {
                        fprintf(stderr,"Queue still empty after push!! (i=%d %d/%d)\n", i, shft, n);
                        dumpMsg(b);
                    }
                } else {
                    fprintf(stderr,"Partial message recv. dropping (i=%d %d/%d)\n", i, shft, n);
                    prnBytes(buf+i,n-i);
                    recv(sock, buf, n,MSG_DONTWAIT);
                    break;
                }
                i=shft;
            } else {
                i++;
            }
        }
        // Actually consume data we parsed. Leave the rest for later.
        if (i>0) {
            n=recv(sock, buf, i,MSG_DONTWAIT);
            if (DEBUG) fprintf(stderr,"Consumed %d/%d bytes (iq.size=%d)\n", n, i, iq.size());
                
        }
    }
    //fprintf(stderr,"Nothing more to read\n");
}

int sendBuffer(int sock, buffer buf, long tout_msec){
    if (sock>0) {
        timeval tv;
        int n;
        tv.tv_usec=(tout_msec%1000)*1000;
        tv.tv_sec=tout_msec/1000;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval));
        n=send(sock, buf.data(), buf.size(), 0);
        msleep(50);
//        if (n!=buf.size()) {
//            fprintf(stderr, "sendBuffer: incomplete send n=%d size=%d\n", n, buf.size());
//        }
        return n;
    } else 
        return 0;
}

bool NexStarAUXScope::sendCmd(AUXCommand *c){
    buffer buf;
    if (DEBUG) { fprintf(stderr,"Send: ");   c->dumpCmd(); }
    c->fillBuf(buf);
    return sendBuffer(sock, buf, 500)==buf.size();
}

void NexStarAUXScope::writeMsgs(){
    fprintf(stderr,"=================  writeMsgs  =====================\n");
//    buffer buf;
//    while (not oq.empty()) {
//        AUXCommand *m=oq.front();
//        fprintf(stderr,"SEND: ");
//        m->dumpCmd();
//        m->fillBuf(buf);
//        send(sock, buf.data(), buf.size(), 0);
//        msleep(50);
//        oq.pop();
//        delete m;
//    }
//    return;
//    /*
//    if (sock<3) return;
//    fprintf(stderr,"Sending get position\n");
//    buffer buf;
//    AUXCommand getALT(MC_GET_POSITION,APP,ALT);
//    AUXCommand getAZM(MC_GET_POSITION,APP,AZM);
//    getALT.fillBuf(buf);
//    //fprintf(stderr,"buffer size: %d\n",buf.size());
//    //dumpMsg(buf);
//    send(sock, buf.data(), buf.size(), 0);
//    getAZM.fillBuf(buf);
//    //dumpMsg(buf);
//    send(sock, buf.data(), buf.size(), 0);
//    */
}

int debug_timeout=30;

bool NexStarAUXScope::TimerTick(double dt){
    bool slewing=false;
    long da;
    int dir;
    
    querryStatus();
    //writeMsgs();
    readMsgs();
    processMsgs();
    if (DEBUG) {if (debug_timeout<0) {DEBUG=false; debug_timeout=30;} else debug_timeout--;}
    
    if (simulator) {
        // update both axis
        if (Alt!=targetAlt){
            da=targetAlt-Alt;
            dir=(da>0)?1:-1;
            Alt+=dir*std::max(std::min(long(abs(da)/2),slewRate),1L);
            slewing=true;
        } 
        if (Az!=targetAz){
            da=targetAz-Az;
            dir=(da>0)?1:-1;
            Az+=dir*std::max(std::min(long(abs(da)/2),slewRate),1L);
            slewing=true;
        }
        
        // if we reach the target at previous tick start tracking if tracking requested
        if (tracking && !slewing && Alt==targetAlt && Az==targetAz) {
            targetAlt=(Alt+=AltRate*dt);
            targetAz=(Az+=AzRate*dt);
        }
    }
    return true;
};

