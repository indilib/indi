/*
    Regression test: every client that connects sends getProperties. Drivers must load their settings
    from the config for the first client only, otherwise each new client reverts the changes made since.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "indilogger.h"
#include "drivers/focuser/focus_simulator.h"
#include "drivers/weather/weatherwatcher.h"
#include "drivers/weather/aagsolo.h"

char _me[] = "test_first_client_config";
char *me = _me;

namespace
{
std::string configFile;

// Write a driver config file that holds the given saved properties.
void writeConfig(const std::string &properties)
{
    FILE *fp = fopen(configFile.c_str(), "w");
    ASSERT_NE(fp, nullptr);
    fprintf(fp, "<INDIDriver>\n%s</INDIDriver>\n", properties.c_str());
    fclose(fp);
}

std::string onSwitch(INDI::DefaultDevice &driver, const char *property)
{
    auto sp = driver.getSwitch(property);
    auto on = sp.findOnSwitch();
    return on ? on->getName() : "";
}

std::string text(INDI::DefaultDevice &driver, const char *property)
{
    auto tp = driver.getText(property);
    return tp[0].getText() ? tp[0].getText() : "";
}

void setText(INDI::DefaultDevice &driver, const char *property, const char *element, const char *value)
{
    char *texts[] = { const_cast<char *>(value) };
    char *names[] = { const_cast<char *>(element) };
    driver.ISNewText(driver.getDeviceName(), property, texts, names, 1);
}
}

TEST(FirstClientConfig, focusSimulatorKeepsTheMode)
{
    writeConfig("<newSwitchVector device='Focuser Simulator' name='Mode'>\n"
                "  <oneSwitch name='All'>Off</oneSwitch>\n"
                "  <oneSwitch name='Absolute'>On</oneSwitch>\n"
                "  <oneSwitch name='Relative'>Off</oneSwitch>\n"
                "  <oneSwitch name='Timer'>Off</oneSwitch>\n"
                "</newSwitchVector>\n");

    FocusSim driver;
    driver.ISGetProperties(nullptr);
    ASSERT_EQ(onSwitch(driver, "Mode"), "Absolute");

    ISState states[] = { ISS_OFF, ISS_OFF, ISS_ON, ISS_OFF };
    char *names[] = { const_cast<char *>("All"), const_cast<char *>("Absolute"), const_cast<char *>("Relative"),
                      const_cast<char *>("Timer")
                    };
    driver.ISNewSwitch(driver.getDeviceName(), "Mode", states, names, 4);
    ASSERT_EQ(onSwitch(driver, "Mode"), "Relative");

    // Another client connects
    driver.ISGetProperties(nullptr);
    EXPECT_EQ(onSwitch(driver, "Mode"), "Relative");
}

TEST(FirstClientConfig, weatherWatcherKeepsTheSourceAndSeparator)
{
    writeConfig("<newTextVector device='Weather Watcher' name='WATCH_SOURCE'>\n"
                "  <oneText name='URL'>/saved/weather.txt</oneText>\n"
                "</newTextVector>\n");

    WeatherWatcher driver;
    driver.ISGetProperties(nullptr);
    ASSERT_EQ(text(driver, "WATCH_SOURCE"), "/saved/weather.txt");
    ASSERT_EQ(text(driver, "SEPARATOR_KEYWORD"), "=");

    setText(driver, "WATCH_SOURCE", "URL", "/changed/weather.txt");
    setText(driver, "SEPARATOR_KEYWORD", "SEPARATOR", ":");

    // Another client connects
    driver.ISGetProperties(nullptr);
    EXPECT_EQ(text(driver, "WATCH_SOURCE"), "/changed/weather.txt");
    EXPECT_EQ(text(driver, "SEPARATOR_KEYWORD"), ":");
}

// The separator was applied instead of saved, so it fell back to "=" on every start.
TEST(FirstClientConfig, weatherWatcherSavesTheSeparator)
{
    writeConfig("");
    {
        WeatherWatcher driver;
        driver.ISGetProperties(nullptr);
        setText(driver, "SEPARATOR_KEYWORD", "SEPARATOR", ":");

        ISState states[] = { ISS_ON };
        char *names[] = { const_cast<char *>("CONFIG_SAVE") };
        driver.ISNewSwitch(driver.getDeviceName(), "CONFIG_PROCESS", states, names, 1);
    }

    // The driver starts again
    WeatherWatcher driver;
    driver.ISGetProperties(nullptr);
    EXPECT_EQ(text(driver, "SEPARATOR_KEYWORD"), ":");
}

TEST(FirstClientConfig, aagSoloKeepsTheHost)
{
    writeConfig("<newTextVector device='AAG Solo Cloudwatcher' name='SOLO_HOST'>\n"
                "  <oneText name='HOSTNAME'>saved.local</oneText>\n"
                "</newTextVector>\n");

    AAGSolo driver;
    driver.ISGetProperties(nullptr);
    ASSERT_EQ(text(driver, "SOLO_HOST"), "saved.local");

    setText(driver, "SOLO_HOST", "HOSTNAME", "changed.local");

    // Another client connects
    driver.ISGetProperties(nullptr);
    EXPECT_EQ(text(driver, "SOLO_HOST"), "changed.local");
}

int main(int argc, char **argv)
{
    // Never read or write the user's real ~/.indi driver configuration.
    // Use a file private to this process so that test runs can happen in parallel.
    configFile = "/tmp/test_first_client_config_" + std::to_string(getpid()) + "_config.xml";
    setenv("INDICONFIG", configFile.c_str(), 1);
    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);
    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();
    std::remove(configFile.c_str());
    std::remove((configFile + ".default").c_str());
    return rc;
}
