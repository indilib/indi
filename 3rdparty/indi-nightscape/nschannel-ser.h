#ifndef __NS_CHANNEL_SER_H__
#define __NS_CHANNEL_SER_H__
#include "nschannel.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

class NsChannelSER : public NsChannel {
	public:
		NsChannelSER() {
			maxxfer = 0;
			opened = 0;
			camnum = 1;
			thedev = -1;
			char * name = new char [64];
			snprintf(name, 64, "/dev/ttyUSB%d", camnum-1);
			cportname = name;
			name = new char [64];
			snprintf(name, 64, "/dev/ttyUSB%d", camnum);
			dportname = name;

		}
		NsChannelSER(int cam) {
			camnum = cam;
			maxxfer = 0;
			opened = 0;
			thedev = -1;
			char * name = new char [64];
			snprintf(name, 64, "/dev/ttyUSB%d", camnum-1);
			cportname = name;
			name = new char [64];
			snprintf(name, 64, "/dev/ttyUSB%d", camnum);
			dportname = name;
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
  	int closecontrol (void);
		int opendownload(void);
		int scan(void);
	private:
		int ftdic, ftdid;
		int thedev;
		const char * cportname;
		const char * dportname;
};

#endif
