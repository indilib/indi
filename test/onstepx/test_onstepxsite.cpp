/*
    OnStepX INDI Driver — test_onstepxsite.cpp
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
    OnStepX INDI Driver — Site/time unit tests (Stage 5)

    OnStepXSite::writeLocation / readLocation / writeTime are tested by
    replaying the commands it sends to the controller via the socketpair
    harness established in earlier stages.

    All tests are self-contained: no real controller required.
*/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <thread>
#include <map>

#include "OnStepXComm.h"
#include "OnStepXSite.h"
#include "MockFirmware.h"

// ---------------------------------------------------------------------------
// Fixture.  Replies are scripted from the firmware's Site.command.cpp.
// ---------------------------------------------------------------------------
class SiteTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            m_comm.setFd(m_fw.start());
            m_site.setComm(&m_comm);
            // m_site.setDevice() left as nullptr — that's fine for unit tests.
        }

        void TearDown() override
        {
            m_fw.stop();
        }

        // The set commands reply a bare '1' on success.
        void acceptSetCommands()
        {
            for (const char *prefix :
                    { ":St", ":Sg", ":Sv", ":SG", ":SL", ":SC"
                    })
                m_fw.setPrefixResponse(prefix, "1");
        }

        void scriptSiteNames(const char *site1 = "Home")
        {
            m_fw.setResponse(":GM", std::string(site1) + "#");
            m_fw.setResponse(":GN", "Site2#");
            m_fw.setResponse(":GO", "Site3#");
            m_fw.setResponse(":GP", "Site4#");
        }

        // :GtH# and :GgH# are sDD*MM:SS.SSS / sDDD*MM:SS.SSS with east negative;
        // :Gv# is n.n metres.
        void scriptLocation(const char *lat, const char *lon, const char *elev)
        {
            m_fw.setResponse(":GtH", std::string(lat) + "#");
            m_fw.setResponse(":GgH", std::string(lon) + "#");
            m_fw.setResponse(":Gv", std::string(elev) + "#");
        }

        bool selectSite(int n)
        {
            ISState states[4] = { ISS_OFF, ISS_OFF, ISS_OFF, ISS_OFF };
            states[n - 1] = ISS_ON;
            const char *names[4] = { "SITE_1", "SITE_2", "SITE_3", "SITE_4" };
            m_site.initProperties();
            return m_site.handleSwitch("OSX_SITE_SELECT", states, const_cast<char **>(names), 4);
        }

        bool writeSiteName(const char *name)
        {
            char *texts[1] = { const_cast<char *>(name) };
            const char *names[1] = { "SITE_NAME" };
            m_site.initProperties();
            return m_site.handleText("OSX_SITE_NAME", texts, const_cast<char **>(names), 1);
        }

        MockFirmware m_fw;
        OnStepXComm  m_comm;
        OnStepXSite  m_site;
};

// ---------------------------------------------------------------------------
// writeLocation tests
// ---------------------------------------------------------------------------

TEST_F(SiteTest, WriteLocation_SendsLatLonElev)
{
    acceptSetCommands();

    bool ok = m_site.writeLocation(51.5, 0.0, 100.0);

    EXPECT_TRUE(ok);

    // Must have sent at least :St#, :Sg#, :Sv#
    const auto cmds = m_fw.commands();
    ASSERT_GE(cmds.size(), 3u);

    bool hasSt = false, hasSg = false, hasSv = false;
    for (const auto &c : cmds)
    {
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 't') hasSt = true;
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 'g') hasSg = true;
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 'v') hasSv = true;
    }
    EXPECT_TRUE(hasSt) << "Missing :St (latitude) command";
    EXPECT_TRUE(hasSg) << "Missing :Sg# (longitude) command";
    EXPECT_TRUE(hasSv) << "Missing :Sv# (elevation) command";
}

