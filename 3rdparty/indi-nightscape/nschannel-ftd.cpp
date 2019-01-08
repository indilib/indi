#include <stdio.h>
#include "nschannel-ftd.h"
#include  "nsdebug.h"


static const char* status_string(FT_STATUS res)
{
	static const char *table[] = {
	"FT_OK",
	"FT_INVALID_HANDLE",
    "FT_DEVICE_NOT_FOUND",
    "FT_DEVICE_NOT_OPENED",
    "FT_IO_ERROR",
    "FT_INSUFFICIENT_RESOURCES",
    "FT_INVALID_PARAMETER",
    "FT_INVALID_BAUD_RATE",
    "FT_DEVICE_NOT_OPENED_FOR_ERASE",
    "FT_DEVICE_NOT_OPENED_FOR_WRITE",
    "FT_FAILED_TO_WRITE_DEVICE",
    "FT_EEPROM_READ_FAILED",
    "FT_EEPROM_WRITE_FAILED",
    "FT_EEPROM_ERASE_FAILED",
	"FT_EEPROM_NOT_PRESENT",
	"FT_EEPROM_NOT_PROGRAMMED",
	"FT_INVALID_ARGS",
	"FT_NOT_SUPPORTED",
	"FT_OTHER_ERROR",
	"FT_DEVICE_LIST_NOT_READY"
	};
	return table[res];
}


int NsChannelFTD::close()
{
	 	FT_Close(ftdic);
    FT_Close(ftdid);
		opened = 0;
		return 0;
}

int NsChannelFTD::closecontrol()
{
		FT_Close(ftdic);
		return 0;
}


int NsChannelFTD::resetcontrol()
{
		closecontrol();
		return opencontrol();
}

int  NsChannelFTD::opendownload(void)
{
	
		FT_STATUS rc2	;
		//unsigned chunksize = DEFAULT_CHUNK_SIZE; //DEFAULT_CHUNK_SIZE;
		 unsigned chunksize = 65536L;
    //chunksize = chunksize - ((chunksize / 64)*2);
	  rc2 = FT_Open(thedev+1, &ftdid);
    if (rc2 != FT_OK)
    {
        DO_ERR( "unable to open ftdi data device: %d (%s)\n", rc2, status_string(rc2));
        return(-1);
    }
    rc2 = FT_ResetDevice(ftdid);
    if (rc2 != FT_OK)
    {
        DO_ERR( "unable to reset ftdi data device: %d (%s)\n", rc2, status_string(rc2));
        return(-1);
    }
    rc2 = FT_Purge(ftdid, FT_PURGE_RX | FT_PURGE_TX);
    if (rc2 != FT_OK)
    {
        DO_ERR( "unable to purge ftdi data device: %d (%s)\n", rc2, status_string(rc2));
        return(-1);
    }
    rc2=FT_SetTimeouts(ftdid, 500, 250);
    if (rc2 != FT_OK)
    {
        DO_ERR( "unable to set timeouts data device: %d (%s)\n", rc2, status_string(rc2));
        return(-1);
    }
    DO_INFO("test read chunksize %d, max xfer %d\n", chunksize, maxxfer); 

    //rc2 = FT_SetUSBParameters(ftdid, chunksize/2, chunksize);
    rc2 = FT_SetUSBParameters(ftdid, chunksize, chunksize);

    if (rc2 != FT_OK)
    {
        DO_ERR( "unable to set USB Parameters: %d (%s)\n", rc2, status_string(rc2));
        return(-1);
    }
   // maxxfer = chunksize - ((chunksize / 64)*2);
    maxxfer = chunksize - ((chunksize / 512)*2);

    //maxxfer = (chunksize/4) - (((chunksize/4) / 64)*2);
    //maxxfer = (chunksize/4) - (((chunksize/4) / 512)*2);
    //maxxfer = chunksize;
    //maxxfer = DEFAULT_OLD_CHUNK_SIZE;
    DO_INFO("actual read chunksize %d, max xfer %d\n", chunksize, maxxfer); 
    rc2 = FT_SetFlowControl(ftdid, FT_FLOW_RTS_CTS, 0, 0);
    if (rc2 != FT_OK)
    {
        DO_ERR( "unable to set flow control: %d (%s)\n", rc2, status_string(rc2));
        return(-1);
    }
    rc2=FT_SetRts (ftdid);
    if (rc2  != FT_OK) 
    {
        DO_ERR( "unable to set rts on data channel: %d (%s)\n", rc2, status_string(rc2));
				return (-1);
    }
    return maxxfer;
}


