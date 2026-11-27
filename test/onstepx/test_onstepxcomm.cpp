/*
    OnStepX INDI Driver — test_onstepxcomm.cpp
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
    OnStepX INDI Driver — OnStepXComm unit tests

    Uses socketpair() to create a pair of connected file descriptors.
    fds[0] is given to OnStepXComm (the "driver" end).
    fds[1] is used by the test to play the firmware: it reads commands and
    writes replies using the framing rules of OnStepX ProcessCmds.cpp:

      text reply    "value#"
      bool reply    bare '1' / '0' (no '#'); unknown commands reply '0'
      no reply      nothing at all
      char reply    one unframed character (e.g. :MS#)

    No INDI device instance is needed; setDevice() is never called.
*/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "OnStepXComm.h"

namespace
{

// Read bytes from fd until '#' (inclusive); return everything before '#'.
std::string readCmd(int fd)
{
    std::string result;
    char c;
    while (read(fd, &c, 1) == 1 && c != '#')
        result += c;
    return result;
}

void writeRaw(int fd, const std::string &s)
{
    if (write(fd, s.c_str(), s.size()) < 0) {}
}

bool readable(int fd, int ms)
{
    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(fd, &rfd);
    struct timeval tv;
    tv.tv_sec  = static_cast<time_t>(ms / 1000);
    tv.tv_usec = static_cast<suseconds_t>((ms % 1000) * 1000);
    return select(fd + 1, &rfd, nullptr, nullptr, &tv) > 0;
}

// Firmware side: drop commands nobody answered.
void discardInput(int fd)
{
    char buf[256];
    while (readable(fd, 0) && read(fd, buf, sizeof(buf)) > 0) {}
}

// Fixture: socketpair + comm bound to the driver end.
class CommTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
            comm.setFd(fds[0]);
        }
        void TearDown() override
        {
            if (responder.joinable())
                responder.join();
            close(fds[0]);
            if (fds[1] >= 0)
                close(fds[1]);
        }

        // Firmware side: read one command, record it, then write raw reply.
        void respond(const std::string &raw)
        {
            responder = std::thread([this, raw]()
            {
                received = readCmd(fds[1]);
                writeRaw(fds[1], raw);
            });
        }

        // Late-reply tests: the firmware side sends its "late" reply only once
        // the driver's command has timed out, and then soon, so the reply
        // lands inside the driver's resync quiet window (250 ms) on a slow CI
        // runner too.  Fixed sleeps against the timeout left 50 ms of margin
        // and failed on macOS CI.  LATE_MS keeps the reply after the pre-send
        // flush, so only the resync can discard it.
        static constexpr int LATE_MS = 50;
        void driverTimedOut()
        {
            timedOut.set_value();
        }
        void waitForDriverTimeout()
        {
            timedOut.get_future().wait_for(std::chrono::seconds(2));   // no hang if the test fails first
            std::this_thread::sleep_for(std::chrono::milliseconds(LATE_MS));
        }

        int fds[2] { -1, -1 };
        OnStepXComm comm;
        std::thread responder;
        std::string received;
        std::promise<void> timedOut;
};

}

// ---------------------------------------------------------------------------
// sendText
// ---------------------------------------------------------------------------
TEST_F(CommTest, text_happy_path)
{
    respond("On-Step#");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GVP#", reply));
    EXPECT_STREQ(reply, "On-Step");
    responder.join();
    EXPECT_EQ(received, ":GVP");
}

TEST_F(CommTest, text_framed_zero_and_one_are_values)
{
    // "0#" / "1#" are legitimate text values (e.g. :GX95#), not errors.
    respond("0#");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GX95#", reply));
    EXPECT_STREQ(reply, "0");
}

TEST_F(CommTest, text_bare_zero_is_firmware_error)
{
    // e.g. :GX9F# without an MCU sensor, :GW# without a mount
    respond("0");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.sendText(":GX9F#", reply, 2000, true));
    EXPECT_STREQ(reply, "0");
}

TEST_F(CommTest, text_slow_framed_reply_is_not_mistaken_for_bare)
{
    // '1' then a pause shorter than the bare-reply window, then the rest.
    responder = std::thread([this]()
    {
        readCmd(fds[1]);
        writeRaw(fds[1], "1");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        writeRaw(fds[1], "0.28q#");
    });
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GVN#", reply));
    EXPECT_STREQ(reply, "10.28q");
}

