/*******************************************************************************
  Copyright(c) 2017 Simon Holmbo. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#ifndef SHELYAK_USIS_INDI_H
#define SHELYAK_USIS_INDI_H

#include "defaultdevice.h"
#include "json.h"

using json = nlohmann::json;

#define MAX_FRAME_LENGTH 150
#define MAX_NAME_LENGTH 25
#define MAX_VALUE_LENGTH 125


struct UsisResponse {
	char buffer[MAX_FRAME_LENGTH];
	char* parts[5];
	int pcount;
};

#define 	ACTION_STOP		1
#define 	ACTION_CALIB	2

#define 	MAX_ACTION		8
#define 	MAX_ENUMS		8

struct Action;

struct TextValue {
	ITextVectorProperty _vec;
	IText 	_val;
};

struct EnumValue {
	ISwitchVectorProperty _vec;
	ISwitch _vals[MAX_ENUMS];
};

struct NumValue {
	INumberVectorProperty _vec;
	INumber _val;
	EnumValue _act;
};

struct EnumItem {
	Action* parent;
	int index;
	std::string val;
};

struct CmdItem {
	Action* parent;
	std::string cmd;
	std::string name;
};

enum PropType {
	_text 	= 0x10,
	_enum 	= 0x11,
	_number = 0x12,

	_eitm 	= 0x01,
	_ecmd 	= 0x02,
};

class Action {
public:
	char  uid[8];
	std::string  name;
	
	PropType type;
	union {
		TextValue text;
		NumValue  num;
		EnumValue enm;
		EnumItem itm;		
		CmdItem  cmd;
	};

public:
	Action( uint32_t _uid, const std::string& _cmd, PropType _type ) {
		sprintf( uid, "%04x", _uid );
		name = _cmd;
		type = _type;
	}
};






class ShelyakDriver 
	: public INDI::DefaultDevice
{
public:
    ShelyakDriver();
    ~ShelyakDriver();

    void ISGetProperties(const char *dev) override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
	bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

	void update( );
	static void __update( void* ptr );

    const char *getDefaultName() override;

private:
	void genCatProps( const char* catName, json& categ );

    bool initProperties() override;
    bool updateProperties() override;
	void clearProperties( );
	
    bool Connect() override;
    bool Disconnect() override;

private:
    int 	_serialPort; 	// file descriptor for serial port
	json    _config;		// json configuration

	uint32_t _guid;
	std::vector<Action*> _actions;

	// 0: port
	ITextVectorProperty _text_line[1];
	IText 	_text_settings[1];
		
	bool sendCmd( UsisResponse* rsp, const char* text, ... );
    bool _send( const char* text, va_list lst );
	bool _receive( UsisResponse* response );
	void scanProperties( );

	bool readConfig( );
	bool findBoard( const char* boardName, json* board_def );

	Action* createAction( PropType type, const std::string& command );
};

#endif // SHELYAK_USIS_INDI_H
