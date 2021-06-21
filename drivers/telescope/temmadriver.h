/*******************************************************************************
  Copyright(c) 2016 Gerry Rozema. All rights reserved.

  Copyright(c) 2017-2020 Jasem Mutlaq. All rights reserved.

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

#include "indiguiderinterface.h"
#include "inditelescope.h"
#include "alignment/AlignmentSubsystemForDrivers.h"

class TemmaMount : public INDI::Telescope, public INDI::GuiderInterface
/*,public INDI::AlignmentSubsystem::AlignmentSubsystemForDrivers*/
{
    public:
        TemmaMount();
        virtual ~TemmaMount() override = default;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual const char *getDefaultName() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                               char *formats[], char *names[], int n) override;
        virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;


    protected:
        virtual bool Handshake() override;
        virtual bool ReadScopeStatus() override;

        virtual bool Goto(double ra, double dec) override;
        virtual bool Sync(double ra, double dec) override;
        virtual bool Park() override;
        virtual bool UnPark() override;
        virtual bool Abort() override;
        virtual bool SetTrackMode(uint8_t mode) override;
        virtual bool SetTrackEnabled(bool enabled) override;

        virtual bool MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command) override;
        virtual bool MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command) override;
        virtual bool updateLocation(double latitude, double longitude, double elevation) override;

        virtual bool SetCurrentPark() override;
        virtual bool SetDefaultPark() override;

        //  methods added for guider interface
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

        //  Initial implementation doesn't need this one
        //virtual void GuideComplete(INDI_EQ_AXIS axis);

    private:
        void mountSim();

        bool getVersion();
        bool getCoords();

        // Send command to mount, and optionally read a response. CR LF is appended to the command.
        // Pass nullptr to response to skip reading the respone.
        // Response size must be 64 bytes (TEMMA_BUFFER). CR LF is removed from response.
        bool sendCommand(const char *cmd, char *response = nullptr);

        bool motorsEnabled();
        bool setMotorsEnabled(bool enable);

        // LST & Latitude functions
        bool setLST();
        bool getLST(double &lst);
        bool setLatitude(double lat);
        bool getLatitude(double &lat);

        //INDI::IEquatorialCoordinates TelescopeToSky(double ra, double dec);
        //INDI::IEquatorialCoordinates SkyToTelescope(double ra, double dec);

        //bool TemmaConnect(const char *port);

        double currentRA = 0, currentDEC = 0, targetRA = 0, targetDEC = 0, alignedRA = 0, alignedDEC = 0;

        bool MotorStatus { false };
        bool TemmaInitialized { false };
        double Longitude { 0 };
        double Latitude { 0 };
        int SlewRate { 1 };
        bool SlewActive { false };
        uint8_t Slewbits { 0 };
};
