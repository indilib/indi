/*
    Tests for the dome shutter park policy (DOME_SHUTTER_PARK_POLICY) applied by INDI::Dome.

    With "Close On Park" enabled, INDI::Dome closes the shutter before asking the driver to park, and the park
    (DOME_PARK) must stay busy until the dome reached the park position AND the shutter is closed. Clients such as
    Ekos rely on DOME_PARK to know that the observatory shutdown is complete. "Open On UnPark" works the other way
    around: the driver unparks first, then INDI::Dome opens the shutter, and the unpark completes once both are done.

    The mock dome below plays the driver. It records every hardware request, and the tests report the dome motion and
    the shutter progress the same ways real drivers do: SetParked(), setDomeState(DOME_PARKED), setShutterState(), or
    by writing DomeShutterSP directly.

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#include "indidome.h"
#include "indidevapi.h"
#include "indilogger.h"

namespace
{

const char *DEVICE_NAME = "Mock Dome";

class MockDome : public INDI::Dome
{
    public:
        MockDome()
        {
            setDeviceName(DEVICE_NAME);
            SetDomeCapability(DOME_CAN_ABORT | DOME_CAN_PARK | DOME_HAS_SHUTTER);
        }

        const char *getDefaultName() override
        {
            return DEVICE_NAME;
        }

        // What the "hardware" answers.
        IPState parkResult {IPS_BUSY};
        IPState unparkResult {IPS_BUSY};
        IPState shutterResult {IPS_BUSY};
        bool abortResult {true};

        // Hardware requests, in order.
        std::vector<std::string> calls;

        int count(const std::string &call) const
        {
            int n = 0;
            for (const auto &c : calls)
                n += (c == call);
            return n;
        }

        void setPolicy(bool closeOnPark, bool openOnUnpark)
        {
            ShutterParkPolicySP[SHUTTER_CLOSE_ON_PARK].setState(closeOnPark ? ISS_ON : ISS_OFF);
            ShutterParkPolicySP[SHUTTER_OPEN_ON_UNPARK].setState(openOnUnpark ? ISS_ON : ISS_OFF);
        }

        void setHandledByDriver(bool enabled)
        {
            SetShutterParkPolicyHandledByDriver(enabled);
        }

        // Driver reports through the park API.
        void reportParked(bool parked)
        {
            SetParked(parked);
        }

        // Driver reports the shutter by writing DomeShutterSP directly, without setShutterState().
        void writeShutterProperty(ShutterOperation operation, IPState state)
        {
            DomeShutterSP.reset();
            DomeShutterSP[operation].setState(ISS_ON);
            DomeShutterSP.setState(state);
            DomeShutterSP.apply();
        }

        bool parked()
        {
            return isParked();
        }

        IPState parkState() const
        {
            return ParkSP.getState();
        }

        bool parkSwitchOn() const
        {
            return ParkSP[0].getState() == ISS_ON;
        }

        IPState shutterPropertyState() const
        {
            return DomeShutterSP.getState();
        }

        // Client requests.
        bool press(const char *property, const char *element)
        {
            ISState states[] = {ISS_ON};
            char *names[] = {const_cast<char *>(element)};
            return ISNewSwitch(DEVICE_NAME, property, states, names, 1);
        }
        bool pressPark()
        {
            return press("DOME_PARK", "PARK");
        }
        bool pressUnpark()
        {
            return press("DOME_PARK", "UNPARK");
        }
        bool pressShutter(ShutterOperation operation)
        {
            return press("DOME_SHUTTER", operation == SHUTTER_OPEN ? "SHUTTER_OPEN" : "SHUTTER_CLOSE");
        }
        bool pressAbort()
        {
            return press("DOME_ABORT_MOTION", "ABORT");
        }

    protected:
        IPState Park() override
        {
            calls.push_back("Park");
            return parkResult;
        }

        IPState UnPark() override
        {
            calls.push_back("UnPark");
            return unparkResult;
        }

        IPState ControlShutter(ShutterOperation operation) override
        {
            calls.push_back(operation == SHUTTER_OPEN ? "OpenShutter" : "CloseShutter");
            return shutterResult;
        }

        bool Abort() override
        {
            calls.push_back("Abort");
            return abortResult;
        }
};

class DomeShutterParkPolicyTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            dome.initProperties();
            dome.setPolicy(true, true);
        }

        // Dome is unparked with the shutter reported open, as during an observing session.
        void startObserving()
        {
            dome.setShutterState(INDI::Dome::SHUTTER_OPENED);
            ASSERT_FALSE(dome.parked());
            dome.calls.clear();
        }

        // Dome is parked with the shutter reported closed, as before an observing session.
        void startClosed()
        {
            dome.setShutterState(INDI::Dome::SHUTTER_CLOSED);
            dome.reportParked(true);
            ASSERT_TRUE(dome.parked());
            dome.calls.clear();
        }

        MockDome dome;
};

// Run the driver event loop, which drives the shutter poll timer of INDI::Dome.
void runEventLoop(int milliseconds)
{
    int neverSet = 0;
    IEDeferLoop(milliseconds, &neverSet);
}

}

///////////////////////////////////////////////////////////////////////////////////////////////
/// Park
///////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DomeShutterParkPolicyTest, parkClosesShutterBeforeParking)
{
    startObserving();
    ASSERT_TRUE(dome.pressPark());
    ASSERT_EQ(dome.calls, (std::vector<std::string> {"CloseShutter", "Park"}));
    EXPECT_EQ(dome.parkState(), IPS_BUSY);
}

TEST_F(DomeShutterParkPolicyTest, parkStaysBusyUntilShutterClosesAfterRotation)
{
    startObserving();
    dome.pressPark();

    dome.reportParked(true); // Dome reached the park position, shutter still closing.
    EXPECT_EQ(dome.parkState(), IPS_BUSY);
    EXPECT_FALSE(dome.parked());

    dome.setShutterState(INDI::Dome::SHUTTER_CLOSED);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_TRUE(dome.parkSwitchOn());
    EXPECT_TRUE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, parkStaysBusyUntilRotationEndsAfterShutter)
{
    startObserving();
    dome.pressPark();

    dome.setShutterState(INDI::Dome::SHUTTER_CLOSED);
    EXPECT_EQ(dome.parkState(), IPS_BUSY);
    EXPECT_FALSE(dome.parked());

    dome.reportParked(true);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_TRUE(dome.parked());
}

// NexDome, MaxDomeII and OCS report the end of the park motion with setDomeState(DOME_PARKED).
TEST_F(DomeShutterParkPolicyTest, parkReportedThroughDomeStateAlsoWaitsForShutter)
{
    startObserving();
    dome.pressPark();

    dome.setDomeState(INDI::Dome::DOME_PARKED);
    EXPECT_EQ(dome.parkState(), IPS_BUSY);
    EXPECT_FALSE(dome.parked());

    dome.setShutterState(INDI::Dome::SHUTTER_CLOSED);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_TRUE(dome.parked());
}

// Some drivers (e.g. MaxDomeII, Baader) never call setShutterState() and only update DomeShutterSP. INDI::Dome then
// learns about the closed shutter from its own poll timer.
TEST_F(DomeShutterParkPolicyTest, shutterReportedOnlyThroughPropertyCompletesPark)
{
    startObserving();
    dome.pressPark();
    dome.reportParked(true);

    dome.writeShutterProperty(INDI::Dome::SHUTTER_CLOSE, IPS_OK);
    runEventLoop(1500);

    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_TRUE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, parkCompletingImmediatelyStillWaitsForShutter)
{
    startObserving();
    dome.parkResult = IPS_OK; // Dome already at the park position.

    dome.pressPark();
    EXPECT_EQ(dome.parkState(), IPS_BUSY);
    EXPECT_FALSE(dome.parked());

    dome.setShutterState(INDI::Dome::SHUTTER_CLOSED);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_TRUE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, shutterErrorFailsParkAndParkCanBeRetried)
{
    startObserving();
    dome.pressPark();
    dome.reportParked(true);

    dome.setShutterState(INDI::Dome::SHUTTER_ERROR);
    EXPECT_EQ(dome.parkState(), IPS_ALERT);
    EXPECT_FALSE(dome.parked()); // The observatory is not closed.

    dome.calls.clear();
    ASSERT_TRUE(dome.pressPark());
    EXPECT_EQ(dome.calls, (std::vector<std::string> {"CloseShutter", "Park"}));
    EXPECT_EQ(dome.parkState(), IPS_BUSY);
}

TEST_F(DomeShutterParkPolicyTest, shutterCommandFailureFailsParkWithoutMovingDome)
{
    startObserving();
    dome.shutterResult = IPS_ALERT;

    EXPECT_FALSE(dome.pressPark());
    EXPECT_EQ(dome.count("Park"), 0);
    EXPECT_EQ(dome.parkState(), IPS_ALERT);
    EXPECT_FALSE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, shutterKnownClosedIsNotClosedAgain)
{
    dome.setShutterState(INDI::Dome::SHUTTER_CLOSED);
    dome.calls.clear();

    dome.pressPark();
    EXPECT_EQ(dome.calls, (std::vector<std::string> {"Park"}));

    dome.reportParked(true);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_TRUE(dome.parked());
}

// DomeShutterSP starts as "closed" before the driver read the shutter, and some drivers (e.g. DomePro2) only update
// it while the shutter moves. That default must not be taken as a closed shutter.
TEST_F(DomeShutterParkPolicyTest, defaultShutterPropertyIsNotTrustedAsClosed)
{
    ASSERT_EQ(dome.getShutterState(), INDI::Dome::SHUTTER_UNKNOWN);
    ASSERT_EQ(dome.shutterPropertyState(), IPS_OK);

    dome.pressPark();
    EXPECT_EQ(dome.calls, (std::vector<std::string> {"CloseShutter", "Park"}));
}

TEST_F(DomeShutterParkPolicyTest, policyOffDoesNotTouchShutter)
{
    dome.setPolicy(false, false);
    startObserving();

    dome.pressPark();
    EXPECT_EQ(dome.calls, (std::vector<std::string> {"Park"}));
    dome.reportParked(true);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_TRUE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, driverHandlingPolicyIsLeftAlone)
{
    dome.setHandledByDriver(true);
    startObserving();

    dome.pressPark();
    EXPECT_EQ(dome.calls, (std::vector<std::string> {"Park"}));
    dome.reportParked(true);
    EXPECT_EQ(dome.parkState(), IPS_OK);
}

TEST_F(DomeShutterParkPolicyTest, manualOpenDuringParkStopsWaitingForShutter)
{
    startObserving();
    dome.pressPark();
    dome.reportParked(true);
    ASSERT_EQ(dome.parkState(), IPS_BUSY);

    dome.pressShutter(INDI::Dome::SHUTTER_OPEN);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_TRUE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, abortWhileWaitingForShutterAllowsNewPark)
{
    startObserving();
    dome.pressPark();
    dome.reportParked(true);
    ASSERT_EQ(dome.parkState(), IPS_BUSY);

    dome.pressAbort();
    EXPECT_NE(dome.parkState(), IPS_BUSY);
    EXPECT_FALSE(dome.parked());

    dome.calls.clear();
    ASSERT_TRUE(dome.pressPark());
    EXPECT_EQ(dome.count("Park"), 1);
}

// The dome simulator refuses an abort while its shutter moves. The park must still not stay stuck.
TEST_F(DomeShutterParkPolicyTest, failedAbortWhileWaitingForShutterAllowsNewPark)
{
    startObserving();
    dome.pressPark();
    dome.reportParked(true);
    dome.abortResult = false;

    dome.pressAbort();
    EXPECT_NE(dome.parkState(), IPS_BUSY);

    dome.calls.clear();
    ASSERT_TRUE(dome.pressPark());
    EXPECT_EQ(dome.count("Park"), 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////
/// UnPark
///////////////////////////////////////////////////////////////////////////////////////////////

TEST_F(DomeShutterParkPolicyTest, unparkOpensShutterAfterUnparking)
{
    startClosed();
    ASSERT_TRUE(dome.pressUnpark());
    ASSERT_EQ(dome.calls, (std::vector<std::string> {"UnPark", "OpenShutter"}));
    EXPECT_EQ(dome.parkState(), IPS_BUSY);
}

TEST_F(DomeShutterParkPolicyTest, unparkStaysBusyUntilShutterOpens)
{
    startClosed();
    dome.unparkResult = IPS_OK;

    dome.pressUnpark();
    EXPECT_EQ(dome.parkState(), IPS_BUSY);
    EXPECT_TRUE(dome.parked());

    dome.setShutterState(INDI::Dome::SHUTTER_OPENED);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_FALSE(dome.parkSwitchOn());
    EXPECT_FALSE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, unparkStaysBusyUntilMotionEndsAfterShutter)
{
    startClosed();
    dome.pressUnpark();

    dome.setShutterState(INDI::Dome::SHUTTER_OPENED);
    EXPECT_EQ(dome.parkState(), IPS_BUSY);

    dome.reportParked(false);
    EXPECT_EQ(dome.parkState(), IPS_OK);
    EXPECT_FALSE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, failedUnparkNeverOpensShutter)
{
    startClosed();
    dome.unparkResult = IPS_ALERT;

    EXPECT_FALSE(dome.pressUnpark());
    EXPECT_EQ(dome.count("OpenShutter"), 0);
    EXPECT_EQ(dome.parkState(), IPS_ALERT);
    EXPECT_TRUE(dome.parked());
}

TEST_F(DomeShutterParkPolicyTest, shutterFailingToOpenUnparksWithAlert)
{
    startClosed();
    dome.unparkResult = IPS_OK;
    dome.shutterResult = IPS_ALERT;

    dome.pressUnpark();
    EXPECT_EQ(dome.parkState(), IPS_ALERT);
    EXPECT_FALSE(dome.parked());
}

int main(int argc, char **argv)
{
    // Never read or write the user's real ~/.indi configuration or ParkData.xml.
    const std::string home = "/tmp/test_dome_shutter_park_policy_" + std::to_string(getpid());
    const std::string configFile = home + "/config.xml";
    std::string mkdir = "mkdir -p '" + home + "/.indi'";
    if (std::system(mkdir.c_str()) != 0)
        return 1;
    setenv("HOME", home.c_str(), 1);
    setenv("INDICONFIG", configFile.c_str(), 1);

    INDI::Logger::getInstance().configure("", INDI::Logger::file_off,
                                          INDI::Logger::DBG_ERROR, INDI::Logger::DBG_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    int rc = RUN_ALL_TESTS();

    std::string cleanup = "rm -rf '" + home + "'";
    if (std::system(cleanup.c_str()) != 0)
        rc = rc ? rc : 1;
    return rc;
}
