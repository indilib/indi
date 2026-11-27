/*
    OnStepX INDI Driver — test_onstepxmodelbuilder.cpp
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
    OnStepX INDI Driver — Stage 7 model-builder transaction tests

    These tests exercise the firmware-model replacement transaction through
    the real OnStepXComm implementation.

    A local socketpair stands in for the serial connection.  StatefulFirmware
    models the firmware-side coefficient store:

        :GX0n#       reads the current RAM model
        :SX0n,value# modifies the current RAM model
        :SX09,2#     activates the model
        :AW#         persists the current model

    The tests deliberately call the transaction helper directly so that the
    numerical fitter is not part of these tests.  Fitter behaviour is tested
    separately by the existing model-fitter tests.
*/

#include <defaultdevice.h>
#include <set>
#include <vector>

#include "OnStepXModelMath.h"
#include "OnStepXModelFitter.h"
#include "OnStepXModelProtocol.h"
#include "OnStepXStatus.h"

#define private public
#include "OnStepXModelBuilder.h"
#undef private

#include "OnStepXComm.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <optional>

#include <sys/socket.h>
#include <unistd.h>

namespace
{

using Values = OnStepXModelProtocol::Values;

struct ExpectedCommand
{
    std::string command;
    std::string reply;
};

/*
 * A small stateful model of the firmware protocol used by Stage 7.
 *
 * This is deliberately not a general OnStepX emulator.  It implements only
 * the protocol operations used by replaceFirmwareModel(), while retaining
 * actual model state so that the tests can verify rollback and persistence.
 */
class StatefulFirmware
{
    public:
        explicit StatefulFirmware(
            int fd,
            Values initialModel,
            std::vector<ExpectedCommand> expectedCommands)
            : m_fd(fd),
              m_model(initialModel),
              m_persistentModel(initialModel),
              m_expectedCommands(std::move(expectedCommands))
        {
        }

        ~StatefulFirmware()
        {
            stop();
        }

        void start()
        {
            m_thread = std::thread([this]()
            {
                run();
            });
        }

        void stop()
        {
            m_stop = true;

            /*
             * Wake any blocking read/write in the responder thread.
             */
            if (m_fd >= 0)
                shutdown(m_fd, SHUT_RDWR);

            if (m_thread.joinable())
                m_thread.join();

            if (m_fd >= 0)
            {
                close(m_fd);
                m_fd = -1;
            }
        }

        bool complete() const
        {
            return m_complete.load();
        }

        bool protocolError() const
        {
            return m_protocolError.load();
        }

        Values model() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_model;
        }

