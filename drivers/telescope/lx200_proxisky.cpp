/*
    Proxisky UMi

    Proxisky UMi mounts run OnStep-derived firmware, so the standard protocol is handled
    entirely by LX200_OnStep. This driver adds the vendor-specific ":P..." extensions that
    OnStep itself does not implement.

    Copyright (C) 2026 Nico Trost

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

#include "lx200_proxisky.h"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <ctime>
#include <limits>

// Mirrors the vendor tool's own tab split (tabSettings / tabLimits / tabACS / tabAdvanced), so
// anyone moving between the two finds things in the same place. Prefixed because these sit
// alongside LX200_OnStep's dozen tabs and INDI's standard ones, where a bare "Settings" would
// be ambiguous.
#define PROXISKY_SETTINGS_TAB "Proxisky Settings"
#define PROXISKY_LIMITS_TAB   "Proxisky RA/Dec Limits"
#define PROXISKY_ACS_TAB      "Proxisky ACS"
#define PROXISKY_ADVANCED_TAB "Proxisky Advanced"

// The vendor ":P..." reads use whatever timeout LX200_OnStep set for the transport (100ms serial,
// 2s TCP); this driver does not touch it, which is why the probes run from updateProperties()
// rather than getBasicData() - see the note there.

// Poll the ACS collision counters every Nth status cycle rather than every one.
#define PROXISKY_ACS_POLL_DIVIDER 5

// Wall-clock budget for the ":Pbvg#" retry loop, polled every 250ms. The mount answers it with a
// bare "0" for roughly a second after connect, so 3s covers that with margin. Bounded by the clock
// rather than by an attempt count because an unanswered read costs a whole transport timeout, and
// on TCP that is 2s - a fixed attempt count would overrun badly there.
// The deadline is tested after each read so that a slow mount still gets one full attempt, which
// means the real worst case is this budget plus one transport timeout: ~3.1s on serial, ~5s on TCP.
#define PROXISKY_MODEL_PROBE_SECONDS 3

// Backstop on the same loop, in case the clock rather than the mount is what misbehaves.
// monotonicSeconds() reports failure as +infinity, which is the fail-closed answer everywhere it
// is compared against a deadline - but not where it *is* the deadline: one failed clock read while
// computing the deadline, followed by successful reads inside the loop, leaves every "finite >=
// infinity" test false and the loop spinning. That needs CLOCK_MONOTONIC to fail and then recover,
// which on Linux does not happen, so this is purely a guarantee of termination. Sized well above
// the ~12 attempts the 3s budget allows on serial so it can never fire before the deadline does.
#define PROXISKY_MODEL_PROBE_MAX_ATTEMPTS 64

// Pause before a capability probe's single retry. The mount answers a healthy vendor read in
// less than 25ms, so this is not about giving it more time to reply - the transport timeout has
// already elapsed by then. It is about not re-issuing into whatever disturbed the line in the first
// place.
#define PROXISKY_PROBE_RETRY_NSEC 50000000L   // 50ms

// Wall-clock budget for the post-commit PID readback. ":Pnas<axis>r#" restarts the axis, which
// reports all four gains as zero until it is back - measured at well under 100ms, so this is
// generous. Bounded by the clock because the alternative is reporting a mismatch that never was.
#define PROXISKY_PID_RESTART_SECONDS 1

// Wall-clock budget for every remaining probe once ":Pbvg#" has answered. A healthy mount finishes
// the sequence in well under a second. This only bites when the mount identifies itself and then
// stops answering, where ~19 further reads would block the event loop inside Connect().
// As above, the budget bounds when a read may *start*, not when it may finish. A probe read is
// retried once and the budget is re-checked in between, so the last one can begin just under the
// deadline and then spend two transport timeouts: worst case ~10.2s on serial, ~14s on TCP. Added
// to the model probe, a mount that identifies itself and then goes silent can hold Connect() for
// ~19s on TCP. That is in family with LX200_OnStep's own connect, which makes dozens of blocking
// reads on the same path, and the alternative - deducting a timeout the transport does not expose -
// would only move the imprecision somewhere less visible.
#define PROXISKY_PROBE_BUDGET_SECONDS 10

// The vendor grammar is case sensitive and the families are not symmetric:
//   :Ps<Feature><g|s>[arg]#   boolean feature, g = get, s = set
//   :Pn<Feature>...           numeric parameter
//   :Pb...                    board / capability query
// Note :PsSdg# (Dec limit enable) and :PsSdG# (Dec limit values) differ only in case, and the
// RA travel limits read their enable flag from the :Ps family but their values from :Pn.
// Keep every command as a literal at its call site rather than composing it from fragments.

// Monotonic seconds since an arbitrary origin. Only ever used for elapsed-time comparisons, so
// the origin does not matter - but CLOCK_MONOTONIC does, since a wall-clock jump mid-connect
// (NTP stepping the clock on a field laptop that just got a network) must not shorten or extend
// a probe budget.
static double monotonicSeconds()
{
    struct timespec now = {0, 0};

    // A failure here would leave now at {0,0}, i.e. "the epoch", making every deadline look
    // infinitely far away - probeModel() would loop forever. CLOCK_MONOTONIC is always available
    // on Linux so this is belt and braces, but infinity is the fail-closed answer: every caller
    // compares against a deadline, and infinity reads as "time is up" in all of them.
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return std::numeric_limits<double>::infinity();

    return now.tv_sec + now.tv_nsec / 1e9;
}

// nanosleep() returns early when a signal arrives; INDI drivers run with SIGALRM and friends live,
// so a bare call can sleep for a fraction of what was asked. Both callers sit in loops bounded by a
// wall-clock deadline, which makes a short sleep merely wasteful rather than wrong - but a settle
// delay that does not settle is worth getting right once here.
static void sleepFor(const struct timespec &duration)
{
    struct timespec remaining = duration;

    while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR)
        continue;
}

// Repairs an ISR_1OFMANY vector after update() has rejected a write. Nothing is logged here:
// IUUpdateSwitch has already sent its own IDSetSwitch describing the error, and a second message
// would only contradict the first. But its two failure paths do not leave the vector in the same
// shape - a bad on-count resets and then puts the previous selection back, while an unknown element
// name returns early with the vector already reset and nothing selected at all
// (libs/indibase/indidriver.c:1348-1352). Only the second case needs fixing, and leaving it unfixed
// strands a 1-of-many switch showing no selection until the next successful write. Reachable only
// from a hand-written indi_setprop that misspells an element.
static void restoreAfterFailedUpdate(INDI::PropertySwitch &sp, bool previous)
{
    if (sp.findOnSwitchIndex() >= 0)
        return;

    sp[previous ? 1 : 0].setState(ISS_ON);
    sp.apply();
}

// One answer for "you wrote to a property this mount does not have". Not reachable through a
// normal client - an unsupported feature is never defined, so it never appears in the panel - but
// a hand-written indi_setprop can still address it by name. Passing it down to LX200_OnStep, which
// has never heard of the property, is not an answer; saying so is. The message is passed whole
// rather than built from a label because half these features are plural ("RA limits are...").
template <typename PropertyT>
static bool reportUnavailable(PropertyT &property, const char *message)
{
    property.setState(IPS_ALERT);
    property.apply("%s", message);
    return true;
}

LX200_Proxisky::LX200_Proxisky() : LX200_OnStep()
{
    // Everything else - LX200 capabilities, telescope capabilities, focuser/rotator/weather
    // interfaces - is inherited unchanged from LX200_OnStep.
    setVersion(1, 0);
}

// Deliberately no saveConfigItems() override. Every property here mirrors state held in the
// mount's own non-volatile storage and is read back at connect, so persisting it client-side
// would create a second source of truth that silently diverges from the hardware.
const char *LX200_Proxisky::getDefaultName()
{
    return "Proxisky UMi";
}

bool LX200_Proxisky::initProperties()
{
    if (!LX200_OnStep::initProperties())
        return false;

    // ============== PROXISKY_SETTINGS_TAB
    ModelTP[0].fill("MODEL", "Model/Firmware", "Unknown");
    ModelTP.fill(getDeviceName(), "PROXISKY_MODEL", "Proxisky Model", PROXISKY_SETTINGS_TAB, IP_RO, 0, IPS_IDLE);

    RALimitEnableSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    RALimitEnableSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    RALimitEnableSP.fill(getDeviceName(), "PROXISKY_RA_LIMIT_ENABLE", "RA Limits", PROXISKY_LIMITS_TAB, IP_RW,
                         ISR_1OFMANY, 0, IPS_IDLE);

    // Left/Right and CCW/CW are the vendor's own terms. Nothing in the protocol establishes a
    // mapping to East/West.
    RALimitNP[0].fill("LEFT", "Left (deg)", "%.0f", 1, 180, 1, 90);
    RALimitNP[1].fill("RIGHT", "Right (deg)", "%.0f", 1, 180, 1, 90);
    RALimitNP.fill(getDeviceName(), "PROXISKY_RA_LIMIT", "RA Limit Values", PROXISKY_LIMITS_TAB, IP_RW, 0, IPS_IDLE);

    DecLimitEnableSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    DecLimitEnableSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    DecLimitEnableSP.fill(getDeviceName(), "PROXISKY_DEC_LIMIT_ENABLE", "Dec Limits", PROXISKY_LIMITS_TAB, IP_RW,
                          ISR_1OFMANY, 0, IPS_IDLE);

    DecLimitNP[0].fill("CCW", "CCW (deg)", "%.0f", 90, 200, 1, 90);
    DecLimitNP[1].fill("CW", "CW (deg)", "%.0f", 90, 200, 1, 90);
    DecLimitNP.fill(getDeviceName(), "PROXISKY_DEC_LIMIT", "Dec Limit Values", PROXISKY_LIMITS_TAB, IP_RW, 0, IPS_IDLE);

    LedSP[0].fill("OFF", "Off", ISS_OFF);
    LedSP[1].fill("ON", "On", ISS_OFF);
    LedSP.fill(getDeviceName(), "PROXISKY_LED", "Status LED", PROXISKY_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    AcsEnableSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    AcsEnableSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    AcsEnableSP.fill(getDeviceName(), "PROXISKY_ACS_ENABLE", "Anti-Collision", PROXISKY_ACS_TAB, IP_RW, ISR_1OFMANY, 0,
                     IPS_IDLE);

    // Bounds taken from the vendor tool's own client-side validation, not from the firmware - an
    // in-range value can still be refused, which is what the readback after each write is for.
    // The asymmetric lower bounds are the tool's, not a typo: RA accepts 10, Dec only 100. The
    // upper bound is the tool's error *text* (59999) rather than its actual check (v <= 60000),
    // which is off by one against itself; we follow the text, since that is what the vendor
    // documents to users.
    AcsThresholdNP[0].fill("RA", "RA Threshold", "%.0f", 10, 59999, 1, 1000);
    AcsThresholdNP[1].fill("DEC", "Dec Threshold", "%.0f", 100, 59999, 1, 1000);
    AcsThresholdNP.fill(getDeviceName(), "PROXISKY_ACS_THRESHOLD", "ACS Sensitivity", PROXISKY_ACS_TAB, IP_RW, 0, IPS_IDLE);

    AcsCollisionNP[0].fill("RA", "RA Collisions", "%.0f", 0, 1e9, 1, 0);
    AcsCollisionNP[1].fill("DEC", "Dec Collisions", "%.0f", 0, 1e9, 1, 0);
    AcsCollisionNP.fill(getDeviceName(), "PROXISKY_ACS_COLLISIONS", "Collision Count", PROXISKY_ACS_TAB, IP_RO, 0,
                        IPS_IDLE);

    AcsClearSP[0].fill("CLEAR", "Clear Counters", ISS_OFF);
    AcsClearSP.fill(getDeviceName(), "PROXISKY_ACS_CLEAR", "Collision Counters", PROXISKY_ACS_TAB, IP_RW, ISR_ATMOST1, 0,
                    IPS_IDLE);

    PowerLossSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    PowerLossSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    PowerLossSP.fill(getDeviceName(), "PROXISKY_POWER_LOSS_MEMORY", "Power-loss Memory", PROXISKY_SETTINGS_TAB, IP_RW,
                     ISR_1OFMANY, 0, IPS_IDLE);

    AsiairSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    AsiairSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    AsiairSP.fill(getDeviceName(), "PROXISKY_ASIAIR", "ASIAIR Homing", PROXISKY_SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0,
                  IPS_IDLE);

    SuperHomeSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    SuperHomeSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    SuperHomeSP.fill(getDeviceName(), "PROXISKY_SUPER_HOME", "Supervised Home", PROXISKY_SETTINGS_TAB, IP_RW,
                     ISR_1OFMANY, 0, IPS_IDLE);

    DecSecHomeSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    DecSecHomeSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    DecSecHomeSP.fill(getDeviceName(), "PROXISKY_DEC_SEC_HOME", "Dec Second Home", PROXISKY_SETTINGS_TAB, IP_RW,
                      ISR_1OFMANY, 0, IPS_IDLE);

    AutoTrackSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    AutoTrackSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    AutoTrackSP.fill(getDeviceName(), "PROXISKY_AUTO_TRACK", "Auto Tracking", PROXISKY_SETTINGS_TAB, IP_RW,
                     ISR_1OFMANY, 0, IPS_IDLE);

    SuperGotoSP[0].fill("DISABLED", "Disabled", ISS_OFF);
    SuperGotoSP[1].fill("ENABLED", "Enabled", ISS_OFF);
    SuperGotoSP.fill(getDeviceName(), "PROXISKY_SUPER_GOTO", "Supervised GOTO", PROXISKY_ADVANCED_TAB, IP_RW,
                     ISR_1OFMANY, 0, IPS_IDLE);

    SuperGotoToleranceNP[0].fill("TOLERANCE", "Tolerance (deg)", "%.0f", 5, 30, 1, 10);
    SuperGotoToleranceNP.fill(getDeviceName(), "PROXISKY_SUPER_GOTO_TOLERANCE", "Supervised GOTO Tolerance",
                              PROXISKY_ADVANCED_TAB, IP_RW, 0, IPS_IDLE);

    // Servo PID gains. Ranges are the vendor tool's own validation: 11..1999 for everything
    // except speed Ki, which it caps at 80. Out-of-range gains can leave the mount unable to
    // track, so these are enforced here as well as by the client's min/max.
    RaPidNP[0].fill("ANGLE_KP", "Angle Kp", "%.0f", 11, 1999, 1, 100);
    RaPidNP[1].fill("ANGLE_KI", "Angle Ki", "%.0f", 11, 1999, 1, 100);
    RaPidNP[2].fill("SPEED_KP", "Speed Kp", "%.0f", 11, 1999, 1, 100);
    RaPidNP[3].fill("SPEED_KI", "Speed Ki", "%.0f", 11, 80, 1, 20);
    RaPidNP.fill(getDeviceName(), "PROXISKY_RA_PID", "RA/Azm PID", PROXISKY_ADVANCED_TAB, IP_RW, 0, IPS_IDLE);

    DecPidNP[0].fill("ANGLE_KP", "Angle Kp", "%.0f", 11, 1999, 1, 100);
    DecPidNP[1].fill("ANGLE_KI", "Angle Ki", "%.0f", 11, 1999, 1, 100);
    DecPidNP[2].fill("SPEED_KP", "Speed Kp", "%.0f", 11, 1999, 1, 100);
    DecPidNP[3].fill("SPEED_KI", "Speed Ki", "%.0f", 11, 80, 1, 20);
    DecPidNP.fill(getDeviceName(), "PROXISKY_DEC_PID", "Dec/Alt PID", PROXISKY_ADVANCED_TAB, IP_RW, 0, IPS_IDLE);

    return true;
}

bool LX200_Proxisky::updateProperties()
{
    // Captured rather than returned straight away: our own teardown has to run even if the parent
    // reports a problem, otherwise a failed disconnect leaves the vendor properties on screen.
    const bool parentOk = LX200_OnStep::updateProperties();

    if (!isConnected())
    {
        deleteVendorProperties();
        return parentOk;
    }

    // Probe here rather than from getBasicData(). Both are reached during connect, but
    // getBasicData() runs from inside LX200Generic::updateProperties(), which the parent calls
    // *before* it picks the transport timeout - so probing there would use the 100ms default even
    // on TCP, where the parent is about to ask for 2s. Running after the parent means the vendor
    // reads use whatever timeout it settled on, with no second copy of its transport detection here.
    //
    // Simulation is skipped outright: every vendor property mirrors a value read back from real
    // hardware, and there is nothing to read. The driver is deliberately indistinguishable from
    // bare OnStep there - documented, not a bug.
    if (!isSimulation())
    {
        runVendorProbes();
        defineVendorProperties();
    }

    return parentOk;
}

void LX200_Proxisky::runVendorProbes()
{
    // Connect-time only. ReadScopeStatus() re-runs LX200_OnStep::updateProperties() when it first
    // detects PEC or pier side; that call is explicitly qualified, so it does not reach our
    // override - but the guard stays, because a second full pass from the event loop would block
    // the status cadence for as long as a connect does.
    if (m_ProbesDone || isSimulation())
        return;

    m_ProbesDone = true;

    // probeModel() absorbs the mount's startup window internally, so by the time it succeeds the
    // mount is answering vendor commands and the optional probes need no retry of their own.
    probeModel();

    if (!m_HasModel)
        return;

    // Start the budget only after the mount has identified itself, so the time probeModel()
    // spent waiting out the startup window is not charged against the feature probes.
    m_ProbeDeadline = monotonicSeconds() + PROXISKY_PROBE_BUDGET_SECONDS;

    probeRALimits();
    probeDecLimits();
    probeLed();
    probeAcs();
    probeSettings();
    probeSuperGoto();

    if (!probeBudgetExpired())
        m_HasRaPid = probePidAxis(RaPidNP, ":Pnag0#", "RA/Azm");

    if (!probeBudgetExpired())
        m_HasDecPid = probePidAxis(DecPidNP, ":Pnag1#", "Dec/Alt");

    // Clearing the deadline takes getVendorString() off the budget for the rest of the session.
    // Every read from here on is a user action or a status poll, and neither should be refused
    // because connect-time probing once ran long.
    m_ProbeDeadline = 0;
}

bool LX200_Proxisky::probeBudgetExpired()
{
    if (m_ProbeDeadline <= 0 || monotonicSeconds() < m_ProbeDeadline)
        return false;

    if (!m_ProbeBudgetWarned)
    {
        m_ProbeBudgetWarned = true;
        LOGF_WARN("Vendor probing exceeded its %ds budget - the mount identified itself but "
                  "stopped answering. Skipping the remaining feature probes; reconnect to retry.",
                  PROXISKY_PROBE_BUDGET_SECONDS);
    }

    return true;
}

void LX200_Proxisky::defineVendorProperties()
{
    if (!isConnected() || m_PropertiesDefined)
        return;

    m_PropertiesDefined = true;

    if (m_HasModel)
        defineProperty(ModelTP);

    if (m_HasRALimits)
    {
        defineProperty(RALimitEnableSP);
        defineProperty(RALimitNP);
    }

    if (m_HasDecLimits)
    {
        defineProperty(DecLimitEnableSP);
        defineProperty(DecLimitNP);
    }

    if (m_HasLed)
        defineProperty(LedSP);

    if (m_HasAcs)
    {
        defineProperty(AcsEnableSP);
        defineProperty(AcsThresholdNP);
        defineProperty(AcsCollisionNP);
        defineProperty(AcsClearSP);
    }

    if (m_HasPowerLoss)
        defineProperty(PowerLossSP);

    if (m_HasAsiair)
        defineProperty(AsiairSP);

    if (m_HasSuperHome)
        defineProperty(SuperHomeSP);

    if (m_HasDecSecHome)
        defineProperty(DecSecHomeSP);

    if (m_HasAutoTrack)
        defineProperty(AutoTrackSP);

    if (m_HasSuperGoto)
        defineProperty(SuperGotoSP);

    if (m_HasSuperGotoTolerance)
        defineProperty(SuperGotoToleranceNP);

    if (m_HasRaPid)
        defineProperty(RaPidNP);

    if (m_HasDecPid)
        defineProperty(DecPidNP);
}

void LX200_Proxisky::deleteVendorProperties()
{
    deleteProperty(ModelTP);
    deleteProperty(RALimitEnableSP);
    deleteProperty(RALimitNP);
    deleteProperty(DecLimitEnableSP);
    deleteProperty(DecLimitNP);
    deleteProperty(LedSP);
    deleteProperty(AcsEnableSP);
    deleteProperty(AcsThresholdNP);
    deleteProperty(AcsCollisionNP);
    deleteProperty(AcsClearSP);
    deleteProperty(PowerLossSP);
    deleteProperty(AsiairSP);
    deleteProperty(SuperHomeSP);
    deleteProperty(DecSecHomeSP);
    deleteProperty(AutoTrackSP);
    deleteProperty(SuperGotoSP);
    deleteProperty(SuperGotoToleranceNP);
    deleteProperty(RaPidNP);
    deleteProperty(DecPidNP);

    // Clear both the capability and the published state so the next connection re-probes from
    // scratch - a different mount may be on the other end of the port. Deleting a property that
    // was never defined is a no-op: DefaultDevice::deleteProperty returns false without emitting
    // IDDelete, so the unconditional list above is safe.
    m_HasModel = false;
    m_ProbesDone = false;
    m_PropertiesDefined = false;
    m_ProbeDeadline = 0;
    m_ProbeBudgetWarned = false;
    m_HasRALimits = false;
    m_HasDecLimits = false;
    m_HasLed = false;
    m_HasAcs = false;
    m_HasPowerLoss = false;
    m_HasAsiair = false;
    m_HasSuperHome = false;
    m_HasDecSecHome = false;
    m_HasAutoTrack = false;
    m_HasSuperGoto = false;
    m_HasSuperGotoTolerance = false;
    m_HasRaPid = false;
    m_HasDecPid = false;
    m_AcsEnabled = false;
    m_AcsCountersGood = true;
    m_AcsPollDivider = 0;
}

/***************************************************************************************
** Vendor command helpers
***************************************************************************************/

