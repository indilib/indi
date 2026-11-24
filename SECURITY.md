# Security Policy

INDI drives real astronomical hardware — mounts, focusers, domes, cameras, power
units — often unattended overnight. A vulnerability here can mean more than a
crash: it can mean equipment moves, loses power, or is left in an unsafe state.
We treat security reports accordingly.

## Supported versions

Only the most recently tagged release of INDI Core receives security fixes.
There are no long-term-support branches; please upgrade to the latest release
to get a fix.

## Reporting a vulnerability

**Please do not open a public GitHub issue or discuss the vulnerability in a
public pull request, forum post, or chat.**

Report it privately using GitHub's private vulnerability reporting:

1. Go to <https://github.com/indilib/indi/security>.
2. Click **"Report a vulnerability"**.
3. Describe the affected code, how to reproduce it, and the potential impact.
   Evidence from source review is welcome, but please say plainly whether it
   was compiled, run, or tested, and on what.

This opens a private GitHub Security Advisory visible only to you and the
maintainers, with its own discussion thread.

If the report is specific to a driver that lives in
[indilib/indi-3rdparty](https://github.com/indilib/indi-3rdparty) (anything
needing a vendor SDK), please report it there instead using the same process;
let us know here too if you're not sure which repository it belongs to.

## What to expect

- **Acknowledgement.** We aim to acknowledge a new report within a few days.
- **Triage and fix.** If confirmed, we develop and test a fix privately
  against the draft advisory, following the same testing bar as any other
  change (see [AGENTS.md](AGENTS.md) §5) — automated tests at minimum, and a
  hardware test where the fix touches device communication.
- **Coordinated disclosure.** We keep the advisory private while the fix is
  prepared, then time its public disclosure — and the CVE it's assigned —
  to land together with the next tagged release that contains the fix, so a
  patched build is available at or before the moment details become public.
  For an actively exploited or unusually severe issue we may disclose sooner,
  independent of the release schedule.
- **Credit.** Reporters are credited by name or handle in the published
  advisory and in the commit that fixes the issue, unless you ask to stay
  anonymous.

## Scope

This policy covers INDI Core: `indiserver`, the driver framework in `libs/`,
and the drivers under `drivers/`. Given what this software controls, a report
doesn't need to be a classic memory-safety bug to matter — anything that lets
an unauthenticated network client, a malicious device reply, or a crafted
config file move, power, or otherwise take unintended control of hardware is
in scope and treated as high priority.
