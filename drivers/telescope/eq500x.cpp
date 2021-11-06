#if 0
LX200 - based Omegon EQ500X Equatorial Mount

Copyright (C) 2019 Eric Dejouhanet (eric.dejouhanet@gmail.com)

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA
#endif

#include <cmath>
#include <memory>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <cassert>

#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include "lx200generic.h"
#include "eq500x.h"

#include "indicom.h"
#include "lx200driver.h"

typedef struct _simEQ500X
{
    char MechanicalRAStr[16];
        char MechanicalDECStr[16];
        double MechanicalRA;
        double MechanicalDEC;
        clock_t last_sim;
    } simEQ500X_t;

simEQ500X_t simEQ500X_zero =
{
    "00:00:00",
    "+00*00'00",
    0.0, 0.0,
    0,
};

simEQ500X_t simEQ500X = simEQ500X_zero;

#define MechanicalPoint_DEC_FormatR "+DD:MM:SS"
#define MechanicalPoint_DEC_FormatW "+DDD:MM:SS"
#define MechanicalPoint_RA_Format  "HH:MM:SS"

// This is the duration the serial port waits for while expecting replies
static int const EQ500X_TIMEOUT = 5;

// One degree, one arcminute, one arcsecond
static double constexpr ONEDEGREE = 1.0;
static double constexpr ARCMINUTE = ONEDEGREE / 60.0;
static double constexpr ARCSECOND = ONEDEGREE / 3600.0;

// This is the minimum detectable movement in RA/DEC
static double /*constexpr*/ RA_GRANULARITY = std::lround((15.0 * ARCSECOND) * 3600.0) / 3600.0;
static double /*constexpr*/ DEC_GRANULARITY = std::lround((1.0 * ARCSECOND) * 3600.0) / 3600.0;

// This is the number of loops expected to achieve convergence on each slew rate
// A full rotation at 5deg/s would take 360/5=72s to complete at RS speed, checking position twice per second
static int MAX_CONVERGENCE_LOOPS = 144;

// Hardcoded adjustment intervals
// RA/DEC deltas are adjusted at specific 'slew_rate' down to 'epsilon' degrees when smaller than 'distance' degrees
// The greater adjustment requirement drives the slew rate (one single command for both axis)
struct _adjustment
{
    char const * slew_rate;
    int switch_index;
    double epsilon;
    double distance;
    int polling_interval;
}
const adjustments[] =
{
    {":RG#", 0,   1 * ARCSECOND,  0.7 * ARCMINUTE, 100 }, // Guiding speed
    {":RC#", 1, 0.7 * ARCMINUTE,   10 * ARCMINUTE, 200 }, // Centering speed
    {":RM#", 2,  10 * ARCMINUTE,    5 * ONEDEGREE, 500 }, // Finding speed
    {":RS#", 3,   5 * ONEDEGREE,  360 * ONEDEGREE, 1000 }
}; // Slew speed

/**************************************************************************************
** EQ500X Constructor
***************************************************************************************/
EQ500X::EQ500X(): LX200Generic()
{
    setVersion(1, 1);

    // Sanitize constants: epsilon of a slew rate must be smaller than distance of its smaller sibling
    for (size_t i = 0; i < sizeof(adjustments) / sizeof(adjustments[0]) - 1; i++)
        assert(adjustments[i + 1].epsilon <= adjustments[i].distance);

    // Sanitize constants: epsilon of slew rates must be smaller than distance
    for (size_t i = 0; i < sizeof(adjustments) / sizeof(adjustments[0]); i++)
        assert(adjustments[i].epsilon <= adjustments[i].distance);

    // No pulse guiding (mount doesn't support Mgx commands) nor tracking frequency nor nothing generic has actually
    setLX200Capability(0);

    // Sync, goto, abort, location and 4 slew rates, no guiding rates and no park position
    SetTelescopeCapability(TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT | TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_PIER_SIDE, 4);

    LOG_DEBUG("Initializing from EQ500X device...");
}

/**************************************************************************************
**
***************************************************************************************/
const char *EQ500X::getDefautName()
{
    return "EQ500X";
}

double EQ500X::getLST()
{
    return get_local_sidereal_time(LocationN[LOCATION_LONGITUDE].value);
}

void EQ500X::resetSimulation()
{
    simEQ500X = simEQ500X_zero;
}

/**************************************************************************************
**
***************************************************************************************/