bool LX200_Proxisky::getVendorString(const char *cmd, char *out, size_t outLen, bool requireTerminated, int *rcOut)
{
    if (outLen > 0)
        out[0] = '\0';

    if (rcOut != nullptr)
        *rcOut = 0;

    // Checked here, at the one place every vendor read passes through, rather than only at the
    // head of each probe. A probe that issues several reads would otherwise overrun the budget by
    // a whole transport timeout per read - seconds, on TCP. runVendorProbes() clears the deadline
    // when it finishes, and probeBudgetExpired() is false whenever the deadline is unset, so this
    // costs nothing once the driver is running.
    if (probeBudgetExpired())
        return false;

    if (readVendorStringOnce(cmd, out, outLen, requireTerminated, rcOut))
        return true;

    // One retry, and only while the capability probes are running - m_ProbeDeadline is non-zero
    // exactly then, which is also what keeps this out of the status loop and off every runtime
    // readback, where a retry would just double the cost of a mount that has stopped talking.
    //
    // Worth it here because during probing a dropped reply is not merely a failed read: "did the
    // mount answer" is the only capability test this half of the protocol offers, so a single lost
    // byte is indistinguishable from "feature not fitted" and hides a working control until the
    // user reconnects. Note this does not retry the vendor's "not supported" sentinel - a bare "0"
    // is a *successful* read here and is rejected later by the parsers - so a mount that genuinely
    // lacks a feature says so once and costs nothing extra.
    //
    // probeModel() is deliberately not covered: it runs before the deadline is set and has its own
    // retry loop, sized for the mount's ~1s startup window rather than for a transport glitch.
    if (m_ProbeDeadline <= 0 || probeBudgetExpired())
        return false;

    const struct timespec retryPause = {0, PROXISKY_PROBE_RETRY_NSEC};
    sleepFor(retryPause);

    LOGF_DEBUG("Retrying %s once before treating the feature as unsupported.", cmd);
    return readVendorStringOnce(cmd, out, outLen, requireTerminated, rcOut);
}

