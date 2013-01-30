#!/bin/bash
#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013

source meteoconfig.py

killall meteoRRD_updater.py
killall meteoRRD_graph.py
killall meteoRRD_MaxMinAvg.py
killall sounding.py
if [ "$INDISERVER" = "localhost" ]
then
	killall indiserver
	mkfifo  /tmp/INDIFIFO
	indiserver -f /tmp/INDIFIFO &
	echo start indi_duino -n \"MeteoStation\" -s \"/usr/local/share/indi/meteostation_sk.xml\" >/tmp/INDIFIFO
fi
./meteoRRD_updater.py &
./meteoRRD_graph.py &
./sounding.py &
./meteoRRD_MaxMinAvg.py &

