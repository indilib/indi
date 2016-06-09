
#include "indi_test_helpers.h"
#include "mocks/mock_indi_tty.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "drivers/dome/baader_dome.h"

TEST(DOME_BAADER, getDefaultName)
{
    MOCK_TTY  *p_mock_tty = new MOCK_TTY;
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    const char *p_actual = dome->getDefaultName();
    ASSERT_STREQ("Baader Dome", p_actual);
}

