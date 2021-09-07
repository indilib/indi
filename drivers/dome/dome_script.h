/*******************************************************************************
 Copyright(c) 2016 CloudMakers, s. r. o.. All rights reserved.

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

#pragma once

#include "indidome.h"

class DomeScript : public INDI::Dome
{
  public:
    DomeScript();
    virtual ~DomeScript() = default;

    virtual const char *getDefaultName() override;
    virtual bool initProperties() override;
    virtual bool saveConfigItems(FILE *fp) override;

    void ISGetProperties(const char *dev) override;
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    bool updateProperties() override;

  protected:
    void TimerHit() override;
    virtual bool Connect() override;
    virtual bool Disconnect() override;
    virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
    virtual IPState MoveAbs(double az) override;
    virtual IPState Park() override;
    virtual IPState UnPark() override;
    virtual IPState ControlShutter(ShutterOperation operation) override;
    virtual bool Abort() override;

  private:
    bool ReadDomeStatus();
    bool RunScript(int script, ...);

    ITextVectorProperty ScriptsTP;
    IText ScriptsT[15] {};
    double TargetAz { 0 };
    int TimeSinceUpdate { 0 };
};
