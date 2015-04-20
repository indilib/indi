#ifndef NEXSTARAUX_H
#define NEXSTARAUX_H

#include <netinet/in.h>
#include <queue>

typedef std::vector<unsigned char> buffer;

enum AUXCommands {
    MC_GET_POSITION=0x01,
    MC_GOTO_FAST=0x02,
    MC_SET_POSITION=0x04,
    MC_SET_POS_GUIDERATE=0x06,
    MC_SET_NEG_GUIDERATE=0x07,
    MC_LEVEL_START=0x0b,
    MC_SLEW_DONE=0x13,
    MC_GOTO_SLOW=0x17,
    MC_SEEK_INDEX=0x19,
    MC_MOVE_POS=0x24,
    MC_MOVE_NEG=0x25,
    GET_VER=0xfe
};

enum AUXtargets {
    ANY=0x00,
    MB=0x01,
    HC=0x04,
    HCP=0x0d,
    AZM=0x10,
    ALT=0x11,
    APP=0x20,
    GPS=0xb0,
    WiFi=0xb5,
    BAT=0xb6,
    CHG=0xb7,
    LIGHT=0xbf
};


class AUXCommand{
public:
    AUXCommand(buffer buf);
    AUXCommand(AUXCommands c, AUXtargets s, AUXtargets d, buffer dat);
    AUXCommand(AUXCommands c, AUXtargets s, AUXtargets d);
    
    void fillBuf(buffer &buf);
    void parseBuf(buffer buf);
    long getPosition();
    void setPosition(long p);
    unsigned char checksum(buffer buf);
    void dumpCmd();
    
    AUXCommands cmd;
    AUXtargets src, dst;
    int len;
    buffer data;
    bool valid;
};


class NexStarAUXScope {

public:
    NexStarAUXScope(char const *ip, int port);
    NexStarAUXScope(int port);
    NexStarAUXScope(char const *ip);
    NexStarAUXScope();
    ~NexStarAUXScope();
    bool Abort();
    long GetALT();
    long GetAZ();
    bool slewing();
    bool GoToFast(long alt, long az, bool track);
    bool GoToSlow(long alt, long az, bool track);
    bool Track(long altRate, long azRate);
    bool TimerTick(double dt);
    bool Connect();
    bool Disconnect();
    
private:
    static const long STEPS_PER_REVOLUTION = 16777216; 
    void initScope(char const *ip, int port);
    void initScope();
    void closeConnection();
    void readMsgs();
    void processMsgs();
    void writeMsgs();
    void querryStatus();
    bool sendCmd(AUXCommand *c);
    long Alt;
    long Az;
    long AltRate;
    long AzRate;
    long targetAlt;
    long targetAz;
    long slewRate;
    bool tracking;
    bool slewingAlt, slewingAz;
    int sock;
    struct sockaddr_in addr;
    bool simulator=false;
    std::queue<AUXCommand *> iq, oq;
};


#endif // NEXSTARAUX_H
