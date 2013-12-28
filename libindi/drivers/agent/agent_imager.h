/*******************************************************************************
  Copyright(c) 2013 CloudMakers, s. r. o. All rights reserved.

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

#ifndef AGENT_IMAGER_H
#define AGENT_IMAGER_H

#include <fitsio.h>
#include <string.h>

#include "defaultdevice.h"

#define MAX_GROUP_COUNT 16

class Group {
  private:
    int id;
    char groupName[16];
    char groupSettingsName[32];
    
  public:
    Group(int id);
    INumberVectorProperty *GroupSettingsNP;
    INumber GroupSettingsN[4];

    bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    
    void defineProperties();
    void deleteProperties();
    void makeReadOnly();
    void makeReadWrite();
};

class Imager : public INDI::DefaultDevice {
  public:
    Imager();
    virtual ~Imager();

    virtual bool initProperties();
    virtual bool updateProperties();
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
    virtual bool ISSnoopDevice (XMLEle *root);

  protected:
    virtual const char *getDefaultName();
    virtual bool Connect();
    virtual bool Disconnect();
    
  private:
    ITextVectorProperty *ActiveDeviceTP;
    IText ActiveDeviceT[2];
    INumberVectorProperty *GroupCountNP;
    INumber GroupCountN[1];
    INumberVectorProperty *ProgressNP;
    INumber ProgressN[2];
    ISwitchVectorProperty *ExecuteSP;
    ISwitch ExecuteS[2];
    INumberVectorProperty *DownloadNP;
    INumber DownloadN[2];
    IBLOBVectorProperty *FitsBP;
    IBLOB FitsB[1];

    Group *group[MAX_GROUP_COUNT];
    
    void defineProperties();
    void deleteProperties();
    void makeReadOnly();
    void makeReadWrite();
    void runBatch();
    void stopBatch();
};

#endif