        Values persistentModel() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_persistentModel;
        }

        bool activated() const
        {
            return m_activated.load();
        }

        std::vector<std::string> commands() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_commands;
        }

        /*
         * Make one GX read return a non-numeric response.
         *
         * readNumber is zero-based over all GX reads received by the fake.
         */
        void malformedRead(std::size_t readNumber,
                           std::string reply)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_malformedReadNumber = readNumber;
            m_malformedReadReply = std::move(reply);
        }

        /*
         * Make a GX read return the actual value plus delta (call once
         * per read to corrupt).
         *
         * This is used to simulate a firmware readback mismatch.
         */
        void mismatchedRead(std::size_t readNumber,
                            char index,
                            std::int64_t delta)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_mismatchReadNumbers.insert(readNumber);
            m_mismatchIndex = index;
            m_mismatchDelta = delta;
        }

        /*
         * Make a particular SX coefficient write return failure without
         * modifying the firmware model.
         */
        void failedWrite(char index)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_failedWriteIndex = index;
        }

        /*
         * Make every write of one coefficient fail.  This is used to
         * exercise rollback failure after an earlier upload failure.
         */
        void failedWriteAlways(char index)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_failedWriteAlwaysIndex = index;
        }

        /*
         * Make :SX09,2# return failure instead of activating the model.
         */
        void failedActivation()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_failActivation = true;
        }

        /*
         * Make :AW# return failure instead of persisting the current model.
         */
        void failedPersistence()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_failPersistence = true;
        }

    private:
        static bool parseIndex(const std::string &command,
                               char &index)
        {
            if (command.size() != 6 ||
                    command.compare(0, 4, ":GX0") != 0 ||
                    command[5] != '#')
                return false;

            index = command[4];
            return true;
        }

        static bool parseWrite(const std::string &command,
                               char &index,
                               std::int64_t &value)
        {
            if (command.size() < 8 ||
                    command.compare(0, 4, ":SX0") != 0 ||
                    command.back() != '#')
                return false;

            index = command[4];

            /*
             * :SX0n,value#
             *
             * Characters:
             *   0 1 2 3 4 5 ...
             *   : S X 0 n , value #
             */
            const std::string valueText =
                command.substr(6, command.size() - 7);

            if (valueText.empty())
                return false;

            char *end = nullptr;
            errno = 0;

            const long long parsed =
                std::strtoll(valueText.c_str(), &end, 10);

            if (errno == ERANGE ||
                    end == valueText.c_str() ||
                    *end != '\0')
                return false;

            value = static_cast<std::int64_t>(parsed);
            return true;
        }

        static bool getValue(const Values &values,
                             char index,
                             std::int64_t &value)
        {
            switch (index)
            {
                case '0':
                    value = values.ax1Cor;
                    return true;
                case '1':
                    value = values.ax2Cor;
                    return true;
                case '2':
                    value = values.altCor;
                    return true;
                case '3':
                    value = values.azmCor;
                    return true;
                case '4':
                    value = values.doCor;
                    return true;
                case '5':
                    value = values.pdCor;
                    return true;
                case '6':
                case '7':
                    value = values.dfCor;
                    return true;
                case '8':
                    value = values.tfCor;
                    return true;
                case 'a':
                    value = values.hcp;
                    return true;
                case 'b':
                    value = values.hca;
                    return true;
                case 'c':
                    value = values.dcp;
                    return true;
                case 'd':
                    value = values.dca;
                    return true;
                default:
                    return false;
            }
        }

        static bool setValue(Values &values,
                             char index,
                             std::int64_t value)
        {
            switch (index)
            {
                case '0':
                    values.ax1Cor = value;
                    return true;
                case '1':
                    values.ax2Cor = value;
                    return true;
                case '2':
                    values.altCor = value;
                    return true;
                case '3':
                    values.azmCor = value;
                    return true;
                case '4':
                    values.doCor  = value;
                    return true;
                case '5':
                    values.pdCor  = value;
                    return true;
                case '6':
                case '7':
                    values.dfCor = value;
                    return true;
                case '8':
                    values.tfCor = value;
                    return true;
                case 'a':
                    values.hcp   = value;
                    return true;
                case 'b':
                    values.hca   = value;
                    return true;
                case 'c':
                    values.dcp   = value;
                    return true;
                case 'd':
                    values.dca   = value;
                    return true;
                default:
                    return false;
            }
        }

        bool readCommand(std::string &command)
        {
            command.clear();

            while (!m_stop)
            {
                char c = 0;
                const ssize_t n = read(m_fd, &c, 1);

                if (n != 1)
                    return false;

                command.push_back(c);

                if (c == '#')
                    return true;
            }

            return false;
        }

        bool writeReply(const std::string &reply)
        {
            const std::string &framedReply = reply;

            std::size_t written = 0;

            while (written < framedReply.size() && !m_stop)
            {
                const ssize_t n = write(
                                      m_fd,
                                      framedReply.data() + written,
                                      framedReply.size() - written);

                if (n <= 0)
                    return false;

                written += static_cast<std::size_t>(n);
            }

            return written == framedReply.size();
        }

        bool validateExpectedCommand(const std::string &command)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_commands.push_back(command);

            if (m_nextExpected >= m_expectedCommands.size() ||
                    command != m_expectedCommands[m_nextExpected].command)
            {
                m_protocolError = true;
                return false;
            }

            ++m_nextExpected;
            return true;
        }

        bool processCommand(const std::string &command,
                            std::string &reply)
        {
            if (!validateExpectedCommand(command))
                return false;

            /*
             * Some lifecycle tests use fixed replies for non-model commands
             * such as :GtH#, :GRH#, :GDH#, and :GSH#.
             */
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                const auto &expected = m_expectedCommands[m_nextExpected - 1];
                if (!expected.reply.empty())
                {
                    reply = expected.reply;
                    return true;
                }
            }

            /*
             * Activation.
             */
            if (command == ":SX09,2#")
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                if (m_failActivation)
                {
                    reply = "0";
                    return true;
                }

                m_activated = true;
                reply = "1";
                return true;
            }

            /*
             * Persistence.
             *
             * A successful AW copies the current firmware model into the
             * simulated persistent store.
             */
            if (command == ":AW#")
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                if (m_failPersistence)
                {
                    reply = "0";
                    return true;
                }

                m_persistentModel = m_model;
                reply = "1";
                return true;
            }

            char index = 0;

            /*
             * Coefficient read.
             */
            if (parseIndex(command, index))
            {
                const std::size_t readNumber = m_readCount++;

                std::lock_guard<std::mutex> lock(m_mutex);

                if (m_malformedReadNumber.has_value() &&
                        readNumber == *m_malformedReadNumber)
                {
                    reply = m_malformedReadReply;
                    return true;
                }

                std::int64_t value = 0;

                if (!getValue(m_model, index, value))
                {
                    m_protocolError = true;
                    return false;
                }

                if (m_mismatchReadNumbers.count(readNumber) &&
                        index == m_mismatchIndex)
                {
                    value += m_mismatchDelta;
                }

                reply = std::to_string(value);
                return true;
            }

            /*
             * Coefficient write.
             */
            std::int64_t value = 0;

            if (parseWrite(command, index, value))
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                if ((m_failedWriteAlwaysIndex.has_value() &&
                        index == *m_failedWriteAlwaysIndex) ||
                        (m_failedWriteIndex.has_value() &&
                         index == *m_failedWriteIndex))
                {
                    /*
                     * failedWrite() fails once so the normal rollback path
                     * can succeed.  failedWriteAlways() is used to exercise
                     * rollback failure itself.
                     */
                    if (m_failedWriteIndex.has_value() &&
                            index == *m_failedWriteIndex)
                        m_failedWriteIndex.reset();

                    reply = "0";
                    return true;
                }

                if (!setValue(m_model, index, value))
                {
                    m_protocolError = true;
                    return false;
                }

                reply = "1";
                return true;
            }

            m_protocolError = true;
            return false;
        }

        void run()
        {
            while (!m_stop)
            {
                std::string command;

                if (!readCommand(command))
                    return;

                std::string reply;

                if (!processCommand(command, reply))
                    return;

                // :SX0n and :AW reply a bare '1'/'0'; everything else is text.
                const bool bare = command.rfind(":SX", 0) == 0 || command == ":AW#";
                if (!writeReply(bare ? reply : reply + "#"))
                    return;

                {
                    std::lock_guard<std::mutex> lock(m_mutex);

                    if (m_nextExpected == m_expectedCommands.size())
                    {
                        m_complete = true;
                        return;
                    }
                }
            }
        }

        int m_fd { -1 };

        mutable std::mutex m_mutex;

        /*
         * Current firmware-side RAM model.
         */
        Values m_model {};

        /*
         * Simulated non-volatile model.
         */
        Values m_persistentModel {};

        std::vector<ExpectedCommand> m_expectedCommands;
        std::vector<std::string> m_commands;

        std::size_t m_nextExpected { 0 };
        std::size_t m_readCount { 0 };

        /*
         * GX failure injection.
         */
        std::optional<std::size_t> m_malformedReadNumber;
        std::string m_malformedReadReply;

        std::set<std::size_t> m_mismatchReadNumbers;
        char m_mismatchIndex { 0 };
        std::int64_t m_mismatchDelta { 0 };

        /*
         * SX/SX09/AW failure injection.
         */
        std::optional<char> m_failedWriteIndex;
        std::optional<char> m_failedWriteAlwaysIndex;
        bool m_failActivation { false };
        bool m_failPersistence { false };

        std::thread m_thread;

        std::atomic<bool> m_stop { false };
        std::atomic<bool> m_complete { false };
        std::atomic<bool> m_protocolError { false };
        std::atomic<bool> m_activated { false };
};

Values makeValues(std::int64_t base)
{
    Values values;
    values.ax1Cor = base + 0;
    values.ax2Cor = base + 1;
    values.altCor = base + 2;
    values.azmCor = base + 3;
    values.doCor  = base + 4;
    values.pdCor  = base + 5;
    values.dfCor  = base + 6;
    values.tfCor  = base + 7;
    values.hcp    = base + 8;
    values.hca    = base + 9;
    values.dcp    = base + 10;
    values.dca    = base + 11;
    return values;
}

std::vector<std::pair<char, std::int64_t>>
                                        valueEntries(const Values &values, char dfIndex)
{
    return
    {
        {'0', values.ax1Cor},
        {'1', values.ax2Cor},
        {'2', values.altCor},
        {'3', values.azmCor},
        {'4', values.doCor},
        {'5', values.pdCor},
        {dfIndex, values.dfCor},
        {'8', values.tfCor},
        {'a', values.hcp},
        {'b', values.hca},
        {'c', values.dcp},
        {'d', values.dca},
    };
}

std::vector<ExpectedCommand>
readCommands(const Values &values, char dfIndex)
{
    std::vector<ExpectedCommand> commands;

    for (const auto &[index, value] : valueEntries(values, dfIndex))
    {
        (void)value;

        commands.push_back(
        {
            std::string(":GX0") + index + "#",
            ""
        });
    }

    return commands;
}

void appendWriteCommands(std::vector<ExpectedCommand> &commands,
                         const Values &values,
                         char dfIndex)
{
    for (const auto &[index, value] : valueEntries(values, dfIndex))
    {
        commands.push_back(
        {
            std::string(":SX0") + index + "," + std::to_string(value) + "#",
            ""
        });
    }
}

void appendReadCommands(std::vector<ExpectedCommand> &commands,
                        const Values &values,
                        char dfIndex)
{
    for (auto &c : readCommands(values, dfIndex))
        commands.push_back(c);
}

