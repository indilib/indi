/*******************************************************************************
  Copyright(c) 2016 Gerry Rozema. All rights reserved.

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

#ifndef TEMMAMOUNT_H
#define TEMMAMOUNT_H

#include "indibase/indiguiderinterface.h"
#include "indibase/inditelescope.h"
#include "indicontroller.h"
#include "indibase/alignment/AlignmentSubsystemForDrivers.h"


#define TEMMA_SLEW_RATES 4

class TemmaMount : public INDI::Telescope, public INDI::GuiderInterface, public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers
{
    private:
	double currentRA;
	double currentDEC;
	int TemmaRead(char *buf,int size);
	bool GetTemmaVersion();
	bool GetTemmaMotorStatus();
	bool SetTemmaMotorStatus(bool);
	bool SetTemmaLst();
	int GetTemmaLst();
	bool SetTemmaLattitude(double);
	double GetTemmaLattitude();

	bool TemmaSync(double,double);

	ln_equ_posn TelescopeToSky(double ra,double dec);
	ln_equ_posn SkyToTelescope(double ra,double dec);

	bool MotorStatus;
	bool GotoInProgress;
	bool ParkInProgress;
	bool TemmaInitialized;
	double Longitude;
	double Lattitude;
	int SlewRate;
	bool SlewActive;
	unsigned char Slewbits;
	//bool TemmaConnect(const char *port);
    INumber GuideRateN[2];
    INumberVectorProperty GuideRateNP;

    public:
		TemmaMount();
        virtual ~TemmaMount();

        //  overrides of base class virtual functions
	//  we need to override the connect function because temma wants even parity
	//  and the default function sets no parity on the serial port
	bool Connect(const char *port, uint32_t baud);

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
	//bool ReadTime();
	//bool ReadLocation();
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
	//  methods added for guider interface
    virtual IPState GuideNorth(float ms);
    virtual IPState GuideSouth(float ms);
    virtual IPState GuideEast(float ms);
    virtual IPState GuideWest(float ms);
	//  Initial implementation doesn't need this one
    //virtual void GuideComplete(INDI_EQ_AXIS axis);

};

#endif // TEMMAMOUNT_H
