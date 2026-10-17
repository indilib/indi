#!/bin/bash

set -e

pushd build/indi-core
if [[ -f build.ninja ]]; then
    $(command -v sudo) cmake --install .
else
    $(command -v sudo) make install
fi
popd

# Re-sign macOS binaries after installation (required to load rpath-resolved dylibs)
if [[ "$(uname -s)" == "Darwin" ]]; then
    BIN_DIR="/usr/local/bin"
    echo "Re-signing macOS INDI binaries..."
    for drv in "$BIN_DIR"/indiserver "$BIN_DIR"/indi_* "$BIN_DIR"/asi_*; do
        if [[ -f "$drv" ]] && [[ -x "$drv" ]]; then
            codesign --force --sign - "$drv" 2>/dev/null || true
        fi
    done
    echo "Re-signing done."
fi
