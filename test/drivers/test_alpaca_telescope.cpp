/*
    Regression tests for the Alpaca Telescope driver: the mount type follows the ASCOM AlignmentModes enumeration,
    algAltAz = 0, algPolar = 1, algGermanPolar = 2 (ASCOM ITelescopeV4, Telescope.AlignmentModes,
    https://ascom-standards.org/newdocs/telescope.html#Telescope.AlignmentModes). The driver read 0 as an equatorial
    fork mount and 1 as Alt-Az, so a Seestar S50 in Alt-Az mode was reported as an equatorial fork mount.

    The driver talks to a fake Alpaca telescope on 127.0.0.1. Its replies are copied from a real Seestar S50
    (telescope driver "Telescope V3" 1.3.1-1, interface version 3) in Alt-Az mode, including the "not implemented"
    reply to SiteElevation. The site latitude and longitude are made up. The S50 reports three tracking rates; that
    they are Sidereal, Lunar and Solar, and that Sidereal is selected, is assumed.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

#include <httplib.h>
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

#include "indilogger.h"
#include "drivers/telescope/indi_alpaca_telescope.h"

namespace
{

const char *DEVICE_NAME = "Mock Alpaca Telescope";

class FakeAlpacaTelescope
{
    public:
        // alignmentMode: the value the telescope reports for AlignmentMode (the S50 in Alt-Az mode reports 0).
        explicit FakeAlpacaTelescope(int alignmentMode)
        {
            // GET replies of a real S50 telescope (device 0); the site coordinates are made up.
            m_Values =
            {
                {"connected", false}, {"description", "Seestar S50_a544ee7b Telescope"}, {"driverinfo", "Telescope V3"},
                {"driverversion", "1.3.1-1"}, {"interfaceversion", 3}, {"alignmentmode", alignmentMode},
                {"atpark", false}, {"athome", false}, {"tracking", true}, {"slewing", false}, {"altitude", 5.207778},
                {"azimuth", 9.729722}, {"rightascension", 2.658333}, {"declination", 45.624167}, {"canslew", true},
                {"canslewasync", true}, {"canslewaltaz", false}, {"canslewaltazasync", false}, {"canpark", true},
                {"canunpark", true}, {"canfindhome", true}, {"cansync", true}, {"cansetdeclinationrate", false},
                {"cansetrightascensionrate", false}, {"trackingrates", nlohmann::json::array({0, 1, 2})},
                {"trackingrate", 0}, {"sitelatitude", 50.0}, {"sitelongitude", 10.0},
                {"utcdate", "2026-11-23T10:51:42.1247942Z"}
            };
            // Error 0x400 replies of the same telescope.
            m_NotImplemented =
            {
                {"siteelevation", "Property read SiteElevation is not implemented in this driver."}
            };

            m_Server.Get(R"(/api/v1/telescope/(\d+)/(\w+))", [this](const httplib::Request & req, httplib::Response & res)
            {
                get(req, res);
            });
            m_Server.Put(R"(/api/v1/telescope/(\d+)/(\w+))", [this](const httplib::Request & req, httplib::Response & res)
            {
                put(req, res);
            });
            m_Port = m_Server.bind_to_any_port("127.0.0.1");
            m_Thread = std::thread([this]()
            {
                m_Server.listen_after_bind();
            });
            m_Server.wait_until_ready();
        }

        ~FakeAlpacaTelescope()
        {
            m_Server.stop();
            if (m_Thread.joinable())
                m_Thread.join();
        }

        int port() const
        {
            return m_Port;
        }

        std::vector<std::string> requests()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Requests;
        }

    private:
        static nlohmann::json reply(int transaction, int error = 0, const std::string &message = "")
        {
            return {{"ClientTransactionID", transaction}, {"ServerTransactionID", 1}, {"ErrorNumber", error}, {"ErrorMessage", message}};
        }

        static int transaction(const httplib::Request &req)
        {
            return req.has_param("ClientTransactionID") ? std::atoi(req.get_param_value("ClientTransactionID").c_str()) : 0;
        }

        void get(const httplib::Request &req, httplib::Response &res)
        {
            const std::string property = req.matches[2];
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Requests.push_back(req.path);

            nlohmann::json body = reply(transaction(req));
            if (m_NotImplemented.count(property))
            {
                body = reply(transaction(req), 0x400, m_NotImplemented[property]);
                body["Value"] = 0;
            }
            else if (m_Values.count(property))
                body["Value"] = m_Values[property];
            else
                body = reply(transaction(req), 0x400, "Property " + property + " is not implemented in this driver.");
            res.set_content(body.dump(), "application/json; charset=utf-8");
        }

        void put(const httplib::Request &req, httplib::Response &res)
        {
            const std::string property = req.matches[2];
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Requests.push_back(req.path);
            if (property == "connected" && req.has_param("Connected"))
                m_Values["connected"] = req.get_param_value("Connected") == "true";
            res.set_content(reply(transaction(req)).dump(), "application/json; charset=utf-8");
        }

        httplib::Server m_Server;
        std::thread m_Thread;
        int m_Port {-1};
        std::mutex m_Mutex;
        std::vector<std::string> m_Requests;
        std::map<std::string, nlohmann::json> m_Values;
        std::map<std::string, std::string> m_NotImplemented;
};

class AlpacaTelescopeUnderTest : public alpacaTelescopeDriver
{
    public:
        AlpacaTelescopeUnderTest()
        {
            setDeviceName(DEVICE_NAME);
            // As at a client's first getProperties; this also runs initProperties().
            ISGetProperties(nullptr);
        }

        bool connectTo(int port)
        {
            std::string host = "127.0.0.1", portText = std::to_string(port);
            char addressName[] = "ADDRESS", portName[] = "PORT";
            char *texts[2] = {&host[0], &portText[0]};
            char *names[2] = {addressName, portName};
            ISNewText(getDeviceName(), "DEVICE_ADDRESS", texts, names, 2);

            ISState connection[2] = {ISS_ON, ISS_OFF};
            char connect[] = "CONNECT", disconnect[] = "DISCONNECT";
            char *connectionNames[2] = {connect, disconnect};
            ISNewSwitch(getDeviceName(), "CONNECTION", connection, connectionNames, 2);
            return isConnected();
        }

        void disconnect()
        {
            ISState connection[2] = {ISS_OFF, ISS_ON};
            char connect[] = "CONNECT", disconnect[] = "DISCONNECT";
            char *connectionNames[2] = {connect, disconnect};
            ISNewSwitch(getDeviceName(), "CONNECTION", connection, connectionNames, 2);
        }

        void setDeviceNumber(double value)
        {
            char member[] = "DEVICE_NUMBER";
            char *names[] = {member};
            ISNewNumber(getDeviceName(), "DEVICE_NUMBER", &value, names, 1);
        }
};

// Captures what the driver publishes on stdout.
class Published
{
    public:
        Published()
        {
            fflush(stdout);
            m_File = tmpfile();
            m_SavedStdout = dup(STDOUT_FILENO);
            dup2(fileno(m_File), STDOUT_FILENO);
        }

        ~Published()
        {
            restore();
            if (m_File)
                fclose(m_File);
        }

        // Everything published so far; stops the capture.
        std::string text()
        {
            restore();
            std::string text;
            rewind(m_File);
            char chunk[4096];
            size_t n;
            while ((n = fread(chunk, 1, sizeof(chunk), m_File)) > 0)
                text.append(chunk, n);
            return text;
        }

    private:
        void restore()
        {
            if (m_SavedStdout < 0)
                return;
            fflush(stdout);
            dup2(m_SavedStdout, STDOUT_FILENO);
            close(m_SavedStdout);
            m_SavedStdout = -1;
        }

        FILE *m_File {nullptr};
        int m_SavedStdout {-1};
};

// The switch that is On in the last TELESCOPE_MOUNT_TYPE vector the driver defined or set, or "" if none.
std::string publishedMountType(const std::string &text)
{
    std::regex vector("<(def|set)SwitchVector\\s+device='[^']*'\\s+name='TELESCOPE_MOUNT_TYPE'[\\s\\S]*?</\\1SwitchVector>");
    std::regex on("<(?:def|one)Switch\\s+name='(\\w+)'[^>]*>\\s*On\\s*<");
    std::string mountType;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), vector); it != std::sregex_iterator(); ++it)
    {
        const std::string block = (*it)[0];
        std::smatch match;
        if (std::regex_search(block, match, on))
            mountType = match[1];
    }
    return mountType;
}

struct ConnectResult
{
    bool connected {false};
    INDI::Telescope::TelescopeMountType mountType {INDI::Telescope::MOUNT_EQ_GEM};
    std::string published;
};

// Connects to a fake telescope that reports the given AlignmentMode and records the resulting mount type.
ConnectResult connectWithAlignmentMode(int alignmentMode)
{
    FakeAlpacaTelescope telescope(alignmentMode);
    ConnectResult result;

    Published published;
    AlpacaTelescopeUnderTest driver;
    result.connected = driver.connectTo(telescope.port());
    result.mountType = driver.getMountType();
    driver.disconnect();
    result.published = publishedMountType(published.text());
    return result;
}

class AlpacaDeviceNumberTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            const char *previous = std::getenv("INDICONFIG");
            if (previous)
                m_PreviousConfig = previous;

            char path[] = "/tmp/test_alpaca_device_number_XXXXXX";
            int fd = mkstemp(path);
            ASSERT_GE(fd, 0);
            close(fd);
            m_ConfigFile = path;
            ASSERT_EQ(std::remove(m_ConfigFile.c_str()), 0);
            ASSERT_EQ(setenv("INDICONFIG", m_ConfigFile.c_str(), 1), 0);
        }

        void TearDown() override
        {
            INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                                  INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
            if (!m_ConfigFile.empty())
            {
                std::remove(m_ConfigFile.c_str());
                std::remove((m_ConfigFile + ".default").c_str());
            }
            if (!m_PreviousConfig.empty())
                setenv("INDICONFIG", m_PreviousConfig.c_str(), 1);
            else
                unsetenv("INDICONFIG");
        }

        std::string m_ConfigFile;

    private:
        std::string m_PreviousConfig;
};

}

// AlignmentMode 0 is algAltAz: a Seestar S50 in Alt-Az mode is an Alt-Az mount, not an equatorial fork mount.
TEST(AlpacaTelescopeTest, alignmentModeAltAzIsReportedAsAltAz)
{
    ConnectResult result = connectWithAlignmentMode(0);

    ASSERT_TRUE(result.connected);
    EXPECT_EQ(result.mountType, INDI::Telescope::MOUNT_ALTAZ);
    EXPECT_EQ(result.published, "ALTAZ");
}

// AlignmentMode 1 is algPolar: an equatorial mount other than a German one, i.e. without meridian flips.
TEST(AlpacaTelescopeTest, alignmentModePolarIsReportedAsEquatorialFork)
{
    ConnectResult result = connectWithAlignmentMode(1);

    ASSERT_TRUE(result.connected);
    EXPECT_EQ(result.mountType, INDI::Telescope::MOUNT_EQ_FORK);
    EXPECT_EQ(result.published, "EQ_FORK");
}

// AlignmentMode 2 is algGermanPolar: a German equatorial mount.
TEST(AlpacaTelescopeTest, alignmentModeGermanPolarIsReportedAsGerman)
{
    ConnectResult result = connectWithAlignmentMode(2);

    ASSERT_TRUE(result.connected);
    EXPECT_EQ(result.mountType, INDI::Telescope::MOUNT_EQ_GEM);
    EXPECT_EQ(result.published, "EQ_GEM");
}

TEST_F(AlpacaDeviceNumberTest, routesRequestsAndRefusesConnectedChanges)
{
    FakeAlpacaTelescope telescope(0);
    AlpacaTelescopeUnderTest driver;
    driver.setDeviceNumber(1);
    auto number = driver.getNumber("DEVICE_NUMBER");
    ASSERT_TRUE(number.isValid());
    ASSERT_DOUBLE_EQ(number[0].getValue(), 1);
    ASSERT_TRUE(driver.connectTo(telescope.port()));

    INDI::Logger::getInstance().configure("", INDI::Logger::file_off | INDI::Logger::screen_on,
                                          INDI::Logger::DBG_ERROR,
                                          INDI::Logger::DBG_ERROR | INDI::Logger::DBG_WARNING);
    const std::string warning = "Cannot change Alpaca device number while connected.";
    Published sameValue;
    driver.setDeviceNumber(1);
    EXPECT_EQ(sameValue.text().find(warning), std::string::npos);

    Published differentValue;
    driver.setDeviceNumber(2);
    EXPECT_NE(differentValue.text().find(warning), std::string::npos);
    EXPECT_DOUBLE_EQ(number[0].getValue(), 1);

    driver.disconnect();
    const auto requests = telescope.requests();
    ASSERT_FALSE(requests.empty());
    EXPECT_EQ(requests.back(), "/api/v1/telescope/1/connected");
    for (const auto &path : requests)
        EXPECT_EQ(path.find("/api/v1/telescope/1/"), 0U) << path;
}

TEST_F(AlpacaDeviceNumberTest, defaultsToDeviceZero)
{
    FakeAlpacaTelescope telescope(0);
    AlpacaTelescopeUnderTest driver;
    ASSERT_TRUE(driver.connectTo(telescope.port()));
    driver.disconnect();
    const auto requests = telescope.requests();
    ASSERT_FALSE(requests.empty());
    for (const auto &path : requests)
        EXPECT_EQ(path.find("/api/v1/telescope/0/"), 0U) << path;
}

TEST_F(AlpacaDeviceNumberTest, savesAndRestoresDeviceNumber)
{
    {
        AlpacaTelescopeUnderTest driver;
        driver.setDeviceNumber(1);
    }

    FakeAlpacaTelescope telescope(0);
    AlpacaTelescopeUnderTest restored;
    auto number = restored.getNumber("DEVICE_NUMBER");
    ASSERT_TRUE(number.isValid());
    EXPECT_DOUBLE_EQ(number[0].getValue(), 1);
    ASSERT_TRUE(restored.connectTo(telescope.port()));
    restored.disconnect();
    const auto requests = telescope.requests();
    ASSERT_FALSE(requests.empty());
    for (const auto &path : requests)
        EXPECT_EQ(path.find("/api/v1/telescope/1/"), 0U) << path;
}

TEST_F(AlpacaDeviceNumberTest, rejectsOutOfRangeDeviceNumberWithoutSaving)
{
    {
        AlpacaTelescopeUnderTest driver;
        driver.setDeviceNumber(1);
        auto number = driver.getNumber("DEVICE_NUMBER");
        ASSERT_TRUE(number.isValid());

        std::ofstream marker(m_ConfigFile, std::ios::app);
        ASSERT_TRUE(marker.is_open());
        marker << "<!-- unchanged configuration -->\n";
        marker.close();
        ASSERT_FALSE(marker.fail());

        Published published;
        driver.setDeviceNumber(11);
        const std::string output = published.text();
        EXPECT_EQ(number.getState(), IPS_ALERT);
        EXPECT_DOUBLE_EQ(number[0].getValue(), 1);
        EXPECT_NE(output.find("Invalid range"), std::string::npos);

        std::ifstream config(m_ConfigFile);
        ASSERT_TRUE(config.is_open());
        const std::string contents((std::istreambuf_iterator<char>(config)), std::istreambuf_iterator<char>());
        EXPECT_NE(contents.find("<!-- unchanged configuration -->"), std::string::npos);
    }

    AlpacaTelescopeUnderTest restored;
    auto number = restored.getNumber("DEVICE_NUMBER");
    ASSERT_TRUE(number.isValid());
    EXPECT_DOUBLE_EQ(number[0].getValue(), 1);
}

int main(int argc, char **argv)
{
    // Never touch the user's real ~/.indi.
    char homeTemplate[] = "/tmp/test_alpaca_telescope_home_XXXXXX";
    const char *home = mkdtemp(homeTemplate);
    if (home)
        setenv("HOME", home, 1);
    // Never read or write the user's real driver configuration.
    const std::string configFile = "/tmp/test_alpaca_telescope_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", configFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile.c_str());
    std::remove((configFile + ".default").c_str());
    if (home)
    {
        const std::string indiDir = std::string(home) + "/.indi";
        std::remove((indiDir + "/ParkData.xml").c_str());
        std::remove(indiDir.c_str());
        std::remove(home);
    }
    return rc;
}
