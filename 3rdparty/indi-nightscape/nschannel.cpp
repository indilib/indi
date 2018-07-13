#include <stdio.h>
#include "nschannel.h"
#include  "nsdebug.h"

int NsChannel::open() {
	  int rc = 0;
		if ((rc = scan()) < 0) return rc;
		if((rc = opencontrol()) < 0) return rc;
		if((rc = opendownload()) < 0) return rc;
		opened = 1;
		return 0;
}

int NsChannel::close() {
		opened = 0;
		return 0;
}

int NsChannel::getMaxXfer() {
		return maxxfer;	
}
