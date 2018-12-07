//
// Created by not7cd on 06/12/18.
//

#pragma once

#include <inditelescope.h>

class Starbook : public INDI::DefaultDevice
{
public:
    Starbook();

protected:
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();

};
