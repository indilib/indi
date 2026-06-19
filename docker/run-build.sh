#!/bin/bash

if [ .$1 == '.' ] ; then 
    export REPO=indilib 
else 
    export REPO=$1 
fi 

if [ .$2 == '.' ] ; then 
    export CIRCLE_BRANCH=master ; 
else 
    export CIRCLE_BRANCH=$2 ; 
fi 

# Use shallow clone to fetch only the latest commit of the specified branch
# This is much faster and uses significantly less bandwidth/disk space
git clone --depth 1 --single-branch --branch $CIRCLE_BRANCH https://github.com/$REPO/indi.git 
cd indi 

./.circleci/build-all.sh
./.circleci/run-tests.sh
