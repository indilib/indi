#!/bin/bash

INSTALL_DIR=$(readlink -f $1)

if [ -f /usr/bin/indi_lx200_10micron ] && [ ! -f /usr/bin/indi_lx200_10micron.bak ]; then
    sudo mv /usr/bin/indi_lx200_10micron /usr/bin/indi_lx200_10micron.bak
    sudo ln -s $INSTALL_DIR/bin/indi_lx200_10micron /usr/bin/indi_lx200_10micron
fi