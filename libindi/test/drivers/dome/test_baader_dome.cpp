


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

ROSC *roCheck;
int nroCheck;			/* # of elements in roCheck */

int verbose;			/* chatty */
char *me;				/* a.out name */
LilXML *clixml;			/* XML parser context */

/*
 * Faker/helper classes.
 */

class MOCK_TTY_FAKE_connect
{
public:
    int _return = 0;
    const char* _buf;
    
    virtual int 
    read(char *buf, int nbytes, int timeout, int *nbytes_read) {
        *nbytes_read = snprintf(buf, nbytes, "%s", _buf);
        return _return;
    }
    
    virtual void
    error_msg(int err_code, char *buf, int nbytes) {
        INDI::TTY::s_error_msg(err_code, buf, nbytes);        
    }
};

/*
 * Test connects ok and acks ok.
 */

TEST(BAADER_DOME, Connect__connects_ok)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    MOCK_TTY_FAKE_connect fake_connect;
    fake_connect._return = INDI::TTY::OK;
    fake_connect._buf = "d#flapclo";
    
    INDI_CAP_STDERR_BEBIN;
    
    EXPECT_CALL((*p_mock_tty), read(_, _, _, _))
        .Times (1)
        .WillOnce(
            Invoke(&fake_connect, &MOCK_TTY_FAKE_connect::read)
        );    
    
    EXPECT_CALL((*p_mock_tty), write(StrEq("d#getflap"), _, _))
        .Times (1)
        .WillRepeatedly (Return( INDI::TTY::OK));
    
    EXPECT_CALL((*p_mock_tty), connect(_, _, _, _, _))
        .Times (1)
        .WillRepeatedly (Return(INDI::TTY::OK));
    
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    
    ASSERT_EQ(true, dome->Connect());
    
    INDI_CAP_STDERR_END;
}


/*
 * Test connection failure.
 */

TEST(BAADER_DOME, Connect__simulate_connect_failure)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    MOCK_TTY_FAKE_connect fake_connect;
    
    INDI_CAP_STDERR_BEBIN;
    
    EXPECT_CALL((*p_mock_tty), error_msg(Eq(INDI::TTY::PORT_FAILURE), _, _))
        .Times (1)
        .WillOnce(
            Invoke(&fake_connect, &MOCK_TTY_FAKE_connect::error_msg)
        );    
    
    EXPECT_CALL((*p_mock_tty), connect(_, _, _, _, _))
        .Times (1)
        .WillOnce (Return(INDI::TTY::PORT_FAILURE));
    
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

TEST(BAADER_DOME, Connect__connects_ok_but_ack_fails)
{
    
    MOCK_TTY *p_mock_tty = new MOCK_TTY;
    
    MOCK_TTY_FAKE_connect fake_connect;
    fake_connect._return = INDI::TTY::OK;
    fake_connect._buf = "mock_resp";
    
    INDI_CAP_STDERR_BEBIN;
    
    EXPECT_CALL((*p_mock_tty), read(_, _, _, _))
        .Times (1)
        .WillOnce(
            Invoke(&fake_connect, &MOCK_TTY_FAKE_connect::read)
        );    
    
    EXPECT_CALL((*p_mock_tty), write(StrEq("d#getflap"), _, _))
        .Times (1)
        .WillRepeatedly (Return( INDI::TTY::OK));
    
    EXPECT_CALL((*p_mock_tty), connect(_, _, _, _, _))
        .Times (1)
        .WillRepeatedly (Return(INDI::TTY::OK));
    
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    
    ASSERT_EQ(false, dome->Connect());
    
    INDI_CAP_STDERR_PRINT;
    //std::cout << "[   stderr ]" << buffer.str();
    
    INDI_CAP_STDERR_END;
}



/****************
 * getDefaultName
 ****************/
TEST(BAADER_DOME, getDefaultName)
{
    MOCK_TTY  *p_mock_tty = new MOCK_TTY;
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    const char *p_actual = dome->getDefaultName();
    ASSERT_STREQ("Baader Dome", p_actual);
}

