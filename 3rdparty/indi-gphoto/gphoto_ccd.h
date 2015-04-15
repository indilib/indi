/*
    Driver type: GPhoto Camera INDI Driver

    Copyright (C) 2009 Geoffrey Hausheer
    Copyright (C) 2013 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
    License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

*/

#ifndef GPHOTO_CCD_H
#define GPHOTO_CCD_H

#include <indiccd.h>
#include <indifocuserinterface.h>

#include <iostream>
#include <map>
#include <string>
#include <cstring>

#define	MAXEXPERR	10		/* max err in exp time we allow, secs */
#define	OPENDT		5		/* open retry delay, secs */

using namespace std;

enum { ON_S, OFF_S };

typedef struct
{
    gphoto_widget *widget;
    union
    {
        INumber	num;
        ISwitch	*sw;
        IText	text;
    } item;
    union
    {
        INumberVectorProperty	num;
        ISwitchVectorProperty	sw;
        ITextVectorProperty	text;
    } prop;
} cam_opt;

class GPhotoCCD: public INDI::CCD, public INDI::FocuserInterface
{
public:
    GPhotoCCD();
    GPhotoCCD(const char* device_name);
    virtual	~GPhotoCCD();

    const char *getDefaultName();

    bool initProperties();
    void ISGetProperties(const char *dev);
    bool updateProperties();

    bool Connect();
    bool Disconnect();

    bool StartExposure(float duration);
    bool UpdateCCDFrame(int x, int y, int w, int h);

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);    

    static void ExposureUpdate(void *vp);
    void ExposureUpdate();

    static void UpdateExtendedOptions(void *vp);
    void UpdateExtendedOptions(bool force = false);

protected:

    // Misc.
    bool saveConfigItems(FILE *fp);
    void addFITSKeywords(fitsfile *fptr, CCDChip *targetChip);
    void TimerHit();
    virtual void debugTriggered(bool enable);

    // Focusing
    bool SetFocuserSpeed(int speed);
    IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);

    // Preview
    bool capturePreview();

private:
    ISwitch *create_switch(const char *basestr, const char **options, int max_opts, int setidx);
    void AddWidget(gphoto_widget *widget);
    void UpdateWidget(cam_opt *opt);
    void ShowExtendedOptions(void);
    void HideExtendedOptions(void);

    float CalcTimeLeft();
    bool grabImage();

    char name[MAXINDINAME];
    struct timeval ExpStart;
    float ExposureRequest;
    bool sim;

    gphoto_driver *gphotodrv;
    map<string, cam_opt *> CamOptions;
    int expTID;			/* exposure callback timer id, if any */
    int optTID;			/* callback for exposure timer id */
    int focusSpeed;

    char *on_off[2];
    int timerID;
    bool frameInitialized;

    ISwitch mConnectS[2];
    ISwitchVectorProperty mConnectSP;
    IText mPortT[1];
    ITextVectorProperty PortTP;

    INumber mExposureN[1];
    INumberVectorProperty mExposureNP;

    ISwitch *mIsoS;
    ISwitchVectorProperty mIsoSP;
    ISwitch *mFormatS;
    ISwitchVectorProperty mFormatSP;

    ISwitch transferFormatS[2];
    ISwitchVectorProperty transferFormatSP;

    ISwitch autoFocusS[1];
    ISwitchVectorProperty autoFocusSP;

    ISwitch livePreviewS[2];
    ISwitchVectorProperty livePreviewSP;

    IBLOBVectorProperty *imageBP;
    IBLOB *imageB;

    friend void ::ISSnoopDevice(XMLEle *root);
    friend void ::ISGetProperties(const char *dev);
    friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
    friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
    friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
    friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
};

#endif
