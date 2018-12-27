#include <gtest/gtest.h>

// We need to override "me" in the INDI driver main.
char *me = nullptr;

int main(int argc, char **argv) 
{
    // We need to populate "me" with something.
    me = new char[256];
    strcpy( me, "AllTheCoolKidsAreUnitTesting" );

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


