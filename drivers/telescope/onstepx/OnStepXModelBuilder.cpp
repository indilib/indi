/*
    OnStepX INDI Driver — OnStepXModelBuilder.cpp
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

    Stage 7 implementation:
      - Build Model mode control
      - Calculate Model / Abort Model controls
      - arbitrary-size observation store
      - Sync interception
      - exact OnStepX equatorial -> native-axis conversions for GEM/FORK,
        ALTAZM and ALTALT
      - exact GeoAlign::mountToObservedPlace() forward-model equations
      - tracking safety invariant
      - numerical fitting and protocol quantisation
      - readback-verified firmware coefficient replacement with rollback
      - model activation and EEPROM persistence
*/

#include "OnStepXModelBuilder.h"
#include "OnStepXComm.h"
#include "OnStepXModelFitter.h"
#include "OnStepXModelProtocol.h"

#include <indilogger.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define MODEL_TAB "Model"

enum { BUILD_ON = 0 };

namespace
{
constexpr double ARCSEC_PER_RAD =
    206264.80624709636;

struct ResidualStatistics
{
    double rmsArcsec { 0.0 };
    double maxAbsArcsec { 0.0 };
    bool finite { true };
};

ResidualStatistics evaluateResiduals(
    const std::vector<OnStepXModelMath::Observation> &observations,
    const OnStepXModelMath::ModelCoefficients &model,
    double latitude)
{
    ResidualStatistics statistics;

    double sumSquared = 0.0;
    std::size_t count = 0;

    for (const auto &observation : observations)
    {
        double r1 = 0.0;
        double r2 = 0.0;

        OnStepXModelMath::residual(
            observation,
            model,
            latitude,
            r1,
            r2);

        r1 *= ARCSEC_PER_RAD;
        r2 *= ARCSEC_PER_RAD;

        if (!std::isfinite(r1) ||
                !std::isfinite(r2))
        {
            statistics.finite = false;
            return statistics;
        }

        sumSquared += r1 * r1;
        sumSquared += r2 * r2;

        statistics.maxAbsArcsec =
            std::max(
                statistics.maxAbsArcsec,
                std::abs(r1));

        statistics.maxAbsArcsec =
            std::max(
                statistics.maxAbsArcsec,
                std::abs(r2));

        count += 2;
    }

    if (count == 0)
    {
        statistics.finite = false;
        return statistics;
    }

    statistics.rmsArcsec =
        std::sqrt(
            sumSquared /
            static_cast<double>(count));

    if (!std::isfinite(statistics.rmsArcsec) ||
            !std::isfinite(statistics.maxAbsArcsec))
    {
        statistics.finite = false;
    }

    return statistics;
}
}

