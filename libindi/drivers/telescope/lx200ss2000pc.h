/*
    SkySensor2000 PC
    Copyright (C) 2015 Camiel Severijns

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

#ifndef LX200SS2000PC_H
#define LX200SS2000PC_H

#include "lx200generic.h"

class LX200SS2000PC : public LX200Generic {
 public:
  LX200SS2000PC(void);
  
  virtual const char* getDefaultName(void);
  virtual bool updateProperties(void);
  virtual bool updateTime      (ln_date * utc, double utc_offset);

 protected:
  virtual void getBasicData  (void);
  virtual bool isSlewComplete(void);
  
 private:
  bool getCalenderDate(int& year,int& month,int& day);
  bool setCalenderDate(int  year,int  month,int  day);
  bool setUTCOffset   (const int offset_in_hours);

  static const int ShortTimeOut;
  static const int LongTimeOut;
};


#endif
 
