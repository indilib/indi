#include <gtest/gtest.h>
#include "maxdomeiidriver.h"


TEST(MaxDomeIIDriver, hex_dump)
{
    char data[] = "abcd";
    char out[(sizeof(data) - 1)*3];

    hex_dump(out, data, sizeof(data) - 1);
    ASSERT_STREQ(out, "61 62 63 64");
}


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
