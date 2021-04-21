#!/bin/bash

set -e

pushd build/indi-core/test
ctest -V
popd
