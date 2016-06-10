/*******************************************************************************
 Copyright(c) 2016 Andy Kirkham. All rights reserved.

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

#include "mocks/mock_indi_tty.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "drivers/dome/baader_dome.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::StrEq;
using ::testing::Return;
using ::testing::Invoke;

TEST(DOME_BAADER_Connect, connect_ok)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    //INDI_CAP_STDERR_BEBIN;
    
    EXPECT_CALL((*p_mock_tty), connect(_, 9600, 8, 0, 1))
        .Times (1)
        .WillOnce(Return(INDI::TTY::OK));
    
    EXPECT_CALL((*p_mock_tty), write(StrEq("d#getflap"), _, _))
        .Times (1)
        .WillOnce(Return(INDI::TTY::OK));
    
    EXPECT_CALL((*p_mock_tty), read(_, _, _, _))
        .Times (1)
        .WillOnce(
            Invoke([](char *buf, int nbytes, int timeout, int *nbytes_read) {
                *nbytes_read = snprintf(buf, nbytes, "d#flapclo");
                return INDI::TTY::OK;
             }));    
    
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    
    ASSERT_EQ(true, dome->Connect());
    
    testing::internal::GetCapturedStderr();
    testing::internal::GetCapturedStdout();
    //INDI_CAP_STDERR_END;
}

TEST(DOME_BAADER_Connect, simulate_connect_failure)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    //INDI_CAP_STDERR_BEBIN;
    
    EXPECT_CALL((*p_mock_tty), connect(_, _, _, _, _))
        .Times (1)
        .WillOnce (Return(INDI::TTY::PORT_FAILURE));
    
    EXPECT_CALL((*p_mock_tty), error_msg(Eq(INDI::TTY::PORT_FAILURE), _, _))
        .Times (1)
        .WillOnce(
            Invoke([](int err_code, char *buf, int nbytes) {
                INDI::TTY::s_error_msg(err_code, buf, nbytes);        
            }));    
    
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    
    ASSERT_EQ(false, dome->Connect());
    
    testing::internal::GetCapturedStderr();
    testing::internal::GetCapturedStdout();
    //INDI_CAP_STDERR_END;
}

TEST(DOME_BAADER_Connect, connect_ok_but_deliberately_fail_the_ack)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    //INDI_CAP_STDERR_BEBIN;
    
    EXPECT_CALL((*p_mock_tty), connect(_, 9600, 8, 0, 1))
        .Times (1)
        .WillRepeatedly (Return(INDI::TTY::OK));
    
    EXPECT_CALL((*p_mock_tty), write(StrEq("d#getflap"), _, _))
        .Times (1)
        .WillRepeatedly (Return( INDI::TTY::OK));
    
    EXPECT_CALL((*p_mock_tty), read(_, _, _, _))
        .Times (1)
        .WillOnce(
            Invoke([](char *buf, int nbytes, int timeout, int *nbytes_read) {
                *nbytes_read = snprintf(buf, nbytes, "mock_resp");
                return INDI::TTY::OK;
            })
        );    
    
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    
    ASSERT_EQ(false, dome->Connect());
    
    /* INDI_CAP_STDERR_PRINT; */
    
    testing::internal::GetCapturedStderr();
    testing::internal::GetCapturedStdout();
    //INDI_CAP_STDERR_END;
}
