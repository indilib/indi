/*
    OnStepX INDI Driver — test_onstepxconfig.cpp
    Copyright (C) 2026 Ed Lee

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
    OnStepX INDI Driver — INDI config filter unit tests

    The mount saves INDI::Telescope's config list without the entries that
    would replay firmware NV settings on a config load.  The filter is fed
    INDI's own config writer output.
*/

#include <gtest/gtest.h>

#include <indipropertynumber.h>
#include <indipropertyswitch.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "OnStepXConfig.h"

namespace
{
std::string capture(const std::function<void(FILE *)> &write)
{
    char *buf = nullptr;
    size_t len = 0;
    FILE *fp = open_memstream(&buf, &len);
    write(fp);
    fclose(fp);
    std::string s(buf, len);
    free(buf);
    return s;
}

struct Props
{
    INDI::PropertyNumber location {3};
    INDI::PropertySwitch slewRate {2};
    INDI::PropertySwitch trackMode {2};

    Props()
    {
        location[0].fill("LAT", "Lat", "%g", -90, 90, 0, 51.5);
        location[1].fill("LONG", "Long", "%g", 0, 360, 0, 0.1);
        location[2].fill("ELEV", "Elev", "%g", -200, 10000, 0, 10);
        location.fill("OnStepX", "GEOGRAPHIC_COORD", "Location", "Site", IP_RW, 60, IPS_IDLE);
        slewRate[0].fill("1x", "Guide", ISS_ON);
        slewRate[1].fill("2x", "Centering", ISS_OFF);
        slewRate.fill("OnStepX", "TELESCOPE_SLEW_RATE", "Slew Rate", "Motion", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
        trackMode[0].fill("TRACK_SIDEREAL", "Sidereal", ISS_OFF);
        trackMode[1].fill("TRACK_LUNAR", "Lunar", ISS_ON);
        trackMode.fill("OnStepX", "TELESCOPE_TRACK_MODE", "Track Mode", "Main", IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    }

    bool saveAll(FILE *fp)
    {
        location.save(fp);
        slewRate.save(fp);
        trackMode.save(fp);
        return true;
    }
};
}

TEST(OnStepXConfig, DropsOnlyTheNamedProperties)
{
    Props p;
    const std::string all = capture([&](FILE * fp)
    {
        p.saveAll(fp);
    });
    ASSERT_NE(all.find("GEOGRAPHIC_COORD"), std::string::npos) << "writer output unexpected:\n" << all;

    const std::string kept = OnStepXConfig::withoutProperties(all, { "GEOGRAPHIC_COORD", "TELESCOPE_SLEW_RATE" });
    EXPECT_EQ(kept.find("GEOGRAPHIC_COORD"), std::string::npos) << kept;
    EXPECT_EQ(kept.find("TELESCOPE_SLEW_RATE"), std::string::npos) << kept;
    EXPECT_EQ(kept.find("oneNumber"), std::string::npos) << "the location's elements went with it";

    const std::string trackOnly = capture([&](FILE * fp)
    {
        p.trackMode.save(fp);
    });
    EXPECT_EQ(kept, trackOnly) << "the rest is copied unchanged";
}

TEST(OnStepXConfig, NothingExcludedKeepsEverything)
{
    Props p;
    const std::string all = capture([&](FILE * fp)
    {
        p.saveAll(fp);
    });
    EXPECT_EQ(OnStepXConfig::withoutProperties(all, {}), all);
}

TEST(OnStepXConfig, SaveExceptWritesTheFilteredOutput)
{
    Props p;
    const std::string out = capture([&](FILE * fp)
    {
        EXPECT_TRUE(OnStepXConfig::saveExcept(fp, { "GEOGRAPHIC_COORD" }, [&](FILE * f)
        {
            return p.saveAll(f);
        }));
    });
    EXPECT_EQ(out.find("GEOGRAPHIC_COORD"), std::string::npos);
    EXPECT_NE(out.find("TELESCOPE_SLEW_RATE"), std::string::npos);
    EXPECT_NE(out.find("TELESCOPE_TRACK_MODE"), std::string::npos);
}

TEST(OnStepXConfig, TextOutsideBlocksIsKept)
{
    const std::string xml = "<!-- header -->\n<newSwitchVector device='D' name='A'>\n  <oneSwitch name='x'>\nOn\n"
                            "  </oneSwitch>\n</newSwitchVector>\ntrailer\n";
    EXPECT_EQ(OnStepXConfig::withoutProperties(xml, { "A" }), "<!-- header -->\ntrailer\n");
}

// indidriver provides a driver main(); define the test runner explicitly.
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
