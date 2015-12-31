/*******************************************************************************
  Copyright(c) 2010 Gerry Rozema. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef SYNSCANMOUNT_H
#define SYNSCANMOUNT_H

#include "indibase/inditelescope.h"
#include "indibase/alignment/AlignmentSubsystemForDrivers.h"


#define SYNSCAN_SLEW_RATES 9

class SynscanMount : public INDI::Telescope, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
    private:
 	double FirmwareVersion;
	int MountModel;
	char LastParkRead[20];
	int NumPark;
	int StopCount;
        int SlewRate;
	int PassthruCommand(int cmd,int target,int msgsize,int data,int numReturn);
	double currentRA;
	double currentDEC;

	bool CanSetLocation;
	bool ReadLatLong;
	bool HasFailed;
	bool FirstConnect;
	//int NumSyncPoints;
      	bool AnalyzeHandset();

    public:
        SynscanMount();
        virtual ~SynscanMount();

        //  overrides of base class virtual functions
        //bool initProperties();
        virtual void ISGetProperties (const char *dev);
    	virtual bool updateProperties();
        virtual const char *getDefaultName();

	virtual bool initProperties();
        virtual bool ReadScopeStatus();
	virtual bool Connect();
        bool Goto(double,double);
        bool Park();
	bool UnPark();
        bool Abort();        
        bool SetSlewRate(int);
	bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command);
	bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command);
	bool ReadTime();
	bool ReadLocation();
       	bool updateLocation(double latitude, double longitude, double elevation);        
        bool updateTime(ln_date *utc, double utc_offset);
	void SetCurrentPark();
	void SetDefaultPark();

	//  methods added for alignment subsystem
	virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
	virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
	virtual bool ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
	virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
	bool Sync(double ra, double dec);
};

#endif // SYNSCANMOUNT_H