bool LX200_Proxisky::readVendorStringOnce(const char *cmd, char *out, size_t outLen, bool requireTerminated,
        int *rcOut)
{
    char response[RB_MAX_LEN] = {0};

    int rc = getCommandSingleCharErrorOrLongResponse(PortFD, response, cmd);

    if (rcOut != nullptr)
        *rcOut = rc;

    // Hand back whatever arrived even on the failure paths below, so a caller that wants to
    // report what the mount actually said does not have to re-issue the command to find out.
    snprintf(out, outLen, "%s", response);

    // getCommandSingleCharErrorOrLongResponse() fills and NUL-terminates the buffer (stripping
    // the '#') before it checks the TTY status, so a reply that arrives without a terminator
    // comes back as a negative rc with usable data. Only an empty buffer is a real failure.
    if (rc < 1 && response[0] == '\0')
    {
        LOGF_DEBUG("No response to %s (rc=%d)", cmd, rc);
        return false;
    }

    if (rc < 1)
    {
        if (requireTerminated)
        {
            LOGF_DEBUG("Discarding unterminated response to %s: <%s> (rc=%d)", cmd, response, rc);
            return false;
        }

        LOGF_DEBUG("Unterminated response to %s: <%s> (rc=%d)", cmd, response, rc);
    }

    return true;
}

bool LX200_Proxisky::getVendorBool(const char *cmd, bool &value)
{
    char response[RB_MAX_LEN] = {0};

    if (!getVendorString(cmd, response, sizeof(response)))
        return false;

    if (response[0] != '0' && response[0] != '1')
    {
        LOGF_DEBUG("%s: unexpected boolean reply <%s>", cmd, response);
        return false;
    }

    value = (response[0] == '1');
    return true;
}

bool LX200_Proxisky::sendVendorSetter(const char *cmd)
{
    char response[RB_MAX_LEN] = {0};

    // The mount acknowledges every setter command: "1#" accepted,
    // "0#" refused. Measured on a UMi20S 1.0.6, all 28 setters tested answered in 3-15ms.
    // Consuming it here is both the success check and what keeps the ack from being read as the
    // *next* command's reply - a blind write leaves it sitting in the buffer.
    //
    // Both failures log at DEBUG, not ERROR: every caller already reports the refusal to the user
    // in its own words, and two messages for one event just makes the log read like two faults.
    // What is kept here is the part the caller cannot say - the exact command and the raw reply.
    if (!getVendorString(cmd, response, sizeof(response)))
    {
        LOGF_DEBUG("%s: no acknowledgement from the mount", cmd);
        return false;
    }

    if (response[0] != '1')
    {
        LOGF_DEBUG("%s: refused, mount replied <%s>", cmd, response);
        return false;
    }

    // No settle delay. Once the ack has been read the write has landed.
    return true;
}

bool LX200_Proxisky::getVendorPair(const char *cmd, double &first, double &second)
{
    char response[RB_MAX_LEN] = {0};

    // Require a properly terminated reply: getVendorString() deliberately accepts a timed-out
    // partial read, which is fine for a free-form string but would silently truncate a number.
    if (!getVendorString(cmd, response, sizeof(response), true))
        return false;

    // A bare "0" means the feature is not supported, and lands here as a failed match.
    if (sscanf(response, "%lf|%lf", &first, &second) != 2)
    {
        LOGF_DEBUG("%s: not a pair reply <%s>, treating feature as unsupported", cmd, response);
        return false;
    }

    return true;
}

bool LX200_Proxisky::getVendorQuad(const char *cmd, double *out)
{
    char response[RB_MAX_LEN] = {0};

    if (!getVendorString(cmd, response, sizeof(response), true))
        return false;

    // At least four fields, not exactly four: a UMi20S answers ":Pnag<axis>#" with seven.
    if (sscanf(response, "%lf|%lf|%lf|%lf", &out[0], &out[1], &out[2], &out[3]) != 4)
    {
        LOGF_DEBUG("%s: fewer than four numeric fields in <%s>, treating feature as unsupported", cmd, response);
        return false;
    }

    LOGF_DEBUG("%s: raw reply <%s>", cmd, response);
    return true;
}

bool LX200_Proxisky::probeBoolSwitch(INDI::PropertySwitch &sp, const char *readCmd, const char *label)
{
    bool value = false;

    if (!getVendorBool(readCmd, value))
    {
        // "Did it answer at all" is the only capability test this part of the protocol offers -
        // an unsupported feature and a dropped reply are indistinguishable here. getVendorString()
        // has already retried once, so this is a feature that stayed silent twice; probing runs
        // once per connection, so it stays hidden until the user reconnects.
        //
        // WARN rather than INFO, matching probeLed() and probeAcs(). The lower level would imply a
        // distinction this probe cannot actually draw - "the board says not fitted" reads as
        // routine, but that is a bare "0", which arrives as a *successful* read and never reaches
        // this branch. Everything that gets here is silence, and silence is worth a warning.
        LOGF_WARN("%s did not answer (%s), even after a retry; its control is hidden. "
                  "Reconnect to probe again.", label, readCmd);
        return false;
    }

    sp.reset();
    sp[value ? 1 : 0].setState(ISS_ON);
    sp.setState(IPS_OK);
    LOGF_INFO("%s: %s", label, value ? "enabled" : "disabled");
    return true;
}

