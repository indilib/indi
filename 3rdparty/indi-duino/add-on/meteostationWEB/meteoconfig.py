#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013



##### INDI RELATED #####
#To start indiserver use 'localhost'
#otherwise not start and connect remote
#indiserver
#INDISERVER="localhost"
INDISERVER="raspberryPI"
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
#PATH TO GRAPHs
CHARTPATH="./html/CHART/"
#EUMETSAT lastimagen. Choose one from:
#http://oiswww.eumetsat.org/IPPS/html/latestImages.html
#This is nice but only work at daylight time:
#EUMETSAT_LAST="http://oiswww.eumetsat.org/IPPS/html/latestImages/EUMETSAT_MSG_RGB-naturalcolor-westernEurope.jpg"
#This show rain
EUMETSAT_LAST="http://oiswww.eumetsat.org/IPPS/html/latestImages/EUMETSAT_MSG_MPE-westernEurope.jpg"
#and this cloud cover at IR 39. Work at night
#EUMETSAT_LAST="http://oiswww.eumetsat.org/IPPS/html/latestImages/EUMETSAT_MSG_IR039E-westernEurope.jpg"



