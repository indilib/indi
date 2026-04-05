/*******************************************************************************
 Copyright(c) 2014 Jasem Mutlaq. All rights reserved.

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

#pragma once

#include "indidome.h"

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

class HakosRoof : public INDI::Dome
{
  public:
    HakosRoof();
    virtual ~HakosRoof() = default;

    const char *getDefaultName() override;
    virtual bool initProperties() override;
    bool updateProperties() override;

    virtual void ISGetProperties(const char *dev) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool saveConfigItems(FILE *fp) override;
    
  protected:
    bool Connect() override;
    void TimerHit() override;

    virtual IPState Move(DomeDirection dir, DomeMotionCommand operation) override;
    virtual IPState Park() override;
    virtual IPState UnPark() override;
    virtual bool Abort() override;

    virtual bool getMountSensorSwitch();
    bool AbortDriver();

  private:
    float CalcTimeLeft(timeval);

    double MotionRequest { 0 };
    struct timeval MotionStart { 0, 0 };

    // Source file/url
    INDI::PropertyText DataSourceTP {1};

    // Roof token
    INDI::PropertyText RoofTokenTP {1};

    // Roof status (5 text elements)
    INDI::PropertyText RoofDataTP {5};
    
    // Encoder Max Ticks
    INDI::PropertyNumber EncoderTicksNP {1};
    
    // Simulation Mode
    INDI::PropertySwitch SimulationSP {1};
    
    bool sendRoofCommand(const char *action);
    bool simulateRoofCommand(const char *action);
    
    //static size_t write_data;
    std::string readBuffer;
    int simulatedPosition { 0 };
    bool isSimulated { false };
    struct timeval simulationStartTime { 0, 0 };
    int simulationTargetPosition { 0 };
    bool isMoving { false };
};