bool EQ500X::initProperties()
{
    LX200Telescope::initProperties();

    // Mount tracks as soon as turned on
    TrackState = SCOPE_TRACKING;

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
void EQ500X::getBasicData()
{
    /* */
}

bool EQ500X::checkConnection()
{
    if (!isSimulation())
    {
        if (PortFD <= 0)
            return false;

        LOG_DEBUG("Testing telescope connection using GR...");
        tty_set_debug(1);

        LOG_DEBUG("Clearing input...");
        tcflush(PortFD, TCIFLUSH);
    }

    for (int i = 0; i < 2; i++)
    {
        if (ReadScopeStatus())
        {
            if(1 <= i)
            {
                LOG_DEBUG("Failure. Telescope is not responding to GR/GD!");
                return false;
            }
        }
        else break;

        const struct timespec timeout = {0, 50000000L};
        nanosleep(&timeout, nullptr);
    }

    /* Blink the control pad */
#if 0
    const struct timespec timeout = {0, 250000000L};
    sendCmd(":RG#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RC#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RM#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RS#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RC#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RM#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RS#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RC#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RM#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RS#");
    nanosleep(&timeout, nullptr);
    sendCmd(":RG#");
#endif

    LOG_DEBUG("Connection check successful!");
    if (!isSimulation())
        tty_set_debug(0);
    return true;
}

bool EQ500X::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(elevation);
    LOGF_INFO("Location updated: Longitude (%g) Latitude (%g)", longitude, latitude);

    // Only update LST if the mount is connected and "parked" looking at the pole
    if (isConnected() && !getCurrentMechanicalPosition(currentMechPosition) && currentMechPosition.atParkingPosition())
    {
        // HACK: Longitude used by getLST is updated after this function returns, so hack a new longitude first
        double const prevLongitude = LocationN[LOCATION_LONGITUDE].value;
        LocationN[LOCATION_LONGITUDE].value = longitude;

        double const LST = getLST();
        Sync(LST - 6, currentMechPosition.DECsky());
        LOGF_INFO("Location updated: mount considered parked, synced to LST %gh.", LST);

        LocationN[LOCATION_LONGITUDE].value = prevLongitude;
    }

    return true;
}

