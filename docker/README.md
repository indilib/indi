Docker images for build environment
===================
![Docker Build Status](https://img.shields.io/docker/build/jochym/indi-docker.svg?label=Docker+images)

This directory contains Dockerfiles for building environments on various linux distributions

The naming convention: `distribution_release` 

The first part may be non existent - like in the case of fedora, centos, opensuse.
The images are hosted on dockerhub jochym/indi-release. This will be migrated to the 
indilib account in the future.
The images are used in CircleCI build tests and could be also used for local testing.
You can run the full build using these images if you have docker installed on your machine.
To run full build of main repository master branch on centos:

	docker run --rm -it jochym/indi-centos ./run-build.sh indilib master

If you have your own repository on github: `user/indi` with branch `drv_name-of-your-driver` you 
can run just the driver compilation against current upstream build by running:

	docker run --rm -it jochym/indi-bionic ./run-build.sh user drv_name-of-your-driver

This works only for ubuntu images (artful, bionic at present).

The build-dockers script may be used to locally rebuild all the images in the directory. 
It creates locally images with name equal to the last part of the directory name. This is mainly
used for testing of the images. For build testing it is better to use pre-built images from dockerhub.

The images may be used for manual interaction/build/modification. You can enter running container and
modify it in any way you like by:

	docker run --rm -it jochym/indi-bionic bash

The above container will be removed automatically when you end its session (due to the --rm argument). 
For further info refer to the docker documentation.
