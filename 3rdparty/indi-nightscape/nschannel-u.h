#ifndef __NS_CHANNEL_U_H__
#define __NS_CHANNEL_U_H__
#include "nschannel.h"
#include <stdlib.h>
#include <libftdi1/ftdi.h>

class NsChannelU : public NsChannel {
	public:
		NsChannelU() {
			devs = NULL;
			maxxfer = 0;
			opened = 0;
			camnum = 0;
		}
		NsChannelU(int cam) {
			devs = NULL;
			camnum = cam;
			maxxfer = 0;
			opened = 0;
		}
		struct ftdi_context * getCommandChannel();
		struct ftdi_context * getDataChannel();
		int open();
		int close();
		int getMaxXfer();
		int readCommand(unsigned char * buf, size_t n);
		int writeCommand(const unsigned char * buf, size_t n);
		int readData(unsigned char * buf, size_t n);
		int purgeData(void);
		int setDataRts(void);
		int resetcontrol (void);

  protected:
  	int opencontrol (void);
		int closecontrol (void);

		int opendownload(void);
		int scan(void);
	private:
		struct ftdi_context scan_channel;
		struct ftdi_context command_channel;
		struct ftdi_context data_channel;
		struct ftdi_device_list * devs;
		struct libusb_device * camdev;
		

};

#endif
