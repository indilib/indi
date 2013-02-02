#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013



##### INDI RELATED #####
#To start indiserver use 'localhost'
#otherwise not start and connect remote
#indiserver
INDISERVER="localhost"
#INDISERVER="raspberryPI"
INDIPORT="7624"
INDIDEVICE="MeteoStation"
INDIDEVICEPORT="/dev/ttyACM0"

##### SITE RELATED ####
OWNERNAME="Nacho Mas"
SITENAME="MADRID"
ALTITUDE=630
#Visit http://weather.uwyo.edu/upperair/sounding.html
#See the sounding location close your site
SOUNDINGSTATION="08221"


##### RRD RELATED #####
CHARTPATH="./html/CHART/"



