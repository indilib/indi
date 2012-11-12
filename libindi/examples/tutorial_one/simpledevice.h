#ifndef SIMPLEDEVICE_H
#define SIMPLEDEVICE_H

/*
   INDI Developers Manual
   Tutorial #1

   "Hello INDI"

   We construct a most basic (and useless) device driver to illustate INDI.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file tutorial_one.c
    \brief Construct a basic INDI driver with only one property.
    \author Jasem Mutlaq
*/

#include "indibase/defaultdevice.h"

class SimpleDevice : public INDI::DefaultDevice
{
public:
    SimpleDevice();

protected:
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();

};

#endif // SIMPLEDEVICE_H
