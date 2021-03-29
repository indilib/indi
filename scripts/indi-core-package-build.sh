#!/bin/bash

set -e

mkdir -p packages/indi-core-package
pushd build/indi-core
make DESTDIR=../../packages/indi-core-package install
popd

pushd packages
command -v bsdtar >/dev/null 2>&1 && \
    bsdtar --gid 0 --uid 0   -cvf indi-core-package.tar indi-core-package || \
    tar --{owner,group}=root -cvf indi-core-package.tar indi-core-package
popd
