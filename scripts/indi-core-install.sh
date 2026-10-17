#!/bin/bash

set -e

pushd build/indi-core
if [[ -f build.ninja ]]; then
    $(command -v sudo) cmake --install .
else
    $(command -v sudo) make install
fi
popd