bool EQ500X::ReadScopeStatus()
{
    if (!isConnected())
        return false;

    // Movement markers, adjustment is done when no movement is required and all flags are cleared
    //static bool east = false, west = false, north = false, south = false;

    // Current adjustment rate
    //static struct _adjustment const * adjustment = nullptr;

    // If simulating, do simulate rates - in that case currentPosition is driven by currentRA/currentDEC
    if (isSimulation())
    {
        // These are the simulated rates
        double const rates[sizeof(adjustments)] =
        {
            /*RG*/5 * ARCSECOND,
            /*RC*/5 * ARCMINUTE,
            /*RM*/20 * ARCMINUTE,
            /*RS*/5 * ONEDEGREE
        };

        // Calculate elapsed time since last status read
        struct timespec clock = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &clock);
        long const now = clock.tv_sec * 1000 + static_cast <long> (round(clock.tv_nsec / 1000000.0));
        double const delta = simEQ500X.last_sim ? static_cast <double> (now - simEQ500X.last_sim) / 1000.0 : 0.0;
        simEQ500X.last_sim = now;

        // Simulate movement if needed
        if (nullptr != adjustment)
        {
            // Use currentRA/currentDEC to store smaller-than-one-arcsecond values
            if (RAmDecrease) simEQ500X.MechanicalRA = std::fmod(simEQ500X.MechanicalRA - rates[adjustment - adjustments] * delta / 15.0
                        + 24.0, 24.0);
            if (RAmIncrease) simEQ500X.MechanicalRA = std::fmod(simEQ500X.MechanicalRA + rates[adjustment - adjustments] * delta / 15.0
                        + 24.0, 24.0);
            if (DECmDecrease) simEQ500X.MechanicalDEC -= rates[adjustment - adjustments] * delta;
            if (DECmIncrease) simEQ500X.MechanicalDEC += rates[adjustment - adjustments] * delta;

            // Update current position and rewrite simulated mechanical positions
            MechanicalPoint p(simEQ500X.MechanicalRA, simEQ500X.MechanicalDEC);
            p.toStringRA(simEQ500X.MechanicalRAStr, sizeof(simEQ500X.MechanicalRAStr));
            p.toStringDEC_Sim(simEQ500X.MechanicalDECStr, sizeof(simEQ500X.MechanicalDECStr));

            LOGF_DEBUG("New mechanical RA/DEC simulated as %lf°/%lf° (%+lf°,%+lf°), stored as %lfh/%lf° = %s/%s",
                       simEQ500X.MechanicalRA * 15.0, simEQ500X.MechanicalDEC, (RAmDecrease
                               || RAmIncrease) ? rates[adjustment - adjustments]*delta : 0, (DECmDecrease
                                       || DECmIncrease) ? rates[adjustment - adjustments]*delta : 0, p.RAm(), p.DECm(), simEQ500X.MechanicalRAStr,
                       simEQ500X.MechanicalDECStr);
        }
    }

    if (getCurrentMechanicalPosition(currentMechPosition))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error reading RA/DEC.");
        return false;
    }

    bool const ra_changed = currentRA != currentMechPosition.RAsky();
    bool const dec_changed = currentDEC != currentMechPosition.DECsky();

    if (dec_changed)
        currentDEC = currentMechPosition.DECsky();

    if (ra_changed)
    {
        currentRA = currentMechPosition.RAsky();

        // Update the side of pier - rangeHA is NOT suitable here
        double HA = rangeHA(getLST() - currentRA);
        while (+12 <= HA) HA -= 24;
        while (HA <= -12) HA += 24;
        switch (currentMechPosition.getPointingState())
        {
            case MechanicalPoint::POINTING_NORMAL:
                LX200Telescope::setPierSide(HA < 6 ? PIER_EAST : PIER_WEST);
                break;
            case MechanicalPoint::POINTING_BEYOND_POLE:
                LX200Telescope::setPierSide(6 < HA ? PIER_EAST : PIER_WEST);
                break;
        }
        LOGF_DEBUG("Mount HA=%lfh pointing %s on %s side", HA,
                   currentMechPosition.getPointingState() == MechanicalPoint::POINTING_NORMAL ? "normal" : "beyond pole",
                   getPierSide() == PIER_EAST ? "east" : "west");
    }

    // If we are using the goto feature, check state
    if (TrackState == SCOPE_SLEWING && _gotoEngaged)
    {
        if (EqN[AXIS_RA].value == currentRA && EqN[AXIS_DE].value == currentDEC)
        {
            _gotoEngaged = false;

            // Preliminary goto is complete, continue
            if (!Goto(targetMechPosition.RAsky(), targetMechPosition.DECsky()))
                goto slew_failure;
        }
    }

    // If we are adjusting, adjust movement and timer time to achieve arcsecond goto precision
    if (TrackState == SCOPE_SLEWING && !_gotoEngaged)
    {
        // Compute RA/DEC deltas - keep in mind RA is in hours on the mount, with a granularity of 15 degrees
        double const ra_delta = currentMechPosition.RA_degrees_to(targetMechPosition);
        double const dec_delta = currentMechPosition.DEC_degrees_to(targetMechPosition);
        double const abs_ra_delta = std::abs(ra_delta);
        double const abs_dec_delta = std::abs(dec_delta);

        // If mount is not at target, adjust
        if (RA_GRANULARITY <= abs_ra_delta || DEC_GRANULARITY <= abs_dec_delta)
        {
            // This will hold required adjustments in RA and DEC axes
            struct _adjustment const *ra_adjust = nullptr, *dec_adjust = nullptr;

            // Choose slew rate for RA based on distance to target
            for(size_t i = 0; i < sizeof(adjustments) / sizeof(adjustments[0]) && nullptr == ra_adjust; i++)
                if (abs_ra_delta <= adjustments[i].distance)
                    ra_adjust = &adjustments[i];
            assert(nullptr != ra_adjust);
            LOGF_DEBUG("RA  %lf-%lf = %+lf° under %lf° would require adjustment at %s until less than %lf°",
                       targetMechPosition.RAm() * 15.0, currentMechPosition.RAm() * 15.0, ra_delta, ra_adjust->distance, ra_adjust->slew_rate,
                       std::max(ra_adjust->epsilon, 15.0 / 3600.0));

            // Choose slew rate for DEC based on distance to target
            for(size_t i = 0; i < sizeof(adjustments) / sizeof(adjustments[0]) && nullptr == dec_adjust; i++)
                if (abs_dec_delta <= adjustments[i].distance)
                    dec_adjust = &adjustments[i];
            assert(nullptr != dec_adjust);
            LOGF_DEBUG("DEC %lf-%lf = %+lf° under %lf° would require adjustment at %s until less than %lf°",
                       targetMechPosition.DECm(), currentMechPosition.DECm(), dec_delta, dec_adjust->distance, dec_adjust->slew_rate,
                       dec_adjust->epsilon);

            // This will hold the command string to send to the mount, with move commands
            char CmdString[32] = {0};

            // Previous alignment marker to spot when to change slew rate
            static struct _adjustment const * previous_adjustment = nullptr;

            // We adjust the axis which has the faster slew rate first, eventually both axis at the same time if they have same speed
            // Because we have only one rate for both axes, we need to choose the fastest rate and control the axis (eventually both) which requires that rate
            adjustment = ra_adjust < dec_adjust ? dec_adjust : ra_adjust;

            // If RA was moving but now would be moving at the wrong rate, stop it
            if (ra_adjust != adjustment)
            {
                if (RAmIncrease)
                {
                    strcat(CmdString, ":Qe#");
                    RAmIncrease = false;
                }
                if (RAmDecrease)
                {
                    strcat(CmdString, ":Qw#");
                    RAmDecrease = false;
                }
            }

            // If DEC was moving but now would be moving at the wrong rate, stop it
            if (dec_adjust != adjustment)
            {
                if (DECmDecrease)
                {
                    strcat(CmdString, ":Qn#");
                    DECmDecrease = false;
                }
                if (DECmIncrease)
                {
                    strcat(CmdString, ":Qs#");
                    DECmIncrease = false;
                }
            }

            // Prepare for the new rate
            if (previous_adjustment != adjustment)
            {
                // Add the new slew rate
                strcat(CmdString, adjustment->slew_rate);

                // If adjustment goes expectedly down, reset countdown
                if (adjustment < previous_adjustment)
                    countdown = MAX_CONVERGENCE_LOOPS;

                // FIXME: wait for the mount to slow down to improve convergence?

                // Remember previous adjustment
                previous_adjustment = adjustment;
            }
            LOGF_DEBUG("Current adjustment speed is %s", adjustment->slew_rate);

            // If RA is being adjusted, check delta against adjustment epsilon to enable or disable movement
            // The smallest change detectable in RA is 1/3600 hours, or 15/3600 degrees
            if (ra_adjust == adjustment)
            {
                // This is the lowest limit of this adjustment
                double const ra_epsilon = std::max(adjustment->epsilon, RA_GRANULARITY);

                // Find requirement
                bool const doDecrease = ra_epsilon <= ra_delta;
                bool const doIncrease = ra_delta <= -ra_epsilon;
                assert(!(doDecrease && doIncrease));

                // Stop movement if required - just stopping or going opposite
                if (RAmIncrease && (!doDecrease || doIncrease))
                {
                    strcat(CmdString, ":Qe#");
                    RAmIncrease = false;
                }
                if (RAmDecrease && (!doIncrease || doDecrease))
                {
                    strcat(CmdString, ":Qw#");
                    RAmDecrease = false;
                }

                // Initiate movement if required
                if (doDecrease && !RAmIncrease)
                {
                    strcat(CmdString, ":Me#");
                    RAmIncrease = true;
                }
                if (doIncrease && !RAmDecrease)
                {
                    strcat(CmdString, ":Mw#");
                    RAmDecrease = true;
                }
            }

            // If DEC is being adjusted, check delta against adjustment epsilon to enable or disable movement
            // The smallest change detectable in DEC is 1/3600 degrees
            if (dec_adjust == adjustment)
            {
                // This is the lowest limit of this adjustment
                double const dec_epsilon = std::max(adjustment->epsilon, DEC_GRANULARITY);

                // Find requirement
                bool const doDecrease = dec_epsilon <= dec_delta;
                bool const doIncrease = dec_delta <= -dec_epsilon;
                assert(!(doDecrease && doIncrease));

                // Stop movement if required - just stopping or going opposite
                if (DECmIncrease && (!doDecrease || doIncrease))
                {
                    strcat(CmdString, ":Qn#");
                    DECmIncrease = false;
                }
                if (DECmDecrease && (!doIncrease || doDecrease))
                {
                    strcat(CmdString, ":Qs#");
                    DECmDecrease = false;
                }

                // Initiate movement if required
                if (doDecrease && !DECmIncrease)
                {
                    strcat(CmdString, ":Mn#");
                    DECmIncrease = true;
                }
                if (doIncrease && !DECmDecrease)
                {
                    strcat(CmdString, ":Ms#");
                    DECmDecrease = true;
                }
            }

            // Basic algorithm sanitization on movement orientation: move one way or the other, or not at all
            assert(!(RAmIncrease && RAmDecrease) && !(DECmDecrease && DECmIncrease));

            // This log shows target in Degrees/Degrees and delta in Degrees/Degrees
            LOGF_DEBUG("Centering (%lf°,%lf°) delta (%lf°,%lf°) moving %c%c%c%c at %s until less than (%lf°,%lf°)",
                       targetMechPosition.RAm() * 15.0, targetMechPosition.DECm(), ra_delta, dec_delta, RAmDecrease ? 'W' : '.',
                       RAmIncrease ? 'E' : '.', DECmDecrease ? 'N' : '.', DECmIncrease ? 'S' : '.', adjustment->slew_rate,
                       std::max(adjustment->epsilon, RA_GRANULARITY), adjustment->epsilon);

            // If we have a command to run, issue it
            if (CmdString[0] != '\0')
            {
                // Send command to mount
                if (sendCmd(CmdString))
                {
                    LOGF_ERROR("Error centering (%lf°,%lf°)", targetMechPosition.RAm() * 15.0, targetMechPosition.DECm());
                    slewError(-1);
                    return false;
                }

                // Update slew rate
                IUResetSwitch(&SlewRateSP);
                SlewRateS[adjustment->switch_index].s = ISS_ON;
                IDSetSwitch(&SlewRateSP, nullptr);
            }

            // If all movement flags are cleared, we are done adjusting
            if (!RAmIncrease && !RAmDecrease && !DECmDecrease && !DECmIncrease)
            {
                LOGF_INFO("Centering delta (%lf,%lf) intermediate adjustment complete (%d loops)", ra_delta, dec_delta,
                          MAX_CONVERGENCE_LOOPS - countdown);
                adjustment = nullptr;
            }
            // Else, if it has been too long since we started, maybe we have a convergence problem.
            // The mount slows down when requested to stop under minimum distance, so we may miss the target.
            // The behavior is improved by changing the slew rate while converging, but is still tricky to tune.
            else if (--countdown <= 0)
            {
                LOGF_ERROR("Failed centering to (%lf,%lf) under loop limit, aborting...", targetMechPosition.RAm(),
                           targetMechPosition.DECm());
                goto slew_failure;
            }
            // Else adjust poll timeout to adjustment speed and continue
            else setCurrentPollingPeriod(static_cast <uint32_t> (adjustment->polling_interval));
        }
        // If we attained target position at one arcsecond precision, finish procedure and track target
        else
        {
            LOG_INFO("Slew is complete. Tracking...");
            sendCmd(":Q#");
            updateSlewRate(savedSlewRateIndex);
            adjustment = nullptr;
            setCurrentPollingPeriod(1000);
            TrackState = SCOPE_TRACKING;
            EqNP.s = IPS_OK;
            IDSetNumber(&EqNP, "Mount is tracking");
        }
    }
    else
    {
        // Force-reset markers in case we got aborted
        if (DECmDecrease) DECmDecrease = false;
        if (DECmIncrease) DECmIncrease = false;
        if (RAmIncrease) RAmIncrease = false;
        if (RAmDecrease) RAmDecrease = false;
        adjustment = nullptr;
    }

    // Update RA/DEC properties
    if (ra_changed || dec_changed)
        NewRaDec(currentRA, currentDEC);

    return true;

