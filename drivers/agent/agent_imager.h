/*******************************************************************************
 Copyright(c) 2013-2016 CloudMakers, s. r. o. All rights reserved.
 Copyright(c) 2017 Marco Gulino <marco.gulino@gmai.com>

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

#include "baseclient.h"
#include "defaultdevice.h"

/* Smart Widget-Property */
#include "indipropertytext.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertylight.h"
#define MAX_GROUP_COUNT 16

class Group;
class Imager : public virtual INDI::DefaultDevice, public virtual INDI::BaseClient
{
    public:
        static const std::string DEVICE_NAME;
        Imager();
        virtual ~Imager() = default;

        // DefaultDevice

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n) override;
        virtual bool ISSnoopDevice(XMLEle *root) override;

        // BaseClient

        virtual void newDevice(INDI::BaseDevice *dp);
        virtual void newProperty(INDI::Property *property);
        virtual void removeProperty(INDI::Property *property);
        virtual void removeDevice(INDI::BaseDevice *dp);
        virtual void newBLOB(IBLOB *bp);
        virtual void newSwitch(ISwitchVectorProperty *svp);
        virtual void newNumber(INumberVectorProperty *nvp);
        virtual void newText(ITextVectorProperty *tvp);
        virtual void newLight(ILightVectorProperty *lvp);
        virtual void newMessage(INDI::BaseDevice *dp, int messageID);
        virtual void serverConnected();
        virtual void serverDisconnected(int exit_code);

    protected:
        virtual const char *getDefaultName();
        virtual bool Connect();
        virtual bool Disconnect();

    private:
        bool isRunning();
        bool isCCDConnected();
        bool isFilterConnected();
        void defineProperties();
        void deleteProperties();
        void initiateNextFilter();
        void initiateNextCapture();
        void startBatch();
        void abortBatch();
        void batchDone();
        void initiateDownload();

        char format[16];
        int group { 0 };
        int maxGroup { 0 };
        int image { 0 };
        int maxImage { 0 };
        const char *controlledCCD { nullptr };
        const char *controlledFilterWheel { nullptr };
        INDI::PropertyText ControlledDeviceTP {2};
        INDI::PropertyNumber GroupCountNP {1};
        INDI::PropertyNumber ProgressNP {3};
        INDI::PropertySwitch BatchSP {2};
        INDI::PropertyLight StatusLP {2};
        INDI::PropertyText ImageNameTP {2};
        INDI::PropertyNumber DownloadNP {2};
        IBLOBVectorProperty FitsBP;
        IBLOB FitsB[1];
        INDI::PropertyNumber CCDImageExposureNP {1};
        INDI::PropertyNumber CCDImageBinNP {2};
        INDI::PropertySwitch CCDUploadSP {3};
        INDI::PropertyText CCDUploadSettingsTP {2};
        INDI::PropertyNumber FilterSlotNP {1};

        std::vector<std::shared_ptr<Group>> groups;
        std::shared_ptr<Group> currentGroup() const;
        std::shared_ptr<Group> nextGroup() const;
        std::shared_ptr<Group> getGroup(int index) const;

};
