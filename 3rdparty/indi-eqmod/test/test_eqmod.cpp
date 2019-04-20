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
                gotoparams.outsidelimits    = false;
                gotoparams.pier_side        = PIER_UNKNOWN; // Auto - keep counterweight down
                if (Hemisphere == NORTH)
                {
                    gotoparams.limiteast        = zeroRAEncoder - (totalRAEncoder / 4) - (totalRAEncoder / 24); // 13h
                    gotoparams.limitwest        = zeroRAEncoder + (totalRAEncoder / 4) + (totalRAEncoder / 24); // 23h
                }
                else
                {
                    gotoparams.limiteast        = zeroRAEncoder + (totalRAEncoder / 4) + (totalRAEncoder / 24); // 13h
                    gotoparams.limitwest        = zeroRAEncoder - (totalRAEncoder / 4) - (totalRAEncoder / 24); // 23h
                }

                juliandate = getJulianDate();
                lst        = getLst(juliandate, getLongitude());

                EncoderTarget(&gotoparams);
                EncodersToRADec(gotoparams.ratargetencoder, gotoparams.detargetencoder, lst, &currentRA, &currentDEC, &currentHA, nullptr);
                EXPECT_NEAR(ra, currentRA, 0.001) << "ra=" << ra << " dec=" << de << std::endl;
                EXPECT_NEAR(de, currentDEC, 0.001) << "ra=" << ra << " dec=" << de << std::endl;

                // With counterweight down it can't go outside limits
                EXPECT_FALSE(gotoparams.outsidelimits) << "limiteast=" << gotoparams.limiteast << " limitwest=" << gotoparams.limitwest << "  pier_side=" << gotoparams.pier_side << " ratargetencoder=" << gotoparams.ratargetencoder << std::endl ;
            }
        }

        return true;
    }

    bool TestHemisphereSymmetry() {

        uint32_t destep = totalDEEncoder / 36;
        // do not test at 90 degree edges because the result pier side is not stable because of float comparison
        uint32_t demin = zeroDEEncoder - totalDEEncoder / 4 + 1;
        uint32_t demax = zeroDEEncoder + totalDEEncoder * 3 / 4  - 1;

        uint32_t rastep = totalRAEncoder / 36;
        uint32_t ramin = zeroRAEncoder - totalRAEncoder / 2 + 1;
        uint32_t ramax = zeroRAEncoder + totalRAEncoder / 2 - 1;


        // test with lst = 0.0 and longitude = 0.0 so we can assume RA = HA
        double lst = 0.0;

        for (currentDEEncoder = demin; currentDEEncoder <= demax; currentDEEncoder += destep)
        {
            for (currentRAEncoder = ramin; currentRAEncoder <= ramax; currentRAEncoder += rastep)
            {
                updateLocation(50.0, 0.0, 0);
                double RA_North, DEC_North, HA_North;
                TelescopePierSide PierSide_North;
                EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &RA_North, &DEC_North, &HA_North, &PierSide_North);

                updateLocation(-50.0, 0.0, 0);
                double RA_South, DEC_South, HA_South;
                TelescopePierSide PierSide_South;
                EncodersToRADec(currentRAEncoder, currentDEEncoder, lst, &RA_South, &DEC_South, &HA_South, &PierSide_South);

                EXPECT_NEAR(RA_North, 24.0 - RA_South, 0.001);
                EXPECT_NEAR(DEC_North, -DEC_South, 0.001);
                EXPECT_NE(PierSide_North, PierSide_South);
            }
        }
        return true;
    }


};


TEST(EqmodTest, hemisphere_symmetry)
{
    TestEQMod eqmod;
    eqmod.TestHemisphereSymmetry();
}

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
