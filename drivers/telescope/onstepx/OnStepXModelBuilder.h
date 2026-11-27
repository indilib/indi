/*
    OnStepX INDI Driver — OnStepXModelBuilder.h
    Copyright (C) 2026 Ed Lee

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

/*
    OnStepX INDI Driver — PC-side pointing-model builder

    This subsystem owns the driver-side model-building session.  It is kept
    separate from OnStepXAlignment because that class exposes the firmware's
    native N-star alignment mechanism; this class captures Ekos Sync
    observations for a PC-side fit and, in later stages, will fit and replace
    the firmware coefficient model.
*/

#pragma once

#include <string>

#include "OnStepXModelFitter.h"
#include "OnStepXModelMath.h"
#include "OnStepXStatus.h"
#include "OnStepXModelProtocol.h"

#include <defaultdevice.h>
#include <cstdio>
#include <vector>

class OnStepXComm;

class OnStepXModelBuilder
{
    public:
        using Observation = OnStepXModelMath::Observation;
        using ModelCoefficients = OnStepXModelMath::ModelCoefficients;

        void setDevice(INDI::DefaultDevice *dev)
        {
            m_dev = dev;
        }
        void setComm(OnStepXComm *comm)
        {
            m_comm = comm;
        }

        void initProperties();
        void updateProperties(bool connected);
        bool handleSwitch(const char *name, ISState *states, char *names[], int n);
        void saveConfig(FILE *fp);

        // Called from the mount polling path.  A confirmed transition to
        // tracking OFF aborts an active build session and discards it.
        void updateTrackingState(bool tracking);
        void updateMountType(MountStatus::MountType mountType)
        {
            m_mountType = mountType;
        }

        // Called from OnStepXMount::Sync().  While a build is active the
        // firmware is never synced: the Sync is stored as an observation
        // (CAPTURED), or refused (REJECTED, so the client's Sync fails rather
        // than appearing to succeed).  NOT_BUILDING: sync the firmware.
        enum class SyncCapture { NOT_BUILDING, CAPTURED, REJECTED };
        SyncCapture captureSync(double ra, double dec, MountStatus::PierSide pierSide,
                                MountStatus::MountType mountType);

        bool isBuilding() const
        {
            return m_building;
        }

        // Abort an active session (restoring the original firmware model).
        // Call before the connection is closed.
        void abortSession(const char *reason);

        // Backup of the firmware model taken at session start.  A session
        // runs with an all-zero model in firmware RAM, and the original is
        // otherwise held only in this object, so a driver crash or kill
        // would leave the zero model active (and a later :AW# would save it
        // to NV).  The file is written before the zero model and removed
        // once the original is restored or replaced.  An empty path (the
        // default) disables it; the mount sets defaultBackupPath().
        void setBackupPath(const std::string &path)
        {
            m_backupPath = path;
        }
        // <INDICONFIG>.model_backup, else ~/.indi/<device>_model_backup.txt
        static std::string defaultBackupPath(const char *device);

        // Call on connect, once the mount type is known.  If a backup is
        // left from an interrupted session and the firmware still holds the
        // all-zero session model, write the original back (RAM only, as
        // Abort does).  Discards a backup the firmware no longer needs.
        void recoverInterruptedSession(MountStatus::MountType mountType);
        std::size_t observationCount() const
        {
            return m_observations.size();
        }

        // "index offsets, polar axis alignment (4 terms)"
        static std::string describeModel(const OnStepXModelFitter::AdaptiveResult &fit,
                                         OnStepXModelMath::MountType mountType);
        // What more observations could add, or why a level isn't used.
        static std::string describeNext(const OnStepXModelFitter::AdaptiveResult &fit,
                                        std::size_t observations,
                                        OnStepXModelMath::MountType mountType);

    private:
        INDI::DefaultDevice *m_dev  { nullptr };
        OnStepXComm         *m_comm { nullptr };

        INDI::PropertySwitch m_buildSP     { 1 };
        INDI::PropertySwitch m_controlSP   { 2 };
        // Observations captured so far; Busy while a session is open, so a
        // client can see that Syncs are being collected, not applied.
        INDI::PropertyNumber m_countNP     { 1 };
        void publishObservationCount();

        // Preview of the model the observations so far support, refitted
        // after every captured Sync, so a client can watch it gain terms:
        // OSX_MODEL_FIT (TERMS, RMS, MAX arcsec) and OSX_MODEL_FIT_INFO
        // (MODEL: the terms in use; NEXT: what more observations would add).
        // Busy while a session is open; cleared when a session starts.
        INDI::PropertyNumber m_fitNP       { 3 };
        INDI::PropertyText   m_fitTP       { 2 };
        void publishPreview();
        void clearPreview();
        // The highest fitted level whose quantised model is within the
        // firmware's limits, with its protocol values; -1 if none.  rejected
        // is the first limit a higher level broke.
        static int uploadableLevel(const OnStepXModelFitter::AdaptiveResult &fit,
                                   OnStepXModelProtocol::Values &protocol, std::string &rejected);

        bool m_building  { false };
        bool m_tracking  { false };
        MountStatus::MountType m_mountType { MountStatus::MountType::UNKNOWN };
        double m_latitudeRad { 0.0 };
        std::vector<Observation> m_observations;

        // Model produced by the most recent successful Calculate Model.
        // It has already been quantised to the firmware protocol and then
        // dequantised to represent the model that will actually exist in
        // firmware after :SX0 writes.
        ModelCoefficients m_pendingModel {};
        OnStepXModelProtocol::Values m_pendingProtocol {};
        bool m_hasPendingModel { false };

        // Firmware model active before the session; restored on abort.
        OnStepXModelProtocol::Values m_originalProtocol {};
        bool m_hasOriginal { false };

        std::string m_backupPath;
        bool saveBackup(const OnStepXModelProtocol::Values &values);
        bool loadBackup(OnStepXModelProtocol::Values &values, int &mountType);
        void removeBackup();

        bool writeAndVerify(const OnStepXModelProtocol::Values &values);
        bool restoreOriginalModel(const char *reason);
        static bool valuesEqual(const OnStepXModelProtocol::Values &a,
                                const OnStepXModelProtocol::Values &b);
        // "ax1Cor 601!=-601, ..." for the coefficients that differ.
        static std::string describeMismatch(const OnStepXModelProtocol::Values &got,
                                            const OnStepXModelProtocol::Values &want);
        // Read the firmware model back and compare; read once more before
        // reporting a mismatch (a reply byte lost on the serial link).
        bool readBackMatches(const OnStepXModelProtocol::Values &want);

        enum { CONTROL_CALCULATE = 0, CONTROL_ABORT = 1 };

        bool startBuild();
        bool abortBuild(const char *reason);
        bool calculateModel();
        bool replaceFirmwareModel();
        bool readFirmwareModel(OnStepXModelProtocol::Values &values);
        bool writeFirmwareModel(const OnStepXModelProtocol::Values &values);
        bool readCurrentMount(double &mountRAHours, double &mountDecDeg,
                              double &lstHours);
        bool readLatitude(double &latitudeDeg);

        static double wrapHours(double hours);
        const char *getDeviceName() const
        {
            return m_dev ? m_dev->getDeviceName() : "Unknown";
        }
};
