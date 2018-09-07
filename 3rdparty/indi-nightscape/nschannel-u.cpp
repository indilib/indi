#include <stdio.h>

#include "nschannel-u.h"
#include  "nsdebug.h"

struct ftdi_context * NsChannelU::getDataChannel(void) {
		return &data_channel;	
}
struct ftdi_context * NsChannelU::getCommandChannel(void) {
		return &command_channel;	
}

int NsChannelU::close()
{
		ftdi_usb_close(&data_channel);
		ftdi_usb_close(&command_channel);
		ftdi_usb_close(&scan_channel);

		ftdi_deinit(&command_channel);
		ftdi_deinit(&data_channel);
		ftdi_deinit(&scan_channel);
		ftdi_list_free(&devs);
		opened = 0;
		return 0;
}

int NsChannelU::closecontrol()
{
		ftdi_usb_close(&command_channel);

		ftdi_deinit(&command_channel);
		return 0;
}

int NsChannelU::resetcontrol()
{
	closecontrol();
	return opencontrol();
}

int  NsChannelU::opendownload(void)
{
		 struct libusb_device * dev = camdev;
		struct ftdi_context * ftdid = &data_channel;
		int rc2;
		unsigned chunksize = DEFAULT_CHUNK_SIZE; //DEFAULT_CHUNK_SIZE;
    //chunksize = chunksize - ((chunksize / 64)*2);
		rc2 = ftdi_init(ftdid);
		if(rc2 < 0) {
			
			DO_ERR( "unable to init ftdi data device: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
			return -1;
		}

    // Select second interface
    ftdi_set_interface(ftdid, INTERFACE_B);
    
    
    // Open device
    rc2 = ftdi_usb_open_dev(ftdid, dev);
    if (rc2 < 0)
    {
        DO_ERR( "unable to open ftdi data device: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }
    rc2 = ftdi_usb_reset(ftdid);
    if (rc2 < 0)
    {
        DO_ERR( "unable to reset ftdi data device: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }
    rc2 = ftdi_usb_purge_buffers(ftdid);
    if (rc2 < 0)
    {
        DO_ERR( "unable to purge ftdi data device: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }
    rc2 = ftdi_set_bitmode(ftdid, 0x0, BITMODE_RESET);
    if (rc2 < 0)
    {
        DO_ERR( "unable to set bitmode data device: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }
    
    ftdid->usb_read_timeout = 20000; 
    ftdid->usb_write_timeout = 250; 
    rc2 = ftdi_write_data_set_chunksize(ftdid, chunksize);
    if (rc2 < 0)
    {
        DO_ERR( "unable to set write chunksize: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }
    rc2 = ftdi_read_data_set_chunksize(ftdid, chunksize);
    if (rc2 < 0)
    {
        DO_ERR( "unable to set read chunksize: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }
    rc2 = ftdi_read_data_get_chunksize (ftdid, &chunksize);
    if (rc2 < 0)
    {
        DO_ERR( "unable to get read chunksize: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }
    //rc2 = ftdi_set_latency_timer (&ftdid, 255);
    rc2 = ftdi_set_latency_timer (ftdid, 2);
    
    if (rc2 < 0)
    {
        DO_ERR( "unable to set latency timer: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }	
     //maxxfer = chunksize - ((chunksize / 64)*2);
    maxxfer = chunksize - ((chunksize / 512)*2);
    //maxxfer = imgsz;
    DO_INFO("actual read chunksize %d, max xfer %d\n", chunksize, maxxfer); 
    rc2 = ftdi_setflowctrl(ftdid, SIO_RTS_CTS_HS);
    if (rc2 < 0)
    {
        DO_ERR( "unable to set flow control: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
       return -1;
    }
    rc2=ftdi_setrts(ftdid, 1);
    if (rc2  < 0) {
        DO_ERR( "unable to set rts on data channel: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
				return -1;
    }
    return maxxfer;
}


int NsChannelU::opencontrol (void)
{
			 struct libusb_device * dev = camdev;

		struct ftdi_context * ftdic = &command_channel;
		int rc;
		rc = ftdi_init(ftdic);
		if(rc < 0) {
			DO_ERR( "unable to init ftdi control device: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
			return -1;
	  }
   // Select first interface
    ftdi_set_interface(ftdic, INTERFACE_A);
    
    // Open device
    rc = ftdi_usb_open_dev(ftdic, dev);
    if (rc < 0)
    {
        DO_ERR( "unable to open ftdi device: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
        return -1;
    }
// Read out FTDIChip-ID of R type chips
    if (ftdic->type == TYPE_2232H)
    {
        unsigned int chipid;
        rc = ftdi_read_chipid(ftdic, &chipid);
        if (rc < 0) {
        	DO_ERR( "unable read ftdi chipid: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
					return -1;
        }
        DO_INFO("FTDI chipid: %X\n", chipid);
    } else { 
       DO_ERR("incorrect ftdi type: %d\n", ftdic->type);
       return -1;
    };
    rc= ftdi_usb_reset(ftdic);
    if (rc  < 0) {
        DO_ERR( "unable to reset: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
				return -1;
    }
    rc= ftdi_usb_purge_buffers(ftdic);
    if (rc  < 0) {
        DO_ERR( "unable to purge: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
				return -1;
    }
    rc=ftdi_set_baudrate(ftdic, 460800*2);
    if (rc  < 0) {
        DO_ERR( "unable to set baudrate: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
				return -1;
    }
    rc=ftdi_set_latency_timer(ftdic, 2);
    if (rc  < 0) {
        DO_ERR( "unable to set latency: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
				return -1;
    }
    ftdic->usb_read_timeout = 500; 
    ftdic->usb_write_timeout = 250; 	
    return 0;
}

int NsChannelU::scan(void) {
		int rc;
	  struct ftdi_context * ftdis = &scan_channel;
		rc = ftdi_init(ftdis);
		if(rc < 0) {
				DO_ERR( "unable to init ftdi scan device: %d (%s)\n", rc, ftdi_get_error_string(ftdis));
				return -1;
		}
		ndevs = ftdi_usb_find_all (ftdis, &devs, vid, pid); 
	  DO_INFO("Found %d devices\n", ndevs);
	  struct ftdi_device_list * dev = devs;
	  camdev = NULL;
	  for (unsigned c = 0; c < ndevs; c++) {
	     char manf[64];
	     char desc[64];
	     //char ser[64];
	     //rc = ftdi_usb_get_strings (ftdis, dev->dev, manf, 64, desc, 64, ser, 64);  	
	     rc = ftdi_usb_get_strings (ftdis, dev->dev, manf, 64, desc, 64, NULL, 0);  	
	     if (rc !=0 ) {
	      DO_ERR( "unable to get strings: %d (%s)\n", rc, ftdi_get_error_string(ftdis));
	      return -1;
	     }
	    DO_INFO("Camera %d, Man: %s, Desc: %s\n", c+1, manf, desc);
	    //DO_INFO("Man: %s, Desc: %s, Ser: %s\n", manf, desc, ser);
	    if (c == (camnum -1)) camdev = dev->dev; 
	    dev = dev->next;
	  }
	
	  if (camdev == NULL) {
			DO_ERR( "Can't find camera number %d\n", camnum);
			return -1;
	  }
	  return ndevs;
}

int NsChannelU::readCommand(unsigned char *buf, size_t size) {
  int rc;
  struct ftdi_context * ftdic = &command_channel;

  rc = ftdi_read_data(ftdic, buf, size);
  if (rc < 0) {
   DO_ERR( "unable to read command: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
  	return -1;
  } else {
  	return rc;	
  }
}


int NsChannelU::writeCommand(const unsigned char *buf, size_t size) {
  int rc;
  struct ftdi_context * ftdic = &command_channel;

  rc = ftdi_write_data(ftdic, buf, size);
  if (rc < 0) {
   DO_ERR( "unable to write command: %d (%s)\n", rc, ftdi_get_error_string(ftdic));
  	return -1;
  } else {
  	return rc;	
  }
}


int NsChannelU::readData(unsigned char *buf, size_t size) {
  int rc;
  struct ftdi_context * ftdid = &data_channel;

  rc = ftdi_read_data(ftdid, buf, size);
  if (rc < 0) {
   DO_ERR( "unable to read data: %d (%s)\n", rc, ftdi_get_error_string(ftdid));
  	return -1;
  } else {
  	return rc;	
  }
}

int NsChannelU::purgeData(void) {
  struct ftdi_context * ftdid = &data_channel;
	int rc2;
	rc2 = ftdi_usb_purge_buffers(ftdid);
	if (rc2 < 0 ) {
		 DO_ERR( "unable to purge: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
		return (-1);
	}
	return 0;
}

int NsChannelU::setDataRts(void) {
 	struct ftdi_context * ftdid = &data_channel;
	int rc2=ftdi_setrts(ftdid, 1);
	if (rc2  < 0) {
		DO_ERR( "unable to set rts on data channel: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
		return (-1);
	}	
	return 0;
}   			
 