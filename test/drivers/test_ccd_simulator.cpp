#include "indicom.h"
#include "indilogger.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::StrEq;

#include "ccd_simulator.h"

char _me[] = "MockCCDSimDriver";
char *me = _me;
class MockCCDSimDriver: public CCDSim
{
public:
    MockCCDSimDriver(): CCDSim()
    {
        initProperties();
        ISGetProperties(me);
    }
};

TEST(CCDSimulatorDriverTest, test_properties)
{
    MockCCDSimDriver ccd;
    INumberVectorProperty * const p = ccd.getNumber("SIMULATOR_SETTINGS");
    ASSERT_NE(p, nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_XRES"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_YRES"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_XSIZE"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_YSIZE"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_MAXVAL"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_BIAS"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_SATURATION"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_LIMITINGMAG"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_NOISE"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_SKYGLOW"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_OAGOFFSET"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_POLAR"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_POLARDRIFT"), nullptr);
    ASSERT_NE(IUFindNumber(p, "SIM_ROTATION"), nullptr);
}

int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
            INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
