//
// Created by not7cd on 09/12/18.
//

#include <gtest/gtest.h>
#include "starbook_types.h"

TEST(StarbookDriver, cmd_res) {
    starbook::CommandResponse res1("OK");
    ASSERT_EQ(res1.status, starbook::OK);
    std::cerr << res1.status << " " << res1.raw;

    starbook::CommandResponse res2("ERROR");
    ASSERT_EQ(res2.status, starbook::ERROR_UNKNOWN);

    starbook::CommandResponse res3("OK    ");
    ASSERT_EQ(res3.status, starbook::OK);
    std::cerr << res3.status << " " << res3.raw;

    starbook::CommandResponse res4("    OK");
    ASSERT_EQ(res4.status, starbook::OK);
    std::cerr << res4.status << " " << res4.raw;
}

TEST(StarbookDriver, time) {
    std::ostringstream result;

    result << starbook::DateTime(2018, 10, 5, 12, 30, 4.4);
    ASSERT_EQ(result.str(), "2018+10+05+12+30+04");

    result.str("");
    result.clear();

    result << starbook::DateTime(2000, 1, 1, 1, 1, 0.);
    ASSERT_EQ(result.str(), "2000+01+01+01+01+00");

    result.str("");
    result.clear();

    result << starbook::DateTime(2345, 12, 29, 23, 59, 59.99);
    ASSERT_EQ(result.str(), "2345+12+29+23+59+59");
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