void OnStepXModelBuilder::initProperties()
{
    const char *dev = getDeviceName();

    m_buildSP[BUILD_ON].fill("BUILD", "Build Model", ISS_OFF);
    m_buildSP.fill(dev, "OSX_MODEL_BUILD", "Build Model", MODEL_TAB,
                   IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    m_controlSP[CONTROL_CALCULATE].fill("CALCULATE", "Calculate Model", ISS_OFF);
    m_controlSP[CONTROL_ABORT].fill("ABORT", "Abort Model", ISS_OFF);
    m_controlSP.fill(dev, "OSX_MODEL_CONTROL", "Model Control", MODEL_TAB,
                     IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    m_countNP[0].fill("COUNT", "Observations", "%.0f", 0, 1000, 0, 0);
    m_countNP.fill(dev, "OSX_MODEL_OBSERVATIONS", "Build Model", MODEL_TAB, IP_RO, 0, IPS_IDLE);

    m_fitNP[0].fill("TERMS", "Terms fitted", "%.0f", 0, 12, 0, 0);
    m_fitNP[1].fill("RMS", "RMS (arcsec)", "%.1f", 0, 1e6, 0, 0);
    m_fitNP[2].fill("MAX", "Max (arcsec)", "%.1f", 0, 1e6, 0, 0);
    m_fitNP.fill(dev, "OSX_MODEL_FIT", "Model so far", MODEL_TAB, IP_RO, 0, IPS_IDLE);

    m_fitTP[0].fill("MODEL", "Fits", "");
    m_fitTP[1].fill("NEXT", "Next", "");
    m_fitTP.fill(dev, "OSX_MODEL_FIT_INFO", "Model terms", MODEL_TAB, IP_RO, 0, IPS_IDLE);
}

int OnStepXModelBuilder::uploadableLevel(const OnStepXModelFitter::AdaptiveResult &fit,
        OnStepXModelProtocol::Values &protocol, std::string &rejected)
{
    rejected.clear();
    for (int i = fit.chosen; i >= 0; --i)
    {
        const auto &level = fit.levels[i];
        if (level.outcome != OnStepXModelFitter::Level::Outcome::USED)
            continue;
        protocol = OnStepXModelProtocol::quantize(level.fit.model);
        std::string reason;
        if (OnStepXModelProtocol::validateForFirmware(protocol, reason))
            return i;
        if (rejected.empty())
            rejected = reason;
    }
    return -1;
}

std::string OnStepXModelBuilder::describeModel(const OnStepXModelFitter::AdaptiveResult &fit,
        OnStepXModelMath::MountType mountType)
{
    if (!fit.success())
        return "none yet";

    std::string text;
    for (int i = 0; i <= fit.chosen; ++i)
    {
        // A lower level that failed still has its terms in a higher chosen one.
        if (!text.empty())
            text += ", ";
        text += OnStepXModelFitter::describe(fit.levels[i].terms, mountType);
    }
    char count[32];
    snprintf(count, sizeof(count), " (%zu terms)", fit.best().freeParameters);
    return text + count;
}

std::string OnStepXModelBuilder::describeNext(const OnStepXModelFitter::AdaptiveResult &fit,
        std::size_t observations,
        OnStepXModelMath::MountType mountType)
{
    using Level = OnStepXModelFitter::Level;
    char text[200];

    if (!fit.success())
    {
        const std::size_t need = OnStepXModelFitter::minimumObservations(OnStepXModelFitter::Terms::INDEX);
        if (observations < need)
        {
            snprintf(text, sizeof(text), "%zu more observation%s for a first model",
                     need - observations, need - observations == 1 ? "" : "s");
            return text;
        }
        return "no model fits these observations; spread them across the sky";
    }

    for (std::size_t i = static_cast<std::size_t>(fit.chosen) + 1; i < OnStepXModelFitter::TERM_LEVELS; ++i)
    {
        const Level &level = fit.levels[i];
        const char *what = OnStepXModelFitter::describe(level.terms, mountType);
        switch (level.outcome)
        {
            case Level::Outcome::NOT_ENOUGH_OBSERVATIONS:
            {
                const std::size_t more = OnStepXModelFitter::minimumObservations(level.terms) - observations;
                snprintf(text, sizeof(text), "%zu more observation%s to try %s",
                         more, more == 1 ? "" : "s", what);
                return text;
            }
            case Level::Outcome::NOT_SEPARABLE:
                snprintf(text, sizeof(text), "%s: the observations can't separate it yet; spread them %s",
                         what, mountType == OnStepXModelMath::MountType::GEM
                         ? "across the sky and both sides of the meridian" : "across the sky");
                return text;
            case Level::Outcome::NOT_SIGNIFICANT:
                snprintf(text, sizeof(text), "%s: no significant improvement yet; more observations may show it",
                         what);
                return text;
            case Level::Outcome::FAILED:
                snprintf(text, sizeof(text), "%s: the fit failed", what);
                return text;
            case Level::Outcome::USED:
                // Above the chosen level only when it was dropped for limits.
                snprintf(text, sizeof(text), "%s: fitted, but outside the firmware's limits", what);
                return text;
        }
    }
    return "all terms fitted";
}

void OnStepXModelBuilder::clearPreview()
{
    m_fitNP[0].setValue(0);
    m_fitNP[1].setValue(0);
    m_fitNP[2].setValue(0);
    m_fitNP.setState(m_building ? IPS_BUSY : IPS_IDLE);
    m_fitNP.apply();
    m_fitTP[0].setText(m_building ? "none yet" : "");
    m_fitTP[1].setText(m_building ? describeNext(OnStepXModelFitter::AdaptiveResult {}, 0,
                       OnStepXModelMath::MountType::GEM) : "");
    m_fitTP.setState(m_building ? IPS_BUSY : IPS_IDLE);
    m_fitTP.apply();
}

void OnStepXModelBuilder::publishPreview()
{
    if (m_observations.empty())
    {
        clearPreview();
        return;
    }

    const auto mountType = m_observations.front().mountType;
    auto fit = OnStepXModelFitter::fitAdaptive(m_observations, m_latitudeRad);
    OnStepXModelProtocol::Values protocol {};
    std::string rejected;
    fit.chosen = uploadableLevel(fit, protocol, rejected);   // what Calculate Model would upload
    const std::string model = describeModel(fit, mountType);
    const std::string next = describeNext(fit, m_observations.size(), mountType);

    m_fitNP[0].setValue(fit.success() ? static_cast<double>(fit.best().freeParameters) : 0.0);
    m_fitNP[1].setValue(fit.success() ? fit.best().rmsArcsec : 0.0);
    m_fitNP[2].setValue(fit.success() ? fit.best().maxAbsResidualArcsec : 0.0);
    m_fitNP.setState(IPS_BUSY);
    m_fitNP.apply();
    m_fitTP[0].setText(model);
    m_fitTP[1].setText(next);
    m_fitTP.setState(IPS_BUSY);
    m_fitTP.apply();

    if (fit.success())
        LOGF_INFO("Build Model: %zu observations; model so far: %s, RMS %.1f\", max %.1f\"; next: %s",
                  m_observations.size(), model.c_str(), fit.best().rmsArcsec,
                  fit.best().maxAbsResidualArcsec, next.c_str());
    else
        LOGF_INFO("Build Model: %zu observation%s; next: %s", m_observations.size(),
                  m_observations.size() == 1 ? "" : "s", next.c_str());
}

void OnStepXModelBuilder::publishObservationCount()
{
    m_countNP[0].setValue(static_cast<double>(m_observations.size()));
    m_countNP.setState(m_building ? IPS_BUSY : IPS_IDLE);
    m_countNP.apply();
}

void OnStepXModelBuilder::updateProperties(bool connected)
{
    if (connected)
    {
        m_dev->defineProperty(m_buildSP);
        m_dev->defineProperty(m_controlSP);
        m_countNP[0].setValue(0);
        m_countNP.setState(IPS_IDLE);
        m_dev->defineProperty(m_countNP);
        m_fitNP[0].setValue(0);
        m_fitNP[1].setValue(0);
        m_fitNP[2].setValue(0);
        m_fitNP.setState(IPS_IDLE);
        m_dev->defineProperty(m_fitNP);
        m_fitTP[0].setText("");
        m_fitTP[1].setText("");
        m_fitTP.setState(IPS_IDLE);
        m_dev->defineProperty(m_fitTP);
    }
    else
    {
        // A disconnect cannot leave a live build session.  Reset all
        // connection-derived and session state so stale tracking, mount-type
        // or pending-model state cannot survive into a later connection.
        m_building = false;
        m_tracking = false;
        m_mountType = MountStatus::MountType::UNKNOWN;
        m_observations.clear();
        m_pendingModel = ModelCoefficients {};
        m_pendingProtocol = OnStepXModelProtocol::Values {};
        m_hasPendingModel = false;

        m_buildSP[BUILD_ON].setState(ISS_OFF);
        m_buildSP.setState(IPS_IDLE);

        m_controlSP.reset();
        m_controlSP.setState(IPS_IDLE);

        m_dev->deleteProperty(m_buildSP);
        m_dev->deleteProperty(m_controlSP);
        m_dev->deleteProperty(m_countNP);
        m_dev->deleteProperty(m_fitNP);
        m_dev->deleteProperty(m_fitTP);
    }
}

bool OnStepXModelBuilder::handleSwitch(const char *name, ISState *states,
                                       char *names[], int n)
{
    if (m_buildSP.isNameMatch(name))
    {
        m_buildSP.update(states, names, n);
        const bool requested = (m_buildSP[BUILD_ON].getState() == ISS_ON);

        bool ok = true;
        if (requested)
        {
            if (!m_building)
                ok = startBuild();
        }
        else if (m_building)
        {
            ok = abortBuild("Build Model disabled");
        }

        m_buildSP[BUILD_ON].setState(ok && m_building ? ISS_ON : ISS_OFF);
        m_buildSP.setState(ok ? IPS_OK : IPS_ALERT);
        m_buildSP.apply();
        return true;
    }

    if (m_controlSP.isNameMatch(name))
    {
        m_controlSP.update(states, names, n);
        const int idx = m_controlSP.findOnSwitchIndex();
        bool ok = false;

        if (idx == CONTROL_CALCULATE)
            ok = calculateModel();
        else if (idx == CONTROL_ABORT)
            ok = abortBuild("User requested Abort Model");

        m_controlSP.reset();
        m_controlSP.setState(ok ? IPS_OK : IPS_ALERT);
        m_controlSP.apply();
        return true;
    }

    return false;
}

void OnStepXModelBuilder::saveConfig(FILE * /*fp*/)
{
    // Build sessions are deliberately not persistent.
}

void OnStepXModelBuilder::updateTrackingState(bool tracking)
{
    const bool wasTracking = m_tracking;
    m_tracking = tracking;

    if (m_building && wasTracking && !tracking)
        abortBuild("Tracking was turned off while Build Model was active");
}

bool OnStepXModelBuilder::startBuild()
{
    if (!m_comm)
    {
        LOG_ERROR("Build Model: communication is not available");
        return false;
    }

    if (!m_tracking)
    {
        LOG_ERROR("Build Model requires tracking to be ON");
        return false;
    }

    if (m_mountType == MountStatus::MountType::UNKNOWN)
    {
        LOG_ERROR("Build Model: mount type is not known");
        return false;
    }

    double latitudeDeg = 0.0;
    if (!readLatitude(latitudeDeg))
    {
        LOG_ERROR("Build Model: failed to read site latitude");
        return false;
    }

    m_latitudeRad =
        latitudeDeg * 3.14159265358979323846 / 180.0;

    // :GRH#/:GDH# report positions corrected by the ACTIVE pointing model.
    // Observations must be uncorrected for the fit to describe the whole
    // model (which replaces the firmware model), so keep the current model
    // for restoring later and run the session with all coefficients zero.
    if (!readFirmwareModel(m_originalProtocol))
    {
        LOG_ERROR("Build Model: unable to read the current firmware model");
        return false;
    }
    m_hasOriginal = true;

    // Keep a copy on disk before suspending the model, so a driver crash
    // cannot leave the zero session model as the only one anywhere.
    if (!saveBackup(m_originalProtocol))
    {
        LOGF_ERROR("Build Model: unable to save the current firmware model to %s; "
                   "the session was not started", m_backupPath.c_str());
        m_hasOriginal = false;
        return false;
    }

    if (!writeAndVerify(OnStepXModelProtocol::Values {}))
    {
        LOG_ERROR("Build Model: unable to neutralise the firmware model for the session");
        restoreOriginalModel("start failed");
        return false;
    }

    m_observations.clear();
    m_pendingModel = ModelCoefficients {};
    m_pendingProtocol = OnStepXModelProtocol::Values {};
    m_hasPendingModel = false;

    m_building = true;
    publishObservationCount();
    clearPreview();
    LOG_INFO("Build Model: session started; the firmware pointing model is suspended "
             "until Calculate Model or Abort");
    LOG_INFO("Build Model: until then each Sync is recorded as an observation and does not sync the mount");
    return true;
}

bool OnStepXModelBuilder::writeAndVerify(const OnStepXModelProtocol::Values &values)
{
    return writeFirmwareModel(values) && readBackMatches(values);
}

bool OnStepXModelBuilder::restoreOriginalModel(const char *reason)
{
    if (!m_hasOriginal)
        return true;

    const bool ok = writeAndVerify(m_originalProtocol);
    if (ok)
    {
        LOGF_INFO("Build Model: original firmware model restored (%s)", reason);
        removeBackup();
    }
    else
        LOGF_ERROR("Build Model: FAILED to restore the original firmware model (%s); %s", reason,
                   m_backupPath.empty() ? "it will be reloaded from NV memory at the next controller restart"
                   : "it will be restored at the next connect, or reloaded from NV at the next controller restart");
    m_hasOriginal = false;
    return ok;
}

// ---------------------------------------------------------------------------
// Model backup file
//
// Plain text, one "name value" pair per line, so it can be read by hand:
//   mount 0
//   ax1Cor -120
//   ...
// ---------------------------------------------------------------------------
namespace
{
struct BackupField
{
    const char *name;
    std::int64_t OnStepXModelProtocol::Values::*member;
};

constexpr BackupField BACKUP_FIELDS[] =
{
    { "ax1Cor", &OnStepXModelProtocol::Values::ax1Cor },
    { "ax2Cor", &OnStepXModelProtocol::Values::ax2Cor },
    { "altCor", &OnStepXModelProtocol::Values::altCor },
    { "azmCor", &OnStepXModelProtocol::Values::azmCor },
    { "doCor",  &OnStepXModelProtocol::Values::doCor  },
    { "pdCor",  &OnStepXModelProtocol::Values::pdCor  },
    { "dfCor",  &OnStepXModelProtocol::Values::dfCor  },
    { "tfCor",  &OnStepXModelProtocol::Values::tfCor  },
    { "hcp",    &OnStepXModelProtocol::Values::hcp    },
    { "hca",    &OnStepXModelProtocol::Values::hca    },
    { "dcp",    &OnStepXModelProtocol::Values::dcp    },
    { "dca",    &OnStepXModelProtocol::Values::dca    },
};
}

std::string OnStepXModelBuilder::defaultBackupPath(const char *device)
{
    if (const char *config = getenv("INDICONFIG"))
        return std::string(config) + ".model_backup";
    const char *home = getenv("HOME");
    return std::string(home ? home : ".") + "/.indi/" + device + "_model_backup.txt";
}

bool OnStepXModelBuilder::saveBackup(const OnStepXModelProtocol::Values &values)
{
    if (m_backupPath.empty())
        return true;

    // Write a temporary file and rename it, so a crash mid-write cannot
    // leave a truncated backup in place of a good one.
    const std::string tmp = m_backupPath + ".tmp";
    FILE *fp = fopen(tmp.c_str(), "w");
    if (!fp)
        return false;

    bool ok = fprintf(fp, "# OnStepX pointing model saved at the start of a Build Model session.\n"
                      "# The driver restores it on the next connect if the session was interrupted.\n"
                      "mount %d\n", static_cast<int>(m_mountType)) > 0;
    for (const auto &field : BACKUP_FIELDS)
        ok = ok && fprintf(fp, "%s %lld\n", field.name, static_cast<long long>(values.*field.member)) > 0;
    ok = (fclose(fp) == 0) && ok;

    if (!ok || rename(tmp.c_str(), m_backupPath.c_str()) != 0)
    {
        remove(tmp.c_str());
        return false;
    }
    return true;
}

bool OnStepXModelBuilder::loadBackup(OnStepXModelProtocol::Values &values, int &mountType)
{
    FILE *fp = fopen(m_backupPath.c_str(), "r");
    if (!fp)
        return false;

    values = OnStepXModelProtocol::Values {};
    bool haveMount = false;
    std::size_t fields = 0;
    char line[128];
    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        char name[16];
        long long value = 0;
        if (sscanf(line, "%15s %lld", name, &value) != 2)
            continue;
        if (strcmp(name, "mount") == 0)
        {
            mountType = static_cast<int>(value);
            haveMount = true;
            continue;
        }
        for (const auto &field : BACKUP_FIELDS)
        {
            if (strcmp(name, field.name) == 0)
            {
                values.*field.member = value;
                ++fields;
            }
        }
    }
    fclose(fp);
    return haveMount && fields == sizeof(BACKUP_FIELDS) / sizeof(BACKUP_FIELDS[0]);
}

void OnStepXModelBuilder::removeBackup()
{
    if (!m_backupPath.empty())
        remove(m_backupPath.c_str());
}

void OnStepXModelBuilder::recoverInterruptedSession(MountStatus::MountType mountType)
{
    if (m_backupPath.empty() || !m_comm)
        return;

    FILE *probe = fopen(m_backupPath.c_str(), "r");
    if (!probe)
        return;
    fclose(probe);

    OnStepXModelProtocol::Values backup;
    int backupMount = -1;
    if (!loadBackup(backup, backupMount))
    {
        const std::string bad = m_backupPath + ".bad";
        rename(m_backupPath.c_str(), bad.c_str());
        LOGF_ERROR("Build Model: the model backup from an interrupted session is unreadable and was "
                   "moved to %s. Check the pointing model; restarting the controller reloads it from NV.",
                   bad.c_str());
        return;
    }
    if (backupMount != static_cast<int>(mountType))
    {
        LOGF_WARN("Build Model: the model backup %s is for a different mount type; discarding it",
                  m_backupPath.c_str());
        removeBackup();
        return;
    }

    // The dfCor index depends on the mount type.
    m_mountType = mountType;
    OnStepXModelProtocol::Values current;
    if (!readFirmwareModel(current))
    {
        LOGF_WARN("Build Model: could not read the firmware model to check the backup from an "
                  "interrupted session (%s); will check again at the next connect", m_backupPath.c_str());
        return;
    }

    if (valuesEqual(current, backup))
    {
        LOG_INFO("Build Model: the firmware already holds the model saved before an interrupted session");
        removeBackup();
        return;
    }
    if (!valuesEqual(current, OnStepXModelProtocol::Values {}))
    {
        // Something else has set a model since: the controller was restarted
        // (NV reloaded) or a model was loaded another way.  Keep it.
        LOG_INFO("Build Model: the firmware model has changed since an interrupted session; "
                 "discarding the old backup");
        removeBackup();
        return;
    }

    if (writeAndVerify(backup))
    {
        LOG_WARN("Build Model: a previous session was interrupted and left the pointing model "
                 "suspended; the model saved at its start has been restored");
        removeBackup();
    }
    else
    {
        LOGF_ERROR("Build Model: a previous session was interrupted and the pointing model is suspended; "
                   "restoring it from %s FAILED. Restart the controller to reload the model from NV.",
                   m_backupPath.c_str());
    }
}

bool OnStepXModelBuilder::valuesEqual(const OnStepXModelProtocol::Values &a,
                                      const OnStepXModelProtocol::Values &b)
{
    return a.ax1Cor == b.ax1Cor &&
           a.ax2Cor == b.ax2Cor &&
           a.altCor == b.altCor &&
           a.azmCor == b.azmCor &&
           a.doCor  == b.doCor &&
           a.pdCor  == b.pdCor &&
           a.dfCor  == b.dfCor &&
           a.tfCor  == b.tfCor &&
           a.hcp    == b.hcp &&
           a.hca    == b.hca &&
           a.dcp    == b.dcp &&
           a.dca    == b.dca;
}

std::string OnStepXModelBuilder::describeMismatch(const OnStepXModelProtocol::Values &got,
        const OnStepXModelProtocol::Values &want)
{
    const struct
    {
        const char *name;
        std::int64_t g, w;
    } fields[] =
    {
        { "ax1Cor", got.ax1Cor, want.ax1Cor }, { "ax2Cor", got.ax2Cor, want.ax2Cor },
        { "altCor", got.altCor, want.altCor }, { "azmCor", got.azmCor, want.azmCor },
        { "doCor", got.doCor, want.doCor },    { "pdCor", got.pdCor, want.pdCor },
        { "dfCor", got.dfCor, want.dfCor },    { "tfCor", got.tfCor, want.tfCor },
        { "hcp", got.hcp, want.hcp },          { "hca", got.hca, want.hca },
        { "dcp", got.dcp, want.dcp },          { "dca", got.dca, want.dca },
    };
    std::string out;
    for (const auto &f : fields)
    {
        if (f.g == f.w)
            continue;
        char buf[80];
        snprintf(buf, sizeof(buf), "%s%s read %lld, wrote %lld", out.empty() ? "" : ", ", f.name,
                 static_cast<long long>(f.g), static_cast<long long>(f.w));
        out += buf;
    }
    return out;
}

bool OnStepXModelBuilder::readBackMatches(const OnStepXModelProtocol::Values &want)
{
    for (int attempt = 1; attempt <= 2; attempt++)
    {
        OnStepXModelProtocol::Values got;
        if (!readFirmwareModel(got))
            continue;   // readFirmwareModel() logged which read failed
        if (valuesEqual(got, want))
            return true;
        LOGF_WARN("Build Model: model read back differs (attempt %d): %s", attempt,
                  describeMismatch(got, want).c_str());
    }
    return false;
}

void OnStepXModelBuilder::abortSession(const char *reason)
{
    if (m_building)
        abortBuild(reason);
}

bool OnStepXModelBuilder::abortBuild(const char *reason)
{
    if (!m_building)
    {
        m_observations.clear();
        return true;
    }

    const std::size_t count = m_observations.size();

    m_observations.clear();

    m_pendingModel = ModelCoefficients {};
    m_pendingProtocol = OnStepXModelProtocol::Values {};
    m_hasPendingModel = false;

    m_building = false;
    publishObservationCount();
    clearPreview();
    const bool restored = restoreOriginalModel(reason);

    m_buildSP[BUILD_ON].setState(ISS_OFF);
    m_buildSP.setState(restored ? IPS_OK : IPS_ALERT);
    m_buildSP.apply();

    LOGF_INFO("Build Model aborted (%s); discarded %zu observations", reason, count);
    return restored;
}

bool OnStepXModelBuilder::calculateModel()
{
    if (!m_building)
    {
        LOG_ERROR(
            "Calculate Model requested but Build Model is not active");

        return false;
    }

    const std::size_t observationCount =
        m_observations.size();

    LOGF_INFO(
        "Calculate Model: fitting %zu observations",
        observationCount);

    const auto mountType = m_observations.empty()
                           ? OnStepXModelMath::MountType::GEM
                           : m_observations.front().mountType;
    const auto adaptive =
        OnStepXModelFitter::fitAdaptive(
            m_observations,
            m_latitudeRad);

    if (!adaptive.success())
    {
        const auto &index = adaptive.levels[0].fit;
        LOGF_ERROR(
            "Calculate Model: no model could be fitted "
            "(status %d, rank %zu, iterations %zu; %s); "
            "%zu observations retained",
            static_cast<int>(index.status),
            index.rank,
            index.iterations,
            describeNext(adaptive, observationCount, mountType).c_str(),
            observationCount);

        return false;
    }

    /*
     * Use the chosen level; if its quantised model is outside the firmware's
     * limits, fall back to the highest lower level that was also fitted.
     */
    OnStepXModelProtocol::Values protocol {};
    std::string rejected;
    const int levelIndex = uploadableLevel(adaptive, protocol, rejected);
    if (levelIndex < 0)
    {
        LOGF_ERROR(
            "Calculate Model: no fitted model is within firmware limits (%s); "
            "%zu observations retained",
            rejected.c_str(),
            observationCount);
        return false;
    }
    if (levelIndex != adaptive.chosen)
        LOGF_WARN("Calculate Model: the %s model is outside firmware limits (%s); using fewer terms",
                  OnStepXModelFitter::describe(adaptive.levels[adaptive.chosen].terms, mountType),
                  rejected.c_str());

    const auto &fit = adaptive.levels[levelIndex].fit;
    OnStepXModelFitter::AdaptiveResult used = adaptive;
    used.chosen = levelIndex;

    LOGF_INFO(
        "Calculate Model: fit succeeded with %s "
        "(rank %zu, iterations %zu, RMS %.9f arcsec, "
        "max %.9f arcsec)",
        describeModel(used, mountType).c_str(),
        fit.rank,
        fit.iterations,
        fit.rmsArcsec,
        fit.maxAbsResidualArcsec);
    if (levelIndex + 1 < static_cast<int>(OnStepXModelFitter::TERM_LEVELS))
        LOGF_INFO("Calculate Model: not fitted: %s",
                  describeNext(used, observationCount, mountType).c_str());

    /*
     * Reconstruct the model exactly as it will exist in firmware after
     * those integer values have been written.
     */
    const auto quantizedModel =
        OnStepXModelProtocol::dequantize(
            protocol);

    /*
     * Validate the actual post-quantisation model against the captured
     * observations using the same residual geometry as the fitter.
     */
    const auto quantizedResiduals =
        evaluateResiduals(
            m_observations,
            quantizedModel,
            m_latitudeRad);

    if (!quantizedResiduals.finite)
    {
        LOGF_ERROR(
            "Calculate Model: quantised model produced "
            "non-finite residuals; %zu observations retained",
            observationCount);

        return false;
    }

    LOGF_INFO(
        "Calculate Model: quantised model validation "
        "RMS %.9f arcsec, max %.9f arcsec",
        quantizedResiduals.rmsArcsec,
        quantizedResiduals.maxAbsArcsec);

    /*
     * Keep the exact protocol values as well as the reconstructed
     * floating-point model. The next stage will use these values for
     * the controlled :SX0 transaction.
     */
    m_pendingProtocol = protocol;
    m_pendingModel = quantizedModel;
    m_hasPendingModel = true;

    LOG_INFO(
        "Calculate Model: quantised model is ready for "
        "firmware upload; Build Model remains active");

    if (!replaceFirmwareModel())
    {
        // replaceFirmwareModel() rolled the firmware back to the original
        // model.  To keep collecting uncorrected observations the session
        // model must be neutral again; if that fails, end the session.
        if (!writeAndVerify(OnStepXModelProtocol::Values {}))
        {
            abortBuild("could not resume the session after a failed upload");
            return false;
        }

        LOGF_ERROR(
            "Calculate Model: firmware replacement failed; "
            "%zu observations retained",
            observationCount);
        return false;
    }

    m_observations.clear();
    m_pendingModel = ModelCoefficients {};
    m_pendingProtocol = OnStepXModelProtocol::Values {};
    m_hasPendingModel = false;
    m_hasOriginal = false;   // the new model replaces it
    removeBackup();
    m_building = false;
    publishObservationCount();
    // Keep the summary of the uploaded model on show.
    m_fitNP.setState(IPS_OK);
    m_fitNP.apply();
    m_fitTP.setState(IPS_OK);
    m_fitTP.apply();
    m_buildSP[BUILD_ON].setState(ISS_OFF);
    m_buildSP.setState(IPS_OK);
    m_buildSP.apply();

    LOG_INFO(
        "Calculate Model: firmware model activated and persisted; "
        "Build Model completed successfully");

    return true;
}

bool OnStepXModelBuilder::readFirmwareModel(
    OnStepXModelProtocol::Values &values)
{
    if (!m_comm)
        return false;

    const auto indices =
        OnStepXModelProtocol::coefficientIndices();

    char reply[OnStepXComm::REPLY_BUF_SIZE] {};

    for (std::size_t i = 0; i < indices.size(); ++i)
    {
        char index = indices[i];
        if (i == 6)
            index = OnStepXModelProtocol::dfCoefficientIndex(
                        static_cast<OnStepXModelMath::MountType>(m_mountType));

        char cmd[OnStepXComm::CMD_MAX_LEN];
        snprintf(cmd, sizeof(cmd), ":GX0%c#", index);

        if (!m_comm->sendText(cmd, reply))
        {
            LOGF_ERROR(
                "Build Model: failed to read firmware coefficient %c",
                index);
            return false;
        }

        errno = 0;
        char *end = nullptr;
        const long long value = std::strtoll(reply, &end, 10);

        if (errno == ERANGE || end == reply || *end != '\0')
        {
            LOGF_ERROR(
                "Build Model: invalid firmware coefficient reply for %c: '%s'",
                index, reply);
            return false;
        }

        switch (i)
        {
            case 0:
                values.ax1Cor = value;
                break;
            case 1:
                values.ax2Cor = value;
                break;
            case 2:
                values.altCor = value;
                break;
            case 3:
                values.azmCor = value;
                break;
            case 4:
                values.doCor  = value;
                break;
            case 5:
                values.pdCor  = value;
                break;
            case 6:
                values.dfCor  = value;
                break;
            case 7:
                values.tfCor  = value;
                break;
            case 8:
                values.hcp    = value;
                break;
            case 9:
                values.hca    = value;
                break;
            case 10:
                values.dcp   = value;
                break;
            case 11:
                values.dca   = value;
                break;
        }
    }

    return true;
}

bool OnStepXModelBuilder::writeFirmwareModel(
    const OnStepXModelProtocol::Values &values)
{
    if (!m_comm)
        return false;

    const auto indices =
        OnStepXModelProtocol::coefficientIndices();


    for (std::size_t i = 0; i < indices.size(); ++i)
    {
        char index = indices[i];
        if (i == 6)
            index = OnStepXModelProtocol::dfCoefficientIndex(
                        static_cast<OnStepXModelMath::MountType>(m_mountType));

        std::int64_t value = 0;
        switch (i)
        {
            case 0:
                value = values.ax1Cor;
                break;
            case 1:
                value = values.ax2Cor;
                break;
            case 2:
                value = values.altCor;
                break;
            case 3:
                value = values.azmCor;
                break;
            case 4:
                value = values.doCor;
                break;
            case 5:
                value = values.pdCor;
                break;
            case 6:
                value = values.dfCor;
                break;
            case 7:
                value = values.tfCor;
                break;
            case 8:
                value = values.hcp;
                break;
            case 9:
                value = values.hca;
                break;
            case 10:
                value = values.dcp;
                break;
            case 11:
                value = values.dca;
                break;
        }

        char cmd[OnStepXComm::CMD_MAX_LEN];
        snprintf(cmd, sizeof(cmd), ":SX0%c,%lld#",
                 index, static_cast<long long>(value));

        if (!m_comm->sendBool(cmd))
        {
            LOGF_ERROR(
                "Build Model: firmware coefficient write failed for %c",
                index);
            return false;
        }
    }

    return true;
}

bool OnStepXModelBuilder::replaceFirmwareModel()
{
    if (!m_hasPendingModel || !m_comm)
    {
        LOG_ERROR("Build Model: no pending model or communication unavailable");
        return false;
    }

    // Roll back to the model that was active before the session started
    // (the firmware itself holds the neutral session model).  Without a
    // session original, fall back to whatever the firmware holds now.
    OnStepXModelProtocol::Values original = m_originalProtocol;
    if (!m_hasOriginal && !readFirmwareModel(original))
    {
        LOG_ERROR(
            "Build Model: unable to establish firmware-model rollback state; "
            "no coefficients were changed");
        return false;
    }

    const auto rollbackAndVerify =
        [&](const char *reason)
    {
        const bool writeOk = writeFirmwareModel(original);
        const bool verifyOk = writeOk && readBackMatches(original);

        LOGF_ERROR(
            "Build Model: %s; rollback write %s, readback %s",
            reason,
            writeOk ? "succeeded" : "FAILED",
            verifyOk ? "matched" : "FAILED");

        return writeOk && verifyOk;
    };

    if (!writeFirmwareModel(m_pendingProtocol))
    {
        (void)rollbackAndVerify("coefficient upload failed");
        return false;
    }

    if (!readBackMatches(m_pendingProtocol))
    {
        (void)rollbackAndVerify("firmware coefficient readback mismatch");
        return false;
    }

    if (!m_comm->sendBool(":SX09,2#"))
    {
        (void)rollbackAndVerify("firmware model activation failed");
        return false;
    }

    if (!m_comm->sendBool(":AW#"))
    {
        (void)rollbackAndVerify("firmware model persistence failed");
        return false;
    }

    return true;
}

bool OnStepXModelBuilder::readCurrentMount(double &mountRAHours,
        double &mountDecDeg,
        double &lstHours)
{
    if (!m_comm)
        return false;

    char reply[OnStepXComm::REPLY_BUF_SIZE] {};

    if (!m_comm->sendText(":GRH#", reply) || f_scansexa(reply, &mountRAHours) < 0)
    {
        LOG_ERROR("Build Model: failed to read mount RA (:GRH#)");
        return false;
    }

    if (!m_comm->sendText(":GDH#", reply) || f_scansexa(reply, &mountDecDeg) < 0)
    {
        LOG_ERROR("Build Model: failed to read mount Dec (:GDH#)");
        return false;
    }

    if (!m_comm->sendText(":GSH#", reply) || f_scansexa(reply, &lstHours) < 0)
    {
        LOG_ERROR("Build Model: failed to read sidereal time (:GSH#)");
        return false;
    }

    return true;
}

bool OnStepXModelBuilder::readLatitude(double &latitudeDeg)
{
    if (!m_comm)
        return false;

    char reply[OnStepXComm::REPLY_BUF_SIZE] {};
    if (!m_comm->sendText(":GtH#", reply) || f_scansexa(reply, &latitudeDeg) < 0)
    {
        LOG_ERROR("Build Model: failed to read site latitude (:GtH#)");
        return false;
    }

    return true;
}

double OnStepXModelBuilder::wrapHours(double hours)
{
    while (hours >= 12.0)
        hours -= 24.0;
    while (hours < -12.0)
        hours += 24.0;
    return hours;
}

OnStepXModelBuilder::SyncCapture OnStepXModelBuilder::captureSync(double ra, double dec,
        MountStatus::PierSide pierSide,
        MountStatus::MountType mountType)
{
    if (!m_building)
        return SyncCapture::NOT_BUILDING;

    double mountRA = 0.0;
    double mountDec = 0.0;
    double lst = 0.0;
    if (!readCurrentMount(mountRA, mountDec, lst))
    {
        LOG_ERROR("Build Model: Sync observation rejected because mount coordinates could not be read; "
                  "the mount was not synced");
        return SyncCapture::REJECTED; // never forward a Sync while building
    }

    if (mountType == MountStatus::MountType::UNKNOWN || mountType != m_mountType)
    {
        LOG_ERROR("Build Model: Sync observation rejected because mount type is unavailable or changed; "
                  "the mount was not synced");
        return SyncCapture::REJECTED;
    }

    const double latitude = m_latitudeRad;
    constexpr double PI = 3.14159265358979323846;
    const double actualHA = wrapHours(lst - ra) * PI / 12.0;
    const double mountHA  = wrapHours(lst - mountRA) * PI / 12.0;
    const double actualDec = dec * PI / 180.0;
    const double mountDecRad = mountDec * PI / 180.0;

    Observation obs;
    obs.actualRAHours = ra;
    obs.actualDecDeg  = dec;
    obs.mountRAHours  = mountRA;
    obs.mountDecDeg   = mountDec;
    obs.lstHours      = lst;
    OnStepXModelMath::MountType mathMountType;
    switch (mountType)
    {
        case MountStatus::MountType::GEM:
            mathMountType = OnStepXModelMath::MountType::GEM;
            break;
        case MountStatus::MountType::FORK:
            mathMountType = OnStepXModelMath::MountType::FORK;
            break;
        case MountStatus::MountType::ALTAZM:
            mathMountType = OnStepXModelMath::MountType::ALTAZM;
            break;
        case MountStatus::MountType::ALTALT:
            mathMountType = OnStepXModelMath::MountType::ALTALT;
            break;
        default:
            LOG_ERROR("Build Model: Sync observation rejected: unsupported mount type; the mount was not synced");
            return SyncCapture::REJECTED;
    }

    obs.pierSide = (pierSide == MountStatus::PierSide::WEST)
                   ? OnStepXModelMath::PierSide::WEST
                   : OnStepXModelMath::PierSide::EAST;
    obs.mountType = mathMountType;

    OnStepXModelMath::equatorialToNative(actualHA, actualDec, latitude,
                                         mathMountType,
                                         obs.actualAxis1, obs.actualAxis2);
    OnStepXModelMath::equatorialToNative(mountHA, mountDecRad, latitude,
                                         mathMountType,
                                         obs.mountAxis1, obs.mountAxis2);

    m_observations.push_back(obs);
    publishObservationCount();

    LOGF_INFO("Build Model: Sync captured as observation %zu; the mount was NOT synced "
              "(Build Model is active until Calculate Model or Abort)", m_observations.size());
    publishPreview();
    LOGF_DEBUG("Build Model: observation %zu: actual axes %.6f %.6f, "
               "mount axes %.6f %.6f, RA %.6fh Dec %.6f, pier side %d",
               m_observations.size(), obs.actualAxis1, obs.actualAxis2,
               obs.mountAxis1, obs.mountAxis2, obs.actualRAHours,
               obs.actualDecDeg, static_cast<int>(obs.pierSide));
    return SyncCapture::CAPTURED;
}
