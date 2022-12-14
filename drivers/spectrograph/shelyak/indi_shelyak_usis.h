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


#define MAX_FRAME_LENGTH 150
#define MAX_NAME_LENGTH 25
#define MAX_VALUE_LENGTH 125

enum UsisPropType {
	_undefined = -1,
	_text = 0,
	_enum = 1,
	_number = 2,
};

struct UsisResponse {
	char buffer[MAX_FRAME_LENGTH];
	char* parts[5];
	int pcount;
};

struct UsisEnum {
	char name[MAX_NAME_LENGTH];
	int  value;
	ISwitch _val;
	UsisEnum* next;
};

#define 	ACTION_SET		1
#define 	ACTION_STOP		2
#define 	ACTION_CALIB	3

struct UsisProperty {
	UsisPropType type;
	char name[MAX_NAME_LENGTH+1];
	char title[MAX_NAME_LENGTH+1];
	uint32_t actions;

	UsisProperty* next;

	union {
		struct {
			ITextVectorProperty _vec;
			IText 	_val;
			char 	value[MAX_VALUE_LENGTH+1];
		} text;

		struct {
			INumberVectorProperty _vec;
			ISwitchVectorProperty _btn;
    		INumber _val;

			float value;		
			float minVal;
			float maxVal;
			float prec;
		} num;

		struct {
			ISwitchVectorProperty _vec;
    		char value[MAX_VALUE_LENGTH+1];
			UsisEnum* _evals;
		} enm;
	};
};


class ShelyakUsis 
	: public INDI::DefaultDevice
{
public:
    ShelyakUsis();
    ~ShelyakUsis();

    void ISGetProperties(const char *dev) override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
	bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

protected:
    const char *getDefaultName() override;

    bool initProperties() override;
    bool updateProperties() override;
	void genCatProps( const char* catName, json_value* categ );

    bool Connect() override;
    bool Disconnect() override;

private:
    int 	_serialPort; 	// file descriptor for serial port
	UsisProperty* _props;

	// 0: port
	ITextVectorProperty _text_line[1];
	IText 	_text_settings[1];
		
	/*
	INumberVectorProperty _settings_tab;
    INumber _num_settings[2];
	*/

	bool sendCmd( UsisResponse* rsp, const char* text, ... );
    bool _send( const char* text, va_list lst );
	bool _receive( UsisResponse* response );
	void scanProperties( );
	void createProperty( const char* catName, UsisProperty* prop );
	void releaseProperty( UsisProperty* prop );
};

#endif // SHELYAK_USIS_INDI_H
