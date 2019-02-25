#include "nsstatus.h"
#include <unistd.h>


static long long millis()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	long long t2 = (long long)now.tv_usec +  (long long)(now.tv_sec*1000000);	
	return t2 / 1000;
}

int  NsStatus::getStatus() {
	return status;
}

void NsStatus::startThread() {
		sched_param sch_params;
      sch_params.sched_priority = 3;
    	statThread = new std::thread (&NsStatus::trun, this); //(&NsDownload::trun, this);
			pthread_setschedparam(statThread->native_handle(), SCHED_FIFO, &sch_params);		

}

void NsStatus::stopThread() {
	setInterrupted();
	statThread->join();
}
	
	
void NsStatus::setInterrupted(){
	std::unique_lock<std::mutex> ulock(stmut);
	interrupted  = 1;
	go_status.notify_all();
}

void NsStatus::trun(){

	do  {
		DO_INFO("%s\n", "status thread");
		std::unique_lock<std::mutex> ulock(stmut);

		while(!do_status && !interrupted) go_status.wait(ulock);
			DO_DBG ("%s\n", "status thread wakeup");
		
		bool download = false;
		bool done = false;
		while (!done&&!interrupted) {
			status = m->rcvstat();
			if (status < 0) {
					done = true;
					DO_ERR("%s\n", "status read failed..");

			}
			if (old_status == 1 && status == 2) {
				stattime = millis();
			}
			if ((old_status == 2 && status == 0) ||
				(status == 2 && ( millis() - stattime >= 9500)) ) {
				if (!download) { 
					d->doDownload();
					DO_DBG("%s\n", "download start..");
				}	
				download = true;
				do_status = 0;
				if(status == 0) done = true;
					
			} 
			if (old_status != status) {
						DO_DBG("status change %d\n", status);
			}
			old_status = status;
			usleep(33000);
			
			
		}
	} while (!interrupted);
	DO_DBG ("%s", "status thread terminated");
}

void NsStatus::doStatus() {
	
	  std::unique_lock<std::mutex> ulock(stmut);

		do_status = 1;
		go_status.notify_all();
}