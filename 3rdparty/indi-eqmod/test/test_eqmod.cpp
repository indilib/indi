#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "eqmod.h"


using ::testing::_;
using ::testing::StrEq;

class TestEQMod : public EQMod
{
public:
    TestEQMod()
    {
    zeroRAEncoder = 1000000;
    totalRAEncoder = 360000;
    zeroDEEncoder = 2000000;
    totalDEEncoder = 360000;
    initProperties();
    }

    bool TestEncoders() {

        uint32_t destep = totalDEEncoder / 360;
        // do not test at 90 degree edges because the result pier side is not stable because of float comparison
        uint32_t demin = zeroDEEncoder - totalDEEncoder / 4 + 1;
        uint32_t demax = zeroDEEncoder + totalDEEncoder * 3 / 4  - 1;

        uint32_t rastep = totalRAEncoder / 360;
        uint32_t ramin = zeroRAEncoder - totalRAEncoder / 2 + 1;
        uint32_t ramax = zeroRAEncoder + totalRAEncoder / 2 - 1;


        for (currentDEEncoder = demin; currentDEEncoder <= demax; currentDEEncoder += destep)
        {
            double de = EncoderToDegrees(currentDEEncoder, zeroDEEncoder, totalDEEncoder, Hemisphere);
            uint32_t currentDEEncoder_res = EncoderFromDegree(de, zeroDEEncoder, totalDEEncoder, Hemisphere);
            EXPECT_EQ(currentDEEncoder, currentDEEncoder_res);
        }

        for (currentRAEncoder = ramin; currentRAEncoder <= ramax; currentRAEncoder += rastep)
        {
            double ha = EncoderToHours(currentRAEncoder, zeroRAEncoder, totalRAEncoder, Hemisphere);
            uint32_t currentRAEncoder_res = EncoderFromHour(ha, zeroRAEncoder, totalRAEncoder, Hemisphere);
            EXPECT_EQ(currentRAEncoder, currentRAEncoder_res);
        }

        double lst;

        for (currentDEEncoder = demin; currentDEEncoder <= demax; currentDEEncoder += destep)
        {
            for (currentRAEncoder = ramin; currentRAEncoder <= ramax; currentRAEncoder += rastep)
            {
                for (lst = 0.0; lst < 24.0; lst++)
                {
                    EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &currentRA, &currentDEC, &currentHA, &currentPierSide);
                    uint32_t currentRAEncoder_res = EncoderFromRA(currentRA, currentPierSide, lst, zeroRAEncoder, totalRAEncoder, Hemisphere);
                    uint32_t currentDEEncoder_res = EncoderFromDec(currentDEC, currentPierSide, zeroDEEncoder, totalDEEncoder, Hemisphere);
                    EXPECT_EQ(currentDEEncoder, currentDEEncoder_res);
                    EXPECT_EQ(currentRAEncoder, currentRAEncoder_res);
                }
            }
        }
        return true;
    }

    bool TestEncoderTarget() {
        double lst;
        double ra, de;
        // do not test at 90 degree edges because the result pier side is not stable because of float comparison
        for (ra = 0.5; ra < 24.0; ra += 1.0)
        {
            for (de = -89.5; de <= 90.0; de += 1.0)
            {
                bzero(&gotoparams, sizeof(gotoparams));
                gotoparams.ratarget  = ra;
                gotoparams.detarget  = de;

                gotoparams.racurrentencoder = currentRAEncoder;
                gotoparams.decurrentencoder = currentDEEncoder;
                gotoparams.completed        = false;
                gotoparams.checklimits      = true;
                gotoparams.forcecwup        = false;
                gotoparams.outsidelimits    = false;
                gotoparams.limiteast        = zeroRAEncoder - (totalRAEncoder / 4) - (totalRAEncoder / 24); // 13h
                gotoparams.limitwest        = zeroRAEncoder + (totalRAEncoder / 4) + (totalRAEncoder / 24); // 23h

                juliandate = getJulianDate();
                lst        = getLst(juliandate, getLongitude());

                EncoderTarget(&gotoparams);
                EncodersToRADec(gotoparams.ratargetencoder, gotoparams.detargetencoder, lst, &currentRA, &currentDEC, &currentHA, nullptr);
                EXPECT_NEAR(ra, currentRA, 0.001) << "ra=" << ra << " dec=" << de << std::endl;
                EXPECT_NEAR(de, currentDEC, 0.001) << "ra=" << ra << " dec=" << de << std::endl;
            }
        }

        return true;
    }
};


TEST(EqmodTest, encoders_north)
{
    TestEQMod eqmod;
    eqmod.updateLocation(50.0, 15.0, 0);
    eqmod.TestEncoders();
}

TEST(EqmodTest, encoders_south)
{
    TestEQMod eqmod;
    eqmod.updateLocation(-50.0, 15.0, 0);
    eqmod.TestEncoders();
}


TEST(EqmodTest, encoder_target_north)
{
    TestEQMod eqmod;
    eqmod.updateLocation(50.0, 15.0, 0);
    eqmod.TestEncoderTarget();
}

TEST(EqmodTest, encoder_target_south)
{
    TestEQMod eqmod;
    eqmod.updateLocation(-50.0, 15.0, 0);
    eqmod.TestEncoderTarget();
}


int main(int argc, char **argv)
{
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
            INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);

    me = strdup("indi_eqmod_driver");

    return RUN_ALL_TESTS();
}
