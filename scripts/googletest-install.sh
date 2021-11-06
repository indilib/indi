#!/bin/bash

set -e

pushd build/googletest
$(command -v sudo) make install
popd
