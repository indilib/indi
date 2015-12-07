/*******************************************************************************
  Copyright(c) 2015 Camiel Severijns. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef SMARTFOCUS_H
#define SMARTFOCUS_H

#include "indibase/indifocuser.h"

class SmartFocus : public INDI::Focuser {
 public:
  SmartFocus(void);

  const char *getDefaultName(void);

  bool initProperties(void);
  bool updateProperties(void);

  bool Connect(void);
  bool Disconnect(void);

  virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
  //virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

  virtual bool    AbortFocuser(void);
  virtual IPState MoveAbsFocuser(uint32_t ticks);
  virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
  virtual void    TimerHit(void);

 protected:
  bool saveConfigItems(FILE *fp);

 private:
  typedef unsigned short Position;
  typedef unsigned char  Flags;

  enum State { Idle, MovingTo, MovingIn, MovingOut };

  enum StatusFlags {
    STATUS_SERIAL_FRAMING_ERROR = 0,
    STATUS_SERIAL_OVERRUN_ERROR,
    STATUS_MOTOR_ENCODE_ERROR,
    STATUS_AT_ZERO_POSITION,
    STATUS_AT_MAX_POSITION,
    STATUS_NUM_FLAGS
  };
  
  enum FlagBits {
    SerFramingError   = 0x02,
    SerOverrunError   = 0x04,
    MotorEncoderError = 0x08,
    AtZeroPosition    = 0x40,
    AtMaxPosition     = 0x80
  };

  static const Position PositionInvalid;
  static const int      TimerInterval;
  static const int      ReadTimeOut;

  static const char     goto_position;
  static const char     stop_focuser;
  static const char     read_id_register;
  static const char     read_id_respons;
  static const char     read_position;
  static const char     read_flags;
  static const char     motion_complete;
  static const char     motion_error;
  static const char     motion_stopped;

  int      serial_port;
  Position position;
  State    state;
  int      timer_id;

  bool     SFacknowledge(void);
  bool     SFisIdle     (void) const { return ( state == Idle ); }
  bool     SFisMoving   (void) const { return !SFisIdle(); }
  Position SFgetPosition(void);
  Flags    SFgetFlags   (void);
  void     SFgetState   (void);
  void     SFstartTimer (void);
  void     SFstopTimer  (void);

  bool send(const char* command,const size_t nbytes,const char* from,const bool log_error = true);
  bool recv(      char* respons,const size_t nbytes,const char* from,const bool log_error = true);
  
  ILight                FlagsL[5];
  ILightVectorProperty  FlagsLP;
  INumber               MaxPositionN[1];
  INumberVectorProperty MaxPositionNP;
};


#endif
