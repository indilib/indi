#!/bin/bash

set -e

pushd build/indi-core/integs
ctest -v --output-on-failure
popd

pushd build/indi-core/test
ctest -V
popd

