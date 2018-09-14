#ifndef __NS_DOWNLOAD_H__
#define __NS_DOWNLOAD_H__
#include "nschannel.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <thread>         // std::thread
#include <condition_variable>

typedef struct ns_readdata {
	int nread;
	int bufsiz;
	unsigned char * buffer;
	int nblks;
	int imgsz;

} ns_readdata_t;


struct img_params {
	float exp;
	float settemp;
	float acttemp;
	time_t expdate;	
	int ybinning;
	int xbinning;
};

struct download_params {
	int increment;
	int imgseq;
	int nexp;
	char fbase[64];
  struct img_params * imgp;
};


class NsDownload {
	public:
		 NsDownload() {

		 	 ctx = &dp;
		 	 ctx->imgp = &ip;
		 	 rd = &rdd;
		 	 retrBuf = &rb;
		 	 ctx->increment = 0;
   		 ctx->nexp = 0;
   		 rd->imgsz =0;
			 ctx->imgseq = 1;

			 //strcpy(ctx->fbase, "");
			 rd->buffer = NULL;
				in_download = 0;
		 		do_download = 0;
		 		write_it = 0;

		 		readdone = 0;
		 		retrBuf = NULL;

		 }
		 NsDownload(NsChannel * chn) {
		 	ctx = &dp;
		 	 ctx->imgp = &ip;
		 	 rd = &rdd;
		 	 retrBuf = &rb;
		 	 ctx->increment = 0;
   		 ctx->nexp = 0;
   		 rd->imgsz =0;
			 ctx->imgseq = 1;

			 //strcpy(ctx->fbase, "");
			 rd->buffer = NULL;
		 		cn = chn;
		 		in_download = 0;
		 		do_download = 0;
		 		write_it = 0;
		 		readdone = 0;
		 		retrBuf = NULL;
		 }
		 void setFrameYBinning(int  binning);
		 void setFrameXBinning(int  binning);

		 void setSetTemp (float temp);
		 void setActTemp(float temp);
		 void setImgSize(int siz);
		 void setIncrement(int inc);
		 void setFbase(const char * name);
		 void nextImage(void);
		 void setNumExp(int n);
		 int getImgSeq(void);
		 void setExpDur(float exp);
		 void doDownload();
		 void startThread();
		 void stopThread();
		 bool inDownload();
		 int getActWriteLines();
		 void trun();
		void initdownload();
		int downloader();
		int purgedownload(); 
		unsigned char * getBuf();
		size_t getBufImageSize();
		void setImgWrite(bool w);
		void freeBuf();
		void setInterrupted();
		void copydownload(unsigned char *buf, int xstart, int xlen, int xbin, int pad, int cooked);
		void writedownload(int pad, int cooked);
		void setZeroReads(int zeroes);
	private:

	  void fitsheader(int x, int y, char * fbase, struct img_params * ip);
		int fulldownload(); 
		bool getDoDownload();
		struct download_params dp;
		struct img_params ip;
	  ns_readdata_t  rdd;

		struct download_params * ctx;
		ns_readdata_t *rd;
		FILE * img;
		volatile int readdone;
		volatile int do_download;
		volatile int in_download;
		volatile int interrupted;

		NsChannel * cn;
		int write_it;
		int lastread;
		std::thread * downthread;
		std::condition_variable go_download;
		std::mutex mutx;

		//static void download_thread(int x);
		ns_readdata_t rb;

		ns_readdata_t * retrBuf;
		int zero_reads { 1 };
		int writelines{0};
};
#endif