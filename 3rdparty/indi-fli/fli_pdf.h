#ifndef FLI_PDF_H
#define FLI_PDF_H

#if 0
    FLI PDF
    INDI Interface for Finger Lakes Instrument Focusers
    Copyright (C) 2003-2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#endif

#include <libfli.h>
#include <indifocuser.h>
#include <iostream>

using namespace std;


class FLIPDF : public INDI::Focuser
{
public:

    FLIPDF();
    virtual ~FLIPDF();

    const char *getDefaultName();

    bool initProperties();
    void ISGetProperties(const char *dev);
    bool updateProperties();

    bool Connect();
    bool Disconnect();

    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);

    protected:

    virtual bool Move(FocusDirection dir, int speed, int duration);
    virtual int MoveAbs(int ticks);
    virtual int MoveRel(FocusDirection dir, unsigned int ticks);
    void TimerHit();

    private:

    typedef struct
    {
        flidomain_t domain;
        char *dname;
        char *name;
        char model[200];
        long HWRevision;
        long FWRevision;
        long current_pos;
        long steps_remaing;
        long max_pos;
        long home;
    } focuser_t;

    ISwitch PortS[4];
    ISwitchVectorProperty PortSP;

    ISwitch HomeS[1];
    ISwitchVectorProperty HomeSP;

    IText FocusInfoT[3];
    ITextVectorProperty FocusInfoTP;

    int timerID;
    int StepRequest;

    bool InStep;
    bool sim;

    flidev_t fli_dev;
    focuser_t FLIFocus;

    bool findFLIPDF(flidomain_t domain);
    bool setupParams();
    void goHomePosition();



};

#endif // FLI_PDF_H
