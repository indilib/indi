// celestronauxpacket.h

#pragma once

#include <vector>

typedef std::vector<unsigned char> buffer;

enum AuxCommand
{
	// motor commands
	MC_GET_POSITION = 0x01,    // return 24 bit position
	MC_GOTO_FAST = 0x02,    // send 24 bit target
	MC_SET_POSITION = 0x04,    // send 24 bit new position
	MC_SET_POS_GUIDERATE = 0x06,
	MC_SET_NEG_GUIDERATE = 0x07,
	MC_LEVEL_START = 0x0b,
	MC_SET_POS_BACKLASH = 0x10,    // 1 byte, 0-99
	MC_SET_NEG_BACKLASH = 0x11,    // 1 byte, 0-99
	MC_SLEW_DONE = 0x13,    // return 0xFF when move finished
	MC_GOTO_SLOW = 0x17,    // send 24 bit target
	MC_SEEK_INDEX = 0x19,
	MC_MOVE_POS = 0x24,    // send move rate 0-9
	MC_MOVE_NEG = 0x25,    // send move rate 0-9
	MC_GET_POS_BACKLASH = 0x40,    // 1 byte, 0-99
	MC_GET_NEG_BACKLASH = 0x41,    // 1 byte, 0-99

	// common to all devices (maybe)
	GET_VER = 0xfe,    // return 2 or 4 bytes major.minor.build

	// GPS device commands
	//GPS_GET_LAT = 0x01,
	//GPS_GET_LONG = 0x02,
	//GPS_GET_DATE = 0x03,
	//GPS_GET_YEAR = 0x04,
	//GPS_GET_TIME = 0x33,
	//GPS_TIME_VALID = 0x36,
	//GPS_LINKED = 0x37,

	// focuser commands
	FOC_CALIB_ENABLE = 42,      // send 0 to start or 1 to stop
	FOC_CALIB_DONE = 43,      // returns 2 bytes [0] done, [1] state 0-12
    FOC_GET_HS_POSITIONS = 44		// returns 2 ints low and high limits
};

enum AuxTarget
{
	ANY = 0x00,
	MB = 0x01,
	HC = 0x04,
	HCP = 0x0d,
	AZM = 0x10,       // azimuth|hour angle axis motor
	ALT = 0x11,       // altitude|declination axis motor
	FOCUSER = 0x12,       // focuser motor
	APP = 0x20,
	NEX_REMOTE = 0x22,
	GPS = 0xb0,
	WiFi = 0xb5,
	BAT = 0xb6,
	CHG = 0xb7,
	LIGHT = 0xbf
};
	
class AuxPacket
{

public:
    AuxPacket();

    AuxPacket(AuxTarget source, AuxTarget destination, AuxCommand command, buffer data);
    AuxPacket(AuxTarget source, AuxTarget destination, AuxCommand command);

    // packet contents
    static const unsigned char AUX_HDR = 0x3b;
    int length;
    AuxTarget source;
    AuxTarget destination;
    AuxCommand command;
    buffer data;

    void FillBuffer(buffer & buf);

    bool Parse (buffer buf);

private: 
	unsigned char checksum(buffer data);
};

class AuxCommunicator
{
public:
	AuxCommunicator();
    AuxCommunicator(AuxTarget source);
    // send command with data and reply
    bool sendCommand(int port, AuxTarget dest, AuxCommand cmd, buffer data, buffer & reply);
    // send command with reply but no data
    bool sendCommand(int port, AuxTarget dest, AuxCommand cmd, buffer& reply);
	// send command with data but no reply
	bool commandBlind(int port, AuxTarget dest, AuxCommand cmd, buffer data);
	
    AuxTarget source;

private:
	bool sendPacket(int port, AuxTarget dest, AuxCommand cmd, buffer data);
    bool readPacket(int port, AuxPacket& reply);
};

	
