#!/bin/bash

set -x -e

br=( master develop package travis pull )

build_all () {
    .circleci/build-core.sh
    .circleci/build-libs.sh
    .circleci/build-3rdparty.sh
}


if [ .${CIRCLE_BRANCH%_*} == '.drv' -a `lsb_release -si` == 'Ubuntu' ] ; then
    # Driver build branch
    build_all
else
    for i in "${br[@]}"
    do
       if [ .${CIRCLE_BRANCH%/*} == .$i ]; then
        build_all
       fi
    done
fi
