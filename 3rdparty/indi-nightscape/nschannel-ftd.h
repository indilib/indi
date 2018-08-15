#ifndef __NS_CHANNEL_FTD_H__
#define __NS_CHANNEL_FTD_H__
#include "nschannel.h"
#include <stdlib.h>
#include <ftd2xx.h>
class NsChannelFTD : public NsChannel {
	public:
		NsChannelFTD() {
			devs = NULL;
			maxxfer = 0;
			opened = 0;
			camnum = 0;
			thedev = -1;
		}
		NsChannelFTD(int cam) {
			devs = NULL;
			camnum = cam;
			maxxfer = 0;
			opened = 0;
			thedev = -1;
		}
		
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
		int closecontrol(void);
		int opendownload(void);
		int scan(void);
	private:
		FT_HANDLE ftdic, ftdid;
    struct ftdi_device_list * devs;
		int thedev;
	

};

#endif
