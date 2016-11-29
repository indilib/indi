/*
    Pulsar 2 INDI driver

    Copyright (C) 2015/2016 Jasem Mutlaq and Camiel Severijns

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef LX200PULSAR2_H
#define LX200PULSAR2_H


#include "lx200generic.h"


class LX200Pulsar2 : public LX200Generic {
 public:  
  LX200Pulsar2(void);
  virtual ~LX200Pulsar2(void) {}
    
  virtual bool updateProperties(void);
  virtual bool initProperties  (void);
  virtual void ISGetProperties (const char *dev);
  virtual bool Connect         (void);
  virtual bool ISNewSwitch     (const char *dev, const char *name, ISState *states, char *names[], int n);
  virtual bool ISNewText       (const char *dev, const char *name, char *texts[], char *names[], int n);
 
 protected:
  virtual const char *getDefaultName(void);
    
  virtual bool Goto           (double,double);
  virtual void getBasicData   (void);
  virtual bool checkConnection(void);
  virtual bool isSlewComplete (void);
  virtual bool Sync           (double ra, double dec);
  virtual bool Park           (void);
  virtual bool UnPark         (void);
  virtual bool updateLocation (double latitude,double longitude,double elevation);
  virtual bool updateTime     (ln_date* utc,double utc_offset);

  // At which side of the pier the telescope is located: east or west
  ISwitchVectorProperty PierSideSP;
  ISwitch PierSideS[2];
  // PEC on or off
  ISwitchVectorProperty PeriodicErrorCorrectionSP;
  ISwitch PeriodicErrorCorrectionS[2];
  // Pole corssing on or off
  ISwitchVectorProperty PoleCrossingSP;
  ISwitch PoleCrossingS[2];
  // Refraction correction on or off
  ISwitchVectorProperty RefractionCorrectionSP;
  ISwitch RefractionCorrectionS[2];

 private:
  bool isHomeSet(void);
  bool isParked (void);
  bool isParking(void);
  bool isSlewing(void);
  bool startSlew(void);
  
  bool can_pulse_guide;
  bool just_started_slewing;
};

#endif