void appendActivationAndPersistence(
    std::vector<ExpectedCommand> &commands)
{
    commands.push_back({":SX09,2#", ""});
    commands.push_back({":AW#", ""});
}

/*
 * startBuild(): read the active model (kept for restore), write a neutral
 * all-zero model, then read it back to verify.  :GRH#/:GDH# include the
 * active model, so observations must be taken with it neutralised.
 */
void appendSessionStart(std::vector<ExpectedCommand> &commands,
                        const Values &original,
                        char dfIndex)
{
    for (auto &c : readCommands(original, dfIndex))
        commands.push_back(c);
    appendWriteCommands(commands, Values {}, dfIndex);
    for (auto &c : readCommands(Values {}, dfIndex))
        commands.push_back(c);
}

/*
 * abortBuild(): write the original model back and verify it.
 */
void appendSessionRestore(std::vector<ExpectedCommand> &commands,
                          const Values &original,
                          char dfIndex)
{
    appendWriteCommands(commands, original, dfIndex);
    for (auto &c : readCommands(original, dfIndex))
        commands.push_back(c);
}

static void expectProtocolValuesEqual(
    const OnStepXModelProtocol::Values &actual,
    const OnStepXModelProtocol::Values &expected)
{
    EXPECT_EQ(actual.ax1Cor, expected.ax1Cor);
    EXPECT_EQ(actual.ax2Cor, expected.ax2Cor);
    EXPECT_EQ(actual.altCor, expected.altCor);
    EXPECT_EQ(actual.azmCor, expected.azmCor);
    EXPECT_EQ(actual.doCor,  expected.doCor);
    EXPECT_EQ(actual.pdCor,  expected.pdCor);
    EXPECT_EQ(actual.dfCor,  expected.dfCor);
    EXPECT_EQ(actual.tfCor,  expected.tfCor);
    EXPECT_EQ(actual.hcp,    expected.hcp);
    EXPECT_EQ(actual.hca,    expected.hca);
    EXPECT_EQ(actual.dcp,    expected.dcp);
    EXPECT_EQ(actual.dca,    expected.dca);
}

} // namespace


TEST(OnStepXModelBuilderStage7,
     SuccessfulReplacementReadsWritesVerifiesActivatesAndPersists)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '7');

    appendWriteCommands(expected, pending, '7');

    for (const auto &[index, value] : valueEntries(pending, '7'))
    {
        (void)value;

        expected.push_back(
        {
            std::string(":GX0") + index + "#",
            ""
        });
    }

    appendActivationAndPersistence(expected);

    StatefulFirmware peer(fds[1], original, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_TRUE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_TRUE(peer.activated());
    expectProtocolValuesEqual(peer.model(), pending);
    expectProtocolValuesEqual(peer.persistentModel(), pending);
}


TEST(OnStepXModelBuilderStage7,
     WriteFailureRollsBackAllOriginalCoefficients)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '7');

    /*
     * The transaction writes through index 4 and then receives failure.
     * No further pending writes occur.
     */
    const auto pendingEntries = valueEntries(pending, '7');

    for (const auto &[index, value] : pendingEntries)
    {
        expected.push_back(
        {
            std::string(":SX0") + index + "," + std::to_string(value) + "#",
            ""
        });

        if (index == '4')
            break;
    }

    /*
     * Rollback writes all original coefficients, then verifies the restored
     * model with a complete GX readback.
     */
    appendWriteCommands(expected, original, '7');
    const auto rollbackReads = readCommands(original, '7');
    expected.insert(expected.end(), rollbackReads.begin(), rollbackReads.end());

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.failedWrite('4');

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_FALSE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_FALSE(peer.activated());
    expectProtocolValuesEqual(peer.model(), original);
    expectProtocolValuesEqual(peer.persistentModel(), original);
}


TEST(OnStepXModelBuilderStage7,
     RollbackWriteFailureReportsFailureWithoutVerification)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '7');

    const auto pendingEntries = valueEntries(pending, '7');

    for (const auto &[index, value] : pendingEntries)
    {
        expected.push_back(
        {
            std::string(":SX0") + index + "," + std::to_string(value) + "#",
            ""
        });

        if (index == '4')
            break;
    }

    /*
     * Rollback fails persistently at coefficient 4 after restoring
     * coefficients 0 through 3.  No rollback readback is attempted because
     * the rollback write sequence did not complete.
     */
    const auto originalEntries = valueEntries(original, '7');
    for (const auto &[index, value] : originalEntries)
    {
        expected.push_back(
        {
            std::string(":SX0") + index + "," + std::to_string(value) + "#",
            ""
        });

        if (index == '4')
            break;
    }

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.failedWriteAlways('4');

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_FALSE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_FALSE(peer.activated());

    const Values actual = peer.model();
    EXPECT_EQ(actual.ax1Cor, original.ax1Cor);
    EXPECT_EQ(actual.ax2Cor, original.ax2Cor);
    EXPECT_EQ(actual.altCor, original.altCor);
    EXPECT_EQ(actual.azmCor, original.azmCor);
    EXPECT_EQ(actual.doCor,  original.doCor);
    EXPECT_EQ(actual.pdCor,  original.pdCor);
    EXPECT_EQ(actual.dfCor,  original.dfCor);
    EXPECT_EQ(actual.tfCor,  original.tfCor);
    EXPECT_EQ(actual.hcp,    original.hcp);
    EXPECT_EQ(actual.hca,    original.hca);
    EXPECT_EQ(actual.dcp,    original.dcp);
    EXPECT_EQ(actual.dca,    original.dca);
    expectProtocolValuesEqual(peer.persistentModel(), original);
}


TEST(OnStepXModelBuilderStage7,
     ReadbackMismatchRollsBackAllOriginalCoefficients)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '7');

    appendWriteCommands(expected, pending, '7');

    /*
     * The fake injects a mismatch into the read of coefficient 3, but the
     * builder reads the complete firmware model before comparing it with
     * the pending model.  Therefore all twelve GX readback commands must
     * be expected, followed by the complete rollback.
     */
    for (const auto &[index, value] : valueEntries(pending, '7'))
    {
        (void)value;

        expected.push_back(
        {
            std::string(":GX0") + index + "#",
            ""
        });
    }

    // A mismatch is read once more (a lost reply byte), then rolled back.
    appendReadCommands(expected, pending, '7');
    appendWriteCommands(expected, original, '7');
    const auto rollbackReads = readCommands(original, '7');
    expected.insert(expected.end(), rollbackReads.begin(), rollbackReads.end());

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.mismatchedRead(15, '3', 1);   // 12 initial reads + 3
    peer.mismatchedRead(27, '3', 1);   // the second readback

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_FALSE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_FALSE(peer.activated());
    expectProtocolValuesEqual(peer.model(), original);
    expectProtocolValuesEqual(peer.persistentModel(), original);
}


