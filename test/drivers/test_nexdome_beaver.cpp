/*
    Tests for the NexDome Beaver dome sync and rotator motor power.

    The driver talks to a fake Beaver controller on a pseudo terminal. The replies are taken from the debug log of
    a real NexDome Beaver (firmware 1.1.5) and from replies captured on its USB serial port:
        !seletek tversion#      -> !seletek tversion:7:1.1.5#
        !dome status#           -> !dome status:4120#        (0x1018: at park/home, no shutter attached)
        !tmc2660 getstoppow#    -> !tmc2660 getstoppow:10#
        !tmc2660 setstoppow 10# -> !tmc2660 setstoppow 10:10#  (echoes the value)
        !dome setaz 10.00#      -> !dome setaz 10.00:0#      (0 = success)
        !tmc2660 estop#         -> !tmc2660 estop:0#
        !tmc2660 enable#        -> !tmc2660 enable:1047680#  (TMC2660 status word)
        !error dome getshutterbatvoltage:error -134 standard#  (error reply format)
    A controller without the tmc2660 command group (the Caterpillar shares this driver) is assumed to answer those
    commands with an error reply in the same format.

    The controller applies its holding power only when a motion ends. The driver applies it at once when the
    rotator is idle (disable for 0, estop then enable otherwise). Connecting and loading the configuration must
    never switch the motor on or off.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <pty.h>
#include <set>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "indilogger.h"
#include "drivers/dome/nexdome_beaver.h"

namespace
{

const char *DEVICE_NAME = "Mock Beaver";

std::string testDirectory;

std::string configFile()
{
    return testDirectory + "/config.xml";
}

// A Beaver controller on a pseudo terminal. It records every command it receives.
class FakeBeaver
{
    public:
        FakeBeaver()
        {
            int slave = -1;
            if (openpty(&m_Master, &slave, nullptr, nullptr, nullptr) != 0)
                return;
            makeRaw(m_Master);
            makeRaw(slave);
            // Keep only the name: the driver sets exclusive mode on the port.
            const char *name = ttyname(slave);
            m_Port = name ? name : "";
            close(slave);
            m_Thread = std::thread(&FakeBeaver::serve, this);
        }

        ~FakeBeaver()
        {
            m_Stop = true;
            if (m_Thread.joinable())
                m_Thread.join();
            if (m_Master >= 0)
                close(m_Master);
        }

        std::string port() const
        {
            return m_Port;
        }

        // Commands that change something in the controller, in the order received.
        std::vector<std::string> writes()
        {
            static const std::set<std::string> readOnly =
            {
                "tversion", "getaz", "gethome", "getpark", "getmaxspeed", "getminspeed", "getacceleration",
                "getmaxfullrotsecs", "getmovepow", "getstoppow", "shutterisup", "status", "atpark", "athome"
            };
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::vector<std::string> result;
            for (const auto &command : m_Commands)
                if (readOnly.count(word(command, 1)) == 0)
                    result.push_back(command);
            return result;
        }

        void clearCommands()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Commands.clear();
        }

        std::atomic<int> movePower {100};
        std::atomic<int> stopPower {0};
        std::atomic<bool> moving {false};
        std::atomic<bool> hasMotorPower {true};

    private:
        static void makeRaw(int fd)
        {
            struct termios tio;
            tcgetattr(fd, &tio);
            cfmakeraw(&tio);
            tcsetattr(fd, TCSANOW, &tio);
        }

        // Word n of "!group command args"
        static std::string word(const std::string &command, size_t n)
        {
            std::istringstream words(command.substr(1, command.find('#') - 1));
            std::string result;
            for (size_t i = 0; i <= n; i++)
                if (!(words >> result))
                    return "";
            return result;
        }

        std::string reply(const std::string &command)
        {
            const std::string group = word(command, 0), name = word(command, 1), argument = word(command, 2);
            const std::string request = command.substr(1);
            if (group == "tmc2660" && !hasMotorPower)
                return "!error " + request + ":error -134 standard#";

            std::string value = "0";
            if (name == "tversion")
                value = "7:1.1.5";
            else if (name == "getaz" || name == "gethome" || name == "getpark")
                value = "0.000000";
            else if (name == "getmaxspeed")
                value = "800";
            else if (name == "getminspeed")
                value = "400";
            else if (name == "getacceleration")
                value = "500";
            else if (name == "getmaxfullrotsecs")
                value = "83";
            else if (name == "getmovepow")
                value = std::to_string(movePower);
            else if (name == "getstoppow")
                value = std::to_string(stopPower);
            else if (name == "setmovepow")
            {
                movePower = std::stoi(argument);
                value = argument;
            }
            else if (name == "setstoppow")
            {
                stopPower = std::stoi(argument);
                value = argument;
            }
            else if (name == "enable" || name == "disable")
                value = "1047680";
            else if (name == "status")
                value = std::to_string(0x1018 | (moving ? 0x0001 : 0));
            else if (name == "atpark")
                value = "1";
            return "!" + request + ":" + value + "#";
        }

        void serve()
        {
            std::string received;
            while (!m_Stop)
            {
                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(m_Master, &readSet);
                struct timeval timeout = {0, 20000};
                if (select(m_Master + 1, &readSet, nullptr, nullptr, &timeout) <= 0)
                    continue;

                char chunk[256];
                ssize_t n = read(m_Master, chunk, sizeof(chunk));
                if (n <= 0)
                {
                    // No driver has the port open (EIO on the master side).
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
                received.append(chunk, n);

                size_t pos;
                while ((pos = received.find('#')) != std::string::npos)
                {
                    std::string command = received.substr(0, pos);
                    received.erase(0, pos + 1);
                    if (command.empty() || command[0] != '!')
                        continue;
                    {
                        std::lock_guard<std::mutex> lock(m_Mutex);
                        m_Commands.push_back(command + "#");
                    }
                    std::string answer = reply(command);
                    if (write(m_Master, answer.data(), answer.size()) < 0)
                        return;
                }
            }
        }

        int m_Master {-1};
        std::string m_Port;
        std::thread m_Thread;
        std::atomic<bool> m_Stop {false};
        std::mutex m_Mutex;
        std::vector<std::string> m_Commands;
};

// Keeps the driver's XML output out of the test log. gtest reports on stdout too, so check results outside it.
class QuietStdout
{
    public:
        QuietStdout()
        {
            fflush(stdout);
            m_Saved = dup(STDOUT_FILENO);
            FILE *null = fopen("/dev/null", "w");
            if (null)
            {
                dup2(fileno(null), STDOUT_FILENO);
                fclose(null);
            }
        }
        ~QuietStdout()
        {
            fflush(stdout);
            if (m_Saved >= 0)
            {
                dup2(m_Saved, STDOUT_FILENO);
                close(m_Saved);
            }
        }
    private:
        int m_Saved {-1};
};

// Every request is sent with stdout silenced.
class BeaverUnderTest : public Beaver
{
    public:
        BeaverUnderTest()
        {
            QuietStdout quiet;
            setDeviceName(DEVICE_NAME);
            INDI::DefaultDevice::ISGetProperties(nullptr);
        }

        bool connectTo(const std::string &port)
        {
            QuietStdout quiet;
            std::vector<char> portText(port.begin(), port.end());
            portText.push_back('\0');
            char portName[] = "PORT";
            char *texts[1] = {portText.data()};
            char *textNames[1] = {portName};
            ISNewText(getDeviceName(), "DEVICE_PORT", texts, textNames, 1);

            // Never scan the system's real serial ports if the connection fails.
            setSwitch("DEVICE_AUTO_SEARCH", "INDI_DISABLED");
            setSwitch("CONNECTION", "CONNECT");
            return isConnected();
        }

        // As a client does after connecting
        void loadConfiguration()
        {
            QuietStdout quiet;
            setSwitch("CONFIG_PROCESS", "CONFIG_LOAD");
        }

        void setSwitch(const char *property, const char *element)
        {
            QuietStdout quiet;
            ISState state[1] = {ISS_ON};
            char *names[1] = {const_cast<char *>(element)};
            ISNewSwitch(getDeviceName(), property, state, names, 1);
        }

        void setMotorPower(double movePower, double stopPower)
        {
            QuietStdout quiet;
            double values[2] = {movePower, stopPower};
            char move[] = "MOTOR_MOVE_POWER", stop[] = "MOTOR_STOP_POWER";
            char *names[2] = {move, stop};
            ISNewNumber(getDeviceName(), "ROTATOR_MOTOR_POWER", values, names, 2);
        }

        void sync(double az)
        {
            QuietStdout quiet;
            double values[1] = {az};
            char value[] = "DOME_SYNC_VALUE";
            char *names[1] = {value};
            ISNewNumber(getDeviceName(), "DOME_SYNC", values, names, 1);
        }

        bool defines(const char *property)
        {
            return getProperty(property).isValid();
        }

        double number(const char *property, const char *element)
        {
            INDI::PropertyNumber vector = getProperty(property);
            auto widget = vector.findWidgetByName(element);
            return widget ? widget->getValue() : -1;
        }

        IPState state(const char *property)
        {
            return getProperty(property).getState();
        }

        bool parked()
        {
            return isParked();
        }
};

using Commands = std::vector<std::string>;

class NexDomeBeaverTest : public ::testing::Test
{
    protected:
        // Saved driver configuration: moving 100 %, holding 10 %, hold off on park enabled.
        // Park state as INDI::Dome stores it in ~/.indi/ParkData.xml.
        void prepare(bool parked)
        {
            std::ofstream config(configFile());
            config << "<INDIDriver>\n";
            config << "<newNumberVector device='" << DEVICE_NAME << "' name='ROTATOR_MOTOR_POWER'>\n";
            config << "  <oneNumber name='MOTOR_MOVE_POWER'>100</oneNumber>\n";
            config << "  <oneNumber name='MOTOR_STOP_POWER'>10</oneNumber>\n";
            config << "</newNumberVector>\n";
            config << "<newSwitchVector device='" << DEVICE_NAME << "' name='ROTATOR_PARK_HOLD_OFF'>\n";
            config << "  <oneSwitch name='INDI_ENABLED'>On</oneSwitch>\n";
            config << "  <oneSwitch name='INDI_DISABLED'>Off</oneSwitch>\n";
            config << "</newSwitchVector>\n";
            config << "</INDIDriver>\n";

            std::ofstream park(testDirectory + "/.indi/ParkData.xml");
            park << "<parkdata><device name='" << DEVICE_NAME << "'>";
            park << "<parkstatus>" << (parked ? "true" : "false") << "</parkstatus>";
            park << "<parkposition><axis1position>0.000000</axis1position></parkposition>";
            park << "</device></parkdata>\n";
        }

        // Connect and load the configuration as a client does, then forget the commands sent so far.
        void connect(bool parked, int controllerStopPower)
        {
            prepare(parked);
            beaver.stopPower = controllerStopPower;
            ASSERT_FALSE(beaver.port().empty());
            ASSERT_TRUE(driver.connectTo(beaver.port()));
            driver.loadConfiguration();
            ASSERT_EQ(driver.parked(), parked);
            beaver.clearCommands();
        }

        FakeBeaver beaver;
        BeaverUnderTest driver;
};

}

/// Connecting

TEST_F(NexDomeBeaverTest, connectWritesNothingWhenControllerHasTheSavedPower)
{
    prepare(true);
    beaver.stopPower = 0; // parked with hold off on park
    ASSERT_TRUE(driver.connectTo(beaver.port()));
    driver.loadConfiguration();

    EXPECT_EQ(beaver.writes(), Commands {});
    // While parked the controller holds 0, the panel still shows the saved holding power.
    EXPECT_EQ(driver.number("ROTATOR_MOTOR_POWER", "MOTOR_STOP_POWER"), 10);
}

TEST_F(NexDomeBeaverTest, configLoadStoresPowerWithoutSwitchingTheMotor)
{
    prepare(false);
    beaver.stopPower = 30;
    ASSERT_TRUE(driver.connectTo(beaver.port()));
    driver.loadConfiguration();

    // The saved holding power is stored, but estop, enable or disable would switch the motor at connect.
    EXPECT_EQ(beaver.writes(), (Commands {"!tmc2660 setmovepow 100#", "!tmc2660 setstoppow 10#", "!seletek savefs#"}));
    EXPECT_EQ(beaver.stopPower, 10);
}

TEST_F(NexDomeBeaverTest, controllerWithoutMotorPowerHidesTheProperties)
{
    beaver.hasMotorPower = false;
    connect(false, 0);

    EXPECT_FALSE(driver.defines("ROTATOR_MOTOR_POWER"));
    EXPECT_FALSE(driver.defines("ROTATOR_PARK_HOLD_OFF"));

    driver.setSwitch("DOME_PARK", "PARK");
    EXPECT_EQ(beaver.writes(), Commands {"!dome gopark#"});
}

/// Holding power

TEST_F(NexDomeBeaverTest, holdingChangeWhileIdleAppliesAtOnce)
{
    connect(false, 10);

    driver.setMotorPower(100, 30);
    EXPECT_EQ(beaver.writes(), (Commands {"!tmc2660 setmovepow 100#", "!tmc2660 setstoppow 30#", "!seletek savefs#",
                                          "!tmc2660 estop#", "!tmc2660 enable#"
                                         }));
    EXPECT_EQ(driver.state("ROTATOR_MOTOR_POWER"), IPS_OK);

    beaver.clearCommands();
    driver.setMotorPower(100, 0);
    EXPECT_EQ(beaver.writes(), (Commands {"!tmc2660 setmovepow 100#", "!tmc2660 setstoppow 0#", "!seletek savefs#",
                                          "!tmc2660 disable#"
                                         }));
}

TEST_F(NexDomeBeaverTest, holdingChangeWhileMovingIsOnlyStored)
{
    connect(false, 10);
    beaver.moving = true;

    driver.setMotorPower(100, 20);
    // estop while moving would stop the dome
    EXPECT_EQ(beaver.writes(), (Commands {"!tmc2660 setmovepow 100#", "!tmc2660 setstoppow 20#", "!seletek savefs#"}));
}

TEST_F(NexDomeBeaverTest, unchangedPowerWritesNothing)
{
    connect(false, 10);

    driver.setMotorPower(100, 10);
    EXPECT_EQ(beaver.writes(), Commands {});
}

/// Parking

TEST_F(NexDomeBeaverTest, parkSwitchesMotorOffBeforeParking)
{
    connect(false, 10);

    driver.setSwitch("DOME_PARK", "PARK");
    // Also when the dome is already at the park position and does not move
    EXPECT_EQ(beaver.writes(), (Commands {"!tmc2660 setmovepow 100#", "!tmc2660 setstoppow 0#", "!seletek savefs#",
                                          "!tmc2660 disable#", "!dome gopark#"
                                         }));
    EXPECT_EQ(driver.number("ROTATOR_MOTOR_POWER", "MOTOR_STOP_POWER"), 10);
}

TEST_F(NexDomeBeaverTest, unparkRestoresHoldingAtOnce)
{
    connect(true, 0);

    driver.setSwitch("DOME_PARK", "UNPARK");
    EXPECT_FALSE(driver.parked());
    EXPECT_EQ(beaver.writes(), (Commands {"!tmc2660 setmovepow 100#", "!tmc2660 setstoppow 10#", "!seletek savefs#",
                                          "!tmc2660 estop#", "!tmc2660 enable#"
                                         }));
}

TEST_F(NexDomeBeaverTest, holdOffDisabledKeepsHoldingWhenParked)
{
    connect(false, 10);

    driver.setSwitch("ROTATOR_PARK_HOLD_OFF", "INDI_DISABLED");
    driver.setSwitch("DOME_PARK", "PARK");
    EXPECT_EQ(beaver.writes(), Commands {"!dome gopark#"});
}

/// Sync

TEST_F(NexDomeBeaverTest, syncSetsAzimuthWithoutMotion)
{
    connect(false, 10);

    driver.sync(123.4);
    EXPECT_EQ(beaver.writes(), Commands {"!dome setaz 123.40#"});
}

TEST_F(NexDomeBeaverTest, syncIsRefusedWhileMoving)
{
    connect(false, 10);
    beaver.moving = true;

    driver.sync(123.4);
    EXPECT_EQ(beaver.writes(), Commands {});
}

int main(int argc, char **argv)
{
    // INDI::Dome keeps the park state in $HOME/.indi/ParkData.xml: use a private home directory.
    char directory[] = "/tmp/test_nexdome_beaver_XXXXXX";
    if (!mkdtemp(directory))
        return 1;
    testDirectory = directory;
    std::string indiDirectory = testDirectory + "/.indi";
    mkdir(indiDirectory.c_str(), 0700);
    setenv("HOME", directory, 1);
    setenv("INDICONFIG", configFile().c_str(), 1);

    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::filesystem::remove_all(testDirectory);
    return rc;
}
