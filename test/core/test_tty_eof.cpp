/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <gtest/gtest.h>

#include "indicom.h"

#include <array>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
class TtyEofTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, m_fds));
            // An EOF must not consume stale buffer contents as a received reply.
            m_buffer.fill('A');
            m_buffer[16] = '#';
        }

        void TearDown() override
        {
            for (int &fd : m_fds)
            {
                if (fd >= 0)
                {
                    EXPECT_EQ(0, close(fd));
                    fd = -1;
                }
            }
        }

        void closeWriter()
        {
            const int writer = m_fds[1];
            m_fds[1] = -1;
            ASSERT_EQ(0, close(writer));
        }

        int m_fds[2] { -1, -1 };
        std::array<char, 64> m_buffer {};
};

TEST_F(TtyEofTest, ReadSectionEmptyEof)
{
    closeWriter();
    int nbytesRead = -1;
    EXPECT_EQ(TTY_READ_ERROR, tty_read_section(m_fds[0], m_buffer.data(), '#', 1, &nbytesRead));
    EXPECT_EQ(0, nbytesRead);
}

TEST_F(TtyEofTest, ReadSectionPartialReplyThenEof)
{
    ASSERT_EQ(2, write(m_fds[1], "AB", 2));
    closeWriter();
    int nbytesRead = -1;
    EXPECT_EQ(TTY_READ_ERROR, tty_read_section(m_fds[0], m_buffer.data(), '#', 1, &nbytesRead));
    EXPECT_EQ(2, nbytesRead);
}

TEST_F(TtyEofTest, ReadSectionValidDelimitedReply)
{
    ASSERT_EQ(3, write(m_fds[1], "OK#", 3));
    int nbytesRead = -1;
    EXPECT_EQ(TTY_OK, tty_read_section(m_fds[0], m_buffer.data(), '#', 1, &nbytesRead));
    EXPECT_EQ(3, nbytesRead);
}

TEST_F(TtyEofTest, NReadSectionEmptyEof)
{
    closeWriter();
    int nbytesRead = -1;
    EXPECT_EQ(TTY_READ_ERROR, tty_nread_section(m_fds[0], m_buffer.data(), m_buffer.size(), '#', 1, &nbytesRead));
    EXPECT_EQ(0, nbytesRead);
}

TEST_F(TtyEofTest, NReadSectionPartialReplyThenEof)
{
    ASSERT_EQ(2, write(m_fds[1], "AB", 2));
    closeWriter();
    int nbytesRead = -1;
    EXPECT_EQ(TTY_READ_ERROR, tty_nread_section(m_fds[0], m_buffer.data(), m_buffer.size(), '#', 1, &nbytesRead));
    EXPECT_EQ(2, nbytesRead);
}

TEST_F(TtyEofTest, NReadSectionValidDelimitedReply)
{
    ASSERT_EQ(3, write(m_fds[1], "OK#", 3));
    int nbytesRead = -1;
    EXPECT_EQ(TTY_OK, tty_nread_section(m_fds[0], m_buffer.data(), m_buffer.size(), '#', 1, &nbytesRead));
    EXPECT_EQ(3, nbytesRead);
}
}
