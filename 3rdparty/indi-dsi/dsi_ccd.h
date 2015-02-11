/**
 * INDI driver for Meade DSI.
 *
 * Copyright (C) 2015 Ben Gilsrud
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef DSICCD_H
#define DSICCD_H

#include <indiccd.h>

using namespace std;

namespace DSI {
    class Device;
}

class DSICCD : public INDI::CCD
{
public:
    DSICCD();

    bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);

protected:
    // General device functions
    bool Connect();
    bool Disconnect();
    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();

    // CCD specific functions
    bool StartExposure(float duration);
    bool AbortExposure();
    void TimerHit();

private:
    // Utility functions
    float CalcTimeLeft();
    void  setupParams();
    void  grabImage();

    // Are we exposing?
    bool InExposure;
    bool capturing;
    // Struct to keep timing
    struct timeval ExpStart;

    float ExposureRequest;
    float TemperatureRequest;
    int   timerID;
    float max_exposure;
    float last_exposure_length;
    int sub_count;

    INumber GainN[1];
    INumberVectorProperty GainNP;

    DSI::Device *dsi;
};

#endif /* DSICCD_H */
