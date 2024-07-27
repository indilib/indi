#!/bin/bash

set -e

pushd build/indi-core/integs
ctest -V --output-on-failure
popd

pushd build/indi-core/test
ctest -V
popd

