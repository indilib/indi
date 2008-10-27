#if 0
    Robofocus Driver
    Copyright (C) 2006 Markus Wildi, markus.wildi@datacomm.ch

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "indicom.h"
#include "indidevapi.h"
#include "indiapi.h"
#include "robofocusdriver.h"

/**********************************************************************
* Communication
**********************************************************************/


int portRFRead(int fd, char *buf, int nbytes, int timeout)
{

  int bytesRead = 0;
  int totalBytesRead = 0;
  int err;
  char *tbuf= buf ; 
 
 
  while (nbytes > 0)
    {
      if ( (err = tty_timeout(fd, timeout)) < 0 )
	return err;

      bytesRead = read(fd, tbuf, nbytes);

      if (bytesRead < 0 )
	return -1;

      
      if((  tbuf[0]== 'I')||  (  tbuf[0]== 'O')){

	/*fprintf(stderr, "Moving\n") ;*/
	tbuf[0]=0 ;
         usleep(100000) ; 
      } else {
	tbuf += bytesRead;
	totalBytesRead++;
	nbytes -= bytesRead;
      }
    }

/*   fprintf(stderr, "READ : (%s,%d), %d\n", buf, 9, totalBytesRead) ; */
/*   fprintf(stderr, "READ : ") ; */
/*   for(i=0; i < 9; i++) { */

/*     fprintf(stderr, "0x%2x ", (unsigned char)buf[i]) ; */
/*   } */
/*   fprintf(stderr, "\n") ; */
  return 9;
}


int portRFWrite(int fd, const char * buf) {
  int nbytes=9 ;
  int totalBytesWritten=0 ;
  int bytesWritten = 0;   

  while (nbytes > 0)
  {
    
    bytesWritten = write(fd, buf, nbytes);

    if (bytesWritten < 0)
     return -1;

    buf += bytesWritten;
    nbytes -= bytesWritten;
  }

  /* Returns the # of bytes written */
  return (totalBytesWritten);
}


unsigned char calsum(char *rf_cmd) {

  unsigned char val= 0 ;
  int i=0 ;

  for(i=0; i < 8; i++) {

    val = val + (unsigned char) rf_cmd[i] ;
  }

  return val % 256 ;
}


unsigned char chksum(char *rf_cmd) {

  char substr[255] ;
  unsigned char val= 0 ;
  int i= 0 ;
  
  for(i=0; i < 8; i++) {

    substr[i]=rf_cmd[i] ;
  }

  val = calsum( substr) ;
  if( val==  (unsigned char) rf_cmd[8]) {

  } else {

    fprintf(stderr,  "Wrong (%s,%d), %x != %x\n",  rf_cmd, strlen(rf_cmd), val, (unsigned char) rf_cmd[8]) ;
  }

  return val ;
}

/**********************************************************************
* Commands
**********************************************************************/


int commRF(int fd, char *rf_cmd) {

  int read_ret=0, check_ret=0;
  char rf_cmd_cks[32] ;

  unsigned char val= 0 ;

  int nbytes= 9 ;
  int i ;

  val = calsum( rf_cmd ) ;
  for(i=0; i < 8; i++) {
      
    rf_cmd_cks[i]= rf_cmd[i] ;
  }

  rf_cmd_cks[8]=  val ;
  rf_cmd_cks[9]= 0 ;

/*   fprintf(stderr, "WRITE: ") ; */
/*   for(i=0; i < 9; i++) { */
      
/*     fprintf(stderr, "0x%2x ", rf_cmd_cks[i]) ; */
/*   } */
/*   fprintf(stderr, "\n") ; */

  if(portRFWrite(fd, rf_cmd_cks) < 0)
    return -1;

  read_ret= portRFRead(fd, rf_cmd, nbytes, RF_TIMEOUT) ;

  if (read_ret < 1)
    return read_ret;

  check_ret= chksum(rf_cmd) ;
   
  rf_cmd[ read_ret- 1] = 0 ;
  return 0;
}

int updateRFPosition(int fd, double *value) {

  float temp ;
  char rf_cmd[32] ;
  int ret_read_tmp ;

  strcpy(rf_cmd, "FG000000" ) ;

  if ((ret_read_tmp= commRF(fd,  rf_cmd)) < 0){

    return ret_read_tmp;
  }
   if (sscanf(rf_cmd, "FD%6f", &temp) < 1){

    return -1;
  }

  *value = (double) temp;

  return 0;

}

int updateRFTemperature(int fd, double *value) {

  float temp ;
  char rf_cmd[32] ;
  int ret_read_tmp ;

  strcpy(rf_cmd, "FT000000" ) ;

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;
    

  if (sscanf(rf_cmd, "FT%6f", &temp) < 1)
    return -1;
   
  *value = (double) temp/2.- 273.15;

  return 0;
}

