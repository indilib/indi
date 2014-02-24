#if 0
  This file is part of the AAG Cloud Watcher INDI Driver.
  A driver for the AAG Cloud Watcher (AAGware - http://www.aagware.eu/)

  Copyright (C) 2012 Sergio Alonso (zerjioi@ugr.es)

  

  AAG Cloud Watcher INDI Driver is free software: you can redistribute it 
  and/or modify it under the terms of the GNU General Public License as 
  published by the Free Software Foundation, either version 3 of the License, 
  or (at your option) any later version.

  AAG Cloud Watcher INDI Driver is distributed in the hope that it will be 
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with AAG Cloud Watcher INDI Driver.  If not, see 
  <http://www.gnu.org/licenses/>.          
#endif
  
  
#ifndef INDI_AAGCLOUDWATCHER_H
#define	INDI_AAGCLOUDWATCHER_H


#include <cstdlib>
#include <memory>
#include <sys/stat.h>
#include <defaultdevice.h>
#include <basedevice.h>
#include "CloudWatcherController.h"

  
enum HeatingAlgorithmStatus { normal, increasingToPulse, pulse };



    
class AAGCloudWatcher : public INDI::DefaultDevice {
  public:
    AAGCloudWatcher();
    ~AAGCloudWatcher();

    virtual void ISGetProperties (const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    const char *getDefaultName();
    bool isConnected();
    bool sendData();
    int getRefreshPeriod();
    float getLastReadPeriod();
    bool heatingAlgorithm();
    
  private:
    const static float ABS_ZERO = 273.15;
    float lastReadPeriod;
    CloudWatcherConstants constants;
    CloudWatcherController *cwc;
    
    virtual bool initProperties();
    virtual bool Connect();
    virtual bool Disconnect();
    bool sendConstants();
    bool resetConstants();
    bool resetData();
    double getNumberValueFromVector(INumberVectorProperty *nvp, const char *name);
    bool isWetRain();
    

    
    HeatingAlgorithmStatus heatingStatus;
    
    time_t pulseStartTime;
    time_t wetStartTime;
    
    float desiredSensorTemperature;
    float globalRainSensorHeater;  
    
};

/**
 * Used to read periodically the data from the device.
 * @param p the AAGCloudWatcher object
 */
void ISPoll(void *p);

/**
 *  Send client definitions of all properties.
 */
void ISInit();

void ISGetProperties (const char *dev);

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);

void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

void ISSnoopDevice (XMLEle *root);

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);


#endif	/* INDI_AAGCLOUDWATCHER_H */