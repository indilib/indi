/*
    Gemini Power Box Hub Advanced v3 INDI Driver

    Copyright (C) 2026 Dieter R Kedrowitsch <dieter@kedrowitsch.net>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA  02111-1307, USA.

    The full GNU General Public License is included in this distribution in the
    file called LICENSE.
*/

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

#include "indi-gemini-pbh/result.h"
#include "indi-gemini-pbh/telemetry_snapshot.h"

namespace geminipbh
{

enum class OperationKind
{
    DcOutput,
    UsbOutput,
    HeaterEnabled,
    HeaterMode,
    HeaterManualPower
};

enum class Confirmation
{
    Confirmed,
    NotConfirmed,
    NotConfirmable,
    InvalidTarget,
    ValueUnavailable
};

class DcOutputTarget
{
    public:
        static ValueResult<DcOutputTarget> create(std::size_t channel, bool enabled)
        {
            if (channel >= TelemetrySnapshot::DcOutputCount)
                return ValueResult<DcOutputTarget>::failure(Error::InvalidChannel);
            return ValueResult<DcOutputTarget>(DcOutputTarget(channel, enabled));
        }

        std::size_t channel() const
        {
            return channel_;
        }
        bool enabled() const
        {
            return enabled_;
        }

    private:
        DcOutputTarget(std::size_t channel, bool enabled) : channel_(channel), enabled_(enabled) {}

        std::size_t channel_ = 0;
        bool enabled_ = false;
};

class UsbOutputTarget
{
    public:
        static ValueResult<UsbOutputTarget> create(std::size_t channel, bool enabled)
        {
            if (channel >= TelemetrySnapshot::UsbOutputCount)
                return ValueResult<UsbOutputTarget>::failure(Error::InvalidChannel);
            return ValueResult<UsbOutputTarget>(UsbOutputTarget(channel, enabled));
        }

        std::size_t channel() const
        {
            return channel_;
        }
        bool enabled() const
        {
            return enabled_;
        }

    private:
        UsbOutputTarget(std::size_t channel, bool enabled) : channel_(channel), enabled_(enabled) {}

        std::size_t channel_ = 0;
        bool enabled_ = false;
};

class HeaterEnabledTarget
{
    public:
        static ValueResult<HeaterEnabledTarget> create(std::size_t channel, bool enabled)
        {
            if (channel >= TelemetrySnapshot::HeaterCount)
                return ValueResult<HeaterEnabledTarget>::failure(Error::InvalidChannel);
            return ValueResult<HeaterEnabledTarget>(HeaterEnabledTarget(channel, enabled));
        }

        std::size_t channel() const
        {
            return channel_;
        }
        bool enabled() const
        {
            return enabled_;
        }

    private:
        HeaterEnabledTarget(std::size_t channel, bool enabled) : channel_(channel), enabled_(enabled) {}

        std::size_t channel_ = 0;
        bool enabled_ = false;
};

class HeaterModeTarget
{
    public:
        static ValueResult<HeaterModeTarget> create(std::size_t channel, HeaterMode mode)
        {
            if (channel >= TelemetrySnapshot::HeaterCount)
                return ValueResult<HeaterModeTarget>::failure(Error::InvalidChannel);
            if (mode != HeaterMode::Auto && mode != HeaterMode::ManualPwm && mode != HeaterMode::BinarySwitch)
                return ValueResult<HeaterModeTarget>::failure(Error::InvalidValue);
            return ValueResult<HeaterModeTarget>(HeaterModeTarget(channel, mode));
        }

        std::size_t channel() const
        {
            return channel_;
        }
        HeaterMode mode() const
        {
            return mode_;
        }

    private:
        HeaterModeTarget(std::size_t channel, HeaterMode mode) : channel_(channel), mode_(mode) {}

        std::size_t channel_ = 0;
        HeaterMode mode_ = HeaterMode::Auto;
};

class HeaterManualPowerTarget
{
    public:
        static ValueResult<HeaterManualPowerTarget> create(std::size_t channel, unsigned percent)
        {
            if (channel >= TelemetrySnapshot::HeaterCount)
                return ValueResult<HeaterManualPowerTarget>::failure(Error::InvalidChannel);
            if (percent > 100)
                return ValueResult<HeaterManualPowerTarget>::failure(Error::InvalidValue);
            return ValueResult<HeaterManualPowerTarget>(HeaterManualPowerTarget(channel, percent));
        }

        std::size_t channel() const
        {
            return channel_;
        }
        unsigned percent() const
        {
            return percent_;
        }

    private:
        HeaterManualPowerTarget(std::size_t channel, unsigned percent) : channel_(channel), percent_(percent) {}

        std::size_t channel_ = 0;
        unsigned percent_ = 0;
};

class OperationTarget
{
    public:
        static ValueResult<OperationTarget> dcOutput(std::size_t channel, bool enabled)
        {
            auto target = DcOutputTarget::create(channel, enabled);
            if (!target) return ValueResult<OperationTarget>::failure(target.error());
            return ValueResult<OperationTarget>(OperationTarget(target.value()));
        }