bool LX200_Proxisky::handleBoolSwitch(INDI::PropertySwitch &sp, const char *setOn, const char *setOff,
                                      const char *readCmd, const char *label,
                                      ISState *states, char *names[], int n, bool *mirror)
{
    // Captured before update() overwrites it: on a refusal this is the state the mount is still
    // running, and it is the only record of it left if the readback also fails.
    const bool previous = (sp[1].getState() == ISS_ON);

    if (!sp.update(states, names, n))
    {
        restoreAfterFailedUpdate(sp, previous);
        return true;
    }

    sp.setState(IPS_BUSY);
    sp.apply();

    const bool wanted = (sp[1].getState() == ISS_ON);
    LOGF_DEBUG("===CMD==> %s = %s", label, wanted ? "enabled" : "disabled");

    bool actual = false;

    if (!sendVendorSetter(wanted ? setOn : setOff))
    {
        // Refused, so the mount is still running the old setting - but sp.update() has already
        // moved the switch to what the user asked for. Put back what the mount actually holds,
        // falling back to the pre-write selection when even the readback fails, so the panel and
        // the mirror below never disagree about what is running.
        const bool restored = getVendorBool(readCmd, actual) ? actual : previous;

        sp.reset();
        sp[restored ? 1 : 0].setState(ISS_ON);
        sp.setState(IPS_ALERT);

        if (mirror != nullptr)
            *mirror = restored;

        sp.apply("The mount refused the %s change; it is still %s.", label,
                 restored ? "enabled" : "disabled");
        LOGF_ERROR("The mount refused the %s change.", label);
        return true;
    }

    sp.reset();

    if (getVendorBool(readCmd, actual))
    {
        sp[actual ? 1 : 0].setState(ISS_ON);

        if (mirror != nullptr)
            *mirror = actual;

        if (actual == wanted)
        {
            sp.setState(IPS_OK);
            sp.apply("%s %s.", label, actual ? "enabled" : "disabled");
            LOGF_INFO("%s %s.", label, actual ? "enabled" : "disabled");
        }
        else
        {
            sp.setState(IPS_ALERT);
            sp.apply("Mount still reports %s %s after requesting %s.", label,
                     actual ? "enabled" : "disabled", wanted ? "enabled" : "disabled");
            LOGF_WARN("Mount did not accept the %s change (wanted %s, reads %s).", label,
                      wanted ? "enabled" : "disabled", actual ? "enabled" : "disabled");
        }

        return true;
    }

    // Leave a valid selection behind: the reset() above cleared every element.
    sp[wanted ? 1 : 0].setState(ISS_ON);
    sp.setState(IPS_ALERT);

    // Assume the write landed. Leaving the mirror stale makes ReadScopeStatus() keep
    // polling a feature the user just turned off.
    if (mirror != nullptr)
        *mirror = wanted;

    sp.apply("Set %s but could not read the state back (%s).", label, readCmd);
    LOGF_ERROR("Could not read back %s state (%s).", label, readCmd);
    return true;
}

bool LX200_Proxisky::handlePendingSwitch(INDI::PropertySwitch &sp, const char *setOn, const char *setOff,
        const char *readCmd, const char *label,
        ISState *states, char *names[], int n)
{
    // Captured before update() overwrites it: on a refusal this is the setting the mount still
    // has stored, and it is the only record of it left if the readback also fails.
    const bool previous = (sp[1].getState() == ISS_ON);

    if (!sp.update(states, names, n))
    {
        restoreAfterFailedUpdate(sp, previous);
        return true;
    }

    sp.setState(IPS_BUSY);
    sp.apply();

    const bool wanted = (sp[1].getState() == ISS_ON);
    LOGF_DEBUG("===CMD==> %s = %s (applies at next power up)", label, wanted ? "enabled" : "disabled");

    // The readback reports the stored setting, which says nothing about what the mount is
    // currently running, so a mismatch only refines the log message - never the verdict. The
    // switch stays IPS_BUSY showing the requested state until the next connection, when the
    // probe reads back whatever the mount actually booted with.
    bool actual = false;

    if (!sendVendorSetter(wanted ? setOn : setOff))
    {
        // Refused, so nothing was stored - but sp.update() has already moved the switch. Put back
        // what the mount still has, falling back to the pre-write selection if the readback fails.
        const bool restored = getVendorBool(readCmd, actual) ? actual : previous;

        sp.reset();
        sp[restored ? 1 : 0].setState(ISS_ON);
        sp.setState(IPS_ALERT);
        sp.apply("The mount refused the %s change; it still has %s stored.", label,
                 restored ? "enabled" : "disabled");
        LOGF_ERROR("The mount refused the %s change.", label);
        return true;
    }

    if (!getVendorBool(readCmd, actual))
    {
        // No answer is different from a mismatch. Telling the user it "will apply after a power
        // cycle" here would be a guess they act on - they would cycle the mount and find the
        // setting unchanged, with nothing but a log line to explain why.
        sp.setState(IPS_ALERT);
        sp.apply("Sent the %s change but could not confirm the mount stored it (%s).", label, readCmd);
        LOGF_WARN("Could not read back %s state (%s) after the write.", label, readCmd);
        return true;
    }

    // IPS_OK, not IPS_BUSY. The state describes the write, and the write is finished and verified
    // - nothing further is in flight. A vector left permanently IPS_BUSY reads to a client as an
    // operation still running, which is how KStars decides to show a spinner; the fact that the
    // mount needs a power cycle before it acts on the value belongs in the message, not the state.
    sp.setState(IPS_OK);
    sp.apply("%s will be %s after the mount is power cycled.", label, wanted ? "enabled" : "disabled");

    if (actual == wanted)
        LOGF_INFO("Mount stored %s = %s; power cycle to apply it.", label, wanted ? "enabled" : "disabled");
    else
        LOGF_INFO("Mount still reports %s = %s; the new setting applies at next power up.", label,
                  actual ? "enabled" : "disabled");

    return true;
}

/***************************************************************************************
** Capability probes
***************************************************************************************/

void LX200_Proxisky::probeModel()
{
    // The mount answers ":Pbvg#" with a bare "0" - its "unsupported" sentinel - for about a
    // second after connect, and only then reports its model. Absorb that here, during connect,
    // where a short delay is expected, rather than retrying from the status loop where a slow
    // read competes with guiding and abort handling.
    const struct timespec settle = {0, 250000000L};   // 250ms
    const double deadline = monotonicSeconds() + PROXISKY_MODEL_PROBE_SECONDS;

    for (int attempt = 1; ; attempt++)
    {
        char response[RB_MAX_LEN] = {0};
        int rc = 0;

        // requireTerminated is off: an unterminated reply still carries a usable model string,
        // and rejecting it would fail a mount that is merely slow. rcOut is what this probe needs
        // that the others do not - it gates every other vendor property, so when it fails it must
        // say why without the user having to enable debug logging first.
        getVendorString(":Pbvg#", response, sizeof(response), false, &rc);

        if (response[0] != '\0' && !(response[0] == '0' && response[1] == '\0'))
        {
            // WARN, not INFO: no vendor command has ever been observed to answer unterminated, so
            // this is an anomaly worth surfacing rather than a normal step being narrated. The
            // model string is still usable, hence accepting it and carrying on.
            if (rc < 1)
                LOGF_WARN("Proxisky :Pbvg# replied without a terminator (rc=%d), accepting '%s'.", rc, response);

            m_HasModel = true;
            ModelTP[0].setText(response);
            ModelTP.setState(IPS_OK);
            LOGF_INFO("Proxisky model/firmware: %s", response);
            return;
        }

        // Checked after the read, not before, so a mount that is merely slow still gets one full
        // attempt. An unanswered read burns a whole transport timeout, so this is what keeps
        // Connect() from blocking on a mount that has no vendor extensions.
        // The attempt count is the backstop for a deadline that can never be reached at all - see
        // PROXISKY_MODEL_PROBE_MAX_ATTEMPTS. In every real run the clock ends the loop first.
        if (monotonicSeconds() >= deadline || attempt >= PROXISKY_MODEL_PROBE_MAX_ATTEMPTS)
        {
            LOGF_WARN("Proxisky vendor extensions not reported by :Pbvg# after %d attempts over %ds "
                      "(rc=%d, response='%s'), vendor properties disabled.",
                      attempt, PROXISKY_MODEL_PROBE_SECONDS, rc, response);
            return;
        }

        LOGF_DEBUG("Proxisky :Pbvg# not ready yet (attempt %d, rc=%d, response='%s'), retrying.",
                   attempt, rc, response);
        sleepFor(settle);
    }
}

void LX200_Proxisky::probeRALimits()
{
    if (probeBudgetExpired())
        return;

    double left = 0, right = 0;

    if (!getVendorPair(":Pnbg#", left, right))
    {
        LOG_INFO("RA travel limits not supported by this mount.");
        return;
    }

    m_HasRALimits = true;
    const double raLimits[2] = {left, right};
    RALimitNP.setState(setReportedValues(RALimitNP, raLimits, 2) ? IPS_OK : IPS_ALERT);

    bool enabled = false;
    RALimitEnableSP.reset();

    if (getVendorBool(":PsSrg#", enabled))
    {
        RALimitEnableSP[enabled ? 1 : 0].setState(ISS_ON);
        RALimitEnableSP.setState(IPS_OK);
        LOGF_INFO("RA travel limits: left %.0f deg, right %.0f deg, %s", left, right,
                  enabled ? "enabled" : "disabled");
    }
    else
    {
        // reset() above cleared both elements; an ISR_1OFMANY vector with nothing ON is invalid
        // and clients render it as an empty selector. Fall back to a known element and flag it.
        RALimitEnableSP[0].setState(ISS_ON);
        RALimitEnableSP.setState(IPS_ALERT);
        LOG_WARN("Could not read RA limit enable state (:PsSrg#), showing it as disabled.");
    }
}

