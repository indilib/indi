#include <stdio.h>
#include "nschannel-ser.h"
#include  "nsdebug.h"

#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

static int
set_interface_attribs (int fd, int speed)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                DO_ERR ("error %d from tcgetattr %s", errno, strerror(errno));
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        //tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cflag |= CRTSCTS;
        

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                DO_ERR ("error %d from tcsetattr %s", errno, strerror(errno));
                return -1;
        }
        return 0;
}

static void
set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                DO_ERR ("error %d from tggetattr %s", errno, strerror(errno));
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                DO_ERR ("error %d setting term attributes %s", errno, strerror(errno));
}



int NsChannelSER::close()
{
	 	::close(ftdic);
    ::close(ftdid);
    delete cportname;
    delete dportname;
		opened = 0;
		return 0;
}


int NsChannelSER::closecontrol()
{
	 	::close(ftdic);
		return 0;
}

int NsChannelSER::resetcontrol()
{
		closecontrol();
		return opencontrol();
}
int  NsChannelSER::opendownload(void)
{
		unsigned chunksize = 4095; //DEFAULT_CHUNK_SIZE;

		ftdid = ::open (dportname, O_RDWR | O_NOCTTY | O_SYNC);

		if (ftdid < 0)
		{
        DO_ERR ("error %d opening data: %s", errno, strerror (errno));
        return -1;
		}
		set_interface_attribs (ftdid, B115200);  // set speed to 115,200 bps, 8n1 (no parity)
		set_blocking (ftdid, 0);                // set no blocking
    //maxxfer = chunksize - ((chunksize / 64)*2);

    //maxxfer = (chunksize/4) - (((chunksize/4) / 64)*2);
    maxxfer = (chunksize) - (((chunksize) / 512)*2);
    //maxxfer = chunksize;
    //maxxfer = DEFAULT_OLD_CHUNK_SIZE;
    setDataRts();
    return maxxfer;
}


int NsChannelSER::opencontrol (void)
{
		
    // Open device
		ftdic = ::open (cportname, O_RDWR | O_NOCTTY | O_SYNC);

		if (ftdic < 0)
		{
        DO_ERR ("error %d opening control %s", errno, strerror (errno));
        return -1;
		}
		set_interface_attribs (ftdic, B115200);  // set speed to 115,200 bps, 8n1 (no parity)
		set_blocking (ftdic, 0);                // set no blocking
 
    return 0;
}

int NsChannelSER::scan(void) {
		//int rc;
	if( access( cportname,  R_OK| W_OK ) != -1 && access( dportname, R_OK| W_OK ) != -1) {
	    return 1;
	} else {
		return 0;
	}
}

int NsChannelSER::readCommand(unsigned char *buf, size_t size) {
	int rc;
	//unsigned nbytes = 0;
	rc = ::read(ftdic, buf, size);		
  if (rc < 0) {
   DO_ERR( "unable to read command: %d (%s)\n", rc,strerror(errno) );
  	return -1;
  } else {
  	return rc;	
  }
}


int NsChannelSER::writeCommand(const unsigned char *buf, size_t size) {
	int rc;
  //unsigned nbytes = 0;
  rc=::write(ftdic, (void *)buf, size);

  if (rc < 0) {
   DO_ERR( "unable to write command: %d (%s)\n", rc, strerror(errno));
  	return -1;
  } else {
  	return rc;	
  }
}


int NsChannelSER::readData(unsigned char *buf, size_t size) {
	int rc2;
	//unsigned nbytes= 0;
	rc2 = ::read(ftdid, buf, size);		
  if (rc2 < 0) {
   DO_ERR( "unable to read data: %d (%s)\n", rc2, strerror(errno));
  	return -1;
  } else {
  	return rc2;	
  }
}

int NsChannelSER::purgeData(void) {
	int rc2;
	rc2= tcflush(ftdid,TCIOFLUSH);

  if (rc2 < 0) {
      DO_ERR( "unable to purge: %d (%s)\n", rc2, strerror(errno));
			return (-1);
  }
	return 0;
}

int NsChannelSER::setDataRts(void) {
	int rc2;
   int RTS_flag;
   RTS_flag = TIOCM_RTS;
 
   rc2 = ioctl(ftdid,TIOCMBIS,&RTS_flag);//Set RTS pin 
	 if (rc2 < 0) {
		  DO_ERR( "unable to set rts: %d: (%s)\n", rc2, strerror(errno));
			return (-1);
	 }
	return 0;
}   			
 
