/*
   INDI Developers Manual
   Tutorial #2

   "Simple Telescope Driver"

   We develop a simple telescope simulator.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file simplescope.h
    \brief Construct a basic INDI telescope device that simulates GOTO commands.
    \author Jasem Mutlaq

    \example simplescope.h
    A simple GOTO telescope that simulator slewing operation.
*/

#pragma once

#include "inditelescope.h"

class SimpleScope : public INDI::Telescope
{
  public:
    SimpleScope();

  protected:
    bool Handshake() override;

    const char *getDefaultName() override;
    bool initProperties() override;

    // Telescope specific functions
    bool ReadScopeStatus() override;
    bool Goto(double, double) override;
    bool Abort() override;

  private:
    double currentRA {0};
    double currentDEC {90};
    double targetRA {0};
    double targetDEC {0};

    // Debug channel to write mount logs to
    // Default INDI::Logger debugging/logging channel are Message, Warn, Error, and Debug
    // Since scope information can be _very_ verbose, we create another channel SCOPE specifically
    // for extra debug logs. This way the user can turn it on/off as desired.
    uint8_t DBG_SCOPE { INDI::Logger::DBG_IGNORE };

    // slew rate, degrees/s
    static const uint8_t SLEW_RATE = 3;
};
