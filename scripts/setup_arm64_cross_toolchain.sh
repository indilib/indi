#!/usr/bin/env bash
# scripts/setup_arm64_cross_toolchain.sh — one-time bootstrap for cross-compiling INDI
# to aarch64 on a faster x86_64 machine, then finishing the build locally via distcc.
#
# This is a SEPARATE, one-time (or rare) setup step from scripts/distcc_build.py, which
# handles every actual build once this is done. Re-run this script any time the remote
# host is reimaged, or with --force to redo it anyway; it's idempotent otherwise.
#
# ── Why this exists ──────────────────────────────────────────────────────────────────
# distcc dispatches a compile job to a remote host by NAME: whatever compiler the local
# build invokes (e.g. "aarch64-unknown-linux-gnu-g++"), the remote's distccd looks for a
# program by that exact name in its own PATH and runs it. For that to produce correct
# aarch64 object code on an x86_64 host, the remote needs a REAL aarch64 cross-compiler
# installed under that name — a plain x86_64 g++ answering to the same name would silently
# produce wrong-architecture objects. This script installs Arch Linux ARM's own official,
# version-matched cross-toolchain (not a hand-built one, and not the AUR — there is no
# ready-made aarch64-linux-gnu-gcc package there; building one from source via crosstool-ng
# is realistically a multi-hour undertaking, and unnecessary since ALARM already publishes
# a prebuilt one matched to what's actually running on the Pi/StellarMate box).
#
# Local side: distcc's local preprocessing step must also invoke a compiler by that same
# name, so this script symlinks the LOCAL machine's own native compiler under the
# aarch64-unknown-linux-gnu-* names too (harmless — same binary, just reachable under an
# extra name that CMake can be pointed at).
#
# ── Requirements ─────────────────────────────────────────────────────────────────────
#   - Local machine: aarch64 (the Pi/CM4/StellarMate box itself), passwordless sudo,
#     `distcc` installed (installed automatically below via
#     /etc/stellarmate/pacman-build.conf if missing).
#   - Remote machine: x86_64, Arch Linux, reachable over SSH with a working SSH key
#     (see ~/.ssh/config for a "workshop"-style host alias), passwordless sudo, `distcc`
#     installed (installed automatically below if missing).
#   - ~120MB free on the remote for the toolchain download+extract.
#
# ── Usage ────────────────────────────────────────────────────────────────────────────
#   scripts/setup_arm64_cross_toolchain.sh --host workshop
#   scripts/setup_arm64_cross_toolchain.sh --host workshop --force   # redo even if present
#
# ── After this succeeds, the recompile workflow is ─────────────────────────────────────
#   1. One-time per build directory (a FRESH directory -- changing the compiler on an
#      already-configured build dir in place is unreliable; CMake partially re-detects
#      and gets confused. Wipe and reconfigure instead):
#
#        mkdir -p ~/Projects/build/indi && cd ~/Projects/build/indi
#        cmake -G Ninja \
#          -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug \
#          -DCCACHE_SUPPORT=ON \
#          -DCMAKE_C_COMPILER=aarch64-unknown-linux-gnu-gcc \
#          -DCMAKE_CXX_COMPILER=aarch64-unknown-linux-gnu-g++ \
#          -DCMAKE_JOB_POOLS="link=1" -DCMAKE_JOB_POOL_LINK=link \
#          ~/Projects/indi
#
#      (The link job pool matters: linking is the one step distcc can't offload -- it
#      always happens locally -- and it's the single biggest memory spike in the whole
#      build. Capping it to 1 concurrent link keeps a modest local machine from getting
#      squeezed by an oversized -j during that step. This matters more for indiserver and
#      the larger drivers than for a small standalone driver target.)
#
#   2. Every build after that:
#
#        scripts/distcc_build.py --host workshop --build-dir ~/Projects/build/indi \
#          --slots 24 --jobs 6
#
#      --slots is the REMOTE core count (distcc's dispatch capacity). --jobs is capped to
#      roughly the LOCAL machine's own core count, not the remote's -- ninja schedules
#      -j jobs total, and the final link is inherently local-only regardless of distcc.
#      Sizing -j from the remote's core count oversubscribes the local machine badly
#      (verified on KStars: -j24 on a 4-core Pi spiked local load to ~10 with the remote
#      sitting idle, before any real compiling had even started). Once real compiles
#      begin, each local ninja job is mostly waiting on the network, not burning a full
#      local core, so this is a safe, conservative number, not an efficiency compromise --
#      distcc's remote-side capacity is unaffected by it.
#
#   3. To install what you built: sudo cmake --install ~/Projects/build/indi
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

TOOLCHAIN_URL="http://archlinuxarm.org/builder/xtools/x-tools8.tar.xz"
TOOLCHAIN_MD5="82d77f1a3e04bf83d0b9259e1eab5d4d"
TRIPLE="aarch64-unknown-linux-gnu"
TOOLS=(gcc g++ cc c++ cpp)
EXTRA_BINUTILS=(ar ranlib nm strip objcopy objdump readelf)

HOST=""
FORCE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) HOST="$2"; shift 2 ;;
    --force) FORCE=1; shift ;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; exit 1 ;;
  esac
done

if [[ -z "$HOST" ]]; then
  echo "ERROR: --host is required (e.g. --host workshop)." >&2
  exit 1
fi

log() { echo "[setup_arm64_cross_toolchain] $*"; }

