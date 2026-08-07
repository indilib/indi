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

#include <optional>
#include <utility>

namespace geminipbh
{

enum class Error
{
    None,
    InvalidChannel,
    InvalidValue,
    NotConnected,
    CommunicationFailure,
    QueueRejected,
    ProtocolRejected,
    ValueUnavailable,
    InvalidState,
    Busy,
    Cancelled
};

class Result
{
    public:
        constexpr Result() = default;

        static constexpr Result success()
        {
            return Result(Error::None);
        }
        static constexpr Result failure(Error error)
        {
            return Result(error == Error::None ? Error::ProtocolRejected : error);
        }

        constexpr bool ok() const
        {
            return error_ == Error::None;
        }
        constexpr explicit operator bool() const
        {
            return ok();
        }
        constexpr Error error() const
        {
            return error_;
        }

    private:
        constexpr explicit Result(Error error) : error_(error) {}

        Error error_ = Error::None;
};

template <typename T>
class ValueResult
{
    public:
        explicit ValueResult(T value) : error_(Error::None), value_(std::move(value)) {}

        static ValueResult failure(Error error)
        {
            return ValueResult(error == Error::None ? Error::ValueUnavailable : error);
        }

        bool ok() const
        {
            return error_ == Error::None;
        }
        explicit operator bool() const
        {
            return ok();
        }
        Error error() const
        {
            return error_;
        }
        const T &value() const
        {
            return *value_;
        }

    private:
        explicit ValueResult(Error error) : error_(error) {}

        Error error_ = Error::None;
        std::optional<T> value_;
};

} // namespace geminipbh