void LX200_Proxisky::probeDecLimits()
{
    // Gate on the capability read: if :PsSdG# answers with a valid pair the feature is there.
    if (probeBudgetExpired())
        return;

    double ccw = 0, cw = 0;

    if (!getVendorPair(":PsSdG#", ccw, cw))
    {
        LOGF_INFO("Dec travel limits not supported by this mount (model '%s').", ModelTP[0].getText());
        return;
    }

    m_HasDecLimits = true;
    const double decLimits[2] = {ccw, cw};
    DecLimitNP.setState(setReportedValues(DecLimitNP, decLimits, 2) ? IPS_OK : IPS_ALERT);

    bool enabled = false;
    DecLimitEnableSP.reset();

    if (getVendorBool(":PsSdg#", enabled))
    {
        DecLimitEnableSP[enabled ? 1 : 0].setState(ISS_ON);
        DecLimitEnableSP.setState(IPS_OK);
        LOGF_INFO("Dec travel limits: CCW %.0f deg, CW %.0f deg, %s", ccw, cw,
                  enabled ? "enabled" : "disabled");
    }
    else
    {
        DecLimitEnableSP[0].setState(ISS_ON);
        DecLimitEnableSP.setState(IPS_ALERT);
        LOG_WARN("Could not read Dec limit enable state (:PsSdg#), showing it as disabled.");
    }
}

void LX200_Proxisky::probeLed()
{
    if (probeBudgetExpired())
        return;

    // The LED is the first feature with a capability query distinct from its state query:
    // :PsLga# asks whether the board has an LED at all, :PsLgb# reads whether it is currently
    // lit. One character apart - do not conflate them.
    bool supported = false;

    if (!getVendorBool(":PsLga#", supported))
    {
        // Probing is once-per-connection, so this hides the control until the user reconnects.
        LOG_WARN("Status LED capability could not be read (:PsLga#); its control is hidden.");
        return;
    }

    if (!supported)
    {
        LOG_INFO("Status LED not fitted to this mount.");
        return;
    }

    // The board has an LED, so the control is published whatever the state read does - hiding a
    // working switch because one read was dropped would cost the user the feature until they
    // reconnect. Same treatment probeAcs() gives its own enable flag.
    m_HasLed = true;
    LedSP.reset();

    bool lit = false;

    if (getVendorBool(":PsLgb#", lit))
    {
        LedSP[lit ? 1 : 0].setState(ISS_ON);
        LedSP.setState(IPS_OK);
        LOGF_INFO("Status LED: %s", lit ? "on" : "off");
    }
    else
    {
        LedSP[0].setState(ISS_ON);
        LedSP.setState(IPS_ALERT);
        LOG_WARN("Mount reports a status LED (:PsLga#) but its state could not be read (:PsLgb#); "
                 "showing it as off.");
    }
}

void LX200_Proxisky::probeAcs()
{
    if (probeBudgetExpired())
        return;

    // :PbC# is a board capability query, distinct from :PsCg# which reports whether the user
    // has the feature switched on.
    bool supported = false;

    if (!getVendorBool(":PbC#", supported))
    {
        LOG_WARN("Anti-collision capability could not be read (:PbC#); its controls are hidden.");
        return;
    }

    if (!supported)
    {
        LOG_INFO("Anti-collision system not fitted to this mount.");
        return;
    }

    double raThreshold = 0, decThreshold = 0;

    if (!getVendorPair(":Pncg#", raThreshold, decThreshold))
    {
        LOG_WARN("Mount reports an anti-collision system (:PbC#) but its thresholds could not be read (:Pncg#).");
        return;
    }

    m_HasAcs = true;
    const double thresholds[2] = {raThreshold, decThreshold};
    AcsThresholdNP.setState(setReportedValues(AcsThresholdNP, thresholds, 2) ? IPS_OK : IPS_ALERT);

    AcsEnableSP.reset();

    if (getVendorBool(":PsCg#", m_AcsEnabled))
    {
        AcsEnableSP[m_AcsEnabled ? 1 : 0].setState(ISS_ON);
        AcsEnableSP.setState(IPS_OK);
    }
    else
    {
        AcsEnableSP[0].setState(ISS_ON);
        AcsEnableSP.setState(IPS_ALERT);
        LOG_WARN("Could not read anti-collision enable state (:PsCg#), showing it as disabled.");
    }

    LOGF_INFO("Anti-collision: RA threshold %.0f, Dec threshold %.0f, %s", raThreshold, decThreshold,
              m_AcsEnabled ? "enabled" : "disabled");

    refreshAcsCounters(false);
}

void LX200_Proxisky::probeSettings()
{
    if (probeBudgetExpired())
        return;

    // Power-loss memory is the one here with a board capability query of its own. Lower-case
    // :Pbc#
    bool fitted = false;

    if (!getVendorBool(":Pbc#", fitted))
        LOG_WARN("Power loss memory capability could not be read (:Pbc#); its control is hidden.");
    else if (fitted)
        m_HasPowerLoss = probeBoolSwitch(PowerLossSP, ":Pscg#", "Power loss memory");
    else
        LOG_INFO("Power loss memory not fitted to this mount.");

    if (probeBudgetExpired())
        return;

    m_HasAsiair = probeBoolSwitch(AsiairSP, ":PsAg#", "ASIAIR homing");

    if (probeBudgetExpired())
        return;

    m_HasSuperHome = probeBoolSwitch(SuperHomeSP, ":PsShg#", "Supervised Home");

    if (probeBudgetExpired())
        return;

    m_HasDecSecHome = probeBoolSwitch(DecSecHomeSP, ":PsDgb#", "Dec second home");

    if (probeBudgetExpired())
        return;

    m_HasAutoTrack = probeBoolSwitch(AutoTrackSP, ":Psag#", "Auto tracking");
}

void LX200_Proxisky::probeSuperGoto()
{
    if (probeBudgetExpired())
        return;

    // Asymmetric family: the enable flag reads from :Pndgo# but writes to :Pndss0|1#.
    if (!probeBoolSwitch(SuperGotoSP, ":Pndgo#", "Supervised GOTO"))
        return;

    // The switch answered, so the feature exists and its control is published regardless of what
    // the tolerance read does below - those are two separate properties and one should not take
    // the other down with it.
    m_HasSuperGoto = true;

    char response[RB_MAX_LEN] = {0};

    // requireTerminated: this is a numeric parse, so a timed-out partial read must be rejected
    // rather than parsed. A truncated "10#" arriving as "1" would otherwise fail the range check
    // below and hide a supported feature for the whole session.
    if (!getVendorString(":Pndgv#", response, sizeof(response), true))
    {
        LOG_WARN("Mount reports Super GOTO (:Pndgo#) but its tolerance could not be read (:Pndgv#); "
                 "the enable switch is still available, the tolerance field is not.");
        return;
    }

    double tolerance = 0;

    // A bare "#" reply decodes to an empty string, which atof() would silently turn into 0 and
    // publish as a valid tolerance.
    if (sscanf(response, "%lf", &tolerance) != 1)
    {
        LOGF_WARN("Supervised GOTO tolerance reply was not numeric <%s>; the tolerance field is hidden.", response);
        return;
    }

    // A bare "0" is the vendor's not-supported sentinel here, same as it is for :Pnbg#. The
    // pair/quad parsers reject it for free because they require a '|'; a single %lf does not, and
    // 0 sits below the element's declared minimum - which is why the bounds are read from the
    // element rather than repeated, so widening the range cannot quietly admit the sentinel.
    if (tolerance < SuperGotoToleranceNP[0].getMin() || tolerance > SuperGotoToleranceNP[0].getMax())
    {
        LOGF_INFO("Supervised GOTO tolerance not supported by this mount (reply '%s'); "
                  "its enable switch is still available.", response);
        return;
    }

    m_HasSuperGotoTolerance = true;
    SuperGotoToleranceNP[0].setValue(tolerance);
    SuperGotoToleranceNP.setState(IPS_OK);
    LOGF_INFO("Supervised GOTO tolerance: %.0f degrees", tolerance);
}

bool LX200_Proxisky::refreshSuperGotoTolerance(double expected)
{
    char response[RB_MAX_LEN] = {0};

    if (!getVendorString(":Pndgv#", response, sizeof(response), true))
    {
        SuperGotoToleranceNP.setState(IPS_ALERT);
        SuperGotoToleranceNP.apply();
        LOG_ERROR("Could not read back Super GOTO tolerance (:Pndgv#).");
        return false;
    }

    double actual = 0;

    if (sscanf(response, "%lf", &actual) != 1)
    {
        SuperGotoToleranceNP.setState(IPS_ALERT);
        SuperGotoToleranceNP.apply();
        LOGF_ERROR("Supervised GOTO tolerance reply was not numeric <%s>.", response);
        return false;
    }

    // Same range gate probeSuperGoto() applies, and read from the same place. Without it a bare
    // "0" - the vendor's not-supported sentinel - or a truncated reply would be published against
    // an element it sits outside, leaving a value on screen the client cannot send back.
    if (actual < SuperGotoToleranceNP[0].getMin() || actual > SuperGotoToleranceNP[0].getMax())
    {
        SuperGotoToleranceNP.setState(IPS_ALERT);
        SuperGotoToleranceNP.apply();
        LOGF_ERROR("Mount reported an out-of-range Super GOTO tolerance <%s>.", response);
        return false;
    }

    SuperGotoToleranceNP[0].setValue(actual);

    const bool matched = (std::lround(actual) == std::lround(expected));
    SuperGotoToleranceNP.setState(matched ? IPS_OK : IPS_ALERT);

    if (matched)
    {
        SuperGotoToleranceNP.apply("Supervised GOTO tolerance set to %.0f degrees.", actual);
        LOGF_INFO("Supervised GOTO tolerance set to %.0f degrees.", actual);
    }
    else
    {
        SuperGotoToleranceNP.apply("Mount reports Supervised GOTO tolerance %.0f after requesting %.0f.",
                                   actual, expected);
        LOGF_WARN("Mount reports Super GOTO tolerance %.0f after requesting %.0f.", actual, expected);
    }

    return matched;
}