slew_failure:
    // If we failed at some point, attempt to stop moving and update properties with error
    sendCmd(":Q#");
    updateSlewRate(savedSlewRateIndex);
    adjustment = nullptr;
    setCurrentPollingPeriod(1000);
    TrackState = SCOPE_TRACKING;
    currentRA = currentMechPosition.RAsky();
    currentDEC = currentMechPosition.DECsky();
    NewRaDec(currentRA, currentDEC);
    slewError(-1);
    return false;
}

bool EQ500X::Goto(double ra, double dec)
{
    // Check whether a meridian flip is required - rangeHA is NOT suitable here
    double HA = getLST() - ra;
    while (+12 <= HA) HA -= 24;
    while (HA <= -12) HA += 24;

    // Deduce required orientation of mount in HA quadrants - set orientation BEFORE coordinates!
    targetMechPosition.setPointingState((0 <= HA
                                         && HA < 12) ? MechanicalPoint::POINTING_NORMAL : MechanicalPoint::POINTING_BEYOND_POLE);
    targetMechPosition.RAsky(ra);
    targetMechPosition.DECsky(dec);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
        if (!Abort())
        {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
        }

        AbortSP.s = IPS_OK;
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&AbortSP, "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = IPS_IDLE;
            MovementWESP.s = IPS_IDLE;
            EqNP.s = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }

        // sleep for 100 mseconds
        const struct timespec timeout = {0, 100000000L};
        nanosleep(&timeout, nullptr);
    }

    /* The goto feature is quite imprecise because it will always use full speed.
     * By the time the mount stops, the position is off by 0-5 degrees, depending on the speed attained during the move.
     * Additionally, a firmware limitation prevents the goto feature from slewing to close coordinates, and will cause uneeded axis rotation.
     * Therefore, don't use the goto feature for a goto, and let ReadScope adjust the position by itself.
     */

    // Set target position and adjust
    if (setTargetMechanicalPosition(targetMechPosition))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC.");
        return false;
    }
    else
    {
        targetMechPosition.RAsky(targetRA = ra);
        targetMechPosition.DECsky(targetDEC = dec);

        LOGF_INFO("Goto target (%lfh,%lf°) HA %lf, quadrant %s", ra, dec, HA,
                  targetMechPosition.getPointingState() == MechanicalPoint::POINTING_NORMAL ? "normal" : "beyond pole");
    }

    // Limit the number of loops
    countdown = MAX_CONVERGENCE_LOOPS;

    // Reset original adjustment
    // Reset movement markers

    TrackState = SCOPE_SLEWING;
    //EqNP.s     = IPS_BUSY;

    // Remember current slew rate
    savedSlewRateIndex = static_cast <enum TelescopeSlewRate> (IUFindOnSwitchIndex(&SlewRateSP));

    // Format RA/DEC for logs
    char RAStr[16] = {0}, DecStr[16] = {0};
    fs_sexa(RAStr, targetRA, 2, 3600);
    fs_sexa(DecStr, targetDEC, 2, 3600);

    LOGF_INFO("Slewing to JNow RA: %s - DEC: %s", RAStr, DecStr);

    return true;
}

