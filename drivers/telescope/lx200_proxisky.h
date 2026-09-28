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

#pragma once

#include "lx200_OnStep.h"

class LX200_Proxisky : public LX200_OnStep
{
    public:
        LX200_Proxisky();
        ~LX200_Proxisky() override = default;

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool ReadScopeStatus() override;

        // Thin wrappers over the comms helpers inherited from LX200_OnStep. The vendor
        // protocol is line based and '#' terminated, same as OnStep, so no new serial
        // code is needed here.
        // requireTerminated rejects a timed-out partial read. On for the numeric parsers, where a
        // truncated value is indistinguishable from a real one; off elsewhere, purely as
        // tolerance. Note that off-by-default is not evidence of anything: every one of the ~30
        // vendor commands measured on a UMi20S 1.0.6 answered with the trailing '#'.
        // rcOut receives the raw return code from the inherited read helper. Only probeModel()
        // needs it - that probe gates every other vendor property, so when it fails it has to
        // say why without the user having to enable debug logging first.
        // getVendorString() adds the probe-budget check and, while the capability probes are
        // running, a single retry; readVendorStringOnce() is the read itself. Callers want the
        // former - the split exists so the retry has something to call twice.
        bool getVendorString(const char *cmd, char *out, size_t outLen, bool requireTerminated = false,
                             int *rcOut = nullptr);
        bool readVendorStringOnce(const char *cmd, char *out, size_t outLen, bool requireTerminated,
                                  int *rcOut);
        bool getVendorBool(const char *cmd, bool &value);
        bool getVendorPair(const char *cmd, double &first, double &second);
        bool getVendorQuad(const char *cmd, double *out);
        // Writes a setter and consumes its acknowledgement ("1#" accepted, "0#" refused).
        // Returns false on a refusal or on no answer at all.
        bool sendVendorSetter(const char *cmd);

        // Capability probes, run once per connection from updateProperties().
        void probeModel();
        void probeRALimits();
        void probeDecLimits();
        void probeLed();
        void probeAcs();
        void probeSettings();
        void probeSuperGoto();

        // Runs every capability probe. Called only from updateProperties(), i.e. during connect,
        // where a second or two of blocking is expected - never from the status loop.
        void runVendorProbes();

        // True once the probe sequence has spent its wall-clock budget. Every probe checks this
        // on entry, so a mount that stops answering part way costs one timeout rather than one
        // per remaining feature. Warns once, then stays quiet.
        bool probeBudgetExpired();

        bool probePidAxis(INDI::PropertyNumber &np, const char *readCmd, const char *label);

        // Range-checks one candidate element value on the way in, against the bounds declared for
        // that element in initProperties() rather than against a literal repeated at the call site.
        // On failure it flags the vector and names the field; the caller only has to stop.
        bool rangeCheck(INDI::PropertyNumber &np, int index, double value, const char *what,
                        const char *unit = "");

        // Publishes mount-reported values and reports whether they all sit inside the elements'
        // declared min/max. INDI enforces those bounds inbound (IUUpdateNumber) but not outbound,
        // so a mount configured through the vendor tool can hand us a value the client cannot
        // send back. We publish it regardless - hiding what the hardware actually holds would be
        // worse - and flag the vector so the discrepancy is visible rather than silently trusted.
        bool setReportedValues(INDI::PropertyNumber &np, const double *values, int count);

        // Writes the four gains for one axis, commits with ":Pnas<axis>r#", then verifies they
        // were stored. Refused unless the mount is idle, because the commit restarts the axis.
        bool handlePidWrite(INDI::PropertyNumber &np, const char *axisPrefix, const char *readCmd,
                            const char *label, double values[], char *names[], int n);
        bool refreshAcsCounters(bool stickyOnFailure = true);

        // Reads a boolean feature and pre-loads its switch. Returns false when the mount does
        // not answer, which is how every optional feature here is detected.
        bool probeBoolSwitch(INDI::PropertySwitch &sp, const char *readCmd, const char *label);

        // Write-then-verify for a setting that applies immediately. Sets IPS_OK on a matching
        // readback, IPS_ALERT otherwise. The setter's own "1#" only says the mount parsed the
        // command; the readback is what says the value stuck. On a refusal the switch is put back
        // to what the mount still reports rather than left showing the change that did not happen.
        bool handleBoolSwitch(INDI::PropertySwitch &sp, const char *setOn, const char *setOff,
                              const char *readCmd, const char *label,
                              ISState *states, char *names[], int n, bool *mirror = nullptr);