bool LX200_Proxisky::rangeCheck(INDI::PropertyNumber &np, int index, double value, const char *what,
                                const char *unit)
{
    auto &widget = np[index];

    // Non-finite first: the comparisons below are false for a NaN, so without this it would pass
    // as "in range" and reach lround(), where it is undefined.
    if (std::isfinite(value) && value >= widget.getMin() && value <= widget.getMax())
        return true;

    np.setState(IPS_ALERT);
    np.apply();
    LOGF_ERROR("%s must be between %.0f and %.0f%s.", what, widget.getMin(), widget.getMax(), unit);
    return false;
}

bool LX200_Proxisky::setReportedValues(INDI::PropertyNumber &np, const double *values, int count)
{
    bool allInRange = true;

    for (int i = 0; i < count; i++)
    {
        np[i].setValue(values[i]);

        if (!std::isfinite(values[i]) || values[i] < np[i].getMin() || values[i] > np[i].getMax())
        {
            allInRange = false;
            LOGF_WARN("Mount reports %s = %.0f, outside this driver's accepted range %.0f..%.0f. "
                      "It was most likely set with the vendor tool; writing a new value will correct it.",
                      np[i].getLabel(), values[i], np[i].getMin(), np[i].getMax());
        }
    }

    return allInRange;
}

bool LX200_Proxisky::probePidAxis(INDI::PropertyNumber &np, const char *readCmd, const char *label)
{
    double gains[4] = {0, 0, 0, 0};

    if (!getVendorQuad(readCmd, gains))
    {
        LOGF_INFO("%s PID gains not supported by this mount.", label);
        return false;
    }

    np.setState(setReportedValues(np, gains, 4) ? IPS_OK : IPS_ALERT);
    LOGF_INFO("%s PID: angle Kp %.0f, angle Ki %.0f, speed Kp %.0f, speed Ki %.0f", label, gains[0], gains[1], gains[2],
              gains[3]);
    return true;
}

bool LX200_Proxisky::handlePidWrite(INDI::PropertyNumber &np, const char *axisPrefix, const char *readCmd,
                                    const char *label, double values[], char *names[], int n)
{
    // The sequence ends in ":Pnas<axis>r#", which the vendor documents as "Commit/restart axis".
    // Measured on an idle UMi20S it answered "1#" in 8ms and disturbed nothing - but that was with
    // the axis unenergised, and nothing here would stop a client sending it mid-slew or in the
    // middle of an exposure. Refuse unless the mount is standing still. Manual N/S/E/W motion does
    // not show in TrackState, so the movement vectors are checked too, exactly as ReadScopeStatus()
    // does before its own vendor reads.
    const bool moving = (MovementNSSP.getState() == IPS_BUSY) || (MovementWESP.getState() == IPS_BUSY);
    const bool stopped = (TrackState == SCOPE_IDLE || TrackState == SCOPE_PARKED) && !moving;

    if (!stopped)
    {
        np.setState(IPS_ALERT);
        np.apply("PID gains can only be changed while the mount is stopped - the commit restarts the axis.");
        LOGF_WARN("Refusing to write %s PID gains while the mount is not idle.", label);
        return true;
    }

    // Seeded from the property so a partial vector still has something to send, but all four
    // arrive in practice: they are one field group behind a single Set button.
    double wanted[4];

    for (int i = 0; i < 4; i++)
        wanted[i] = np[i].getValue();

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (np[j].isNameMatch(names[i]))
            {
                wanted[j] = values[i];
                LOGF_DEBUG("===CMD==> %s %s= %f", label, np[j].getLabel(), wanted[j]);
            }
        }
    }

    // All four gains are written on every commit, so all four are checked. The bounds live on the
    // elements - speed Ki has a much tighter ceiling than the other three - and rangeCheck() reads
    // them from there, so this loop never needs to know that.
    for (int i = 0; i < 4; i++)
    {
        char field[64] = {0};
        snprintf(field, sizeof(field), "%s %s", label, np[i].getLabel());

        if (!rangeCheck(np, i, wanted[i], field))
            return true;
    }

    // Order matters: all four gains, then the commit.
    static const char *suffix[4] = {"Ap", "Ai", "Sp", "Si"};
    char cmd[CMD_MAX_LEN] = {0};

    for (int i = 0; i < 4; i++)
    {
        snprintf(cmd, sizeof(cmd), "%s%s%d#", axisPrefix, suffix[i], static_cast<int>(std::lround(wanted[i])));

        if (!sendVendorSetter(cmd))
        {
            // Abandoning here is safe precisely because the commit below never goes out: without
            // it the mount does not retain any of the gains, so it keeps its previous set rather
            // than a half-written one.
            np.setState(IPS_ALERT);
            np.apply("The mount did not accept the %s PID gains.", label);
            return true;
        }
    }

    snprintf(cmd, sizeof(cmd), "%sr#", axisPrefix);

    if (!sendVendorSetter(cmd))
    {
        np.setState(IPS_ALERT);
        np.apply("The mount did not accept the %s PID gain commit.", label);
        return true;
    }

    // Unlike the LED, the readback here is worth acting on: it confirms the gains were stored,
    // and a mismatch means a value did not stick.
    //
    // But the commit really does restart the axis, and while it is coming back the mount reports
    // every gain as zero. Measured on a UMi20S: ":Pnag0#" answered "0|0|0|0|0|0|0#" 68ms after the
    // commit acknowledgement and "300|200|200|50|0|0|0#" 7ms after that. A single read lands in
    // that window and reports a mismatch that never happened. This is the only readback in the
    // driver that retries against a measured transient.
    double actual[4] = {0, 0, 0, 0};
    const struct timespec axisSettle = {0, 20000000L};   // 20ms
    const double restartDeadline = monotonicSeconds() + PROXISKY_PID_RESTART_SECONDS;
    bool readOk = false;

    while (true)
    {
        readOk = getVendorQuad(readCmd, actual);

        // All four zero is also the vendor's "unsupported" sentinel, but this axis answered its
        // gains moments ago, so here it can only mean the restart is still in progress.
        const bool restarting = readOk && actual[0] == 0 && actual[1] == 0 && actual[2] == 0 && actual[3] == 0;

        if ((readOk && !restarting) || monotonicSeconds() >= restartDeadline)
            break;

        sleepFor(axisSettle);
    }

    if (!readOk)
    {
        np.setState(IPS_ALERT);
        np.apply();
        LOGF_ERROR("Could not read back %s PID gains (%s) after the write.", label, readCmd);
        return true;
    }

    // Keep the requested values on display. These gains only take effect after a power cycle,
    // so the readback returning the still-running values is the expected outcome, not a failed
    // write - overwriting the property with them would silently discard what the user asked for
    // and invite them to "correct" it back to the old gains.
    bool matched = true;

    for (int i = 0; i < 4; i++)
    {
        np[i].setValue(wanted[i]);

        if (std::lround(actual[i]) != std::lround(wanted[i]))
            matched = false;
    }

    // IPS_OK for the same reason as handlePendingSwitch: the write is complete and verified, so
    // there is nothing left in flight for a IPS_BUSY to represent. The power cycle is in the message.
    np.setState(IPS_OK);
    np.apply("%s PID gains stored. Power cycle the mount for them to take effect.", label);

    if (matched)
        LOGF_INFO("Mount stored %s PID gains; power cycle to apply them.", label);
    else
        LOGF_INFO("Mount still reports the previous %s PID gains (%.0f|%.0f|%.0f|%.0f); "
                  "the new ones apply at next power up.", label, actual[0], actual[1], actual[2], actual[3]);

    return true;
}

bool LX200_Proxisky::refreshAcsCounters(bool stickyOnFailure)
{
    double raCount = 0, decCount = 0;

    if (!getVendorPair(":Pnccg#", raCount, decCount))
    {
        // Only the polling path latches the sticky flag. The probe-time call happens during
        // connect, in the same window where the mount is still waking up and vendor commands
        // are unreliable - letting one boot-time timeout disable the counters for the whole
        // session would be wrong, and probeAcs() does not run again to undo it.
        AcsCollisionNP.setState(IPS_ALERT);

        if (m_PropertiesDefined)
            AcsCollisionNP.apply();

        if (stickyOnFailure)
        {
            m_AcsCountersGood = false;
            LOG_WARN("Collision counters did not respond (:Pnccg#), disabling further checks this session.");
        }
        else
        {
            LOG_WARN("Collision counters did not respond (:Pnccg#) during startup.");
        }

        return false;
    }

    AcsCollisionNP[0].setValue(raCount);
    AcsCollisionNP[1].setValue(decCount);
    AcsCollisionNP.setState(IPS_OK);

    // defineVendorProperties() has not run yet when this is called from probeAcs(); applying a
    // property the client has never been sent a definition for is dropped and logged as unknown.
    if (m_PropertiesDefined)
        AcsCollisionNP.apply();

    return true;
}

