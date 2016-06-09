


#include "indi_test_helpers.h"
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

/*
 * Test connects ok and acks ok.
 */

TEST(DOME_BAADER, Connect__connect_ok)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    INDI_CAP_STDERR_BEBIN;
    
    EXPECT_CALL((*p_mock_tty), connect(_, 9600, 8, 0, 1))
        .Times (1)
        .WillOnce(Return(INDI::TTY::OK));
    
    EXPECT_CALL((*p_mock_tty), write(StrEq("d#getflap"), _, _))
        .Times (1)
        .WillOnce(Return( INDI::TTY::OK));
    
    EXPECT_CALL((*p_mock_tty), read(_, _, _, _))
        .Times (1)
        .WillOnce(
            Invoke([](char *buf, int nbytes, int timeout, int *nbytes_read) {
                *nbytes_read = snprintf(buf, nbytes, "d#flapclo");
                return INDI::TTY::OK;
             }));    
    
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    
    ASSERT_EQ(true, dome->Connect());
    
    INDI_CAP_STDERR_END;
}


/*
 * Test connection failure.
 */

TEST(DOME_BAADER, Connect__simulate_connect_failure)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    INDI_CAP_STDERR_BEBIN;
    
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
    
    INDI_CAP_STDERR_END;
}

/*
 * Test connection ok but dome responds incorrectly on connect.
 */

/*
 * Test connects ok and ack fails.
 */

TEST(DOME_BAADER, Connect__connect_ok_but_deliberately_fail_the_ack)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    INDI_CAP_STDERR_BEBIN;
    
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
    
    //INDI_CAP_STDERR_PRINT;
    
    INDI_CAP_STDERR_END;
}
