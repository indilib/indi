#ifndef __NS_MSG_H__
#define __NS_MSG_H__

#define CMD_SIZE 16
#include <libftdi1/ftdi.h>
#include "kaf_constants.h"
#include"nschannel.h"
#include <mutex>
class Nsmsg
{
	public:
		Nsmsg();
		Nsmsg(NsChannel * channel) {
			this->chan = channel;
		};
		int sendtemp(float temp, bool cooler_on);
		int senddur(float expo, int framediv, bool dark);
		int sendzone(int start_y, int lines, int framediv);
		int sendfan(int speed);
		int rcvstat(void);
		float rcvtemp(void);
		int getRawImgSize();
		int getRawImgSize(int start_y, int lines, int framediv);
		bool inquiry (void);
		int abort(void);
		int getResp4();
		const char * getFirmwareVer();
  private:
    static const unsigned char inq [CMD_SIZE];
		static const unsigned char sts [CMD_SIZE];
		static const unsigned char abt [CMD_SIZE];
		static const unsigned char stp [CMD_SIZE];
		static const unsigned char gtp [CMD_SIZE];
		static const unsigned char fan [CMD_SIZE];
		static const unsigned char inqr_8600 [CMD_SIZE];
		static const unsigned char dur[CMD_SIZE];
		static const unsigned char zon[CMD_SIZE];
		
  	short start_y;
  	short lines;
  	int imgsz;
  	float temp_set;
  	float temp_act;
  	int curr_status;
  	int resp_4;  //purpose?
  	char firmware_ver [25];
  	unsigned char cmd [CMD_SIZE];

		unsigned char resp [CMD_SIZE + 1];

  	NsChannel * chan;
		
		void calczone(int start_y_offset, int num_lines, int framediv);

  	void settemp(float temp, bool cooler_on);
		void setdur(float expo, int framediv, bool dark);
		void setzone(int start_y, int lines, int framediv);
		void setfan(int speed);
		float gettemp(void);
		int sendcmd (const char * name);
  	 void hexdump(const char *pre, const void *data, int size);
    std::mutex cmdmutex;
	
};
#endif