bool EQ500X::Sync(double ra, double dec)
{
    targetMechPosition.RAsky(targetRA = ra);
    targetMechPosition.DECsky(targetDEC = dec);

    if(!setTargetMechanicalPosition(targetMechPosition))
    {
        if (!isSimulation())
        {
            char b[64/*RB_MAX_LEN*/] = {0};
            tcflush(PortFD, TCIFLUSH);

            if (getCommandString(PortFD, b, ":CM#") < 0)
                goto sync_error;
            if (!strncmp("No name", b, sizeof(b)))
                goto sync_error;
        }
        else
        {
            targetMechPosition.toStringRA(simEQ500X.MechanicalRAStr, sizeof(simEQ500X.MechanicalRAStr));
            targetMechPosition.toStringDEC_Sim(simEQ500X.MechanicalDECStr, sizeof(simEQ500X.MechanicalDECStr));
            simEQ500X.MechanicalRA = targetMechPosition.RAm();
            simEQ500X.MechanicalDEC = targetMechPosition.DECm();
        }

        if (getCurrentMechanicalPosition(currentMechPosition))
            goto sync_error;

        currentRA = currentMechPosition.RAsky();
        currentDEC = currentMechPosition.DECsky();
        NewRaDec(currentRA, currentDEC);

        LOGF_INFO("Mount synced to target RA '%lf' DEC '%lf'", currentRA, currentDEC);
        return true;
    }

sync_error:
    EqNP.s = IPS_ALERT;
    IDSetNumber(&EqNP, "Synchronization failed.");
    LOGF_ERROR("Mount sync to target RA '%lf' DEC '%lf' failed", ra, dec);
    return false;
}

bool EQ500X::Abort()
{
    setCurrentPollingPeriod(1000);
    TrackState = SCOPE_TRACKING;
    return LX200Telescope::Abort() && updateSlewRate(savedSlewRateIndex);
}

void EQ500X::setPierSide(TelescopePierSide side)
{
    INDI_UNUSED(side);
    PierSideSP.s = IPS_ALERT;
    IDSetSwitch(&PierSideSP, "Not supported");
}

bool EQ500X::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    // EQ500X has North/South directions inverted
    int current_move = (dir == DIRECTION_NORTH) ? LX200_SOUTH : LX200_NORTH;

    switch (command)
    {
        case MOTION_START:
            if (!isSimulation() && MoveTo(PortFD, current_move) < 0)
            {
                LOG_ERROR("Error setting N/S motion direction.");
                return false;
            }
            else
                LOGF_DEBUG("Moving toward %s.",
                           (current_move == LX200_NORTH) ? "North" : "South");
            break;

        case MOTION_STOP:
            if (!isSimulation() && HaltMovement(PortFD, current_move) < 0)
            {
                LOG_ERROR("Error stopping N/S motion.");
                return false;
            }
            else
                LOGF_DEBUG("Movement toward %s halted.",
                           (current_move == LX200_NORTH) ? "North" : "South");
            break;
    }

    return true;
}

