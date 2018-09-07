#include "nsmsg.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include  "nsdebug.h"
		const unsigned char Nsmsg::inq [CMD_SIZE] = {0xa5, 0x1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		const unsigned char Nsmsg::sts [CMD_SIZE] = {0xa5, 0x2, 0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		const unsigned char Nsmsg::abt [CMD_SIZE] = {0xa5, 0x4, 0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		const unsigned char Nsmsg::stp [CMD_SIZE] = {0xa5, 0x8, 0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		const unsigned char Nsmsg::gtp [CMD_SIZE] = {0xa5, 0x9, 0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		const unsigned char Nsmsg::fan [CMD_SIZE] = {0xa5, 0xa, 0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		const unsigned char Nsmsg::inqr_8600 [CMD_SIZE] = {0xA5,1,1,0x80,0,0x4F,0xD8,1,0x26,0,0,0,0,0,0,0};
		const unsigned char Nsmsg::dur[CMD_SIZE] = {0xa5, 0x3, 0,0,0,0,0xb,0,0,0,0,0,0,0,0,0};
		const unsigned char Nsmsg::zon[CMD_SIZE] = {0xa5, 0x7, 0,0,0,0, 0,0,0,0, 0x1a,0x82,0,0,7,0};
		
		
int Nsmsg::getRawImgSize(int start_y_offset, int num_lines, int framediv) {
	cmdmutex.lock();
	calczone(start_y_offset, num_lines, framediv);
	
	cmdmutex.unlock();

	return imgsz;	
}

int Nsmsg::getRawImgSize() {
	return imgsz;	
}

int  Nsmsg::sendtemp(float temp, bool cooler_on) {
	int rc;
	cmdmutex.lock();

	settemp(temp, cooler_on);
	rc = sendcmd("settemp");
	cmdmutex.unlock();

	return rc;
}


int Nsmsg::senddur(float expo, int framediv, bool dark) {
	int rc;
	cmdmutex.lock();
	setdur(expo, framediv, dark);
	rc = sendcmd("setdur");
	cmdmutex.unlock();

	return rc;

}
	
int Nsmsg::sendzone(int start_y_offset, int num_lines, int framediv){
	int rc;
	cmdmutex.lock();
	setzone(start_y_offset, num_lines, framediv);
	rc =  sendcmd("setzone");
	cmdmutex.unlock();

	return rc;
}

int Nsmsg::sendfan(int speed){
	int rc;
	cmdmutex.lock();
	setfan(speed);
	rc =  sendcmd("setfan");
	cmdmutex.unlock();
	return rc;
}

int Nsmsg::rcvstat(void){
		cmdmutex.lock();
		memcpy(cmd, sts, CMD_SIZE);
		if (sendcmd("status") < 0) {
			cmdmutex.unlock();
			return -1;
		}
		curr_status = resp[2];
		cmdmutex.unlock();
		return curr_status;
}

float Nsmsg::rcvtemp(void){
	cmdmutex.lock();
	memcpy(cmd, gtp, CMD_SIZE);
	if (sendcmd("gettemp") < 0) {
			cmdmutex.unlock();
			return 99999.9;
	}
	resp_4 = resp[4];
	temp_act = gettemp();
	cmdmutex.unlock();
	return temp_act;
}

int Nsmsg::getResp4() {
	return resp_4;	
}

bool Nsmsg::inquiry (void) {
	cmdmutex.lock();
  memcpy(cmd, inq, CMD_SIZE);
    if(sendcmd("inquiry")< 0) return false;
    if (memcmp(resp, inqr_8600, 5) != 0) {
				DO_ERR("%s", "not an 8600\n");
	  		hexdump("<", resp, sizeof(inqr_8600));
		  	hexdump(">", inqr_8600, sizeof(inqr_8600));
		  	cmdmutex.unlock();

		  	return false;
    } else{
    	snprintf(firmware_ver,25, "%c.%d.%d.%d\n", resp[5], resp[6], resp[7], resp[8]); // )hexdump("<", resp +5,4);

    	DO_INFO("%c.%d.%d.%d\n", resp[5], resp[6], resp[7], resp[8]); // )hexdump("<", resp +5,4);
	  	DO_INFO("%c.%d.%d.%d\n", inqr_8600[5], inqr_8600[6], inqr_8600[7], inqr_8600[8] );//hexdump(">", inqr_8600 +5,4);
    }
    hexdump("<", resp, sizeof(inqr_8600));

    cmdmutex.unlock();
    return true;
}

const char * Nsmsg::getFirmwareVer() {
	return 	firmware_ver;
}

int Nsmsg::abort(void) {
	int rc;
	cmdmutex.lock();
	 memcpy(cmd, abt, CMD_SIZE);
   rc = sendcmd("abort");
   cmdmutex.unlock();

	return rc;
}

void Nsmsg::settemp(float temp, bool cooler_on) {
	short temp2;
	memcpy(cmd, stp, CMD_SIZE);
	temp2 = (short)(temp*100);
  cmd [2] = cooler_on; 	
	if (!cooler_on) temp2 = 10000;
	cmd[3] = temp2>>8;
	cmd[4] = temp2 & 0xff;
	DO_DBG( "temp %02x %02x\n", cmd[3], cmd[4]);
}

void Nsmsg::setdur(float expo, int framediv, bool dark) {
	int exp2;
	memcpy(cmd, dur, CMD_SIZE);

	exp2 = expo*1000;
	if (framediv == 2) {
		cmd[6] = 0x2b;
	} else if (framediv == 4) {
   	cmd[6] = 0x4b;
  } else {
 		cmd[6] = 0xb;
  };
        
  if (dark) {
  	cmd[6] &= ~0x8;
  }
	cmd[2] = exp2>>24;
	cmd[3] = exp2>>16;
	cmd[4] = exp2>>8;
	cmd[5] = exp2 & 0xff;
	DO_DBG( " exp %02x %02x %02x %02x %02x\n", cmd[2], cmd[3], cmd[4], cmd[5], cmd[6]);
}

 void Nsmsg::calczone(int start_y_offset, int num_lines, int framediv) {
	
	start_y =start_y_offset;
	lines = num_lines;
  const int min_y = 0x24;
	const int max_y = 0x09ca;
	const int max_half = 0x04e6;
  const int max_quarter = 0x274;
  int max;
	if (framediv == 2) {
		max = max_half;
	} else if (framediv == 4)  {
		max = max_quarter;
	} else {
		max = max_y;	
	}
	if (start_y < min_y) start_y = min_y;
	if (start_y > max_y) start_y = max_y;
	if (lines < 1 || lines > max) lines = max;
	if ((lines + start_y) > (min_y+max)) lines = max -start_y;
	if(lines < 1) lines = 1;
	imgsz = KAF8300_MAX_X*lines*2;

}

void  Nsmsg::setzone(int start_y, int num_lines, int framediv) {
	memcpy(cmd, zon, CMD_SIZE);
	calczone(start_y, num_lines, framediv);
	cmd[2] = start_y>>8;
	cmd[3] = start_y & 0xff;
	cmd[4] = lines>>8;
	cmd[5] = lines & 0xff;
	//cmd[6] = dark * 128;
	DO_DBG( "zone  %02x %02x %02x %02x \n", zon[2], zon[3], zon[4], zon[5]);
}

void Nsmsg::setfan(int speed) {
  memcpy(cmd, fan, CMD_SIZE);

  const unsigned char spds [3] = {0x96, 0xc8, 0xff};
  cmd[2] = spds[speed -1];
}

float Nsmsg::gettemp(void) {
	float temp = 0.0;
	short temp2;
	//DO_DBG( "gtemp %02x %02x\n", resp[2], resp[3]);
	temp2 = (short)((resp[2]<<8)|(resp[3]&0xff)); 
	//DO_DBG( "gtemp %d %02x %02x\n", temp2, resp[2], resp[3]);
	temp = temp2/100.0;
	return temp;
}


int Nsmsg::sendcmd (const char * name) {
		int rc;
		int hardloop;
    rc=chan->writeCommand(cmd, CMD_SIZE);
		if (rc != CMD_SIZE) {
     DO_ERR( "unable to write(%s): %d\n", name, rc);
			chan->resetcontrol();
			return -1;
		}
    usleep(1000);
		hardloop = 7; 
		int usec = 1000;
    while((rc = chan->readCommand(resp, sizeof(resp))) == 0 && hardloop > 0) {
			if (hardloop % 2 == 0) DO_INFO("CW%d\n",hardloop);
    			usleep(usec);
			usec*= 2;
			//if (usec > 100000) usec = 100000;
			hardloop--;
		} 
   	if (rc != sizeof(resp)) {
    	DO_ERR("unable to read(%s) rc %d\n", name, rc);
			chan->resetcontrol();

			return -1;
    }
	  if (resp[0] != cmd[0] || resp[1] != cmd[1]) {
				DO_ERR( "not a %s  %02x %02x\n", name, resp[0], resp[1]);
			   hexdump("<", resp, rc);
				 return -1;
	  }
		//hexdump("<", resp, rc);

     return 0;
}

 void Nsmsg::hexdump(const char *pre, const void *data, int size)
{
    /* dumps size bytes of *data to stdout. Looks like:
     * [0000] 75 6E 6B 6E 6F 77 6E 20
     *                  30 FF 00 00 00 00 39 00 unknown 0.....9.
     * (in a single line of course)
     */

    unsigned char *p = (unsigned char *)data;
    unsigned char c;
    int n;
    char bytestr[4] = {0};
    char addrstr[10] = {0};
    char hexstr[ 16*3 + 5] = {0};
    char charstr[16*1 + 5] = {0};

    for(n=1;n<=size;n++) {
        if (n%16 == 1) {
            /* store address for this line */
            snprintf(addrstr, sizeof(addrstr), "%.4lx",
               ((unsigned long)p-(unsigned long)data) );
        }
            
        c = *p;
        if (isalnum(c) == 0) {
            c = '.';
        }

        /* store hex str (for left side) */
        snprintf(bytestr, sizeof(bytestr), "%02X ", *p);
        strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

        /* store char str (for right side) */
        snprintf(bytestr, sizeof(bytestr), "%c", c);
        strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

        if(n%16 == 0) { 
            /* line completed */
			DO_DBG("%s [%4.4s]   %-50.50s  %s\n", pre, addrstr, hexstr, charstr);
            hexstr[0] = 0;
            charstr[0] = 0;
        } else if(n%8 == 0) {
            /* half line: add whitespaces */
            strncat(hexstr, "  ", sizeof(hexstr)-strlen(hexstr)-1);
            strncat(charstr, " ", sizeof(charstr)-strlen(charstr)-1);
        }
        p++; /* next byte */
    }

    if (strlen(hexstr) > 0) {
        /* print rest of buffer if not empty */
		DO_DBG("%s [%4.4s]   %-50.50s  %s\n", pre, addrstr, hexstr, charstr);
    }
}
	