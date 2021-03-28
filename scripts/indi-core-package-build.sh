#!/bin/bash

set -e

mkdir -p packages/indi-core-package
pushd build/indi-core
make DESTDIR=../../packages/indi-core-package install
popd

pushd packages
tar -cvf indi-core-package.tar indi-core-package --{owner,group}=root
popd