/**************************************************************************************
**
***************************************************************************************/

int EQ500X::sendCmd(char const *data)
{
    LOGF_DEBUG("CMD <%s>", data);
    if (!isSimulation())
    {
        int nbytes_write = 0;
        return tty_write_string(PortFD, data, &nbytes_write);
    }
    return 0;
}

int EQ500X::getReply(char *data, size_t const len)
{
    if (!isSimulation())
    {
        int nbytes_read = 0;
        int error_type = tty_read(PortFD, data, len, EQ500X_TIMEOUT, &nbytes_read);

        LOGF_DEBUG("RES <%.*s> (%d)", nbytes_read, data, error_type);
        return error_type;
    }
    return 0;
}

bool EQ500X::gotoTargetPosition(MechanicalPoint const &p)
{
    if (!isSimulation())
    {
        if (!setTargetMechanicalPosition(p))
        {
            if (!sendCmd(":MS#"))
            {
                char buf[64/*RB_MAX_LEN*/] = {0};
                if (!getReply(buf, 1))
                    return buf[0] != '0'; // 0 is valid for :MS
            }
            else return true;
        }
        else return true;
    }
    else
    {
        return !Sync(p.RAsky(), p.DECsky());
    }

    return false;
}

bool EQ500X::getCurrentMechanicalPosition(MechanicalPoint &p)
{
    char b[64/*RB_MAX_LEN*/] = {0};
    MechanicalPoint result = p;

    // Always read DEC first as it gives the side of pier the scope is on, and has an impact on RA

    if (isSimulation())
        memcpy(b, simEQ500X.MechanicalDECStr, std::min(sizeof(b), sizeof(simEQ500X.MechanicalDECStr)));
    else if (getCommandString(PortFD, b, ":GD#") < 0)
        goto radec_error;

    if (result.parseStringDEC(b, 64))
        goto radec_error;

    LOGF_DEBUG("Mount mechanical DEC reads '%s' as %lf.", b, result.DECm());

    if (isSimulation())
        memcpy(b, simEQ500X.MechanicalRAStr, std::min(sizeof(b), sizeof(simEQ500X.MechanicalRAStr)));
    else if (getCommandString(PortFD, b, ":GR#") < 0)
        goto radec_error;

    if (result.parseStringRA(b, 64))
        goto radec_error;

    LOGF_DEBUG("Mount mechanical RA reads '%s' as %lf.", b, result.RAm());

    p = result;
    return false;

radec_error:
    return true;
}

bool EQ500X::setTargetMechanicalPosition(MechanicalPoint const &p)
{
    if (!isSimulation())
    {
        // Size string buffers appropriately
        char CmdString[] = ":Sr" MechanicalPoint_RA_Format "#:Sd" MechanicalPoint_DEC_FormatW "#";
        char bufRA[sizeof(MechanicalPoint_RA_Format)], bufDEC[sizeof(MechanicalPoint_DEC_FormatW)];

        // Write RA/DEC in placeholders
        snprintf(CmdString, sizeof(CmdString), ":Sr%s#:Sd%s#", p.toStringRA(bufRA, sizeof(bufRA)), p.toStringDEC(bufDEC,
                 sizeof(bufDEC)));
        LOGF_DEBUG("Target RA '%f' DEC '%f' converted to '%s'", static_cast <float> (p.RAm()), static_cast <float> (p.DECm()),
                   CmdString);

        char buf[64/*RB_MAX_LEN*/] = {0};

        if (!sendCmd(CmdString))
            if (!getReply(buf, 2))
                if (buf[0] == '1' && buf[1] == '1')
                    return false;
                else LOGF_ERROR("Failed '%s', mount replied %c%c", CmdString, buf[0], buf[1]);
            else LOGF_ERROR("Failed getting 2-byte reply to '%s'", CmdString);
        else LOGF_ERROR("Failed '%s'", CmdString);

        return true;
    }

    return false;
}

/**************************************************************************************
**
***************************************************************************************/

EQ500X::MechanicalPoint::MechanicalPoint(double ra, double dec)
{
    RAm(ra);
    DECm(dec);
}

bool EQ500X::MechanicalPoint::atParkingPosition() const
{
    // Consider mount 0/0 is pole - no way to check if synced already
    return _RAm == 0 && _DECm == 0;
}

double EQ500X::MechanicalPoint::RAm() const
{
    return static_cast <double> (_RAm) / 3600.0;
}

double EQ500X::MechanicalPoint::DECm() const
{
    return static_cast <double> (_DECm) / 3600.0;
}

double EQ500X::MechanicalPoint::RAm(double const value)
{
    _RAm = std::lround(std::fmod(value + 24.0, 24.0) * 3600.0);
    return RAm();
}

double EQ500X::MechanicalPoint::DECm(double const value)
{
    // Should be inside [-180,+180] but mount supports a larger (not useful) interval
    _DECm = std::lround(std::fmod(value, 256.0) * 3600.0);

    // Deduce pier side from mechanical DEC
    if ((-256 * 3600 < _DECm && _DECm < -180 * 3600) || (0 <= _DECm && _DECm <= +180 * 3600))
        _pointingState = POINTING_NORMAL;
    else
        _pointingState = POINTING_BEYOND_POLE;

    return DECm();
}

