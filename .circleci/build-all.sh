#!/bin/bash

set -x -e

br=( master develop package travis )

build_all () {
    ./build-core.sh
    ./build-libs.sh
    ./build-3rdparty.sh
}

set

if [ .${CIRCLE_BRANCH%_*} == '.drv' ] ; then
    # Driver build branch
    build_all
else
    for i in "${br[@]}"
    do
       if [ ${CIRCLE_BRANCH} == $i ]; then
        build_all
       fi
    done
fi





