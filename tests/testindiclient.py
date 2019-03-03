import PyIndi
import time
import sys
import threading
     
class IndiClient(PyIndi.BaseClient):
    def __init__(self):
        super(IndiClient, self).__init__()
    def newDevice(self, d):
        pass
    def newProperty(self, p):
        pass
    def removeProperty(self, p):
        pass
    def newBLOB(self, bp):
        global blobEvent
        print("new BLOB ", bp.name)
        blobEvent.set()
        pass
    def newSwitch(self, svp):
        pass
    def newNumber(self, nvp):
        pass
    def newText(self, tvp):
        pass
    def newLight(self, lvp):
        pass
    def newMessage(self, d, m):
        pass
    def serverConnected(self):
        pass
    def serverDisconnected(self, code):
        pass
 
# connect the server
indiclient=IndiClient()
print(indiclient.getDevices())
indiclient.setServer("localhost",7624)
 
if (not(indiclient.connectServer())):
     print("No indiserver running on "+indiclient.getHost()+":"+str(indiclient.getPort())+" - Try to run")
     print("  indiserver indi_simulator_telescope indi_simulator_ccd")
     sys.exit(1)
 
# connect the scope
telescope="10micron"
device_telescope=None
telescope_connect=None
 
# get the telescope device
device_telescope=indiclient.getDevice(telescope)
while not(device_telescope):
    time.sleep(0.5)
    device_telescope=indiclient.getDevice(telescope)

# wait CONNECTION property be defined for telescope
#telescope_connect=device_telescope.getSwitch("CONNECTION")
while not(telescope_connect):
    time.sleep(0.5)
    telescope_connect=device_telescope.getSwitch("CONNECTION")
print("done")
 ####
# if the telescope device is not connected, we do connect it
#if not(device_telescope.isConnected()):
    # Property vectors are mapped to iterable Python objects
    # Hence we can access each element of the vector using Python indexing
    # each element of the "CONNECTION" vector is a ISwitch
#    telescope_connect[0].s=PyIndi.ISS_ON  # the "CONNECT" switch
#    telescope_connect[1].s=PyIndi.ISS_OFF # the "DISCONNECT" switch
#    indiclient.sendNewSwitch(telescope_connect) # send this new value to the device
      
# Now let's make a goto to vega
# Beware that ra/dec are in decimal hours/degrees
#vega={'ra': (279.23473479 * 24.0)/360.0, 'dec': +38.78368896 }
 
# We want to set the ON_COORD_SET switch to engage tracking after goto
# device.getSwitch is a helper to retrieve a property vector
#telescope_on_coord_set=device_telescope.getSwitch("ON_COORD_SET")
#while not(telescope_on_coord_set):
#    time.sleep(0.5)
#    telescope_on_coord_set=device_telescope.getSwitch("ON_COORD_SET")
# the order below is defined in the property vector, look at the standard Properties page
# or enumerate them in the Python shell when you're developing your program
#telescope_on_coord_set[0].s=PyIndi.ISS_ON  # TRACK
#telescope_on_coord_set[1].s=PyIndi.ISS_OFF # SLEW
#telescope_on_coord_set[2].s=PyIndi.ISS_OFF # SYNC
#indiclient.sendNewSwitch(telescope_on_coord_set)
# We set the desired coordinates
#telescope_radec=device_telescope.getNumber("EQUATORIAL_EOD_COORD")
#while not(telescope_radec):
#    time.sleep(0.5)
#    telescope_radec=device_telescope.getNumber("EQUATORIAL_EOD_COORD")
#telescope_radec[0].value=vega['ra']
#telescope_radec[1].value=vega['dec']
#indiclient.sendNewNumber(telescope_radec)
# and wait for the scope has finished moving
#while (telescope_radec.s==PyIndi.IPS_BUSY):
#    print("Scope Moving ", telescope_radec[0].value, telescope_radec[1].value)
#    time.sleep(2)
 