double EQ500X::MechanicalPoint::RAsky() const
{
    switch (_pointingState)
    {
        case POINTING_BEYOND_POLE:
            return static_cast <double> ((12 * 3600 + _RAm) % (24 * 3600)) / 3600.0;
        case POINTING_NORMAL:
        default:
            return static_cast <double> ((24 * 3600 + _RAm) % (24 * 3600)) / 3600.0;
    }
}

double EQ500X::MechanicalPoint::DECsky() const
{
    // Convert to sky DEC inside [-90,90], allowing +/-90 values
    long _DEC = 90 * 3600 - _DECm;
    if (POINTING_BEYOND_POLE == _pointingState)
        _DEC = 180 * 3600 - _DEC;
    while (+90 * 3600 < _DEC) _DEC -= 180 * 3600;
    while (_DEC < -90 * 3600) _DEC += 180 * 3600;
    return static_cast <double> (_DEC) / 3600.0;
}

double EQ500X::MechanicalPoint::RAsky(const double _RAsky)
{
    switch (_pointingState)
    {
        case POINTING_BEYOND_POLE:
            _RAm = static_cast <long> (std::fmod(12.0 + _RAsky, 24.0) * 3600.0);
            break;
        case POINTING_NORMAL:
            _RAm = static_cast <long> (std::fmod(24.0 + _RAsky, 24.0) * 3600.0);
            break;
    }
    return RAsky();
}

double EQ500X::MechanicalPoint::DECsky(const double _DECsky)
{
    switch (_pointingState)
    {
        case POINTING_BEYOND_POLE:
            _DECm = 90 * 3600 - std::lround((180.0 - _DECsky) * 3600.0);
            break;
        case POINTING_NORMAL:
            _DECm = 90 * 3600 - std::lround(_DECsky * 3600.0);
            break;
    }
    return DECsky();
}

char const * EQ500X::MechanicalPoint::toStringRA(char *buf, size_t buf_length) const
{
    // See /test/test_eq500xdriver.cpp for description of RA conversion

    //int const hours = ((_pierSide == PIER_WEST ? 12 : 0) + 24 + static_cast <int> (_RAm/3600)) % 24;
    int const hours   = (24 + static_cast <int> (_RAm / 3600)) % 24;
    int const minutes = static_cast <int> (_RAm / 60) % 60;
    int const seconds = static_cast <int> (_RAm) % 60;

    int const written = snprintf(buf, buf_length, "%02d:%02d:%02d", hours, minutes, seconds);

    return (0 < written && written < (int) buf_length) ? buf : (char const*)0;
}

bool EQ500X::MechanicalPoint::parseStringRA(char const *buf, size_t buf_length)
{
    if (buf_length < sizeof(MechanicalPoint_RA_Format) - 1)
        return true;

    // Mount replies to "#GR:" with "HH:MM:SS".
    // HH, MM and SS are respectively hours, minutes and seconds in [00:00:00,23:59:59].
    // FIXME: Sanitize.

    int hours = 0, minutes = 0, seconds = 0;
    if (3 == sscanf(buf, "%02d:%02d:%02d", &hours, &minutes, &seconds))
    {
        //_RAm = (    (_pierSide == PIER_WEST ? -12*3600 : +0) + 24*3600 +
        _RAm = (24 * 3600 + (hours % 24) * 3600 + minutes * 60 + seconds) % (24 * 3600);
        return false;
    }

    return true;
}

char const * EQ500X::MechanicalPoint::toStringDEC_Sim(char *buf, size_t buf_length) const
{
    // See /test/test_eq500xdriver.cpp for description of DEC conversion

    int const degrees = static_cast <int> (_DECm / 3600) % 256;
    int const minutes = static_cast <int> (std::abs(_DECm) / 60) % 60;
    int const seconds = static_cast <int> (std::abs(_DECm)) % 60;

    if (degrees < -255 || +255 < degrees)
        return (char const*)0;

    char high_digit = '0';
    if (-100 < degrees && degrees < 100)
    {
        high_digit = '0' + (std::abs(degrees) / 10);
    }
    else switch (std::abs(degrees) / 10)
        {
            case 10:
                high_digit = ':';
                break;
            case 11:
                high_digit = ';';
                break;
            case 12:
                high_digit = '<';
                break;
            case 13:
                high_digit = '=';
                break;
            case 14:
                high_digit = '>';
                break;
            case 15:
                high_digit = '?';
                break;
            case 16:
                high_digit = '@';
                break;
            case 17:
                high_digit = 'A';
                break;
            case 18:
                high_digit = 'B';
                break;
            case 19:
                high_digit = 'C';
                break;
            case 20:
                high_digit = 'D';
                break;
            case 21:
                high_digit = 'E';
                break;
            case 22:
                high_digit = 'F';
                break;
            case 23:
                high_digit = 'G';
                break;
            case 24:
                high_digit = 'H';
                break;
            case 25:
                high_digit = 'I';
                break;
            default:
                return (char const*)0;
                break;
        }

    char const low_digit = '0' + (std::abs(degrees) % 10);

    int const written = snprintf(buf, buf_length, "%c%c%c:%02d:%02d", (0 <= degrees) ? '+' : '-', high_digit, low_digit,
                                 minutes, seconds);

    return (0 < written && written < (int) buf_length) ? buf : (char const*)0;
}