TEST_F(CommTest, text_partial_unterminated_reply_fails)
{
    // Old behaviour accepted a partial reply after a 50 ms gap.
    respond("10.2");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.sendText(":GVN#", reply, 400, true));
}

TEST_F(CommTest, text_timeout)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.sendText(":GVP#", reply, 100, true));
}

TEST_F(CommTest, text_max_firmware_reply)
{
    // The longest reply the firmware can produce (79 chars in its reply[80]).
    const std::string longest(79, 'x');
    respond(longest + "#");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GVC#", reply));
    EXPECT_EQ(std::string(reply), longest);
}

TEST_F(CommTest, text_overlong_reply_bounded)
{
    respond(std::string(300, 'y') + "#");

    // Canary region after the REPLY_BUF_SIZE bytes callers must provide.
    char big[OnStepXComm::REPLY_BUF_SIZE + 64];
    memset(big, 'Z', sizeof(big));
    EXPECT_FALSE(comm.sendText(":GVC#", big, 2000, true));
    for (size_t i = OnStepXComm::REPLY_BUF_SIZE; i < sizeof(big); i++)
        ASSERT_EQ(big[i], 'Z') << "overflow at byte " << i;
}

// ---------------------------------------------------------------------------
// sendBool
// ---------------------------------------------------------------------------
TEST_F(CommTest, bool_one_is_true)
{
    respond("1");
    EXPECT_TRUE(comm.sendBool(":Te#"));
    responder.join();
    EXPECT_EQ(received, ":Te");
}

TEST_F(CommTest, bool_zero_is_false)
{
    respond("0");
    EXPECT_FALSE(comm.sendBool(":Te#", 2000, true));
}

TEST_F(CommTest, bool_no_reply_is_false)
{
    EXPECT_FALSE(comm.sendBool(":Te#", 100, true));
}

TEST_F(CommTest, bool_unexpected_reply_is_drained)
{
    // A mis-classified command answering text must not leak into the next reply.
    respond("Hello#");
    EXPECT_FALSE(comm.sendBool(":XX#", 2000, true));
    responder.join();

    respond("Next#");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GVP#", reply));
    EXPECT_STREQ(reply, "Next");
}

// ---------------------------------------------------------------------------
// sendNoReply
// ---------------------------------------------------------------------------
TEST_F(CommTest, noreply_sends_and_returns_immediately)
{
    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(comm.sendNoReply(":Q#"));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, std::chrono::milliseconds(50));
    EXPECT_EQ(readCmd(fds[1]), ":Q");
}

// Abort's :Q# must not wait out a resync left by a controller stall (up to
// RESYNC_MAX_MS), and the resync must still protect the next command.
TEST_F(CommTest, urgent_skips_pending_resync_which_still_protects_next_reply)
{
    std::string urgent;
    responder = std::thread([this, &urgent]()
    {
        readCmd(fds[1]);                                   // :GU#
        waitForDriverTimeout();
        writeRaw(fds[1], "nNpE000#");                      // late, after the timeout
        urgent = readCmd(fds[1]);                          // :Q#
        received = readCmd(fds[1]);                        // :GRH#
        writeRaw(fds[1], "12:34:56.0000#");
    });

    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.sendText(":GU#", reply, 200, true));   // leaves a resync pending
    driverTimedOut();

    const auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(comm.sendUrgent(":Q#"));
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(50));

    EXPECT_TRUE(comm.sendText(":GRH#", reply));
    EXPECT_STREQ(reply, "12:34:56.0000");
    responder.join();
    EXPECT_EQ(urgent, ":Q");
    EXPECT_EQ(received, ":GRH");
}

TEST_F(CommTest, urgent_not_connected_sends_nothing)
{
    comm.setFd(-1);
    EXPECT_FALSE(comm.sendUrgent(":Q#"));
    EXPECT_FALSE(readable(fds[1], 50));
}