int NsChannelFTD::opencontrol (void)
{
		FT_STATUS rc;

    // Open device
    rc = FT_Open(thedev, &ftdic);
    if (rc != FT_OK)
    {
        DO_ERR( "unable to open ftdi device: %d (%s)\n", rc, status_string(rc));
        return(-1);
    }
    rc= FT_ResetDevice(ftdic);
    if (rc  != FT_OK) {
        DO_ERR( "unable to reset: %d (%s)\n", rc, status_string(rc));
				return (-1);
    }
    rc= FT_Purge(ftdic, FT_PURGE_RX | FT_PURGE_TX);
    if (rc  != FT_OK) {
        DO_ERR( "unable to purge: %d (%s)\n", rc, status_string(rc));
				return (-1);
    }
    rc=FT_SetLatencyTimer(ftdic, 2);
    if (rc  != FT_OK) {
        DO_ERR( "unable to set latency: %d (%s)\n", rc, status_string(rc));
				return (-1);
    }
    rc=FT_SetTimeouts(ftdic, 500, 250);
    if (rc != FT_OK)
    {
        DO_ERR( "unable to set timeouts: %d (%s)\n", rc, status_string(rc));
        return (-1);
    }
     rc = FT_SetFlowControl(ftdic, FT_FLOW_RTS_CTS, 0, 0);
    if (rc != FT_OK)
    {
        DO_ERR( "unable to set control flow control: %d (%s)\n", rc, status_string(rc));
        return(-1);
    }
    rc=FT_SetRts (ftdic);
    if (rc  != FT_OK) 
    {
        DO_ERR( "unable to set rts on control channel: %d (%s)\n", rc, status_string(rc));
				return (-1);
    }
    return 0;
}

int NsChannelFTD::scan(void) {
		FT_STATUS rc;
		thedev = -1;
		rc = FT_SetVIDPID(vid, pid);
    if (rc != FT_OK) {
         DO_ERR("unable to set vip/pid: %d(%s)\n", (int)rc, status_string(rc));
        return (-1);
    }
        rc = FT_CreateDeviceInfoList((unsigned long*)&ndevs);
    if (rc != FT_OK) {
        DO_ERR("unable to get device info: %d(%s)\n", (int)rc, status_string(rc));
        return (-1);
    }
    DO_INFO("Found %d devices\n", ndevs);
    FT_DEVICE_LIST_INFO_NODE * dev = NULL;
    if (ndevs > 0) {
    	dev = (FT_DEVICE_LIST_INFO_NODE *)malloc (sizeof(FT_DEVICE_LIST_INFO_NODE) *ndevs);
        rc = FT_GetDeviceInfoList (dev, (unsigned long*)&ndevs);
    	if (rc != FT_OK) {
            DO_ERR("unable to get device info list: %d(%s)\n", (int)rc, status_string(rc));
        	return(-1);
    	}
    	for (unsigned c = 0; c < ndevs; c++) {
				DO_INFO("Dev %d:\n",c); 
				DO_INFO(" Flags=0x%x\n",dev[c].Flags); 
				DO_INFO(" Type=0x%x\n",dev[c].Type); 
		    DO_INFO(" ID=0x%x\n",dev[c].ID); 
				DO_INFO(" LocId=0x%x\n",dev[c].LocId); 
		
		    DO_INFO(" SerialNumber=%s\n",dev[c].SerialNumber); 
				DO_INFO(" Description=%s\n",dev[c].Description); 
				DO_INFO(" ftHandle=%p\n",dev[c].ftHandle);
		    if (c  == (camnum-1)*2) thedev = c; 
    	}
    }
    if (thedev == -1) {
			DO_ERR("Can't find camera number %d\n", camnum);
			return(-1);
		}
    // Read out FTDIChip-ID of R type chips
    if (dev[thedev].Type != FT_DEVICE_2232H) {
       DO_ERR("incorrect ftdi type: %d\n", dev[thedev].Type);
       return(-1);
    };
    free(dev);
	  return ndevs;
}

int NsChannelFTD::readCommand(unsigned char *buf, size_t size) {
	FT_STATUS rc;
	unsigned nbytes = 0;
    rc = FT_Read(ftdic, buf, size, (unsigned long*)&nbytes);
  if (rc != FT_OK) {
   DO_ERR( "unable to read command: %d (%s)\n", rc,status_string(rc) );
  	return -1;
  } else {
  	return nbytes;	
  }
}


int NsChannelFTD::writeCommand(const unsigned char *buf, size_t size) {
	FT_STATUS rc;
  unsigned nbytes = 0;
  rc=FT_Write(ftdic, (void *)buf, size, (unsigned long*)&nbytes);

  if (rc != FT_OK) {
   DO_ERR( "unable to write command: %d (%s)\n", (int)rc, status_string(rc));
  	return -1;
  } else {
  	return nbytes;	
  }
}


int NsChannelFTD::readData(unsigned char *buf, size_t size) {
	FT_STATUS rc2;
	unsigned nbytes= 0;
    rc2 = FT_Read(ftdid, buf, size, (unsigned long*)&nbytes);
  if (rc2 != FT_OK) {
   DO_ERR( "unable to read data: %d (%s)\n", (int)rc2, status_string(rc2));
  	return -1;
  } else {
  	return nbytes;	
  }
}

int NsChannelFTD::purgeData(void) {
	FT_STATUS rc2;
	rc2= FT_Purge(ftdid, FT_PURGE_RX | FT_PURGE_TX);
  if (rc2  != FT_OK) {
      DO_ERR( "unable to purge: %d (%s)\n", (int)rc2, status_string(rc2));
			return (-1);
  }
	return 0;
}

int NsChannelFTD::setDataRts(void) {
	FT_STATUS rc2;
 	rc2=FT_SetRts (ftdid);
	if (rc2  != FT_OK) {
    DO_ERR("unable to set rts on data channel: %d (%s)\n", (int)rc2, status_string(rc2));
		return (-1);
	}
	return 0;
}   			
 