        static ValueResult<OperationTarget> usbOutput(std::size_t channel, bool enabled)
        {
            auto target = UsbOutputTarget::create(channel, enabled);
            if (!target) return ValueResult<OperationTarget>::failure(target.error());
            return ValueResult<OperationTarget>(OperationTarget(target.value()));
        }

        static ValueResult<OperationTarget> heaterEnabled(std::size_t channel, bool enabled)
        {
            auto target = HeaterEnabledTarget::create(channel, enabled);
            if (!target) return ValueResult<OperationTarget>::failure(target.error());
            return ValueResult<OperationTarget>(OperationTarget(target.value()));
        }

        static ValueResult<OperationTarget> heaterMode(std::size_t channel, HeaterMode mode)
        {
            auto target = HeaterModeTarget::create(channel, mode);
            if (!target) return ValueResult<OperationTarget>::failure(target.error());
            return ValueResult<OperationTarget>(OperationTarget(target.value()));
        }

        static ValueResult<OperationTarget> heaterManualPower(std::size_t channel, unsigned percent)
        {
            auto target = HeaterManualPowerTarget::create(channel, percent);
            if (!target) return ValueResult<OperationTarget>::failure(target.error());
            return ValueResult<OperationTarget>(OperationTarget(target.value()));
        }

        OperationKind kind() const
        {
            return std::visit([](const auto & target) -> OperationKind
            {
                using Target = std::decay_t<decltype(target)>;
                if constexpr (std::is_same_v<Target, DcOutputTarget>) return OperationKind::DcOutput;
                if constexpr (std::is_same_v<Target, UsbOutputTarget>) return OperationKind::UsbOutput;
                if constexpr (std::is_same_v<Target, HeaterEnabledTarget>) return OperationKind::HeaterEnabled;
                if constexpr (std::is_same_v<Target, HeaterModeTarget>) return OperationKind::HeaterMode;
                return OperationKind::HeaterManualPower;
            }, target_);
        }

    private:
        using Variant =
            std::variant<DcOutputTarget, UsbOutputTarget, HeaterEnabledTarget, HeaterModeTarget, HeaterManualPowerTarget>;

        explicit OperationTarget(Variant target) : target_(std::move(target)) {}

        Variant target_;

        friend Confirmation confirmOperation(const OperationTarget &, const TelemetrySnapshot *);
};

inline Confirmation confirmOperation(const DcOutputTarget &target, const TelemetrySnapshot *snapshot)
{
    if (!snapshot) return Confirmation::ValueUnavailable;
    if (target.channel() >= snapshot->dcOutputs.size()) return Confirmation::InvalidTarget;
    return snapshot->dcOutputs[target.channel()] == target.enabled() ? Confirmation::Confirmed : Confirmation::NotConfirmed;
}

inline Confirmation confirmOperation(const UsbOutputTarget &target, const TelemetrySnapshot *snapshot)
{
    if (!snapshot) return Confirmation::ValueUnavailable;
    if (target.channel() >= snapshot->usbOutputs.size()) return Confirmation::InvalidTarget;
    return snapshot->usbOutputs[target.channel()] == target.enabled() ? Confirmation::Confirmed : Confirmation::NotConfirmed;
}

inline Confirmation confirmOperation(const HeaterEnabledTarget &target, const TelemetrySnapshot *snapshot)
{
    if (!snapshot) return Confirmation::ValueUnavailable;
    if (target.channel() >= snapshot->heaterEnabled.size()) return Confirmation::InvalidTarget;
    return snapshot->heaterEnabled[target.channel()] == target.enabled() ? Confirmation::Confirmed : Confirmation::NotConfirmed;
}

inline Confirmation confirmOperation(const HeaterModeTarget &target, const TelemetrySnapshot *snapshot)
{
    if (!snapshot) return Confirmation::ValueUnavailable;
    if (target.channel() >= snapshot->heaterModes.size()) return Confirmation::InvalidTarget;
    return snapshot->heaterModes[target.channel()] == target.mode() ? Confirmation::Confirmed : Confirmation::NotConfirmed;
}

inline Confirmation confirmOperation(const HeaterManualPowerTarget &, const TelemetrySnapshot *)
{
    return Confirmation::NotConfirmable;
}

inline Confirmation confirmOperation(const OperationTarget &target, const TelemetrySnapshot *snapshot)
{
    return std::visit([snapshot](const auto & typedTarget)
    {
        return confirmOperation(typedTarget, snapshot);
    }, target.target_);
}

inline Confirmation confirmOperation(const OperationTarget &target, const TelemetrySnapshot &snapshot)
{
    return confirmOperation(target, &snapshot);
}

} // namespace geminipbh
