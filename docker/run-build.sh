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

git clone https://github.com/$REPO/indi.git 
cd indi 
git checkout $CIRCLE_BRANCH

./.circleci/build-all.sh
./.circleci/run-tests.sh