bool LX200_Proxisky::ReadScopeStatus()
{
    if (!LX200_OnStep::ReadScopeStatus())
        return false;

    // Nothing slow happens while the mount is actually moving. TrackState is NOT SCOPE_SLEWING
    // during manual N/S/E/W motion - only MovementNSSP/MovementWESP show it - and the parent
    // early-returns for that case precisely to keep RA/Dec updates
    // fast. A vendor command here would undo that. SCOPE_PARKED and SCOPE_TRACKING both count
    // as settled: excluding them would mean a mount parked at connect never gets probed.
    const bool moving = (MovementNSSP.getState() == IPS_BUSY) || (MovementWESP.getState() == IPS_BUSY);
    const bool settled = (TrackState != SCOPE_SLEWING && TrackState != SCOPE_PARKING) && !moving;

    // Poll the collision counters, but sparingly. Vendor commands are slow relative to the
    // status cadence, so doing this every cycle would compete with OnStep's own polling for
    // the port. Skipped while ACS is switched off.
    if (m_HasAcs && m_AcsEnabled && m_AcsCountersGood && settled)
    {
        if (++m_AcsPollDivider >= PROXISKY_ACS_POLL_DIVIDER)
        {
            m_AcsPollDivider = 0;
            refreshAcsCounters();
        }
    }

    return true;
}

/***************************************************************************************
** Readback after write
***************************************************************************************/

bool LX200_Proxisky::refreshRALimits(double expectedLeft, double expectedRight)
{
    double left = 0, right = 0;

    if (!getVendorPair(":Pnbg#", left, right))
    {
        RALimitNP.setState(IPS_ALERT);
        RALimitNP.apply();
        LOG_ERROR("Could not read back RA travel limits (:Pnbg#).");
        return false;
    }

    RALimitNP[0].setValue(left);
    RALimitNP[1].setValue(right);

    const bool matched = (std::lround(left) == std::lround(expectedLeft) &&
                          std::lround(right) == std::lround(expectedRight));

    RALimitNP.setState(matched ? IPS_OK : IPS_ALERT);

    if (matched)
    {
        RALimitNP.apply("RA travel limits set to %.0f / %.0f degrees.", left, right);
        LOGF_INFO("RA travel limits set to left %.0f, right %.0f degrees.", left, right);
    }
    else
    {
        RALimitNP.apply("Mount reports RA travel limits %.0f / %.0f after requesting %.0f / %.0f.",
                        left, right, expectedLeft, expectedRight);
        LOGF_WARN("Mount reports RA travel limits %.0f|%.0f after requesting %.0f|%.0f.", left, right,
                  expectedLeft, expectedRight);
    }

    return matched;
}

bool LX200_Proxisky::refreshDecLimits(double expectedCCW, double expectedCW)
{
    double ccw = 0, cw = 0;

    if (!getVendorPair(":PsSdG#", ccw, cw))
    {
        DecLimitNP.setState(IPS_ALERT);
        DecLimitNP.apply();
        LOG_ERROR("Could not read back Dec travel limits (:PsSdG#).");
        return false;
    }

    DecLimitNP[0].setValue(ccw);
    DecLimitNP[1].setValue(cw);

    const bool matched = (std::lround(ccw) == std::lround(expectedCCW) &&
                          std::lround(cw) == std::lround(expectedCW));

    DecLimitNP.setState(matched ? IPS_OK : IPS_ALERT);

    if (matched)
    {
        DecLimitNP.apply("Dec travel limits set to %.0f / %.0f degrees.", ccw, cw);
        LOGF_INFO("Dec travel limits set to CCW %.0f, CW %.0f degrees.", ccw, cw);
    }
    else
    {
        DecLimitNP.apply("Mount reports Dec travel limits %.0f / %.0f after requesting %.0f / %.0f.",
                         ccw, cw, expectedCCW, expectedCW);
        LOGF_WARN("Mount reports Dec travel limits %.0f|%.0f after requesting %.0f|%.0f.", ccw, cw,
                  expectedCCW, expectedCW);
    }

    return matched;
}

bool LX200_Proxisky::refreshAcsThresholds(double expectedRA, double expectedDec)
{
    double raThreshold = 0, decThreshold = 0;

    if (!getVendorPair(":Pncg#", raThreshold, decThreshold))
    {
        AcsThresholdNP.setState(IPS_ALERT);
        AcsThresholdNP.apply();
        LOG_ERROR("Could not read back anti-collision thresholds (:Pncg#).");
        return false;
    }

    AcsThresholdNP[0].setValue(raThreshold);
    AcsThresholdNP[1].setValue(decThreshold);

    const bool matched = (std::lround(raThreshold) == std::lround(expectedRA) &&
                          std::lround(decThreshold) == std::lround(expectedDec));

    AcsThresholdNP.setState(matched ? IPS_OK : IPS_ALERT);

    if (matched)
    {
        AcsThresholdNP.apply("Anti-collision thresholds set to RA %.0f / Dec %.0f.", raThreshold, decThreshold);
        LOGF_INFO("Anti-collision thresholds set to RA %.0f, Dec %.0f.", raThreshold, decThreshold);
    }
    else
    {
        AcsThresholdNP.apply("Mount reports anti-collision thresholds %.0f / %.0f after requesting %.0f / %.0f.",
                             raThreshold, decThreshold, expectedRA, expectedDec);
        LOGF_WARN("Mount reports anti-collision thresholds %.0f|%.0f after requesting %.0f|%.0f.", raThreshold,
                  decThreshold, expectedRA, expectedDec);
    }

    return matched;
}

/***************************************************************************************
** Client writes
***************************************************************************************/

bool LX200_Proxisky::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (RALimitNP.isNameMatch(name))
        {
            if (!m_HasRALimits)
                return reportUnavailable(RALimitNP, "RA limits are not available on this mount.");

            // Seeded from the property so a partial vector still has something to send, but in
            // practice every element arrives: a number vector is one field group behind a single
            // Set button, so the client transmits all of it. The values on screen are what the
            // user is looking at and what they mean to write.
            double left = RALimitNP[0].getValue();
            double right = RALimitNP[1].getValue();

            for (int i = 0; i < n; i++)
            {
                if (RALimitNP[0].isNameMatch(names[i]))
                {
                    left = values[i];
                    LOGF_DEBUG("===CMD==> RA limit left= %f", left);
                }
                else if (RALimitNP[1].isNameMatch(names[i]))
                {
                    right = values[i];
                    LOGF_DEBUG("===CMD==> RA limit right= %f", right);
                }
            }

            // Both values go out on every write, so both are checked - short-circuiting on the
            // first bad one, which is also the one worth naming.
            if (!rangeCheck(RALimitNP, 0, left, "RA left limit", " degrees") ||
                    !rangeCheck(RALimitNP, 1, right, "RA right limit", " degrees"))
                return true;

            char cmd[CMD_MAX_LEN] = {0};
            snprintf(cmd, sizeof(cmd), ":Pnbsl%d#", static_cast<int>(std::lround(left)));
            const bool sentLeft = sendVendorSetter(cmd);
            snprintf(cmd, sizeof(cmd), ":Pnbsr%d#", static_cast<int>(std::lround(right)));
            const bool sentRight = sendVendorSetter(cmd);

            // Refresh even when a setter was refused. The two limits are written separately, so a
            // refusal can leave one landed and the other not; only the readback says which, and
            // skipping it would leave the panel showing a pair the mount never took.
            if (!sentLeft || !sentRight)
                LOGF_ERROR("The mount refused the RA %s travel limit.",
                           !sentLeft ? (!sentRight ? "left and right" : "left") : "right");

            refreshRALimits(left, right);
            return true;
        }

        if (RaPidNP.isNameMatch(name))
        {
            if (!m_HasRaPid)
                return reportUnavailable(RaPidNP, "RA/Azm PID gains are not available on this mount.");

            return handlePidWrite(RaPidNP, ":Pnas0", ":Pnag0#", "RA/Azm", values, names, n);
        }

        if (DecPidNP.isNameMatch(name))
        {
            if (!m_HasDecPid)
                return reportUnavailable(DecPidNP, "Dec/Alt PID gains are not available on this mount.");

            return handlePidWrite(DecPidNP, ":Pnas1", ":Pnag1#", "Dec/Alt", values, names, n);
        }

        if (SuperGotoToleranceNP.isNameMatch(name))
        {
            if (!m_HasSuperGotoTolerance)
                return reportUnavailable(SuperGotoToleranceNP,
                                         "The Supervised GOTO tolerance is not available on this mount.");

            double tolerance = SuperGotoToleranceNP[0].getValue();

            for (int i = 0; i < n; i++)
            {
                if (SuperGotoToleranceNP[0].isNameMatch(names[i]))
                {
                    tolerance = values[i];
                    LOGF_DEBUG("===CMD==> Supervised GOTO tolerance= %f", tolerance);
                }
            }

            if (!rangeCheck(SuperGotoToleranceNP, 0, tolerance, "Supervised GOTO tolerance", " degrees"))
                return true;

            char cmd[CMD_MAX_LEN] = {0};
            snprintf(cmd, sizeof(cmd), ":Pndsv%d#", static_cast<int>(std::lround(tolerance)));

            // Refresh regardless - see the RA travel limits above for why a refusal still needs
            // the readback.
            if (!sendVendorSetter(cmd))
                LOG_ERROR("The mount refused the Supervised GOTO tolerance.");

            refreshSuperGotoTolerance(tolerance);
            return true;
        }

        if (AcsThresholdNP.isNameMatch(name))
        {
            if (!m_HasAcs)
                return reportUnavailable(AcsThresholdNP, "The anti-collision system is not available on this mount.");

            double raThreshold = AcsThresholdNP[0].getValue();
            double decThreshold = AcsThresholdNP[1].getValue();

            for (int i = 0; i < n; i++)
            {
                if (AcsThresholdNP[0].isNameMatch(names[i]))
                {
                    raThreshold = values[i];
                    LOGF_DEBUG("===CMD==> ACS RA threshold= %f", raThreshold);
                }
                else if (AcsThresholdNP[1].isNameMatch(names[i]))
                {
                    decThreshold = values[i];
                    LOGF_DEBUG("===CMD==> ACS Dec threshold= %f", decThreshold);
                }
            }

            // Both values go out on every write, so both are checked - see the RA limit handler
            // above. The lower bounds are asymmetric (RA accepts 10, Dec only 100), which is
            // another reason to read them off the elements rather than repeat them here.
            if (!rangeCheck(AcsThresholdNP, 0, raThreshold, "RA collision threshold") ||
                    !rangeCheck(AcsThresholdNP, 1, decThreshold, "Dec collision threshold"))
                return true;

            char cmd[CMD_MAX_LEN] = {0};
            snprintf(cmd, sizeof(cmd), ":Pncsr%d#", static_cast<int>(std::lround(raThreshold)));
            const bool sentRa = sendVendorSetter(cmd);
            snprintf(cmd, sizeof(cmd), ":Pncsd%d#", static_cast<int>(std::lround(decThreshold)));
            const bool sentDec = sendVendorSetter(cmd);

            // Refresh regardless - see the RA travel limits above for why a refusal still needs
            // the readback.
            if (!sentRa || !sentDec)
                LOGF_ERROR("The mount refused the %s anti-collision threshold.",
                           !sentRa ? (!sentDec ? "RA and Dec" : "RA") : "Dec");

            refreshAcsThresholds(raThreshold, decThreshold);
            return true;
        }

        if (DecLimitNP.isNameMatch(name))
        {
            if (!m_HasDecLimits)
                return reportUnavailable(DecLimitNP, "Dec limits are not available on this mount.");

            double ccw = DecLimitNP[0].getValue();
            double cw = DecLimitNP[1].getValue();

            for (int i = 0; i < n; i++)
            {
                if (DecLimitNP[0].isNameMatch(names[i]))
                {
                    ccw = values[i];
                    LOGF_DEBUG("===CMD==> Dec limit CCW= %f", ccw);
                }
                else if (DecLimitNP[1].isNameMatch(names[i]))
                {
                    cw = values[i];
                    LOGF_DEBUG("===CMD==> Dec limit CW= %f", cw);
                }
            }

            // Both values go out on every write, so both are checked - see the RA limit handler above.
            if (!rangeCheck(DecLimitNP, 0, ccw, "Dec CCW limit", " degrees") ||
                    !rangeCheck(DecLimitNP, 1, cw, "Dec CW limit", " degrees"))
                return true;

            char cmd[CMD_MAX_LEN] = {0};
            snprintf(cmd, sizeof(cmd), ":PsSdl%d#", static_cast<int>(std::lround(ccw)));
            const bool sentCCW = sendVendorSetter(cmd);
            snprintf(cmd, sizeof(cmd), ":PsSdr%d#", static_cast<int>(std::lround(cw)));
            const bool sentCW = sendVendorSetter(cmd);

            // Refresh regardless - see the RA travel limits above for why a refusal still needs
            // the readback.
            if (!sentCCW || !sentCW)
                LOGF_ERROR("The mount refused the Dec %s travel limit.",
                           !sentCCW ? (!sentCW ? "counter-clockwise and clockwise" : "counter-clockwise") : "clockwise");

            refreshDecLimits(ccw, cw);
            return true;
        }
    }

    return LX200_OnStep::ISNewNumber(dev, name, values, names, n);
}

