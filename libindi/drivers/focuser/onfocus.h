/*
 *  OnFocus
 *  James Lancaster https://github.com/james-lan
 *  portions based on LX200 OnStep by azwing (alain@zwingelstein.org)
 *  Contributors:
 *  James Lan https://github.com/james-lan
 *  Ray Wells https://github.com/blueshawk
 * 
 *  and LX200 Driver by Jasem Mutlaq (mutlaqja@ikarustech.com)*
 *  
 *  Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)
 *  Copyright (C) 2018 azwing (alain@zwingelstein.org)
 *  Copyright (C) 2018 James Lancaster https://github.com/james-lan
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 * 
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 * 
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 *  ===========================================
 *  Version 1.0:
 *  	Initial Release. Not hardware tested.
 * 
 */


#pragma once
#include "indifocuser.h"


#define RB_MAX_LEN 64
#define ONFOCUS_TIMEOUT 3
#define OnFocus_H



class OnFocus : public INDI::Focuser
{
  public:
	OnFocus();
	~OnFocus() {}
	
	virtual const char *getDefaultName() override;
	virtual bool initProperties() override;
	virtual void ISGetProperties(const char *dev) override;
	virtual bool updateProperties() override;
	virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
	virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
	
protected:

	
	
	//FocuserInterface
	
	IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration) override;
	IPState MoveAbsFocuser (uint32_t targetTicks) override;
	IPState MoveRelFocuser (FocusDirection dir, uint32_t ticks) override;
	bool AbortFocuser () override;
	
	
	//End FocuserInterface

	bool sendOnStepCommand(const char *cmd);
	bool sendOnStepCommandBlind(const char *cmd);
	
	void OSUpdateFocuser();
	
	// Focuser controls
	// Focuser 1
	bool OSFocuser1=false;
	ISwitchVectorProperty OSFocus1InitializeSP;
	ISwitch OSFocus1InitializeS[4];
	
private:
	bool Ack();

};


//From LX200Driver
int getCommandString(int fd, char *data, const char *cmd);
