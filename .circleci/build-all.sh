#!/bin/bash

set -x -e

br=( master develop package pull )

build_all () {
    .circleci/build-core.sh
    .circleci/build-libs.sh    
}