TEST_F(SiteTest, WriteLocation_LatCommandFormat)
{
    acceptSetCommands();
    m_site.writeLocation(51.5081, 0.0, 0.0);

    // Find the :St# command and check it starts with '+' (Northern hemisphere)
    bool found = false;
    for (const auto &c : m_fw.commands())
    {
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 't')
        {
            EXPECT_EQ('+', c[3]) << "Northern latitude should start with '+'";
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SiteTest, WriteLocation_NegativeLatitude)
{
    acceptSetCommands();
    m_site.writeLocation(-33.87, 151.21, 50.0);

    bool found = false;
    for (const auto &c : m_fw.commands())
    {
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 't')
        {
            EXPECT_EQ('-', c[3]) << "Southern latitude should start with '-'";
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SiteTest, WriteLocation_LongitudeConversion)
{
    // INDI lon 151.21 East -> OnStepX West-positive = 360 - 151.21 = 208.79
    // :Sg# should encode 208 degrees.
    acceptSetCommands();
    m_site.writeLocation(-33.87, 151.21, 0.0);

    bool found = false;
    for (const auto &c : m_fw.commands())
    {
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 'g')
        {
            // Command is ":Sg208:..." — degree field is characters 3-5
            std::string deg = c.substr(3, 3);
            EXPECT_EQ("208", deg) << "Expected 208 degrees West for 151.21 East; got: " << c;
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SiteTest, WriteLocation_ReturnsFalseOnNack)
{
    m_fw.setPrefixResponse(":St", "0");

    EXPECT_FALSE(m_site.writeLocation(51.508, 0.0, 0.0));
}

// ---------------------------------------------------------------------------
// readLocation tests
// ---------------------------------------------------------------------------

TEST_F(SiteTest, ReadLocation_ParsesLatLon)
{
    scriptLocation("+51*30:29.200", "+000*00:00.000", "100.0");

    double lat = 0, lon = 0, elev = 0;
    EXPECT_TRUE(m_site.readLocation(lat, lon, elev));

    EXPECT_NEAR(51.508, lat,  0.01);
    EXPECT_NEAR(0.0,    lon,  0.001);
    EXPECT_NEAR(100.0,  elev, 0.01);
}

// The firmware stores east as negative: 151.21 E reads back as -151*12:36.
TEST_F(SiteTest, ReadLocation_LongitudeEastConversion)
{
    scriptLocation("-33*52:12.000", "-151*12:36.000", "50.0");

    double lat = 0, lon = 0, elev = 0;
    EXPECT_TRUE(m_site.readLocation(lat, lon, elev));

    EXPECT_NEAR(151.21, lon, 0.01);
}

TEST_F(SiteTest, ReadLocation_WestLongitudeIsReturnedAs0To360East)
{
    scriptLocation("+40*00:00.000", "+075*30:00.000", "10.0");

    double lat = 0, lon = 0, elev = 0;
    EXPECT_TRUE(m_site.readLocation(lat, lon, elev));

    EXPECT_NEAR(284.5, lon, 0.01);
}

TEST_F(SiteTest, ReadLocation_FailsOnBareReply)
{
    double lat = 0, lon = 0, elev = 0;
    EXPECT_FALSE(m_site.readLocation(lat, lon, elev));
}

// ---------------------------------------------------------------------------
// writeTime tests
// ---------------------------------------------------------------------------

TEST_F(SiteTest, WriteTime_SendsThreeCommands)
{
    acceptSetCommands();

    ln_date utc;
    utc.years = 2026;
    utc.months = 4;
    utc.days = 1;
    utc.hours = 12;
    utc.minutes = 0;
    utc.seconds = 0.0;

    bool ok = m_site.writeTime(&utc, 10.0);   // UTC+10

    EXPECT_TRUE(ok);

    bool hasSG = false, hasSL = false, hasSC = false;
    for (const auto &c : m_fw.commands())
    {
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 'G') hasSG = true;
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 'L') hasSL = true;
        if (c.size() >= 3 && c[1] == 'S' && c[2] == 'C') hasSC = true;
    }
    EXPECT_TRUE(hasSG) << "Missing :SG# (UTC offset) command";
    EXPECT_TRUE(hasSL) << "Missing :SL# (local time) command";
    EXPECT_TRUE(hasSC) << "Missing :SC# (local date) command";
}

// :SG takes hours ADDED TO LOCAL TIME TO YIELD UT (firmware Site::localToUT1),
// which is the negation of INDI's utc_offset (local = UTC + offset).
static std::string findCmd(const std::vector<std::string> &cmds, const char *prefix)
{
    for (const auto &c : cmds)
        if (c.rfind(prefix, 0) == 0)
            return c;
    return "";
}

TEST_F(SiteTest, WriteTime_TimezoneSignIsInverted)
{
    const std::pair<double, const char *> cases[] =
    {
        {   0.0,  ":SG+00"    },   // UTC
        {   1.0,  ":SG-01"    },   // CET / BST
        {  -5.0,  ":SG+05"    },   // EST
        {  10.0,  ":SG-10"    },   // AEST
        {   5.5,  ":SG-05:30" },   // India
        {  -3.5,  ":SG+03:30" },   // Newfoundland
        {   5.75, ":SG-05:45" },   // Nepal
        {  -0.5,  ":SG+00:30" },   // sign must survive a zero hour field
        {   0.5,  ":SG-00:30" },
    };

    for (const auto &c : cases)
    {
        MockFirmware fw;
        for (const char *prefix :
                { ":SG", ":SL", ":SC"
                })
            fw.setPrefixResponse(prefix, "1");
        OnStepXComm comm;
        comm.setFd(fw.start());
        OnStepXSite site;
        site.setComm(&comm);

        ln_date utc { 2026, 4, 1, 12, 0, 0.0 };
        EXPECT_TRUE(site.writeTime(&utc, c.first));
        fw.stop();

        EXPECT_EQ(findCmd(fw.commands(), ":SG"), c.second) << "utc_offset=" << c.first;
    }
}

// Civil <-> epoch helpers: round trips, rollovers and second rounding.
TEST(SiteTimeMath, EpochKnownValues)
{
    EXPECT_EQ(OnStepXSite::civilToEpochSeconds(1970, 1, 1, 0, 0, 0.0), 0);
    EXPECT_EQ(OnStepXSite::civilToEpochSeconds(2000, 1, 1, 0, 0, 0.0), 946684800LL);
    EXPECT_EQ(OnStepXSite::civilToEpochSeconds(2026, 11, 7, 12, 34, 56.0),
              1794054896LL);   // date -u -d "2026-11-07 12:34:56" +%s
}

TEST(SiteTimeMath, SecondsRoundToNearest)
{
    // libnova-style 59.9999 must become the next whole second, not truncate.
    int y, mo, d, h, mi, s;
    OnStepXSite::epochSecondsToCivil(
        OnStepXSite::civilToEpochSeconds(2026, 3, 31, 18, 59, 59.9999), y, mo, d, h, mi, s);
    EXPECT_EQ(h, 19);
    EXPECT_EQ(mi, 0);
    EXPECT_EQ(s, 0);
}

TEST(SiteTimeMath, Rollovers)
{
    struct Case
    {
        int y, mo, d, h, mi, s;
        long long delta;
        int ey, emo, ed, eh, emi, es;
    };
    const Case cases[] =
    {
        { 2026,  4,  1,  0,  0,  0, -5 * 3600, 2026,  3, 31, 19,  0,  0 },  // back over month end
        { 2026, 12, 31, 23, 30,  0,  3600,     2027,  1,  1,  0, 30,  0 },  // forward over year end
        { 2027,  1,  1,  0, 30,  0, -3600,     2026, 12, 31, 23, 30,  0 },  // back over year end
        { 2028,  2, 28, 23,  0,  0,  3600,     2028,  2, 29,  0,  0,  0 },  // leap day
        { 2027,  2, 28, 23,  0,  0,  3600,     2027,  3,  1,  0,  0,  0 },  // non-leap
        { 1969, 12, 31, 23, 59, 59,  1,        1970,  1,  1,  0,  0,  0 },  // pre-epoch
    };
    for (const auto &c : cases)
    {
        int y, mo, d, h, mi, s;
        OnStepXSite::epochSecondsToCivil(
            OnStepXSite::civilToEpochSeconds(c.y, c.mo, c.d, c.h, c.mi, c.s) + c.delta,
            y, mo, d, h, mi, s);
        EXPECT_EQ(y, c.ey);
        EXPECT_EQ(mo, c.emo);
        EXPECT_EQ(d, c.ed);
        EXPECT_EQ(h, c.eh);
        EXPECT_EQ(mi, c.emi);
        EXPECT_EQ(s, c.es);
    }
}

// Local time/date sent to the firmware must be UTC + utc_offset.
TEST_F(SiteTest, WriteTime_LocalTimeAndDate)
{
    acceptSetCommands();

    ln_date utc { 2026, 4, 1, 0, 0, 0.0 };   // 2026-04-01 00:00 UTC
    EXPECT_TRUE(m_site.writeTime(&utc, -5.0));   // EST -> 2026-03-31 19:00 local

    EXPECT_EQ(findCmd(m_fw.commands(), ":SG"), ":SG+05");
    EXPECT_EQ(findCmd(m_fw.commands(), ":SL"), ":SL19:00:00");
    EXPECT_EQ(findCmd(m_fw.commands(), ":SC"), ":SC03/31/26");
}

// ---------------------------------------------------------------------------
// Stage 16: site profile tests
// ---------------------------------------------------------------------------

// :W[n]# replies nothing and the firmware numbers sites 0..3.
TEST_F(SiteTest, SelectSite_SendsZeroBasedSiteAndRereadsLocation)
{
    m_fw.setResponse(":W1", "");
    scriptLocation("+51*28:38.000", "+000*27:28.000", "17.0");

    EXPECT_TRUE(selectSite(2));

    EXPECT_EQ(m_fw.commands().front(), ":W1");
    EXPECT_TRUE(m_fw.received(":GtH"));
    EXPECT_EQ(2, m_site.activeSite());
    EXPECT_TRUE(m_site.locationUpdated());
    EXPECT_NEAR(51.477, m_site.lastLat(), 0.01);
}

TEST_F(SiteTest, SelectSite_LastSiteDoesNotWaitForReply)
{
    m_fw.setResponse(":W3", "");
    scriptLocation("-33*52:00.000", "-151*12:00.000", "0.0");

    EXPECT_TRUE(selectSite(4));

    EXPECT_EQ(m_fw.commands().front(), ":W3");
    EXPECT_EQ(4, m_site.activeSite());
}

TEST_F(SiteTest, WriteSiteName_SendsCorrectCommandAndRefreshesNames)
{
    m_fw.setResponse(":SMNewName", "1");
    scriptSiteNames("NewName");

    EXPECT_TRUE(writeSiteName("NewName"));

    EXPECT_TRUE(m_fw.received(":SMNewName"));
    for (const char *query :
            { ":GM", ":GN", ":GO", ":GP"
            })
        EXPECT_TRUE(m_fw.received(query)) << query;
    EXPECT_EQ(IPS_OK, m_site.siteNameTP().getState());
}

// The firmware's command buffer drops every space.
TEST_F(SiteTest, WriteSiteName_ReplacesSpacesWithUnderscores)
{
    m_fw.setResponse(":SMDark_Site", "1");
    scriptSiteNames("Dark_Site");

    EXPECT_TRUE(writeSiteName("Dark Site"));

    EXPECT_TRUE(m_fw.received(":SMDark_Site"));
    EXPECT_STREQ("Dark_Site", m_site.siteNameTP()[0].getText());
}

TEST_F(SiteTest, WriteSiteName_AcceptsFifteenCharacters)
{
    m_fw.setResponse(":SM123456789012345", "1");
    scriptSiteNames("123456789012345");

    EXPECT_TRUE(writeSiteName("123456789012345"));

    EXPECT_TRUE(m_fw.received(":SM123456789012345"));
}

TEST_F(SiteTest, WriteSiteName_RejectsLongNameWithoutSending)
{
    EXPECT_TRUE(writeSiteName("1234567890123456"));

    EXPECT_TRUE(m_fw.commands().empty());
    EXPECT_EQ(IPS_ALERT, m_site.siteNameTP().getState());
}

TEST_F(SiteTest, WriteSiteName_RejectsHashWithoutSending)
{
    EXPECT_TRUE(writeSiteName("Bad#Name"));

    EXPECT_TRUE(m_fw.commands().empty());
    EXPECT_EQ(IPS_ALERT, m_site.siteNameTP().getState());
}

TEST_F(SiteTest, WriteSiteName_FirmwareRefusalRaisesAlert)
{
    EXPECT_TRUE(writeSiteName("Name"));   // unscripted: bare '0'

    EXPECT_EQ(IPS_ALERT, m_site.siteNameTP().getState());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