TEST(OnStepXModelBuilderStage7,
     RollbackReadbackMismatchReportsFailure)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '7');

    appendWriteCommands(expected, pending, '7');

    for (const auto &[index, value] : valueEntries(pending, '7'))
    {
        (void)value;
        expected.push_back(
        {
            std::string(":GX0") + index + "#",
            ""
        });
    }

    appendReadCommands(expected, pending, '7');
    appendWriteCommands(expected, original, '7');

    /*
     * Read number 15 is coefficient 3 of the pending-model readback:
     * 12 initial reads + 3.  This forces the transaction into rollback.
     */
    auto rollbackReads = readCommands(original, '7');

    /*
     * The rollback then completes all writes, but coefficient 3 of the
     * rollback readback is deliberately wrong.
     */
    rollbackReads[3].reply = "999";
    expected.insert(expected.end(), rollbackReads.begin(), rollbackReads.end());
    expected.insert(expected.end(), rollbackReads.begin(), rollbackReads.end());

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.mismatchedRead(15, '3', 1);
    peer.mismatchedRead(27, '3', 1);

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_FALSE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_FALSE(peer.activated());
    expectProtocolValuesEqual(peer.model(), original);
    expectProtocolValuesEqual(peer.persistentModel(), original);
}


// Regression (hardware, 2026-11-11): a single corrupted readback rolled back
// a good model.  The model is read once more before it counts as a mismatch.
TEST(OnStepXModelBuilderStage7,
     TransientReadbackMismatchIsReadAgainAndActivates)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected = readCommands(original, '7');
    appendWriteCommands(expected, pending, '7');
    appendReadCommands(expected, pending, '7');
    appendReadCommands(expected, pending, '7');
    appendActivationAndPersistence(expected);

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.mismatchedRead(15, '3', 1);   // first readback only

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_TRUE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_TRUE(peer.activated());
    expectProtocolValuesEqual(peer.persistentModel(), pending);
}

TEST(OnStepXModelBuilderStage7,
     ActivationFailureRollsBackAllOriginalCoefficients)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '7');

    appendWriteCommands(expected, pending, '7');

    for (const auto &[index, value] : valueEntries(pending, '7'))
    {
        (void)value;

        expected.push_back(
        {
            std::string(":GX0") + index + "#",
            ""
        });
    }

    expected.push_back({":SX09,2#", ""});
    appendWriteCommands(expected, original, '7');
    const auto rollbackReads = readCommands(original, '7');
    expected.insert(expected.end(), rollbackReads.begin(), rollbackReads.end());

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.failedActivation();

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_FALSE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_FALSE(peer.activated());
    expectProtocolValuesEqual(peer.model(), original);
    expectProtocolValuesEqual(peer.persistentModel(), original);
}


TEST(OnStepXModelBuilderStage7,
     PersistenceFailureRollsBackAllOriginalCoefficients)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '7');

    appendWriteCommands(expected, pending, '7');

    for (const auto &[index, value] : valueEntries(pending, '7'))
    {
        (void)value;

        expected.push_back(
        {
            std::string(":GX0") + index + "#",
            ""
        });
    }

    appendActivationAndPersistence(expected);
    appendWriteCommands(expected, original, '7');
    const auto rollbackReads = readCommands(original, '7');
    expected.insert(expected.end(), rollbackReads.begin(), rollbackReads.end());

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.failedPersistence();

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_FALSE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_TRUE(peer.activated());
    expectProtocolValuesEqual(peer.model(), original);
    expectProtocolValuesEqual(peer.persistentModel(), original);
}


TEST(OnStepXModelBuilderStage7,
     ForkUsesCoefficientIndexSixForDf)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '6');

    appendWriteCommands(expected, pending, '6');

    for (const auto &[index, value] : valueEntries(pending, '6'))
    {
        (void)value;

        expected.push_back(
        {
            std::string(":GX0") + index + "#",
            ""
        });
    }

    appendActivationAndPersistence(expected);

    StatefulFirmware peer(fds[1], original, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::FORK;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_TRUE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_TRUE(peer.activated());
    expectProtocolValuesEqual(peer.model(), pending);
    expectProtocolValuesEqual(peer.persistentModel(), pending);
}


TEST(OnStepXModelBuilderStage7,
     MalformedInitialReadPreventsAnyWrite)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);
    const Values pending = makeValues(200);

    /*
     * Only six GX commands are expected because the malformed sixth reply
     * terminates the initial read before any coefficient write occurs.
     */
    std::vector<ExpectedCommand> expected;

    const auto entries = valueEntries(original, '7');

    for (std::size_t i = 0; i < 6; ++i)
    {
        expected.push_back(
        {
            std::string(":GX0") + entries[i].first + "#", ""
        });
    }

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.malformedRead(5, "not-an-integer");

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_pendingProtocol = pending;
    builder.m_hasPendingModel = true;

    peer.start();

    EXPECT_FALSE(builder.replaceFirmwareModel());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_FALSE(peer.activated());
    expectProtocolValuesEqual(peer.model(), original);
    expectProtocolValuesEqual(peer.persistentModel(), original);

    /*
     * Six GX reads and no SX writes.
     */
    const auto commands = peer.commands();
    ASSERT_EQ(commands.size(), 6u);

    for (const auto &command : commands)
        EXPECT_EQ(command.compare(0, 4, ":GX0"), 0);
}


TEST(OnStepXModelBuilderLifecycle,
     StartBuildRequiresTracking)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_tracking = false;
    builder.m_mountType = MountStatus::MountType::GEM;

    EXPECT_FALSE(builder.startBuild());
    EXPECT_FALSE(builder.isBuilding());

    close(fds[0]);
    close(fds[1]);
}


TEST(OnStepXModelBuilderLifecycle,
     StartBuildRequiresKnownMountType)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_tracking = true;
    builder.m_mountType = MountStatus::MountType::UNKNOWN;

    EXPECT_FALSE(builder.startBuild());
    EXPECT_FALSE(builder.isBuilding());

    close(fds[0]);
    close(fds[1]);
}