# ── Local side: same-named compiler symlinks, so distcc's local preprocessing step and
#    the remote's actual compilation step agree on which "compiler" is being invoked. ──
log "Local machine: linking native compiler under ${TRIPLE}-* names in /usr/local/bin..."
for f in "${TOOLS[@]}"; do
  target="/usr/local/bin/${TRIPLE}-${f}"
  if [[ -L "$target" && $FORCE -eq 0 ]]; then
    continue
  fi
  sudo ln -sf "/usr/bin/${f}" "$target"
done

if ! command -v distcc >/dev/null 2>&1; then
  log "distcc not found locally -- installing via pacman-build.conf..."
  if [[ -f /etc/stellarmate/pacman-build.conf ]]; then
    sudo pacman --config /etc/stellarmate/pacman-build.conf -Sy --needed --noconfirm distcc
  else
    echo "ERROR: distcc missing and /etc/stellarmate/pacman-build.conf not found -- install distcc manually." >&2
    exit 1
  fi
fi

# ── Remote side: real cross-toolchain + distccd masquerade whitelist ─────────────────
# NOTE: these are sent via `ssh host bash -s` with the script piped over stdin, NOT as
# `ssh host VAR=val '...'` -- ssh joins trailing argv elements with spaces and the remote
# shell re-tokenizes that joined string, which silently destroys quoting on any VAR whose
# value itself contains spaces (e.g. a list of tool names). Piping over stdin avoids that
# entirely. Local variables are substituted here (heredoc delimiter is unquoted on purpose).
log "Remote ($HOST): checking distcc..."
ssh -o BatchMode=yes -o ConnectTimeout=5 "$HOST" bash -s <<EOF
  set -euo pipefail
  if ! command -v distccd >/dev/null 2>&1; then
    echo "[remote] distcc not found -- installing..."
    sudo pacman -Sy --needed --noconfirm distcc
  fi
EOF

log "Remote ($HOST): fetching official Arch Linux ARM cross-toolchain (matched to the Pi's own GCC) if not already present..."
ssh -o BatchMode=yes "$HOST" bash -s <<EOF
  set -euo pipefail
  if [[ -x ~/x-tools8/${TRIPLE}/bin/${TRIPLE}-gcc && "${FORCE}" -eq 0 ]]; then
    echo "[remote] toolchain already present at ~/x-tools8, skipping download (use --force to redo)."
    exit 0
  fi
  cd ~
  echo "[remote] downloading ${TOOLCHAIN_URL} ..."
  wget -q -O x-tools8.tar.xz "${TOOLCHAIN_URL}"
  actual_md5=\$(md5sum x-tools8.tar.xz | cut -d" " -f1)
  if [[ "\$actual_md5" != "${TOOLCHAIN_MD5}" ]]; then
    echo "[remote] ERROR: md5 mismatch (got \$actual_md5, expected ${TOOLCHAIN_MD5}) -- refusing to extract." >&2
    exit 1
  fi
  echo "[remote] checksum OK, extracting..."
  tar xf x-tools8.tar.xz -C ~
EOF

log "Remote ($HOST): wiring up distccd masquerade whitelist and PATH symlinks..."
ssh -o BatchMode=yes "$HOST" bash -s <<EOF
  set -euo pipefail
  # distccd only executes compilers whose name it finds as a symlink to /usr/bin/distcc
  # in /usr/lib/distcc -- this is a security whitelist, independent of PATH resolution.
  for f in ${TOOLS[@]}; do
    sudo ln -sf /usr/bin/distcc "/usr/lib/distcc/${TRIPLE}-\$f"
  done
  # Actual resolution when distccd executes a whitelisted name: point it at the real
  # cross-compiler. Using /usr/local/bin (not a shell rc file) because non-interactive
  # SSH-invoked distccd sessions do not reliably source .bashrc/.profile.
  for f in ${TOOLS[@]} ${EXTRA_BINUTILS[@]}; do
    sudo ln -sf ~/x-tools8/${TRIPLE}/bin/${TRIPLE}-\$f /usr/local/bin/${TRIPLE}-\$f
  done
EOF

# ── End-to-end verification: a real compile, dispatched, round-tripped ────────────────
log "Verifying end-to-end: local preprocess -> dispatch to $HOST -> aarch64 object returned..."
tmpdir=$(mktemp -d)
echo 'int main(){return 0;}' > "$tmpdir/t.cpp"
slots=$(ssh -o BatchMode=yes -o ConnectTimeout=5 "$HOST" nproc)
if DISTCC_HOSTS="@${HOST}/${slots}" distcc "${TRIPLE}-g++" -c "$tmpdir/t.cpp" -o "$tmpdir/t.o" 2>"$tmpdir/err.log"; then
  filetype=$(file -b "$tmpdir/t.o")
  if [[ "$filetype" == *"ARM aarch64"* ]]; then
    log "OK: verified -- $HOST compiled a genuine ARM aarch64 object for us."
  else
    echo "ERROR: compile succeeded but produced unexpected output: $filetype" >&2
    rm -rf "$tmpdir"; exit 1
  fi
else
  echo "ERROR: end-to-end verification compile failed:" >&2
  cat "$tmpdir/err.log" >&2
  rm -rf "$tmpdir"
  exit 1
fi
rm -rf "$tmpdir"

log "Done. See this script's header comment for the configure + build recipe."