        // Same, for a setting that only takes effect after a power cycle. The switch ends IPS_OK
        // showing the requested state - the write itself is complete, and that is what the state
        // describes; that the mount is still running the old value is said in the message.
        bool handlePendingSwitch(INDI::PropertySwitch &sp, const char *setOn, const char *setOff,
                                 const char *readCmd, const char *label,
                                 ISState *states, char *names[], int n);

        // Publishes whatever the probes found. Safe to call more than once.
        void defineVendorProperties();
        void deleteVendorProperties();

        // Re-read after a write, and publish whatever comes back. Called even when a setter was
        // refused: these pairs are written one command at a time, so only the readback can say
        // which half of the pair the mount actually took.
        bool refreshRALimits(double expectedLeft, double expectedRight);
        bool refreshDecLimits(double expectedCCW, double expectedCW);
        bool refreshAcsThresholds(double expectedRA, double expectedDec);
        bool refreshSuperGotoTolerance(double expected);

    private:
        bool m_HasModel     = false;    // :Pbvg# answered with a model string

        // Probing happens once, during connect. The parent re-runs updateProperties() on detecting
        // PEC or pier side, so this guards against a second full pass landing in the status loop.
        bool m_ProbesDone        = false;
        bool m_PropertiesDefined = false;

        // Monotonic deadline for the probe sequence; 0 until runVendorProbes() sets it.
        double m_ProbeDeadline    = 0;
        bool   m_ProbeBudgetWarned = false;

        bool m_HasRALimits  = false;
        bool m_HasDecLimits = false;
        bool m_HasLed        = false;
        bool m_HasAcs        = false;
        bool m_HasPowerLoss  = false;
        bool m_HasAsiair     = false;
        bool m_HasSuperHome  = false;
        bool m_HasDecSecHome = false;
        bool m_HasAutoTrack  = false;
        // Two flags, not one: ":Pndgo#" (the enable switch) and ":Pndgv#" (the tolerance) are
        // separate reads, and a mount that answers the first but not the second should still get
        // its enable switch published.
        bool m_HasSuperGoto          = false;
        bool m_HasSuperGotoTolerance = false;
        bool m_HasRaPid      = false;
        bool m_HasDecPid     = false;

        // ACS collision counters are polled. m_AcsCountersGood is a sticky negative cache in the
        // style of LX200_OnStep's OSCpuTemp_good: one failure stops us asking again for the rest
        // of the session, so a mount that does not answer cannot stall every status update.
        //
        // m_AcsEnabled is written at probe time and by handleBoolSwitch(), and deliberately not
        // re-read from the status loop: the firmware never disables anti-collision on its own, not
        // even after a collision, so the only thing that can change it is a write from this driver.
        bool m_AcsEnabled      = false;
        bool m_AcsCountersGood = true;
        int  m_AcsPollDivider  = 0;


        INDI::PropertyText   ModelTP           {1};
        INDI::PropertySwitch RALimitEnableSP   {2};
        INDI::PropertyNumber RALimitNP         {2};
        INDI::PropertySwitch DecLimitEnableSP  {2};
        INDI::PropertyNumber DecLimitNP        {2};
        INDI::PropertySwitch LedSP             {2};
        INDI::PropertySwitch AcsEnableSP       {2};
        INDI::PropertyNumber AcsThresholdNP    {2};
        INDI::PropertyNumber AcsCollisionNP    {2};
        INDI::PropertySwitch AcsClearSP        {1};
        INDI::PropertySwitch PowerLossSP       {2};
        INDI::PropertySwitch AsiairSP          {2};
        INDI::PropertySwitch SuperHomeSP       {2};
        INDI::PropertySwitch DecSecHomeSP      {2};
        INDI::PropertySwitch AutoTrackSP       {2};
        INDI::PropertySwitch SuperGotoSP       {2};
        INDI::PropertyNumber SuperGotoToleranceNP {1};
        INDI::PropertyNumber RaPidNP           {4};
        INDI::PropertyNumber DecPidNP          {4};
};