int updateRFBacklash(int fd, double *value) {

  float temp ;
  char rf_cmd[32] ;
  char vl_tmp[4] ;
  int ret_read_tmp ;
  int sign= 0 ;

  if(*value== BACKLASH_READOUT) {
 
    strcpy(rf_cmd, "FB000000" ) ;

  } else {

    rf_cmd[0]=  'F' ;
    rf_cmd[1]=  'B' ;
    
    if( *value > 0) {

      rf_cmd[2]= '3' ;

    } else {
      *value= - *value ;
      rf_cmd[2]= '2' ;
    }
    rf_cmd[3]= '0' ;
    rf_cmd[4]= '0' ;

    if(*value > 99) {
      sprintf( vl_tmp, "%3d", (int) *value) ;

    } else if(*value > 9) {
      sprintf( vl_tmp, "0%2d", (int) *value) ;
    } else {
      sprintf( vl_tmp, "00%1d", (int) *value) ;
    }
    rf_cmd[5]= vl_tmp[0] ;
    rf_cmd[6]= vl_tmp[1] ; 
    rf_cmd[7]= vl_tmp[2] ;
  }

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;
    

  if (sscanf(rf_cmd, "FB%1d%5f", &sign, &temp) < 1)
    return -1;

  *value = (double) temp  ;

  if(( sign== 2) && ( *value > 0)) {
    *value = - (*value) ;
  }
  return 0;
}

int updateRFFirmware(int fd, char *rf_cmd) {

  int ret_read_tmp ;

  strcpy(rf_cmd, "FV000000" ) ;

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;

  return 0;
}

int updateRFMotorSettings(int fd, double *duty, double *delay, double *ticks) {

  char rf_cmd[32] ; 
  int ret_read_tmp ;

  if(( *duty== 0 ) && (*delay== 0) && (*ticks== 0) ){

    strcpy(rf_cmd, "FC000000" ) ;

  } else {

    rf_cmd[0]=  'F' ;
    rf_cmd[1]=  'C' ;
    rf_cmd[2]= (char) *duty ;
    rf_cmd[3]= (char) *delay ;
    rf_cmd[4]= (char) *ticks ;
    rf_cmd[5]= '0' ;
    rf_cmd[6]= '0' ;
    rf_cmd[7]= '0' ;
    rf_cmd[8]=  0 ;

  }

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;
 
  *duty=  (float) rf_cmd[2] ;
  *delay= (float) rf_cmd[3] ;
  *ticks= (float) rf_cmd[4] ;
  
  return 0;
}

int updateRFPositionRelativeInward(int fd, double *value) {

  char rf_cmd[32] ;
  int ret_read_tmp ;
  float temp ;
  rf_cmd[0]= 0 ;

  if(*value > 9999) {
    sprintf( rf_cmd, "FI0%5d", (int) *value) ;
  } else if(*value > 999) {
    sprintf( rf_cmd, "FI00%4d", (int) *value) ;
  } else if(*value > 99) {
    sprintf( rf_cmd, "FI000%3d", (int) *value) ;
  } else if(*value > 9) {
    sprintf( rf_cmd, "FI0000%2d", (int) *value) ;
  } else {
    sprintf( rf_cmd, "FI00000%1d", (int) *value) ;
  }

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;

  if (sscanf(rf_cmd, "FD0%5f", &temp) < 1)
    return -1;

  *value = (double) temp  ;

  return 0;
}

int updateRFPositionRelativeOutward(int fd, double *value) {

  char rf_cmd[32] ;
  int ret_read_tmp ;
  float temp ;

  rf_cmd[0]= 0 ;

  if(*value > 9999) {
    sprintf( rf_cmd, "FO0%5d", (int) *value) ;
  } else if(*value > 999) {
    sprintf( rf_cmd, "FO00%4d", (int) *value) ;
  } else if(*value > 99) {
    sprintf( rf_cmd, "FO000%3d", (int) *value) ;
  } else if(*value > 9) {
    sprintf( rf_cmd, "FO0000%2d", (int) *value) ;
  } else {
    sprintf( rf_cmd, "FO00000%1d", (int) *value) ;
  }

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;

  if (sscanf(rf_cmd, "FD0%5f", &temp) < 1)
    return -1;

  *value = (double) temp  ;

  return 0;
}

