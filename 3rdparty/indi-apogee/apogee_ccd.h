/*
    Apogee CCD
    INDI Driver for Apogee CCDs and Filter Wheels
    Copyright (C) 2014-2019 Jasem Mutlaq <mutlaqja AT ikarustech DOT com>

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

#include <indiccd.h>
#include <indiguiderinterface.h>
#include <indifilterinterface.h>
#include <iostream>

#include "ApogeeCam.h"
#include "ApogeeFilterWheel.h"
#include "FindDeviceEthernet.h"
#include "FindDeviceUsb.h"

class ApogeeCCD : public INDI::CCD, public INDI::FilterInterface
{
    public:
        ApogeeCCD();

        const char *getDefaultName() override;

        void ISGetProperties(const char *dev) override;
        bool initProperties() override;
        bool updateProperties() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

    protected:

        bool Connect() override;
        bool Disconnect() override;

        int SetTemperature(double temperature) override;
        bool StartExposure(float duration) override;
        bool AbortExposure() override;

        void TimerHit() override;

        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;

        virtual void debugTriggered(bool enabled) override;
        virtual bool saveConfigItems(FILE *fp) override;

        virtual bool SelectFilter(int) override;
        virtual int QueryFilter() override;

    private:
        std::unique_ptr<ApogeeCam> ApgCam;
        std::unique_ptr<ApogeeFilterWheel> ApgCFW;

        INumber CoolerN[1];
        INumberVectorProperty CoolerNP;

        ISwitch CoolerS[2];
        ISwitchVectorProperty CoolerSP;

        ISwitch ReadOutS[2];
        ISwitchVectorProperty ReadOutSP;

        ISwitchVectorProperty PortTypeSP;
        ISwitch PortTypeS[2];
        enum
        {
            PORT_USB,
            PORT_NETWORK
        };

        ITextVectorProperty NetworkInfoTP;
        IText NetworkInfoT[2] {};
        enum
        {
            NETWORK_SUBNET,
            NETWORK_ADDRESS
        };

        IText CamInfoT[2] {};
        ITextVectorProperty CamInfoTP;

        ISwitchVectorProperty FanStatusSP;
        ISwitch FanStatusS[4];
        enum
        {
            FAN_OFF,
            FAN_SLOW,
            FAN_MED,
            FAN_FAST
        };

        // Filter Type
        ISwitchVectorProperty FilterTypeSP;
        ISwitch FilterTypeS[5];
        enum
        {
            TYPE_UNKNOWN,
            TYPE_FW50_9R,
            TYPE_FW50_7S,
            TYPE_AFW50_10S,
            TYPE_AFW31_17R
        };

        // Filter Information
        ITextVectorProperty FilterInfoTP;
        IText FilterInfoT[2] {};
        enum
        {
            INFO_NAME,
            INFO_FIRMWARE,
        };

        double minDuration;
        double ExposureRequest;
        int imageWidth, imageHeight;
        int timerID;
        bool cameraFound {false}, cfwFound {false};
        INDI::CCDChip::CCD_FRAME imageFrameType;
        struct timeval ExpStart;

        std::string ioInterface;
        std::string subnet;
        std::string firmwareRev;
        std::string modelStr;
        FindDeviceEthernet look4cam;
        FindDeviceUsb lookUsb;
        CamModel::PlatformType model;

        void checkStatus(const Apg::Status status);
        std::vector<std::string> MakeTokens(const std::string &str, const std::string &separator);
        std::string GetItemFromFindStr(const std::string &msg, const std::string &item);
        //std::string GetAddress( const std::string & msg );
        std::string GetUsbAddress(const std::string &msg);
        std::string GetEthernetAddress(const std::string &msg);
        std::string GetIPAddress(const std::string &msg);
        CamModel::PlatformType GetModel(const std::string &msg);
        uint16_t GetID(const std::string &msg);
        uint16_t GetFrmwrRev(const std::string &msg);

        bool IsDeviceCamera(const std::string &msg);
        bool IsDeviceFilterWheel(const std::string &msg);
        bool IsAscent(const std::string &msg);
        void printInfo(const std::string &model, uint16_t maxImgRows, uint16_t maxImgCols);

        float CalcTimeLeft(timeval, float);
        int grabImage();
        bool getCameraParams();
        void activateCooler(bool enable);
};

