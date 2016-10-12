/*
 ASI Filter Wheel INDI Driver

 Copyright (c) 2016 by Rumen G.Bogdanovski and Hans Lambermont.
 All Rights Reserved.

 Code is based on SX Filter Wheel INDI Driver by Gerry Rozema
 Copyright(c) 2010 Gerry Rozema.
 All rights reserved.

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#include <stdio.h>
#include <memory>
#include <unistd.h>

#include "config.h"
#include "EFW_filter.h"
#include "asi_wheel.h"

std::unique_ptr<ASIWHEEL> asiwheel(new ASIWHEEL());

void ISGetProperties(const char *dev) {
	asiwheel->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num) {
	asiwheel->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num) {
	asiwheel->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num) {
	asiwheel->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) {
	INDI_UNUSED(dev);
	INDI_UNUSED(name);
	INDI_UNUSED(sizes);
	INDI_UNUSED(blobsizes);
	INDI_UNUSED(blobs);
	INDI_UNUSED(formats);
	INDI_UNUSED(names);
	INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root) {
	asiwheel->ISSnoopDevice(root);
}

ASIWHEEL::ASIWHEEL() {
	DEBUG(INDI::Logger::DBG_DEBUG, "Entering ASIWHEEL::ASIWHEEL");
	FilterSlotN[0].min = 1;
	FilterSlotN[0].max = -1;
	CurrentFilter = 1;
	fw_id = -1;
	setDeviceName(getDefaultName());
	setVersion(ASI_VERSION_MAJOR, ASI_VERSION_MINOR);
}

ASIWHEEL::~ASIWHEEL() {
	DEBUG(INDI::Logger::DBG_DEBUG, "Entering ASIWHEEL::~ASIWHEEL");
	if (isSimulation()) {
		IDMessage(getDeviceName(), "simulation: disconnected");
	} else { 
		Disconnect();
	}
}

void ASIWHEEL::debugTriggered(bool enable) {
	INDI_UNUSED(enable);
}

void ASIWHEEL::simulationTriggered(bool enable) {
	INDI_UNUSED(enable);
}

const char * ASIWHEEL::getDefaultName() {
	return (char *) "ASI Wheel";
}

bool ASIWHEEL::GetFilterNames(const char* groupName) {
	char filterName[MAXINDINAME];
	char filterLabel[MAXINDILABEL];
	int MaxFilter = FilterSlotN[0].max;
	if (FilterNameT != NULL) delete FilterNameT;
	FilterNameT = new IText[MaxFilter];
	for (int i=0; i < MaxFilter; i++) {
		snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i+1);
		snprintf(filterLabel, MAXINDILABEL, "Filter #%d", i+1);
		IUFillText(&FilterNameT[i], filterName, filterLabel, filterLabel);
	}
	IUFillTextVector(FilterNameTP, FilterNameT, MaxFilter, getDeviceName(), "FILTER_NAME", "Filter", groupName, IP_RW, 0, IPS_IDLE);
	return true;
}

bool ASIWHEEL::Connect() {
	DEBUG(INDI::Logger::DBG_SESSION, "Entering ASIWHEEL::Connect");
	if (isSimulation()) {
		IDMessage(getDeviceName(), "simulation: connected");
		fw_id = 0;
	} else if (fw_id<0) {
		int EFW_count = EFWGetNum();
		if (EFW_count <= 0) {
			DEBUG(INDI::Logger::DBG_SESSION, "ASIWHEEL::Connect no filter wheels found");
			return false;
		}
		DEBUG(INDI::Logger::DBG_DEBUG, "Entering ASIWHEEL::Connect EFWOpen");
		if (EFWOpen(0) != EFW_SUCCESS) {
			DEBUG(INDI::Logger::DBG_SESSION, "ASIWHEEL::Connect cannot EFWOpen");
			return false;
		}
		DEBUG(INDI::Logger::DBG_DEBUG, "Entering ASIWHEEL::Connect EFWGetID");
		EFWGetID(0, &fw_id);
		if (fw_id < 0) {
			DEBUG(INDI::Logger::DBG_SESSION, "ASIWHEEL::Connect cannot EFWGetID");
			return false;
		}
		DEBUGF(INDI::Logger::DBG_SESSION, "ASIWHEEL::Connect id %d", fw_id);

		// get number of filter slots
		DEBUG(INDI::Logger::DBG_DEBUG, "Entering ASIWHEEL::Connect EFWGetProperty");
		EFW_INFO EFWInfo;
		EFW_ERROR_CODE err;
		while (1) {
			err = EFWGetProperty(fw_id, &EFWInfo);
			if (err != EFW_ERROR_MOVING )
				break;
			usleep(500);
		}
		DEBUGF(INDI::Logger::DBG_SESSION, "ASIWHEEL::Connect %d filter slots", EFWInfo.slotNum);
		FilterSlotN[0].max = EFWInfo.slotNum;

		// get current filter
		DEBUG(INDI::Logger::DBG_DEBUG, "Entering ASIWHEEL::Connect EFWGetPosition");
		int currentSlot;
		while (1) {
			err = EFWGetPosition(fw_id, &currentSlot);
			if(err != EFW_SUCCESS || currentSlot != -1 )
				break;
			usleep(500);
		}
		CurrentFilter = currentSlot; // Note: not + 1
		DEBUGF(INDI::Logger::DBG_SESSION, "ASIWHEEL::Connect current filter position %d", CurrentFilter);
	}
	return true;
}

bool ASIWHEEL::Disconnect() {
	DEBUG(INDI::Logger::DBG_SESSION, "Entering ASIWHEEL::Disconnect\n");
	if (isSimulation()) {
		IDMessage(getDeviceName(), "simulation: disconnected");
	} else if(fw_id >= 0) {
		// MORE TODO!!!
		EFWClose(fw_id);
	}
	fw_id = -1;
	return true;
}

bool ASIWHEEL::initProperties() {
	INDI::FilterWheel::initProperties();
	addDebugControl();
	addSimulationControl();
	return true;
}

void ASIWHEEL::ISGetProperties(const char *dev) {
	INDI::FilterWheel::ISGetProperties(dev);
	return;
}

int ASIWHEEL::QueryFilter() {
	DEBUG(INDI::Logger::DBG_DEBUG, "Entering ASIWHEEL::QueryFilter");
	if (!isSimulation() && fw_id >= 0)
		// MORE TODO!!!
		EFWGetPosition(fw_id, &CurrentFilter);
	return CurrentFilter;
}

bool ASIWHEEL::SelectFilter(int f) {
	DEBUGF(INDI::Logger::DBG_SESSION,"Entering ASIWHEEL::SelectFilter target %d", f);
	TargetFilter = f;
	if (isSimulation()) {
		CurrentFilter = TargetFilter;
	} else if (fw_id >= 0) {
		EFW_ERROR_CODE err;
		err = EFWSetPosition(fw_id, f);
		if (err == EFW_SUCCESS) {
			DEBUG(INDI::Logger::DBG_DEBUG,"ASIWHEEL::SelectFilter Moving...");
			SetTimer(3500);
			while (1) {
				err = EFWGetPosition(fw_id, &CurrentFilter);
				if (err != EFW_SUCCESS || CurrentFilter != -1)
					break;
				DEBUG(INDI::Logger::DBG_DEBUG,"ASIWHEEL::SelectFilter Still Moving");
				sleep(1);
			}
			DEBUGF(INDI::Logger::DBG_SESSION, "ASIWHEEL::SelectFilter Done moving to %d", CurrentFilter);
			// Note: no need to set CurrentFilter = TargetFilter as EFWGetPosition already did that
		} else {
			DEBUG(INDI::Logger::DBG_SESSION,"ASIWHEEL::SelectFilter EFWSetPosition error");
			return false;
		}
	} else {
		DEBUG(INDI::Logger::DBG_SESSION,"ASIWHEEL::SelectFilter no fw_id");
		return false;
	}
	return true;
}

void ASIWHEEL::TimerHit() {
	DEBUG(INDI::Logger::DBG_DEBUG, "Entering ASIWHEEL::TimerHit");
	QueryFilter();
	if (CurrentFilter != TargetFilter) {
		SetTimer(3500);
	} else {
		SelectFilterDone(CurrentFilter);
	}
}