TEST(OnStepXModelBuilderLifecycle,
     StartBuildReadsLatitudeAndEntersBuildMode)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<ExpectedCommand> expected =
    {
        {":GtH#", "+51*30:00.000"}
    };
    appendSessionStart(expected, Values {}, '7');

    StatefulFirmware peer(fds[1], Values {}, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_tracking = true;
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_building = false;

    peer.start();

    EXPECT_TRUE(builder.startBuild());
    EXPECT_TRUE(builder.isBuilding());
    EXPECT_NEAR(builder.m_latitudeRad,
                51.5 * 3.14159265358979323846 / 180.0,
                1e-12);

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


/*
 * Regression: observations used to be taken with the active firmware model
 * applied, then fitted and uploaded as a complete replacement.  A session
 * must suspend the model, and Abort must put the original back.
 */
TEST(OnStepXModelBuilderLifecycle,
     SessionNeutralisesModelAndAbortRestoresIt)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const Values original = makeValues(100);

    std::vector<ExpectedCommand> expected =
    {
        {":GtH#", "+51*30:00.000"}
    };
    appendSessionStart(expected, original, '7');
    appendSessionRestore(expected, original, '7');

    StatefulFirmware peer(fds[1], original, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.initProperties();
    builder.m_tracking = true;
    builder.m_mountType = MountStatus::MountType::GEM;

    peer.start();

    ASSERT_TRUE(builder.startBuild());
    ASSERT_TRUE(builder.isBuilding());
    expectProtocolValuesEqual(peer.model(), Values {});

    EXPECT_TRUE(builder.abortBuild("test"));
    EXPECT_FALSE(builder.isBuilding());
    expectProtocolValuesEqual(peer.model(), original);

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


TEST(OnStepXModelBuilderLifecycle,
     RepeatedBuildOnDoesNotRestartActiveSession)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<ExpectedCommand> expected =
    {
        {":GtH#", "+51*30:00.000"}
    };
    appendSessionStart(expected, Values {}, '7');

    StatefulFirmware peer(fds[1], Values {}, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_tracking = true;
    builder.m_mountType = MountStatus::MountType::GEM;

    builder.initProperties();

    peer.start();

    ISState states[] = {ISS_ON};
    char buildName[] = "BUILD";
    char *names[] = {buildName};

    ASSERT_TRUE(
        builder.handleSwitch(
            "OSX_MODEL_BUILD",
            states,
            names,
            1));

    ASSERT_TRUE(builder.isBuilding());

    /*
     * Simulate state accumulated during the active build.
     * A second ON must not destroy either of these.
     */
    builder.m_observations.emplace_back();

    OnStepXModelMath::ModelCoefficients pendingModel;
    pendingModel.doCor = 0.001;
    builder.m_pendingModel = pendingModel;
    builder.m_hasPendingModel = true;

    ASSERT_TRUE(
        builder.handleSwitch(
            "OSX_MODEL_BUILD",
            states,
            names,
            1));

    EXPECT_TRUE(builder.isBuilding());
    EXPECT_EQ(builder.observationCount(), 1u);
    EXPECT_TRUE(builder.m_hasPendingModel);
    EXPECT_DOUBLE_EQ(builder.m_pendingModel.doCor, 0.001);

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


TEST(OnStepXModelBuilderLifecycle,
     StartBuildRejectsLatitudeReadFailure)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<ExpectedCommand> expected =
    {
        {":GtH#", "not-a-coordinate"}
    };

    StatefulFirmware peer(fds[1], Values {}, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_tracking = true;
    builder.m_mountType = MountStatus::MountType::GEM;

    peer.start();

    EXPECT_FALSE(builder.startBuild());
    EXPECT_FALSE(builder.isBuilding());

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


TEST(OnStepXModelBuilderLifecycle,
     CaptureSyncStoresObservation)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<ExpectedCommand> expected =
    {
        {":GRH#", "05:00:00.0000"},
        {":GDH#", "+20*00:00.000"},
        {":GSH#", "08:00:00.0000"}
    };

    StatefulFirmware peer(fds[1], Values {}, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_building = true;
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_latitudeRad = 51.5 * 3.14159265358979323846 / 180.0;

    peer.start();

    EXPECT_EQ(builder.captureSync(6.0, 30.0,
                                  MountStatus::PierSide::WEST,
                                  MountStatus::MountType::GEM),
              OnStepXModelBuilder::SyncCapture::CAPTURED);
    ASSERT_EQ(builder.observationCount(), 1u);

    const auto &observation = builder.m_observations.front();
    EXPECT_DOUBLE_EQ(observation.actualRAHours, 6.0);
    EXPECT_DOUBLE_EQ(observation.actualDecDeg, 30.0);
    EXPECT_DOUBLE_EQ(observation.mountRAHours, 5.0);
    EXPECT_DOUBLE_EQ(observation.mountDecDeg, 20.0);
    EXPECT_DOUBLE_EQ(observation.lstHours, 8.0);

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


TEST(OnStepXModelBuilderLifecycle,
     CaptureSyncMapsUnavailablePierSideToPositiveSide)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<ExpectedCommand> expected =
    {
        {":GRH#", "05:00:00.0000"},
        {":GDH#", "+20*00:00.000"},
        {":GSH#", "08:00:00.0000"}
    };

    StatefulFirmware peer(
        fds[1],
        Values {},
        std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_building = true;
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_latitudeRad =
        51.5 * 3.14159265358979323846 / 180.0;

    peer.start();

    EXPECT_EQ(
        builder.captureSync(
            6.0,
            30.0,
            MountStatus::PierSide::NONE,
            MountStatus::MountType::GEM),
        OnStepXModelBuilder::SyncCapture::CAPTURED);

    ASSERT_EQ(builder.observationCount(), 1u);
    EXPECT_EQ(
        builder.m_observations.front().pierSide,
        OnStepXModelMath::PierSide::EAST);

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


TEST(OnStepXModelBuilderLifecycle,
     CaptureSyncRejectsObservationWhenMountReadFails)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<ExpectedCommand> expected =
    {
        {":GRH#", "not-an-angle"}
    };

    StatefulFirmware peer(fds[1], Values {}, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_building = true;
    builder.m_mountType = MountStatus::MountType::GEM;

    peer.start();

    // Rejected, so the client's Sync fails instead of appearing to succeed.
    EXPECT_EQ(builder.captureSync(6.0, 30.0,
                                  MountStatus::PierSide::EAST,
                                  MountStatus::MountType::GEM),
              OnStepXModelBuilder::SyncCapture::REJECTED);
    EXPECT_EQ(builder.observationCount(), 0u);

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


TEST(OnStepXModelBuilderLifecycle,
     CaptureSyncRejectsObservationWhenMountTypeChanges)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<ExpectedCommand> expected =
    {
        {":GRH#", "05:00:00.0000"},
        {":GDH#", "+20*00:00.000"},
        {":GSH#", "08:00:00.0000"}
    };

    StatefulFirmware peer(fds[1], Values {}, std::move(expected));

    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.m_building = true;
    builder.m_mountType = MountStatus::MountType::GEM;

    peer.start();

    EXPECT_EQ(builder.captureSync(6.0, 30.0,
                                  MountStatus::PierSide::EAST,
                                  MountStatus::MountType::FORK),
              OnStepXModelBuilder::SyncCapture::REJECTED);
    EXPECT_EQ(builder.observationCount(), 0u);

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


TEST(OnStepXModelBuilderLifecycle,
     CaptureSyncOutsideASessionLeavesTheSyncToTheMount)
{
    OnStepXModelBuilder builder;
    builder.m_building = false;
    EXPECT_EQ(builder.captureSync(6.0, 30.0, MountStatus::PierSide::EAST, MountStatus::MountType::GEM),
              OnStepXModelBuilder::SyncCapture::NOT_BUILDING);
    EXPECT_EQ(builder.observationCount(), 0u);
}


// A captured Sync does not sync the mount, so the session must be visible:
// the observation count is published, and Busy while the session is open.
TEST(OnStepXModelBuilderLifecycle,
     ObservationCountIsPublishedWhileASessionIsOpen)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    std::vector<ExpectedCommand> expected = { {":GtH#", "+51*30:00.000"} };
    appendSessionStart(expected, Values {}, '7');
    expected.push_back({":GRH#", "05:00:00.0000"});
    expected.push_back({":GDH#", "+20*00:00.000"});
    expected.push_back({":GSH#", "08:00:00.0000"});
    appendSessionRestore(expected, Values {}, '7');

    StatefulFirmware peer(fds[1], Values {}, std::move(expected));
    OnStepXComm comm;
    comm.setFd(fds[0]);

    OnStepXModelBuilder builder;
    builder.setComm(&comm);
    builder.initProperties();
    builder.m_tracking = true;
    builder.m_mountType = MountStatus::MountType::GEM;
    peer.start();

    ASSERT_TRUE(builder.startBuild());
    EXPECT_EQ(builder.m_countNP.getState(), IPS_BUSY);
    EXPECT_EQ(builder.m_countNP[0].getValue(), 0);

    EXPECT_EQ(builder.captureSync(6.0, 30.0, MountStatus::PierSide::WEST, MountStatus::MountType::GEM),
              OnStepXModelBuilder::SyncCapture::CAPTURED);
    EXPECT_EQ(builder.m_countNP[0].getValue(), 1);

    EXPECT_TRUE(builder.abortBuild("test"));
    EXPECT_EQ(builder.m_countNP.getState(), IPS_IDLE);
    EXPECT_EQ(builder.m_countNP[0].getValue(), 0);

    peer.stop();
    close(fds[0]);
    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
}


TEST(OnStepXModelBuilderLifecycle,
     TrackingOffAbortsActiveBuild)
{
    OnStepXModelBuilder builder;
    builder.m_tracking = true;
    builder.m_building = true;
    builder.m_observations.emplace_back();

    builder.updateTrackingState(false);

    EXPECT_FALSE(builder.m_tracking);
    EXPECT_FALSE(builder.isBuilding());
    EXPECT_EQ(builder.observationCount(), 0u);
}


TEST(OnStepXModelBuilderCalculate,
     CalculateRequiresActiveBuild)
{
    OnStepXModelBuilder builder;

    EXPECT_FALSE(builder.calculateModel());
    EXPECT_FALSE(builder.isBuilding());
    EXPECT_EQ(builder.observationCount(), 0u);
    EXPECT_FALSE(builder.m_hasPendingModel);
}


TEST(OnStepXModelBuilderCalculate,
     InsufficientObservationsRetainBuildSession)
{
    OnStepXModelBuilder builder;
    builder.m_building = true;
    builder.m_latitudeRad = 51.5 * 3.14159265358979323846 / 180.0;

    EXPECT_FALSE(builder.calculateModel());
    EXPECT_TRUE(builder.isBuilding());
    EXPECT_EQ(builder.observationCount(), 0u);
    EXPECT_FALSE(builder.m_hasPendingModel);
}


TEST(OnStepXModelBuilderCalculate,
     FitterFailureRetainsObservationsAndDoesNotTouchFirmware)
{
    OnStepXModelBuilder builder;
    builder.m_building = true;
    builder.m_latitudeRad = 51.5 * 3.14159265358979323846 / 180.0;

    OnStepXModelMath::Observation observation;
    observation.mountAxis1 = 0.7;
    observation.mountAxis2 = 0.5;
    observation.actualAxis1 = 0.7;
    observation.actualAxis2 = 0.5;
    observation.pierSide = OnStepXModelMath::PierSide::EAST;
    observation.mountType = OnStepXModelMath::MountType::GEM;

    // Identical observations would still fit the index terms, so make every
    // level fail: one observation's position is not finite.
    builder.m_observations.assign(24, observation);
    builder.m_observations[5].actualAxis2 = std::nan("");

    const auto fit =
        OnStepXModelFitter::fitAdaptive(
            builder.m_observations,
            builder.m_latitudeRad);
    ASSERT_FALSE(fit.success());

    EXPECT_FALSE(builder.calculateModel());
    EXPECT_TRUE(builder.isBuilding());
    EXPECT_EQ(builder.observationCount(), 24u);
    EXPECT_FALSE(builder.m_hasPendingModel);
}


TEST(OnStepXModelBuilderCalculate,
     SuccessfulCalculateFitsQuantisesReplacesActivatesPersistsAndCompletes)
{
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    constexpr double PI = 3.14159265358979323846;
    const double latitude = 51.5 * PI / 180.0;

    OnStepXModelBuilder builder;
    builder.m_building = true;
    builder.m_mountType = MountStatus::MountType::GEM;
    builder.m_latitudeRad = latitude;

    /*
     * Generate a broad, zero-error synthetic data set from the same
     * forward model used by the fitter.  The broad axis coverage and both
     * pier sides are required to establish the twelve-parameter Jacobian
     * rank.
     */
    const OnStepXModelMath::ModelCoefficients sourceModel {};

    for (int side = 0; side < 2; ++side)
    {
        const auto pierSide =
            side == 0 ? OnStepXModelMath::PierSide::EAST
            : OnStepXModelMath::PierSide::WEST;

        for (int i = 0; i < 9; ++i)
        {
            const double axis1 = -2.4 + i * 0.6;

            for (int j = 0; j < 5; ++j)
            {
                const double axis2 = -0.9 + j * 0.45;

                OnStepXModelMath::Observation observation;
                observation.mountAxis1 = axis1;
                observation.mountAxis2 = axis2;
                observation.pierSide = pierSide;
                observation.mountType = OnStepXModelMath::MountType::GEM;

                OnStepXModelMath::predictObserved(
                    observation,
                    sourceModel,
                    latitude,
                    observation.actualAxis1,
                    observation.actualAxis2);

                builder.m_observations.push_back(observation);
            }
        }
    }

    ASSERT_EQ(builder.observationCount(), 90u);

    const auto fit =
        OnStepXModelFitter::fit(
            builder.m_observations,
            latitude);
    ASSERT_TRUE(fit.success());

    const Values pending =
        OnStepXModelProtocol::quantize(fit.model);
    const Values original = makeValues(100);

    std::vector<ExpectedCommand> expected =
        readCommands(original, '7');
    appendWriteCommands(expected, pending, '7');

    for (const auto &[index, value] : valueEntries(pending, '7'))
    {
        (void)value;
        expected.push_back(
        {
            std::string(":GX0") + index + "#",
            ""
        });
    }

    appendActivationAndPersistence(expected);

    StatefulFirmware peer(fds[1], original, std::move(expected));
    peer.start();

    OnStepXComm comm;
    comm.setFd(fds[0]);
    builder.setComm(&comm);

    // A session backup is removed once the new model replaces the original.
    const std::string path = ::testing::TempDir() + "onstepx_model_backup_calculate.txt";
    builder.setBackupPath(path);
    ASSERT_TRUE(builder.saveBackup(original));

    EXPECT_TRUE(builder.calculateModel());
    FILE *left = fopen(path.c_str(), "r");
    EXPECT_EQ(left, nullptr);
    if (left)
        fclose(left);

    peer.stop();
    close(fds[0]);

    EXPECT_TRUE(peer.complete());
    EXPECT_FALSE(peer.protocolError());
    EXPECT_TRUE(peer.activated());
    expectProtocolValuesEqual(peer.model(), pending);
    expectProtocolValuesEqual(peer.persistentModel(), pending);

    EXPECT_FALSE(builder.isBuilding());
    EXPECT_EQ(builder.observationCount(), 0u);
    EXPECT_FALSE(builder.m_hasPendingModel);
}


// ---------------------------------------------------------------------------
// Model backup across an interrupted session
//
// A session runs with an all-zero model in firmware RAM.  Without a copy on
// disk, a driver crash would leave the zero model active.
// ---------------------------------------------------------------------------
namespace
{
std::string backupPath(const char *test)
{
    std::string path = ::testing::TempDir() + "onstepx_model_backup_" + test + ".txt";
    remove(path.c_str());
    remove((path + ".bad").c_str());
    return path;
}

bool fileExists(const std::string &path)
{
    FILE *fp = fopen(path.c_str(), "r");
    if (fp)
        fclose(fp);
    return fp != nullptr;
}

// Write a backup as the builder does, for a mount type.
void writeBackup(const std::string &path, const Values &values, MountStatus::MountType mountType)
{
    OnStepXModelBuilder writer;
    writer.setBackupPath(path);
    writer.m_mountType = mountType;
    ASSERT_TRUE(writer.saveBackup(values));
}

struct BackupSession
{
    int fds[2] = {-1, -1};
    OnStepXComm comm;
    OnStepXModelBuilder builder;
    std::unique_ptr<StatefulFirmware> peer;
    bool expectsCommands { false };

    BackupSession(const Values &firmwareModel, std::vector<ExpectedCommand> expected, const std::string &path)
    {
        expectsCommands = !expected.empty();
        EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
        peer = std::make_unique<StatefulFirmware>(fds[1], firmwareModel, std::move(expected));
        comm.setFd(fds[0]);
        builder.setComm(&comm);
        builder.initProperties();
        builder.setBackupPath(path);
        builder.m_tracking = true;
        builder.m_mountType = MountStatus::MountType::GEM;
        peer->start();
    }

    void finish()
    {
        peer->stop();
        close(fds[0]);
        // The fake marks itself complete on its last expected command, so a
        // script with none never completes; those tests check commands().
        if (expectsCommands)
        {
            EXPECT_TRUE(peer->complete());
        }
        EXPECT_FALSE(peer->protocolError());
    }
};
} // namespace

TEST(OnStepXModelBuilderBackup, StartSavesTheOriginalAndAbortRemovesIt)
{
    const std::string path = backupPath("start_abort");
    const Values original = makeValues(100);

    std::vector<ExpectedCommand> expected = { {":GtH#", "+51*30:00.000"} };
    appendSessionStart(expected, original, '7');
    appendSessionRestore(expected, original, '7');
    BackupSession s(original, std::move(expected), path);

    ASSERT_TRUE(s.builder.startBuild());

    Values saved;
    int mount = -1;
    ASSERT_TRUE(s.builder.loadBackup(saved, mount));
    expectProtocolValuesEqual(saved, original);
    EXPECT_EQ(mount, static_cast<int>(MountStatus::MountType::GEM));

    EXPECT_TRUE(s.builder.abortBuild("test"));
    EXPECT_FALSE(fileExists(path));
    s.finish();
}

TEST(OnStepXModelBuilderBackup, StartRefusedWhenTheBackupCannotBeSaved)
{
    const std::string path = ::testing::TempDir() + "no_such_dir/onstepx_model_backup.txt";
    const Values original = makeValues(100);

    // The original is read, then nothing is written: the model stays active.
    std::vector<ExpectedCommand> expected = { {":GtH#", "+51*30:00.000"} };
    appendReadCommands(expected, original, '7');
    BackupSession s(original, std::move(expected), path);

    EXPECT_FALSE(s.builder.startBuild());
    EXPECT_FALSE(s.builder.isBuilding());
    s.finish();
    expectProtocolValuesEqual(s.peer->model(), original);
}

TEST(OnStepXModelBuilderBackup, FailedRestoreKeepsTheBackup)
{
    const std::string path = backupPath("failed_restore");
    const Values original = makeValues(100);

    std::vector<ExpectedCommand> expected = { {":GtH#", "+51*30:00.000"} };
    appendSessionStart(expected, original, '7');
    // Restore writes 0..3, then coefficient 4 is refused.
    for (const auto &[index, value] : valueEntries(original, '7'))
    {
        expected.push_back({ std::string(":SX0") + index + "," + std::to_string(value) + "#", "" });
        if (index == '4')
            break;
    }
    BackupSession s(original, std::move(expected), path);

    ASSERT_TRUE(s.builder.startBuild());
    s.peer->failedWriteAlways('4');
    EXPECT_FALSE(s.builder.abortBuild("test"));
    EXPECT_TRUE(fileExists(path));
    s.finish();
    remove(path.c_str());
}

TEST(OnStepXModelBuilderBackup, RecoveryRestoresASuspendedModel)
{
    const std::string path = backupPath("recover_zero");
    const Values original = makeValues(100);
    writeBackup(path, original, MountStatus::MountType::GEM);

    std::vector<ExpectedCommand> expected;
    appendReadCommands(expected, Values {}, '7');
    appendSessionRestore(expected, original, '7');
    BackupSession s(Values {}, std::move(expected), path);
    s.builder.m_mountType = MountStatus::MountType::UNKNOWN;   // as after a reconnect

    s.builder.recoverInterruptedSession(MountStatus::MountType::GEM);
    s.finish();
    expectProtocolValuesEqual(s.peer->model(), original);
    EXPECT_FALSE(fileExists(path));
}

TEST(OnStepXModelBuilderBackup, RecoveryDiscardsABackupTheFirmwareAlreadyHolds)
{
    const std::string path = backupPath("recover_same");
    const Values original = makeValues(100);
    writeBackup(path, original, MountStatus::MountType::GEM);

    std::vector<ExpectedCommand> expected;
    appendReadCommands(expected, original, '7');
    BackupSession s(original, std::move(expected), path);

    s.builder.recoverInterruptedSession(MountStatus::MountType::GEM);
    s.finish();
    EXPECT_FALSE(fileExists(path));
}

TEST(OnStepXModelBuilderBackup, RecoveryKeepsADifferentFirmwareModel)
{
    const std::string path = backupPath("recover_other");
    writeBackup(path, makeValues(100), MountStatus::MountType::GEM);
    const Values other = makeValues(500);

    std::vector<ExpectedCommand> expected;
    appendReadCommands(expected, other, '7');
    BackupSession s(other, std::move(expected), path);

    s.builder.recoverInterruptedSession(MountStatus::MountType::GEM);
    s.finish();
    expectProtocolValuesEqual(s.peer->model(), other);
    EXPECT_FALSE(fileExists(path));
}

TEST(OnStepXModelBuilderBackup, RecoveryIgnoresABackupForAnotherMountType)
{
    const std::string path = backupPath("recover_mount");
    writeBackup(path, makeValues(100), MountStatus::MountType::ALTAZM);

    BackupSession s(Values {}, {}, path);
    s.builder.recoverInterruptedSession(MountStatus::MountType::GEM);
    s.finish();
    EXPECT_TRUE(s.peer->commands().empty());
    EXPECT_FALSE(fileExists(path));
}

TEST(OnStepXModelBuilderBackup, RecoveryKeepsAnUnreadableBackupForInspection)
{
    const std::string path = backupPath("recover_bad");
    FILE *fp = fopen(path.c_str(), "w");
    ASSERT_NE(fp, nullptr);
    fputs("mount 0\nax1Cor 12\n", fp);   // truncated
    fclose(fp);

    BackupSession s(Values {}, {}, path);
    s.builder.recoverInterruptedSession(MountStatus::MountType::GEM);
    s.finish();
    EXPECT_TRUE(s.peer->commands().empty());
    EXPECT_FALSE(fileExists(path));
    EXPECT_TRUE(fileExists(path + ".bad"));
    remove((path + ".bad").c_str());
}

TEST(OnStepXModelBuilderBackup, NoBackupMeansNoCommands)
{
    const std::string path = backupPath("recover_none");
    BackupSession s(Values {}, {}, path);
    s.builder.recoverInterruptedSession(MountStatus::MountType::GEM);
    s.finish();
    EXPECT_TRUE(s.peer->commands().empty());
}


// ---------------------------------------------------------------------------
// Progress feedback: the preview refitted after each captured observation
// ---------------------------------------------------------------------------

namespace
{
// An observation of a GEM with index and polar alignment errors only.
OnStepXModelMath::Observation previewObservation(double ha, double dec, bool west, double latitude)
{
    constexpr double ARCSEC = 3.14159265358979323846 / 180.0 / 3600.0;
    OnStepXModelMath::ModelCoefficients model;
    model.ax1Cor = 300.0 * ARCSEC;
    model.ax2Cor = -200.0 * ARCSEC;
    model.altCor = 150.0 * ARCSEC;
    model.azmCor = -120.0 * ARCSEC;

    OnStepXModelMath::Observation observation;
    observation.mountType = OnStepXModelMath::MountType::GEM;
    observation.pierSide = west ? OnStepXModelMath::PierSide::WEST : OnStepXModelMath::PierSide::EAST;
    observation.mountAxis1 = ha;
    observation.mountAxis2 = dec;
    OnStepXModelMath::predictObserved(observation, model, latitude,
                                      observation.actualAxis1, observation.actualAxis2);
    return observation;
}
}

TEST(OnStepXModelBuilderPreview, ModelGainsTermsAsObservationsArrive)
{
    constexpr double latitude = 51.5 * 3.14159265358979323846 / 180.0;
    OnStepXModelBuilder builder;
    builder.initProperties();
    builder.m_building = true;
    builder.m_latitudeRad = latitude;
    builder.clearPreview();
    EXPECT_EQ(builder.m_fitNP.getState(), IPS_BUSY);
    EXPECT_STREQ(builder.m_fitTP[0].getText(), "none yet");
    EXPECT_STREQ(builder.m_fitTP[1].getText(), "2 more observations for a first model");

    builder.m_observations.push_back(previewObservation(-0.8, 0.2, false, latitude));
    builder.publishPreview();
    EXPECT_EQ(builder.m_fitNP[0].getValue(), 0.0);
    EXPECT_STREQ(builder.m_fitTP[1].getText(), "1 more observation for a first model");

    builder.m_observations.push_back(previewObservation(0.6, 0.9, true, latitude));
    builder.publishPreview();
    EXPECT_EQ(builder.m_fitNP[0].getValue(), 2.0);
    EXPECT_GT(builder.m_fitNP[1].getValue(), 1.0);     // the polar error isn't fitted yet
    EXPECT_STREQ(builder.m_fitTP[0].getText(), "index offsets (2 terms)");
    EXPECT_STREQ(builder.m_fitTP[1].getText(), "1 more observation to try polar axis alignment");

    builder.m_observations.push_back(previewObservation(0.1, -0.3, false, latitude));
    builder.m_observations.push_back(previewObservation(1.2, 0.5, true, latitude));
    builder.publishPreview();
    EXPECT_EQ(builder.m_fitNP[0].getValue(), 4.0);
    EXPECT_LT(builder.m_fitNP[1].getValue(), 1.0e-3);  // index + polar errors fully fitted
    EXPECT_STREQ(builder.m_fitTP[0].getText(), "index offsets, polar axis alignment (4 terms)");
    EXPECT_EQ(builder.m_fitTP.getState(), IPS_BUSY);
    // Cone error is tried at 4 observations; it adds nothing here.
    EXPECT_STREQ(builder.m_fitTP[1].getText(),
                 "cone error: no significant improvement yet; more observations may show it");

    builder.m_building = false;
    builder.clearPreview();
    EXPECT_EQ(builder.m_fitNP.getState(), IPS_IDLE);
    EXPECT_EQ(builder.m_fitNP[0].getValue(), 0.0);
    EXPECT_STREQ(builder.m_fitTP[0].getText(), "");
}

TEST(OnStepXModelBuilderPreview, NextStepNamesWhatHoldsTheModelBack)
{
    using Fitter = OnStepXModelFitter;
    using Outcome = Fitter::Level::Outcome;
    const auto gem = OnStepXModelMath::MountType::GEM;

    Fitter::AdaptiveResult fit;
    for (std::size_t i = 0; i < Fitter::TERM_LEVELS; ++i)
        fit.levels[i].terms = static_cast<Fitter::Terms>(i);
    fit.levels[0].outcome = Outcome::USED;
    fit.levels[0].fit.freeParameters = 2;
    fit.chosen = 0;

    fit.levels[1].outcome = Outcome::NOT_SEPARABLE;
    EXPECT_EQ(OnStepXModelBuilder::describeNext(fit, 8, gem),
              "polar axis alignment: the observations can't separate it yet; "
              "spread them across the sky and both sides of the meridian");
    EXPECT_EQ(OnStepXModelBuilder::describeNext(fit, 8, OnStepXModelMath::MountType::ALTAZM),
              "azimuth axis tilt: the observations can't separate it yet; spread them across the sky");

    fit.levels[1].outcome = Outcome::USED;      // fitted, but dropped for the firmware's limits
    EXPECT_EQ(OnStepXModelBuilder::describeNext(fit, 8, gem),
              "polar axis alignment: fitted, but outside the firmware's limits");

    fit.levels[1].outcome = Outcome::FAILED;
    EXPECT_EQ(OnStepXModelBuilder::describeNext(fit, 8, gem), "polar axis alignment: the fit failed");

    for (std::size_t i = 1; i < Fitter::TERM_LEVELS; ++i)
        fit.levels[i].outcome = Outcome::USED;
    fit.chosen = 4;
    fit.levels[4].fit.freeParameters = 12;
    EXPECT_EQ(OnStepXModelBuilder::describeNext(fit, 20, gem), "all terms fitted");
    EXPECT_EQ(OnStepXModelBuilder::describeModel(fit, gem),
              "index offsets, polar axis alignment, cone error, axis non-perpendicularity and flexure, "
              "periodic errors (12 terms)");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}