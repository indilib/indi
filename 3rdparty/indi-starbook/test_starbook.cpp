//
// Created by not7cd on 09/12/18.
//

#include <gtest/gtest.h>
#include "starbook_types.h"


TEST(StarbookDriver, time) {
    std::ostringstream result;

    result << starbook::UTC(2018, 10, 5, 12, 30, 4.4);
    ASSERT_EQ(result.str(), "2018+10+05+12+30+04");

    result.str("");
    result.clear();

    result << starbook::UTC(2000, 1, 1, 1, 1, 0.);
    ASSERT_EQ(result.str(), "2000+01+01+01+01+00");

    result.str("");
    result.clear();

    result << starbook::UTC(2345, 12, 29, 23, 59, 59.99);
    ASSERT_EQ(result.str(), "2345+12+29+23+59+59");
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
