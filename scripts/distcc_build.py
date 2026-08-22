#!/usr/bin/env python3
"""
Build INDI (or any ninja/make project) offloading compilation to a remote
distcc host over SSH.

This wraps two things people otherwise get wrong by hand:

  1. DISTCC_HOSTS must be set to "@<host>/<slots>" (the leading "@" means
     "dispatch via ssh"). Omitting the "@" silently falls back to a plain
     TCP connection to distccd on port 3632, which usually isn't listening.
  2. The parallelism distcc actually achieves is capped by whatever -j value
     you pass to ninja/make -- distcc itself does not create parallelism, it
     only lets each already-parallel job land on a different machine. Ninja's
     default -j is derived from the *local* CPU count (local-cores + 2), so
     running `ninja` with no -j on a 4-core laptop offloading to a 16+ core
     remote box still only ever has ~6 compiles in flight -- the remote
     machine's extra cores just sit idle. This script fixes that by sizing
     -j from the remote host's actual `nproc`, not the local one.

Assumes distcc (and, for the ssh-pump mode used here, an sshd + distccd
reachable via `ssh <host> distccd --inetd`) is already installed on both the
local machine and the remote host -- this script does not install anything,
it only wires up the environment and job count correctly for a single build.

INDI-specific prerequisite: cmake_modules/CMakeCommon.cmake only sets
RULE_LAUNCH_COMPILE/RULE_LAUNCH_LINK to ccache when the CCACHE_SUPPORT cache
variable is ON (default OFF). This script's CCACHE_PREFIX=distcc trick (see
below) has nothing to hook into unless the build was configured with
-DCCACHE_SUPPORT=ON, e.g.:
    cmake -B build -DCCACHE_SUPPORT=ON ...
If your existing build/ has CCACHE_SUPPORT=OFF, re-run cmake once with that
flag (no need to wipe the build dir) before using this script.

Usage:
    scripts/distcc_build.py --host workshop
    scripts/distcc_build.py --host 192.168.1.50 --build-dir ../build indi_celestron_dewpower
    scripts/distcc_build.py --host workshop --jobs 24 --slots 20 --dry-run

Cross-architecture (e.g. building on an aarch64 Pi, offloading to a faster x86_64 box):
    this script needs no changes for that -- distcc dispatches by whatever compiler NAME
    the local build invokes, so as long as the remote answers to that same name with a
    real cross-compiler, this works unmodified. --jobs should then be sized from the
    LOCAL machine's core count, not --slots/the remote's -- CMake codegen steps and the
    final link are always local regardless of distcc, and oversubscribing a weak local
    machine during those is the main way this goes wrong.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BUILD_DIR = REPO_ROOT / "build"


def parse_args():
    p = argparse.ArgumentParser(
        description="Build with compilation offloaded to a remote distcc host over SSH.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("Usage:")[1] and "Usage:" + __doc__.split("Usage:")[1],
    )
    p.add_argument("--host", "-H", required=True,
                   help="Remote hostname or IP with distcc/sshd already running.")
    p.add_argument("--build-dir", "-C", type=Path, default=DEFAULT_BUILD_DIR,
                   help=f"Build directory containing build.ninja or Makefile (default: {DEFAULT_BUILD_DIR}).")
    p.add_argument("--slots", type=int, default=None,
                   help="Max concurrent distcc jobs on the remote host (DISTCC_HOSTS '@host/N'). "
                        "Default: the remote host's own `nproc`.")
    p.add_argument("--jobs", "-j", type=int, default=None,
                   help="Parallelism passed to ninja/make -j. Default: --slots plus --local-jobs -- "
                        "this is the number that actually controls how many compiles run at once; "
                        "distcc only decides *where* each one runs. If this is left at just --slots "
                        "while --local-jobs is also set, the local slots never get used: ninja only "
                        "ever launches enough jobs to fill the remote pool.")
    p.add_argument("--local-jobs", type=int, default=0,
                   help="Also allow this many jobs to compile on the local machine concurrently "
                        "with the remote ones (adds a bare 'localhost/N' entry to DISTCC_HOSTS). "
                        "Default 0: fully offload, no local compiles.")
    p.add_argument("--ssh-user", default=None,
                   help="SSH user for the remote host, if not the same as the local user "
                        "(passed as '@user@host/slots' in DISTCC_HOSTS).")
    p.add_argument("--skip-preflight", action="store_true",
                   help="Skip the SSH reachability/distccd check before building.")
    p.add_argument("--verbose", action="store_true",
                   help="Set DISTCC_VERBOSE=1 so distcc logs where each job actually ran -- "
                        "useful for confirming jobs are landing on the remote host at all.")
    p.add_argument("--dry-run", action="store_true",
                   help="Print the environment and command that would run, without building.")
    p.add_argument("targets", nargs="*",
                   help="Specific ninja/make targets to build (default: build everything).")
    return p.parse_args()


def ssh_target(args):
    return f"{args.ssh_user}@{args.host}" if args.ssh_user else args.host


def remote_nproc(args):
    """SSH to the host and return its core count, or None if unreachable."""
    try:
        result = subprocess.run(
            ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5", ssh_target(args), "nproc"],
            capture_output=True, text=True, timeout=15,
        )
    except (subprocess.SubprocessError, OSError) as e:
        print(f"[distcc_build] ERROR: could not SSH to {args.host}: {e}", file=sys.stderr)
        return None
    if result.returncode != 0:
        print(f"[distcc_build] ERROR: `ssh {ssh_target(args)} nproc` failed:\n{result.stderr}",
              file=sys.stderr)
        return None
    try:
        return int(result.stdout.strip())
    except ValueError:
        print(f"[distcc_build] ERROR: unexpected `nproc` output from {args.host}: {result.stdout!r}",
              file=sys.stderr)
        return None


def preflight(args):
    """Confirm the remote host is reachable and has distccd installed."""
    print(f"[distcc_build] Checking {args.host} over SSH...")
    try:
        result = subprocess.run(
            ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5", ssh_target(args),
             "command -v distccd"],
            capture_output=True, text=True, timeout=15,
        )
    except (subprocess.SubprocessError, OSError) as e:
        sys.exit(f"[distcc_build] ERROR: could not SSH to {args.host}: {e}")
    if result.returncode != 0 or not result.stdout.strip():
        sys.exit(
            f"[distcc_build] ERROR: distccd not found on {args.host} (or SSH key auth isn't set "
            f"up). This script assumes distcc is already installed on both machines -- install "
            f"it there first, or pass --skip-preflight if you know better."
        )
    print(f"[distcc_build] OK: distccd found at {result.stdout.strip()}")


def check_ccache_support(build_dir: Path):
    """Warn if the build was configured without CCACHE_SUPPORT=ON, since then
    RULE_LAUNCH_COMPILE never points at ccache and CCACHE_PREFIX=distcc below
    has nothing to hook into -- every compile will silently run locally."""
    cache_file = build_dir / "CMakeCache.txt"
    if not cache_file.is_file():
        return
    for line in cache_file.read_text(errors="replace").splitlines():
        if line.startswith("CCACHE_SUPPORT:BOOL="):
            value = line.split("=", 1)[1].strip().upper()
            if value not in ("ON", "TRUE", "1", "YES"):
                print(
                    f"[distcc_build] WARNING: {cache_file} has CCACHE_SUPPORT={value}. "
                    f"cmake_modules/CMakeCommon.cmake only wires RULE_LAUNCH_COMPILE to ccache "
                    f"when this is ON, so CCACHE_PREFIX=distcc (below) will have nothing to act "
                    f"on and every compile will run locally. Re-run cmake once with "
                    f"-DCCACHE_SUPPORT=ON to fix this.",
                    file=sys.stderr,
                )
            return


def detect_build_tool(build_dir: Path):
    if (build_dir / "build.ninja").is_file():
        if not shutil.which("ninja"):
            sys.exit("[distcc_build] ERROR: build.ninja present but `ninja` is not on PATH.")
        return "ninja"
    if (build_dir / "Makefile").is_file():
        return "make"
    sys.exit(f"[distcc_build] ERROR: no build.ninja or Makefile in {build_dir}. "
             f"Configure the build first (cmake -B {build_dir} ...).")


def main():
    args = parse_args()

    if not args.build_dir.is_dir():
        sys.exit(f"[distcc_build] ERROR: build dir {args.build_dir} does not exist.")

    check_ccache_support(args.build_dir)

    slots = args.slots
    if slots is None:
        if args.skip_preflight:
            sys.exit("[distcc_build] ERROR: --slots is required when --skip-preflight is set "
                     "(otherwise there's no way to size it without asking the remote host).")
        n = remote_nproc(args)
        if n is None:
            sys.exit(f"[distcc_build] ERROR: could not determine {args.host}'s core count. "
                     f"Pass --slots explicitly to skip this check.")
        slots = n
        print(f"[distcc_build] {args.host} reports {slots} cores -- using that as the distcc slot count.")

    jobs = args.jobs if args.jobs is not None else slots + args.local_jobs

    if not args.skip_preflight:
        preflight(args)

    host_spec = f"{ssh_target(args)}/{slots}"
    distcc_hosts = f"@{host_spec}"
    if args.local_jobs > 0:
        distcc_hosts = f"localhost/{args.local_jobs} {distcc_hosts}"

    build_tool = detect_build_tool(args.build_dir)
    cmd = [build_tool, f"-j{jobs}"] + args.targets

    env = os.environ.copy()
    env["DISTCC_HOSTS"] = distcc_hosts
    # cmake_modules/CMakeCommon.cmake sets RULE_LAUNCH_COMPILE=ccache as a global
    # CMake property (only when configured with -DCCACHE_SUPPORT=ON), which bakes a
    # bare "ccache <compiler>" into every generated ninja rule and completely
    # overrides whatever CMAKE_CXX_COMPILER_LAUNCHER was configured with -- distcc
    # never appears in the compile command, so DISTCC_HOSTS alone has nothing to act
    # on and every compile silently runs locally. CCACHE_PREFIX is ccache's own hook
    # for exactly this: on a cache miss it runs "$CCACHE_PREFIX <real-compiler> ..."
    # instead of the compiler directly, which works regardless of how the launcher
    # was wired into the ninja rule. Confirm jobs are actually landing remotely with
    # `ssh <host> 'pgrep -ac distccd'` during a build if in doubt.
    env["CCACHE_PREFIX"] = "distcc"
    if args.verbose:
        env["DISTCC_VERBOSE"] = "1"

    print(f"[distcc_build] DISTCC_HOSTS={distcc_hosts}")
    print(f"[distcc_build] CCACHE_PREFIX={env['CCACHE_PREFIX']}")
    print(f"[distcc_build] Running: {' '.join(cmd)}  (cwd={args.build_dir})")

    if args.dry_run:
        print("[distcc_build] --dry-run: not actually building.")
        return 0

    result = subprocess.run(cmd, cwd=args.build_dir, env=env)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