int updateRFPositionAbsolute(int fd, double *value) {

  char rf_cmd[32] ;
  int ret_read_tmp ;
  float temp ;

  rf_cmd[0]= 0 ;

  if(*value > 9999) {
    sprintf( rf_cmd, "FG0%5d", (int) *value) ;
  } else if(*value > 999) {
    sprintf( rf_cmd, "FG00%4d", (int) *value) ;
  } else if(*value > 99) {
    sprintf( rf_cmd, "FG000%3d", (int) *value) ;
  } else if(*value > 9) {
    sprintf( rf_cmd, "FG0000%2d", (int) *value) ;
  } else {
    sprintf( rf_cmd, "FG00000%1d", (int) *value) ;
  }

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;

  if (sscanf(rf_cmd, "FD0%5f", &temp) < 1)
    return -1;

  *value = (double) temp  ;

  return 0;
}
int updateRFPowerSwitches(int fd, int s, int  new_sn, int *cur_s1LL, int *cur_s2LR, int *cur_s3RL, int *cur_s4RR) {

  char rf_cmd[32] ;
  char rf_cmd_tmp[32] ;
  int ret_read_tmp ;
  int i = 0 ;


    /* Get first the status */
    strcpy(rf_cmd_tmp, "FP000000" ) ;

    if ((ret_read_tmp= commRF(fd, rf_cmd_tmp)) < 0)   
      return ret_read_tmp ;

    for(i= 0; i < 9; i++) {
      rf_cmd[i]= rf_cmd_tmp[i] ;
    } 


    if( rf_cmd[new_sn + 4]== '2') {
      rf_cmd[new_sn + 4]= '1' ;
    } else {
      rf_cmd[new_sn + 4]= '2' ;
    }


 /*    if( s== ISS_ON) { */

/*       rf_cmd[new_sn + 4]= '2' ; */
/*     } else { */
/*       rf_cmd[new_sn + 4]= '1' ; */
/*     } */

    rf_cmd[8]= 0 ;

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)   
    return ret_read_tmp ;
  

  *cur_s1LL= *cur_s2LR= *cur_s3RL= *cur_s4RR= ISS_OFF ;

  if(rf_cmd[4]== '2' ) {
    *cur_s1LL= ISS_ON ;
  }

  if(rf_cmd[5]== '2' ) {
    *cur_s2LR= ISS_ON ;
  }

  if(rf_cmd[6]== '2' ) {
    *cur_s3RL= ISS_ON ;
  }

  if(rf_cmd[7]== '2' ) {
    *cur_s4RR= ISS_ON ;
  }
  return 0 ; 
}


int updateRFMaxPosition(int fd, double *value) {

  float temp ;
  char rf_cmd[32] ;
  char vl_tmp[6] ;
  int ret_read_tmp ;
  char waste[1] ;

  if(*value== MAXTRAVEL_READOUT) {
 
    strcpy(rf_cmd, "FL000000" ) ;

  } else {

    rf_cmd[0]=  'F' ;
    rf_cmd[1]=  'L' ;
    rf_cmd[2]=  '0' ;

    if(*value > 9999) {
      sprintf( vl_tmp, "%5d", (int) *value) ;

    } else if(*value > 999) {

      sprintf( vl_tmp, "0%4d", (int) *value) ;

    } else if(*value > 99) {
      sprintf( vl_tmp, "00%3d", (int) *value) ;

    } else if(*value > 9) {
      sprintf( vl_tmp, "000%2d", (int) *value) ;
    } else {
      sprintf( vl_tmp, "0000%1d", (int) *value) ;
    }
    rf_cmd[3]= vl_tmp[0] ;
    rf_cmd[4]= vl_tmp[1] ;
    rf_cmd[5]= vl_tmp[2] ;
    rf_cmd[6]= vl_tmp[3] ; 
    rf_cmd[7]= vl_tmp[4] ;
    rf_cmd[8]= 0 ;
  }

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;
    

  if (sscanf(rf_cmd, "FL%1c%5f", waste, &temp) < 1)
    return -1;

  *value = (double) temp  ;

  return 0;
}

int updateRFSetPosition(int fd, double *value) {

  char rf_cmd[32] ;
  char vl_tmp[6] ;
  int ret_read_tmp ;

  rf_cmd[0]=  'F' ;
  rf_cmd[1]=  'S' ;
  rf_cmd[2]=  '0' ;

  if(*value > 9999) {
    sprintf( vl_tmp, "%5d", (int) *value) ;
  } else if(*value > 999) {
    sprintf( vl_tmp, "0%4d", (int) *value) ;
  } else if(*value > 99) {
    sprintf( vl_tmp, "00%3d", (int) *value) ;
  } else if(*value > 9) {
    sprintf( vl_tmp, "000%2d", (int) *value) ;
  } else {
    sprintf( vl_tmp, "0000%1d", (int) *value) ;
  }
  rf_cmd[3]= vl_tmp[0] ;
  rf_cmd[4]= vl_tmp[1] ;
  rf_cmd[5]= vl_tmp[2] ;
  rf_cmd[6]= vl_tmp[3] ; 
  rf_cmd[7]= vl_tmp[4] ;
  rf_cmd[8]= 0 ;

  if ((ret_read_tmp= commRF(fd, rf_cmd)) < 0)
    return ret_read_tmp;
  
  return 0;
}
