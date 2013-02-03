#!/usr/bin/python
#-*- coding: iso-8859-15 -*-
# INDUINO METEOSTATION
# http://induino.wordpress.com 
# 
# NACHO MAS 2013


import sys, os
import math
from indiclient import *
import time
import signal
import rrdtool
from meteoconfig import *
import simplejson

signal.signal(signal.SIGINT, signal.SIG_DFL)


def recv_indi():
	tim=time.localtime()
        HR=indi.get_float(INDIDEVICE,"HR","HR")
        Thr=indi.get_float(INDIDEVICE,"HR","T")
        P=indi.get_float(INDIDEVICE,"Presure","P")
        Tp=indi.get_float(INDIDEVICE,"Presure","T")
        IR=indi.get_float(INDIDEVICE,"IR","IR")
        Tir=indi.get_float(INDIDEVICE,"IR","T")
        dew=indi.get_float(INDIDEVICE,"Meteo","DEW")
        light=indi.get_float(INDIDEVICE,"LIGHT","LIGHT")
        T=indi.get_float(INDIDEVICE,"Meteo","T")
        clouds=indi.get_float(INDIDEVICE,"Meteo","clouds") 
        skyT=indi.get_float(INDIDEVICE,"Meteo","SkyT") 
        statusVector=indi.get_vector(INDIDEVICE,"STATUS")
	cloudFlag=int(statusVector.get_element("clouds").is_ok())
	dewFlag=int(statusVector.get_element("dew").is_ok())
	frezzingFlag=int(statusVector.get_element("frezzing").is_ok())
	return (("HR",HR),("Thr",Thr),("IR",IR),("Tir",Tir),("P",P),("Tp",Tp),("Dew",dew),("Light",light),
           ("T",T),("clouds",clouds),("skyT",skyT),("cloudFlag",cloudFlag),("dewFlag",dewFlag),
           ("frezzingFlag",frezzingFlag))


############# MAIN #############

print "Starting UPDATER"
## Write configuration javascript
fi=open(CHARTPATH+"meteoconfig.js","w")
fi.write("var altitude=%s\n" % ALTITUDE)
fi.write("var sitename=\"%s\"\n" % SITENAME)
fi.write("var INDISERVER=\"%s\"\n" % INDISERVER)
fi.write("var INDIPORT=%s\n" % INDIPORT)
fi.write("var INDIDEVICE=\"%s\"\n" % INDIDEVICE)
fi.write("var INDIDEVICEPORT=\"%s\"\n" % INDIDEVICEPORT)
fi.write("var OWNERNAME=\"%s\"\n" % OWNERNAME)
fi.close()

#connect ones to configure the port
INDIPORT=int(INDIPORT)
indi=indiclient(INDISERVER,INDIPORT)
indi.set_and_send_text(INDIDEVICE,"DEVICE_PORT","PORT",INDIDEVICEPORT)
vector=indi.get_vector(INDIDEVICE,"CONNECTION")
vector.set_by_elementname("CONNECT")
indi.send_vector(vector)
print "CONNECT INDI Server host:%s port:%s device:%s" % (INDISERVER,INDIPORT,INDIDEVICE)
time.sleep(5)
indi.quit()
time.sleep(1)

#connect an retrive info
while (True):
  try:
	indi=indiclient(INDISERVER,INDIPORT)
	now=time.localtime()
	json_dict={"TIME":time.strftime("%c",now)}
    	data=recv_indi()	
	updateString="N"
	for d in data:
		#print d[0],d[1]
		updateString=updateString+":"+str(d[1])
		json_dict[d[0]]=int(d[1]*100)/100.
        #print updateString
 	ret = rrdtool.update('meteo.rrd',updateString);
 	if ret:
 		print rrdtool.error() 
        x = simplejson.dumps(json_dict)
	#print x
        fi=open(CHARTPATH+"RTdata.json","w")
	fi.write(x)
	fi.close()
        del data
	del json_dict 
	indi.quit()
	time.sleep(10)
  except:
	print "UPDATER FAIL"
	time.sleep(10)



