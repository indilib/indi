#!/bin/bash

set -e

pushd build/indi-core
$(command -v sudo) make install
popd
