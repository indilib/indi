/*******************************************************************************
  Driver type: TitanTCS for HOBYM CRUX Mount INDI Driver

  Copyright(c) 2020 Park Suyoung <hparksy@gmail.com>. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "inditelescope.h"
#include "indiguiderinterface.h"

#define RESPONSE_TIMEOUT  3
#define USE_PEC           1

typedef struct
{
    double  ra;
    double  dec;
    //
    int     Parking;
#if USE_PEC
    int     PECStatus;
#endif
    int     Landscape;
    int     TrackingRate;
    int     TrackingStatus;
} stTitanTCS;

class TitanTCS : public INDI::Telescope, public INDI::GuiderInterface
{
    public:
        TitanTCS();

    private:
        int m_Connect;

    public:
        virtual const char *getDefaultName() override;
        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual bool Handshake() override;
        virtual bool ReadScopeStatus() override;

        //virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool ISNewText(const char *dev, const char *name, char **texts, char **names, int n) override;

    private:
        // Mount Info.
        ITextVectorProperty MountInfoTP;
        IText MountInfoT[2] {};
        int GuideNSTID { -1 };
        int GuideWETID { -1 };

        stTitanTCS  info;
#if USE_PEC
        // PECStatus
        int _PECStatus {0};

        // PEC Training
        ISwitch PECTrainingS[2];
        ISwitchVectorProperty PECTrainingSP;
        // PEC Info.
        ITextVectorProperty PECInfoTP;
        IText PECInfoT[2] {};
#endif

    protected:
        // Goto, Sync, and Motion
        virtual bool Goto(double ra, double dec) override;
        //bool GotoAzAlt(double az, double alt);
        virtual bool Sync(double ra, double dec) override;
        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool Abort() override;
        virtual bool SetSlewRate(int index) override;

        // Time and Location
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;
        virtual bool updateTime(ln_date *utc, double utc_offset) override;

        //GUIDE: guiding functions
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;
        // Tracking
        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackEnabled(bool enabled) override;

        // Parking
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;
        virtual bool SetParkPosition(double Axis1Value, double Axis2Value) override;

    private:
        bool SendCommand(const char *cmd_org);
        bool SendCommand(const char *cmd, int val);
        bool SendCommand(const char *cmd, double val);

        bool CommandResponse(const char* pCommand, const char* pResponse, char delimeter, double *pDouble, int *pInteger = nullptr);
        bool CommandResponseHour(const char* pCommand, const char* pResponse, char delimeter, double* Hour);
        bool CommandResponseStr(const char* pCommand, const char* pResponse, char delimeter, char* pReturn, int len = 0);
        bool CommandResponseChar(const char* pCommand, const char* pResponse, char* pReturn);

        bool GetMountParams(bool bAll = false);

        bool GetParamStr(const char* pInStr, char* pOutStr, int len, const char* pResponse, char delimeter);
        bool GetParamNumber(const char* pInStr, char* pOutStr, int len, const char* pResponse, char delimeter, double *pDouble,
                            int *pInteger = NULL);
        bool GetParamHour(const char* pInStr, char* pOutStr, int len, const char* pResponse, char delimeter, double *pHour);

        bool GetEqPosition(INDI_EQ_AXIS axis, double *value);
        bool GetEqPosition(double *ra, double *dec);

        void ReadFlush();
        int  ReadResponse(char *buf, int len, char delimeter, int timeout = RESPONSE_TIMEOUT);

        bool SetTarget(double ra, double dec);

        void guideTimeoutNS();
        void guideTimeoutWE();

        static void guideTimeoutHelperNS(void * p);
        static void guideTimeoutHelperWE(void * p);
#if USE_PEC
        void _setPECState(int pec_status);
#endif
};


