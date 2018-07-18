#ifndef _NS_STATUS_H__
#define _NS_STATUS_H__
#include <pthread.h>
#include <thread>         // std::thread
#include "nsdebug.h"

#include <condition_variable>
#include "nsmsg.h"
#include "nschannel.h"
#include "nsdownload.h"

class NsStatus {
 public:
 NsStatus () {
    m = NULL;
    status = 0;	
    old_status = 0;
    do_status = 0;
    interrupted = 0;

  }
   NsStatus (Nsmsg * ms, NsDownload *dn) {
    m = ms;
    d = dn;
    status = 0;	
    old_status = 0;
    do_status = 0;
    interrupted = 0;

  }
	int getStatus();
	void doStatus();
	void startThread();
	void stopThread();
	
	private:
		long long stattime {0};
		void setInterrupted();
		void trun();
		volatile int status;
		int old_status;
		volatile int do_status { 0 };
		volatile int interrupted;

		std::thread * statThread;
		std::condition_variable go_status;
		std::mutex stmut;

		Nsmsg * m;
		NsDownload * d;
};

#endif