// ---------------------------------------------------------------------------
// Link health
// ---------------------------------------------------------------------------
TEST_F(CommTest, link_failures_are_counted_and_any_reply_resets_them)
{
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.sendText(":GU#", reply, 100, true));      // nothing comes back
    EXPECT_FALSE(comm.sendBool(":Te#", 100, true));
    EXPECT_EQ(comm.failStreak(), 2);

    discardInput(fds[1]);                                       // the unanswered commands
    respond("0");                                               // a bare '0' still proves the link
    EXPECT_FALSE(comm.sendBool(":Te#"));
    responder.join();
    EXPECT_EQ(comm.failStreak(), 0);
}

TEST_F(CommTest, link_lost_needs_both_the_time_and_the_failures)
{
    comm.setLinkLostSeconds(0.3);
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.sendText(":GU#", reply, 100, true));
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EXPECT_FALSE(comm.linkLost());                              // long enough, but one failure

    EXPECT_FALSE(comm.sendText(":GU#", reply, 100, true));
    EXPECT_FALSE(comm.sendText(":GU#", reply, 100, true));
    EXPECT_TRUE(comm.linkLost());

    discardInput(fds[1]);
    respond("nNpE000#");
    EXPECT_TRUE(comm.sendText(":GU#", reply));
    responder.join();
    EXPECT_FALSE(comm.linkLost());
}

TEST_F(CommTest, reopening_the_link_resets_its_health)
{
    comm.setLinkLostSeconds(0);
    for (int i = 0; i < 3; ++i)
        EXPECT_FALSE(comm.sendBool(":Te#", 50, true));
    EXPECT_TRUE(comm.linkLost());
    comm.setFd(fds[0]);
    EXPECT_EQ(comm.failStreak(), 0);
    EXPECT_FALSE(comm.linkLost());
}

// A dropped network connection (WiFi, SmartWebServer restarted): the write
// must fail and count against the link.  A plain write() would raise
// SIGPIPE and kill the driver, and this test with it.
TEST_F(CommTest, write_to_a_dropped_connection_fails_without_sigpipe)
{
    close(fds[1]);
    fds[1] = -1;
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.sendText(":GU#", reply, 100, true));
    EXPECT_FALSE(comm.sendUrgent(":Q#"));
    EXPECT_EQ(comm.failStreak(), 2);
}

// Regression: a bare reply left by one command must not be glued onto the
// next command's reply.
TEST_F(CommTest, stale_bare_reply_does_not_corrupt_next_reply)
{
    writeRaw(fds[1], "1");   // stale byte, e.g. from an earlier command
    usleep(10000);

    respond("+45*30:00.000#");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GDH#", reply));
    EXPECT_STREQ(reply, "+45*30:00.000");
}

// Seen on hardware (OnStepX 10.29c on ESP32): bursts of 0xFF ahead of a
// reply around NV writes.  Text/bool/char replies never contain 0xFF.
TEST_F(CommTest, text_ignores_line_noise)
{
    respond(std::string(64, '\xff') + "+00:00#");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GG#", reply));
    EXPECT_STREQ(reply, "+00:00");
}

TEST_F(CommTest, bool_ignores_line_noise)
{
    respond(std::string(64, '\xff') + "1");
    EXPECT_TRUE(comm.sendBool(":Te#"));
}

// Seen on hardware: a controller stall makes a reply arrive after the
// timeout.  The late bytes must not be taken as the NEXT command's reply.
TEST_F(CommTest, late_reply_does_not_shift_next_reply)
{
    responder = std::thread([this]()
    {
        readCmd(fds[1]);                                   // :GU#
        waitForDriverTimeout();
        writeRaw(fds[1], "nNpE000#");                      // arrives after the timeout
        received = readCmd(fds[1]);                        // :GRH#
        writeRaw(fds[1], "12:34:56.0000#");
    });

    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.sendText(":GU#", reply, 200, true));
    driverTimedOut();
    EXPECT_TRUE(comm.sendText(":GRH#", reply));
    EXPECT_STREQ(reply, "12:34:56.0000");
}

TEST_F(CommTest, late_binary_reply_does_not_shift_next_reply)
{
    responder = std::thread([this]()
    {
        readCmd(fds[1]);                                   // :Gu#
        waitForDriverTimeout();
        writeRaw(fds[1], std::string("\x83\xb0\x81\x91\x80\x80\x81\x86\x80#", 10));
        readCmd(fds[1]);                                   // :GU#
        writeRaw(fds[1], "nNpE000#");
    });

    uint8_t bin[10];
    EXPECT_FALSE(comm.sendReadN(":Gu#", bin, 10, 200, true));
    driverTimedOut();
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GU#", reply));
    EXPECT_STREQ(reply, "nNpE000");
}

