/*
    OpenAstroTech
    Copyright (C) 2021 Anjo Krank

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

#pragma once

#include "lx200gps.h"

#define MAJOR_VERSION 0
#define MINOR_VERSION 9
 
class LX200_OpenAstroTech : public LX200GPS
{
  public:
    LX200_OpenAstroTech(void);

    virtual bool Handshake() override;
    virtual const char *getDefaultName(void) override;
    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

  protected:
//    virtual void getBasicData(void) override;

  private:
    virtual int executeMeadeCommand(char *cmd);

  private:
    IText MeadeCommandT;
    ITextVectorProperty MeadeCommandTP;
    char MeadeCommandResult[1024];
};