char const * EQ500X::MechanicalPoint::toStringDEC(char *buf, size_t buf_length) const
{
    // See /test/test_eq500xdriver.cpp for description of DEC conversion

    int const degrees = static_cast <int> (_DECm / 3600) % 256;
    int const minutes = static_cast <int> (std::abs(_DECm) / 60) % 60;
    int const seconds = static_cast <int> (std::abs(_DECm)) % 60;

    if (degrees < -255 || +255 < degrees)
        return (char const*)0;

    int const written = snprintf(buf, buf_length, "%+03d:%02d:%02d", degrees, minutes, seconds);

    return (0 < written && written < (int) buf_length) ? buf : (char const*)0;
}

bool EQ500X::MechanicalPoint::parseStringDEC(char const *buf, size_t buf_length)
{
    if (buf_length < sizeof(MechanicalPoint_DEC_FormatR) - 1)
        return true;

    char b[sizeof(MechanicalPoint_DEC_FormatR)] = {0};
    strncpy(b, buf, sizeof(b));

    // Mount replies to "#GD:" with "sDD:MM:SS".
    // s is in {+,-} and provides a sign.
    // DD are degrees, unit D spans '0' to '9' in [0,9] but high D spans '0' to 'I' in [0,25].
    // MM are minutes, SS are seconds in [00:00,59:59].
    // The whole reply is in [-255:59:59,+255:59:59].

    struct _DecValues
    {
        char degrees[4];
        char minutes[3];
        char seconds[3];
    } * const DecValues = (struct _DecValues*) b;

    if (DecValues->degrees[1] < '0' || 'I' < DecValues->degrees[1])
        return true;

    int const sgn = DecValues->degrees[0] == '-' ? -1 : +1;

    // Replace sign with hundredth, or space if lesser than 100
    switch (DecValues->degrees[1])
    {
        case ':':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '0';
            break;
        case ';':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '1';
            break;
        case '<':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '2';
            break;
        case '=':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '3';
            break;
        case '>':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '4';
            break;
        case '?':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '5';
            break;
        case '@':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '6';
            break;
        case 'A':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '7';
            break;
        case 'B':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '8';
            break;
        case 'C':
            DecValues->degrees[0] = '1';
            DecValues->degrees[1] = '9';
            break;
        case 'D':
            DecValues->degrees[0] = '2';
            DecValues->degrees[1] = '0';
            break;
        case 'E':
            DecValues->degrees[0] = '2';
            DecValues->degrees[1] = '1';
            break;
        case 'F':
            DecValues->degrees[0] = '2';
            DecValues->degrees[1] = '2';
            break;
        case 'G':
            DecValues->degrees[0] = '2';
            DecValues->degrees[1] = '3';
            break;
        case 'H':
            DecValues->degrees[0] = '2';
            DecValues->degrees[1] = '4';
            break;
        case 'I':
            DecValues->degrees[0] = '2';
            DecValues->degrees[1] = '5';
            break;
        default:
            DecValues->degrees[0] = '0';
            break;
    }
    DecValues->degrees[3] = DecValues->minutes[2] = DecValues->seconds[2] = '\0';

    _DECm = sgn * (
                atol(DecValues->degrees) * 3600 +
                atol(DecValues->minutes) * 60 +
                atol(DecValues->seconds) );

    // Deduce pointing state from mechanical DEC
    if ((-256 * 3600 < _DECm && _DECm <= -180 * 3600) || (0 <= _DECm && _DECm <= +180 * 3600))
        _pointingState = POINTING_NORMAL;
    else
        _pointingState = POINTING_BEYOND_POLE;

    return false;
}

double EQ500X::MechanicalPoint::RA_degrees_to(EQ500X::MechanicalPoint const &b) const
{
    // RA is circular, DEC is not
    // We have hours and not degrees because that's what the mount is handling in terms of precision
    // We need to be cautious, as if we were to use real degrees, the RA movement would need to be 15 times more precise
    long delta = b._RAm - _RAm;
    if (delta > +12 * 3600) delta -= 24 * 3600;
    if (delta < -12 * 3600) delta += 24 * 3600;
    return static_cast <double> (delta * 15) / 3600.0;
}

double EQ500X::MechanicalPoint::DEC_degrees_to(EQ500X::MechanicalPoint const &b) const
{
    // RA is circular, DEC is not
    return static_cast <double> (b._DECm - _DECm) / 3600.0;
}

double EQ500X::MechanicalPoint::operator -(EQ500X::MechanicalPoint const &b) const
{
    double const ra_distance = RA_degrees_to(b);
    double const dec_distance = DEC_degrees_to(b);
    // FIXME: Incorrect distance calculation, but enough for our purpose
    return std::sqrt(ra_distance * ra_distance + dec_distance * dec_distance);
}

bool EQ500X::MechanicalPoint::operator !=(EQ500X::MechanicalPoint const &b) const
{
    return (_pointingState != b._pointingState) || (RA_GRANULARITY <= std::abs(RA_degrees_to(b)))
           || (DEC_GRANULARITY <= std::abs(DEC_degrees_to(b)));
}

bool EQ500X::MechanicalPoint::operator ==(EQ500X::MechanicalPoint const &b) const
{
    return !this->operator !=(b);
}

enum EQ500X::MechanicalPoint::PointingState EQ500X::MechanicalPoint::setPointingState(enum
        EQ500X::MechanicalPoint::PointingState pointingState)
{
    return _pointingState = pointingState;
}

enum EQ500X::MechanicalPoint::PointingState EQ500X::MechanicalPoint::getPointingState() const
{
    return _pointingState;
}
