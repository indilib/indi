/*
 ATIK Filter Wheel Driver

 Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

*/

#pragma once

#include <AtikCameras.h>

#include <indifilterwheel.h>

class ATIKWHEEL : public INDI::FilterWheel
{
    public:
        explicit ATIKWHEEL(std::string filterName, int id);
        ~ATIKWHEEL() override = default;

        virtual const char *getDefaultName() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

    protected:
        virtual bool initProperties() override;
        virtual void TimerHit() override;

        // Filter wheel
        virtual bool SelectFilter(int) override;
        virtual int QueryFilter() override;

    private:
        // Setup initial params
        bool setupParams();

        // Device name
        char name[MAXINDIDEVICE];

        // FW info
        ArtemisHandle hWheel { nullptr };
        int m_iDevice {-1};

        friend void ::ISGetProperties(const char *dev);
        friend void ::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num);
        friend void ::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num);
        friend void ::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num);
        friend void ::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                                char *formats[], char *names[], int n);
};
