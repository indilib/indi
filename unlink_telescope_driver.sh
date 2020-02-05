#!/bin/bash

if [ -f /usr/bin/indi_lx200_10micron ] && [ -f /usr/bin/indi_lx200_10micron.bak ]; then
    sudo rm /usr/bin/indi_lx200_10micron
    sudo mv /usr/bin/indi_lx200_10micron.bak /usr/bin/indi_lx200_10micron
fi