// Noise read in place of a binary reply: the caller rejects it and requests a
// resync, so the real (late) binary bytes do not leak into the next reply.
TEST_F(CommTest, requested_resync_discards_late_binary)
{
    responder = std::thread([this]()
    {
        readCmd(fds[1]);                                   // :Gu#
        writeRaw(fds[1], std::string(10, '\xff'));         // noise fills the 10-byte read
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        writeRaw(fds[1], std::string("\x83\xb0\x81\x91\x80\x80\x81\x86\x80#", 10));
        readCmd(fds[1]);                                   // :GU#
        writeRaw(fds[1], "nNpE000#");
    });

    uint8_t bin[10];
    EXPECT_TRUE(comm.sendReadN(":Gu#", bin, 10));          // read "succeeds" with noise
    comm.requestResync();                                  // caller found it invalid
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GU#", reply));
    EXPECT_STREQ(reply, "nNpE000");
}

// ---------------------------------------------------------------------------
// sendChar / sendReadN
// ---------------------------------------------------------------------------
TEST_F(CommTest, char_reply)
{
    respond("4");
    char c = '\0';
    EXPECT_TRUE(comm.sendChar(":MS#", c));
    EXPECT_EQ(c, '4');
}

TEST_F(CommTest, readN_binary)
{
    respond(std::string("\x81\x82\x83\x84\x85\x86\x87\x88\x89#", 10));
    uint8_t buf[10] {};
    EXPECT_TRUE(comm.sendReadN(":Gu#", buf, 10));
    EXPECT_EQ(buf[0], 0x81);
    EXPECT_EQ(buf[8], 0x89);
    EXPECT_EQ(buf[9], '#');
}

TEST_F(CommTest, readN_short_reply_times_out)
{
    respond(std::string("\x81\x82", 2));
    uint8_t buf[10] {};
    EXPECT_FALSE(comm.sendReadN(":Gu#", buf, 10, 200, true));
}

// ---------------------------------------------------------------------------
// Connection state, flushing, concurrency
// ---------------------------------------------------------------------------
TEST_F(CommTest, not_connected_sends_nothing)
{
    comm.setFd(-1);
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_FALSE(comm.isOpen());
    EXPECT_FALSE(comm.sendText(":GVP#", reply, 100, true));
    EXPECT_FALSE(comm.sendBool(":Te#", 100, true));
    EXPECT_FALSE(comm.sendNoReply(":Q#"));
    EXPECT_FALSE(readable(fds[1], 20)) << "nothing may be written while disconnected";
}

TEST_F(CommTest, flushIO_clears_stale)
{
    writeRaw(fds[1], "STALE#DATA_WITHOUT_HASH");
    usleep(10000);

    comm.flushIO();
    EXPECT_FALSE(readable(fds[0], 5)) << "flushIO must drain all stale data";

    respond("Clean#");
    char reply[OnStepXComm::REPLY_BUF_SIZE];
    EXPECT_TRUE(comm.sendText(":GVP#", reply, 1000));
    EXPECT_STREQ(reply, "Clean");
}

// N client threads call sendText simultaneously; the mutex must serialise
// each send+receive so all N get their own reply.
TEST_F(CommTest, mutex_concurrent)
{
    const int N = 4;

    responder = std::thread([this]()
    {
        for (int i = 0; i < N; i++)
        {
            readCmd(fds[1]);
            writeRaw(fds[1], "OK#");
        }
    });

    std::vector<std::thread> clients;
    std::atomic<int> successCount { 0 };
    for (int i = 0; i < N; i++)
    {
        clients.emplace_back([&]()
        {
            char reply[OnStepXComm::REPLY_BUF_SIZE];
            if (comm.sendText(":GVP#", reply, 2000) && strcmp(reply, "OK") == 0)
                successCount.fetch_add(1);
        });
    }
    for (auto &t : clients)
        t.join();

    EXPECT_EQ(successCount.load(), N);
}

// ---------------------------------------------------------------------------
// main() — defined here so it is compiled directly into the .o and takes
// precedence over indidrivermain.c's main() exported from libindidriver.so.
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
