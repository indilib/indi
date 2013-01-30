#!/bin/bash
#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013
# Startup script

source meteoconfig.py

./stopMETEO.sh
if [ "$INDISERVER" = "localhost" ]
then
	killall indiserver
	mkfifo  /tmp/INDIFIFO
	indiserver -f /tmp/INDIFIFO &
	echo start indi_duino -n \"MeteoStation\" -s \"/usr/local/share/indi/meteostation_sk.xml\" >/tmp/INDIFIFO
fi
if [ -f "meteo.rrd" ];
then
   echo "RRD file exists."
else
   echo "RRD file exists does not exist. Creating"
   ./meteoRRD_create.pu
fi
./meteoRRD_updater.py &
./meteoRRD_graph.py &
./sounding.py &
./meteoRRD_MaxMinAvg.py &