bool LX200_Proxisky::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Settings that apply immediately.
        if (PowerLossSP.isNameMatch(name))
        {
            if (!m_HasPowerLoss)
                return reportUnavailable(PowerLossSP, "Power-loss memory is not available on this mount.");

            return handleBoolSwitch(PowerLossSP, ":Pscs1#", ":Pscs0#", ":Pscg#", "power loss memory", states, names, n);
        }

        if (AsiairSP.isNameMatch(name))
        {
            if (!m_HasAsiair)
                return reportUnavailable(AsiairSP, "ASIAIR homing is not available on this mount.");

            return handleBoolSwitch(AsiairSP, ":PsAs1#", ":PsAs0#", ":PsAg#", "ASIAIR homing", states, names, n);
        }

        if (SuperHomeSP.isNameMatch(name))
        {
            if (!m_HasSuperHome)
                return reportUnavailable(SuperHomeSP, "Supervised Home is not available on this mount.");

            return handleBoolSwitch(SuperHomeSP, ":PsShs1#", ":PsShs0#", ":PsShg#", "Supervised Home", states, names, n);
        }

        // Reads :PsDgb#, writes :PsDs0|1# - the read and write halves are not symmetric.
        if (DecSecHomeSP.isNameMatch(name))
        {
            if (!m_HasDecSecHome)
                return reportUnavailable(DecSecHomeSP, "Dec second home is not available on this mount.");

            return handleBoolSwitch(DecSecHomeSP, ":PsDs1#", ":PsDs0#", ":PsDgb#", "Dec second home", states, names, n);
        }

        // Reads :Pndgo#, writes :Pndss0|1# - likewise asymmetric.
        if (SuperGotoSP.isNameMatch(name))
        {
            if (!m_HasSuperGoto)
                return reportUnavailable(SuperGotoSP, "Supervised GOTO is not available on this mount.");

            return handleBoolSwitch(SuperGotoSP, ":Pndss1#", ":Pndss0#", ":Pndgo#", "Supervised GOTO", states, names, n);
        }

        if (RALimitEnableSP.isNameMatch(name))
        {
            if (!m_HasRALimits)
                return reportUnavailable(RALimitEnableSP, "RA limits are not available on this mount.");

            return handleBoolSwitch(RALimitEnableSP, ":PsSrs1#", ":PsSrs0#", ":PsSrg#", "RA limits", states, names, n);
        }

        if (DecLimitEnableSP.isNameMatch(name))
        {
            if (!m_HasDecLimits)
                return reportUnavailable(DecLimitEnableSP, "Dec limits are not available on this mount.");

            return handleBoolSwitch(DecLimitEnableSP, ":PsSds1#", ":PsSds0#", ":PsSdg#", "Dec limits", states, names, n);
        }

        // Settings that only take effect after a power cycle.
        if (LedSP.isNameMatch(name))
        {
            if (!m_HasLed)
                return reportUnavailable(LedSP, "This mount has no status LED.");

            return handlePendingSwitch(LedSP, ":PsLs1#", ":PsLs0#", ":PsLgb#", "Status LED", states, names, n);
        }

        if (AutoTrackSP.isNameMatch(name))
        {
            if (!m_HasAutoTrack)
                return reportUnavailable(AutoTrackSP, "Auto tracking is not available on this mount.");

            return handlePendingSwitch(AutoTrackSP, ":Psas1#", ":Psas0#", ":Psag#", "Auto tracking", states, names, n);
        }

        if (AcsEnableSP.isNameMatch(name))
        {
            if (!m_HasAcs)
                return reportUnavailable(AcsEnableSP, "The anti-collision system is not available on this mount.");

            // mirror keeps m_AcsEnabled in step with what the mount reports, which is what the
            // counter polling in ReadScopeStatus() gates on.
            handleBoolSwitch(AcsEnableSP, ":PsCs1#", ":PsCs0#", ":PsCg#", "anti-collision", states, names, n,
                             &m_AcsEnabled);

            // Switching the feature on is an explicit user action, so give the collision
            // counters another chance even if an earlier read had disabled polling.
            if (m_AcsEnabled)
            {
                m_AcsCountersGood = true;
                m_AcsPollDivider = PROXISKY_ACS_POLL_DIVIDER;
            }

            return true;
        }

        if (AcsClearSP.isNameMatch(name))
        {
            if (!m_HasAcs)
                return reportUnavailable(AcsClearSP, "The anti-collision system is not available on this mount.");

            // update() has already sent its own IDSetSwitch on rejection; adding a second message
            // here would only contradict it.
            if (!AcsClearSP.update(states, names, n))
                return true;

            // Clearing the counters is destructive and irreversible, so act only on an explicit
            // ON. ISR_ATMOST1 permits a client to send OFF - ":CLEAR=Off" from indi_setprop, or
            // an untoggle in a client that mirrors switch state - and that must not fire it.
            if (AcsClearSP[0].getState() != ISS_ON)
            {
                AcsClearSP.reset();
                AcsClearSP.setState(IPS_IDLE);
                AcsClearSP.apply();
                return true;
            }

            AcsClearSP.setState(IPS_BUSY);
            AcsClearSP.apply();

            if (!sendVendorSetter(":Pnccc#"))
            {
                AcsClearSP.reset();
                AcsClearSP.setState(IPS_ALERT);
                AcsClearSP.apply("The mount did not accept the counter clear.");
                return true;
            }

            // Momentary action - the button never stays latched. Whether the clear worked is
            // reported by the counters themselves, so re-read them regardless of the sticky
            // polling flag.
            AcsClearSP.reset();
            m_AcsCountersGood = true;

            // Not sticky: the user just asked for this, so a single failure should not disable
            // polling for the session (and would undo the line above).
            if (refreshAcsCounters(false))
            {
                AcsClearSP.setState(IPS_OK);
                LOG_INFO("Collision counters cleared.");
            }
            else
            {
                AcsClearSP.setState(IPS_ALERT);
            }

            AcsClearSP.apply();
            return true;
        }

    }

    return LX200_OnStep::ISNewSwitch(dev, name, states, names, n);
}
