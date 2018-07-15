#include "nsdownload.h"
#include "kaf_constants.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include  <unistd.h>
#include <string.h>
#include "nsdebug.h"
#include <math.h>

void NsDownload::setFrameYBinning(int binning) {
			ctx->imgp->ybinning = binning;	

}

void NsDownload::setFrameXBinning(int binning) {
			ctx->imgp->xbinning = binning;	

}
void NsDownload::setImgSize(int siz) {
	rd->imgsz = siz;
}

void NsDownload::setNumExp(int n){
	ctx->nexp = n;		 	
}

void NsDownload::nextImage() {
		ctx->imgseq++;	
}
int NsDownload::getImgSeq(void) {
	return 		 ctx->imgseq;
	
}
void NsDownload::setSetTemp (float temp) {
		ctx->imgp->settemp = temp;	

}

void NsDownload::setActTemp(float temp) {
			ctx->imgp->acttemp = temp;	

}

void NsDownload::setExpDur(float exp){
				ctx->imgp->exp = exp;	

}

void NsDownload::setIncrement(int inc) {
	ctx->increment = inc;	
}

void NsDownload::setFbase(const char * name) {
	strncpy(ctx->fbase,name, 64) ;	
}

void NsDownload::doDownload() {
	
	  ctx->imgp->expdate = time(NULL);
	  std::unique_lock<std::mutex> ulock(mutx);

		do_download = 1;
		go_download.notify_all();
}

bool NsDownload::inDownload() {
	return in_download||do_download;	
} 


bool NsDownload::getDoDownload() {
	return do_download;	
} 

unsigned char * NsDownload::getBuf() {
	if (retrBuf == NULL) return NULL;
	return retrBuf->buffer;	
}


size_t NsDownload::getBufImageSize() {
	if (retrBuf == NULL) return 0;

	return retrBuf->imgsz;	
}


void NsDownload::setImgWrite(bool w) {
	write_it = w;	
}

void NsDownload::freeBuf() {
	if (!retrBuf) return;
	if (retrBuf->buffer) free(retrBuf->buffer);
	retrBuf->buffer = NULL;
	retrBuf = NULL;
}

void NsDownload::setInterrupted(){
	std::unique_lock<std::mutex> ulock(mutx);
	interrupted = 1;
	go_download.notify_all();
}

void NsDownload::setZeroReads(int zeroes){
	zero_reads = zeroes;
}
 int NsDownload::getActWriteLines(){
		 return writelines;	
}

int NsDownload::downloader() 
{
			//struct ftdi_context * ftdid = cn->->getDataChannel();
			//struct ftdi_transfer_control * ctl;

      int rc2;
      int hardloop = 20; 
      int sleepage = 1000;
    
			int download =1;
			if (rd->nread > rd->bufsiz) {
            DO_ERR("image too large %d\n", rd->nread);
		     		return (-1);
			}
    	while((rc2 = cn->readData(rd->buffer+rd->nread, cn->getMaxXfer())) == 0 && hardloop > 0) {
				if (hardloop % 5  == 0) DO_INFO("W%d\n",hardloop);
    		usleep(sleepage);
				sleepage *= 2;
				if (sleepage > 100000) sleepage = 100000;
				hardloop--;
			}
      /* ctl = ftdi_read_data_submit(ftdid, rd->buffer+rd->nread,  maxxfer);
      if (ctl == NULL) {
      	DO_ERR( "unable to submit read: %d (%s)\n", rc2, ftdi_get_error_string(ftdid));
				return (-1);
      }
      DO_INFO( ".");
      rc2 = ftdi_transfer_data_done(ctl);
      */
   		if (rc2 < 0 ) {
        DO_ERR("unable to read download data: %d\n", rc2);
				return (-1);
			}
			rd->nread += rc2;
			if (rc2 != cn->getMaxXfer()) {
				DO_INFO("short! %d %d\n", rd->nblks, rc2);
			}		
			
			if (rc2 == 0) {
				readdone = 1;
			} else { rd->nblks ++; }
			if (rd->nread >= rd->imgsz) {
				readdone = 1;	
			}
			if (readdone) {
			  download=0;
				lastread = rc2;
			
				rb = rdd;
				retrBuf = &rb;
				rd->buffer = NULL;
			}	
			return download;		
}


/*

SIMPLE  =                    T                                                  
BITPIX  =                   16 /8 unsigned int, 16 & 32 int, -32 & -64 real     
NAXIS   =                    2 /number of axes                                  NAXIS1  =                 3326 /fastest changing axis                           
NAXIS2  =                 2504 /next to fastest changing axis                   BSCALE  =   1.0000000000000000 /physical = BZERO + BSCALE*array_value           
BZERO   =   32768.000000000000 /physical = BZERO + BSCALE*array_value           
DATE-OBS= '2016-03-18T23:22:47' /YYYY-MM-DDThh:mm:ss observation start, UT      
EXPTIME =   1.0000000000000000 /Exposure time in seconds                        EXPOSURE=   1.0000000000000000 /Exposure time in seconds                        
SET-TEMP=  -7.0000000000000000 /CCD temperature setpoint in C                   
CCD-TEMP=  -4.1500000000000004 /CCD temperature at start of exposure in C       
XPIXSZ  =   5.4000000000000004 /Pixel Width in microns (after binning)          
YPIXSZ  =   5.4000000000000004 /Pixel Height in microns (after binning)         
XBINNING=                    1 /Binning factor in width                         
YBINNING=                    1 /Binning factor in height                        
XORGSUBF=                    0 /Subframe X position in binned pixels            
YORGSUBF=                    0 /Subframe Y position in binned pixels            
READOUTM= 'Raw     ' /          Readout mode of image                           
IMAGETYP= 'LIGHT   ' /          Type of image                                   
SWCREATE= 'Celestron AstroFX V1.06' /Name of software that created the image    
COLORTYP=                    2                                                  
XBAYROFF=                    0                                                  
YBAYROFF=                    0                                                  
OBJECT  = 'test3   '                                                            
INSTRUME= 'Celestron Nightscape 8300C' /instrument or camera used               
SWOWNER = 'Dirk    ' /          Licensed owner of software                      
END                                                                                                                                                                     
*/

void NsDownload::fitsheader(int x, int y, char * fbase, struct img_params * ip)
{
	FILE * f = img;
	char datebuf[25] = "''";
	char fbase2[64];
	struct tm t;
	gmtime_r(&ip->expdate, &t);
	strncpy (fbase2, fbase, 12);
	fbase2[12] = 0;
	snprintf(datebuf, 25, "'%4d-%02d-%02dT%02d:%02d:%02d'", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	int nl = 0;
	nl++; fprintf(f, "%-8s=%21s %-49s", "SIMPLE", "T", "");	
	nl++; fprintf(f, "%-8s=%21s %-49s", "BITPIX", "16", "/8 unsigned int, 16 & 32 int, -32 & -64 real");	
	nl++; fprintf(f, "%-8s=%21s %-49s", "NAXIS", "2", "/number of axes");
	nl++; fprintf(f, "%-8s=%21d %-49s", "NAXIS1", x, "/fastest changing axis");
	nl++; fprintf(f, "%-8s=%21d %-49s", "NAXIS2", y, "/next to fastest changing axis");
 	nl++; fprintf(f, "%-8s=%22s %-48s", "DATE-OBS", datebuf, "/YYYY-MM-DDThh:mm:ss observation start, UT");
  nl++; fprintf(f, "%-8s=%21.12f %-49s", "BZERO", 32768.0, "/physical = BZERO + BSCALE*array_value");
  nl++; fprintf(f, "%-8s=%21.16f %-49s", "EXPTIME", ip->exp, "/Exposure time in seconds");
  nl++; fprintf(f, "%-8s=%21.16f %-49s", "SET-TEMP", ip->settemp, "/CCD temperature setpoint in C");
  nl++; fprintf(f, "%-8s=%21.16f %-49s", "CCD-TEMP", ip->acttemp, "/CCD temperature at start of exposure in C");
  nl++; fprintf(f, "%-8s=%21.16f %-49s", "XPIXSZ", (float)ip->xbinning*5.40, "/Pixel Width in microns (after binning) ");
  nl++; fprintf(f, "%-8s=%21.16f %-49s", "YPIXSZ", (float)ip->ybinning*5.40, "/Pixel Height in microns (after binning) ");
 	nl++; fprintf(f, "%-8s=%21d %-49s", "XBINNING", ip->xbinning, "/Binning factor in width");
 	nl++; fprintf(f, "%-8s=%21d %-49s", "YBINNING", ip->ybinning, "/Binning factor in height");
 	nl++; fprintf(f, "%-8s=%21s %-49s", "XORGSUBF", "0", "/Subframe X position in binned pixels");
 	nl++; fprintf(f, "%-8s=%21s %-49s", "YORGSUBF", "0", "/Subframe Y position in binned pixels");
 	nl++; fprintf(f, "%-8s= '%-8s' %-59s", "READOUTM", "Raw", "/          Readout mode of image");	
 	nl++; fprintf(f, "%-8s= '%-8s' %-59s", "IMAGETYP", "LIGHT", "/          Type of image");	
  nl++; fprintf(f, "%-8s= '%-13s' %-54s", "SWCREATE", "nstest-u 0.90", "/Name of software that created the image");	
	nl++; fprintf(f, "%-8s=%21s %-49s", "COLORTYP", "2", "");	
 	nl++; fprintf(f, "%-8s= '%-4s' %-63s", "BAYERPAT", "BGGR", "/          Baye pattern");	
 	nl++; fprintf(f, "%-8s=%21s %-49s", "XBAYROFF", "0", "");
 	nl++; fprintf(f, "%-8s=%21s %-49s", "YBAYROFF", "0", "");
  nl++; fprintf(f, "%-8s= '%-12s' %-55s", "OBJECT", fbase, "");	
  nl++; fprintf(f, "%-8s= '%-26s' %-41s", "INSTRUME", "Celestron Nightscape 8300C", "/instrument or camera used");	
  nl++; fprintf(f, "%-80s", "END");
  for (int x = nl; x < 36; x++) fprintf(f, "%-80s", "");
  
}


void NsDownload::writedownload(int pad, int cooked)
{
	char fname[64];
	char fnab[64];
	unsigned char linebuf[KAF8300_MAX_X*2];
	const char * extn;
  int procid = getpid();
	int nwrite = 0;
	if (cooked) {
		extn = ".fts";
	} else {
	 	extn = ".bin";	
	}
	if (retrBuf == NULL) {
		DO_DBG("%s", "no image");
		return;	
	}
	DO_INFO("done! blks %d totl %d last %d\n", retrBuf->nblks, retrBuf->nread,lastread);
	if (strlen(ctx->fbase) == 0) {
		snprintf(ctx->fbase, 64, "img_%d", procid);
	}
	if (ctx->increment) {
		snprintf(fname, 64, "%s_%d%s", ctx->fbase, ctx->imgseq, extn);
		snprintf(fnab, 64, "%s_%d", ctx->fbase, ctx->imgseq);

	} else {
		snprintf(fname, 64, "%s%s",ctx->fbase, extn);
		strcpy(fnab, ctx->fbase);
  }
	printf("%s\n",fname);

	img = fopen(fname, "wb");
	if (img == NULL) {
		DO_ERR( "cannot create file %s, error %s\n", fname, strerror(errno));
	}
	if (pad) {
		nwrite = retrBuf->imgsz;
	} else {
		nwrite = retrBuf->nread;
	}
	if (!cooked) {
		
		if (img) fwrite (retrBuf->buffer, sizeof(char), nwrite , img);
	} else if (img) {
		int actlines = nwrite / (KAF8300_MAX_X*2);
	  //int rem = nwrite % (KAF8300_MAX_X*2);
	  
		fitsheader(KAF8300_ACTIVE_X, actlines, fnab, ctx->imgp);
		int nwriteleft = nwrite;
		unsigned char * bufp = retrBuf->buffer;
		writelines = 0;
	  while (nwriteleft >= (KAF8300_MAX_X*2)) {
	  	swab (bufp + (KAF8300_POSTAMBLE*2),  linebuf, KAF8300_ACTIVE_X*2 );
			fwrite (linebuf, sizeof(char), KAF8300_ACTIVE_X*2 , img);
			bufp +=  KAF8300_MAX_X*2;
			nwriteleft -= KAF8300_MAX_X*2;
			writelines++;
	  }
	 DO_INFO( "wrote %d lines\n", writelines);
	}	 
	if (img) fclose(img);	
}



void NsDownload::copydownload(unsigned char *buf, int xstart, int xlen, int xbin, int pad, int cooked)
{
	unsigned char linebuf[KAF8300_MAX_X*2];
	bool forwards = true;
	bool rms = false;
	int binning = xbin;
	uint8_t * dbufp = buf;
	uint8_t * bufp;
	int nwrite = 0;
	
	if (retrBuf == NULL) {
		DO_DBG("%s", "no image");
		return;	
	}
	DO_INFO("done! blks %d totl %d last %d\n", retrBuf->nblks, retrBuf->nread,lastread);
			
	if (!cooked) {
		if (pad) {
			nwrite = retrBuf->imgsz;
		} else {
			nwrite = retrBuf->nread;
		}
		memcpy (dbufp, retrBuf->buffer, nwrite);
	} else {
		//int actlines = nwrite / (KAF8300_MAX_X*2);
	  //int rem = nwrite % (KAF8300_MAX_X*2);
	  nwrite = retrBuf->nread;
		int nwriteleft = nwrite;
		if (forwards) {
			bufp = retrBuf->buffer;
			//dbufp =  (dbufp +(KAF8300_ACTIVE_X*2*IMG_Y)) - (KAF8300_ACTIVE_X*2);
		} else {
			bufp = (retrBuf->buffer + nwrite) - (KAF8300_MAX_X*2);
			dbufp =  (dbufp +(KAF8300_ACTIVE_X*2*IMG_Y)) - (KAF8300_ACTIVE_X*2);
		}
		writelines = 0;
	  while (nwriteleft >= (KAF8300_MAX_X*2)) {
	  //swab (bufp + (KAF8300_POSTAMBLE*2),  linebuf, KAF8300_ACTIVE_X*2 );
	   //memcpy (dbufp, linebuf, KAF8300_ACTIVE_X*2);
	    if (binning > 1) {
	    	uint8_t * lbufp = bufp + (KAF8300_POSTAMBLE*2) + xstart*2;
	    	int len = xlen * 2;
	    	int linelen = 0;
	    	while (len > 0) {
	    		short px[4];
	    		long long pxsq =0;
	    		long pxav =0;
	    		short pxa;
	    		memcpy(px, lbufp,binning * 2);
	    		for (int a = 0; a < binning; a++) {
	    			pxav += px[a];
	    			pxsq	+= px[a]*px[a];
	    		}
	    		pxav /= binning;
	    		pxsq /= binning;
	    		if(rms) {
	    			pxa = round(sqrt((double)	pxsq));
	    			
	    		} else {
	    			pxa = pxav;	
	    		}
	    		memcpy (linebuf + linelen, &pxa, 2);
	    		linelen += 2;
	    		lbufp += 2*binning;
	    		len -= 2* binning;
				}
				memcpy (dbufp, linebuf, (xlen*2)/binning);

	    } else {
	  		memcpy (dbufp, bufp + (KAF8300_POSTAMBLE*2) + xstart*2, xlen * 2 ); //KAF8300_ACTIVE_X*2 );
	  	}
			if (forwards) { 
				bufp +=  KAF8300_MAX_X*2;
				dbufp +=(xlen*2)/binning;
			} else {
				bufp -=  KAF8300_MAX_X*2;
				dbufp-=(xlen*2)/binning;
			}
			nwriteleft -= KAF8300_MAX_X*2;
			writelines++;
	  }
	 DO_INFO( "wrote %d lines\n", writelines);
	}	 
}

int NsDownload::purgedownload() 
{
		int rc2;
		rc2 = cn->readData(rd->buffer, rd->bufsiz);
		if (rc2 < 0 ) {
			DO_ERR( "purge: unable to read: %d \n", rc2);
			return (-1);
		}
		if (rc2 > 0) {
			DO_ERR("purge: spare read %d\n", rc2);
		  rc2 = cn->purgeData();
		  if (rc2 < 0 ) {
		    DO_ERR( "unable to purge: %d\n", rc2);
				return (-1);
			}
		}	
		return 0;
}


int NsDownload::fulldownload() 
{
		int rc2;
		
		rc2 = cn->readData(rd->buffer+rd->nread, rd->bufsiz - rd->nread);
		if (rc2 < 0 ) {
			DO_ERR( "unable to read: %d\n", rc2);
			return (-1);
		}
		if (rc2 > 0) {
			DO_INFO("read %d\n", rc2);
		  rd->nread += rc2;
		  rd->nblks += rc2/65536;
		  DO_INFO("read %d tot %d\n", rc2, rd->nread);

		}	
		return rc2;
}


void NsDownload::initdownload()
{
	  long imgszmax = KAF8300_MAX_X*0x9ca*2 + DEFAULT_CHUNK_SIZE;
		readdone = 0;
		rd->nread = 0;
		if(!rd->buffer) {
			rd->buffer = (unsigned char *)malloc(imgszmax);
		}
		memset(rd->buffer, 0, imgszmax);

		rd->bufsiz = imgszmax;
		rd->nblks = 0;	
}


// static void  download_thread(NsDownload * d) {
//	d->trun();	
//}


void NsDownload::trun()
{

	int maxxfer;
  int zeroes = 0;
	//struct ftdi_context * ftdid = cn->getDataChannel();
	maxxfer = cn->getMaxXfer();

	do  {
		DO_INFO("%s\n", "initdownload");
		std::unique_lock<std::mutex> ulock(mutx);

		while(!do_download && !interrupted) go_download.wait(ulock);
		if(interrupted) break;
		initdownload();

	
		if (do_download && !in_download) {
			//ftdi_usb_close(ftdid);
			//maxxfer = opendownload(ftdid, ctx->dev);
			in_download = 1;
			ctx->imgseq++;
			zeroes = 0;
		}
	  while (in_download && !interrupted) {
	  	//int rc2= cn->setDataRts();;
    	//if (rc2  < 0) {
      //  		DO_ERR( "unable to set rts: %d\n", rc2);
    	//}	
    	int down = 0;
	    if (zero_reads > 1) 
	  		down = fulldownload();
	  	else
	  		down = downloader();
	  	if (down < 0) {
	  		DO_ERR( "unable to read download: %d\n", down);
	  		do_download = 0;
	  		in_download = 0;
	  		continue;
	  	}
	  	if (rd->nread < rd->imgsz) {
	  		if (down == 0 && rd->nread > 0) {
    			zeroes++;
	  		}
	  		if (zeroes < zero_reads) continue;
	  	}
	  	int pad = 0;
	  	if (rd->nread != rd->imgsz) {
	  		int actlines = rd->nread / (KAF8300_MAX_X*2);
	  		int rem = rd->nread % (KAF8300_MAX_X*2);
	  		DO_INFO( "siz %d read %d act lines %d rem %d\n",  rd->imgsz,rd->nread, actlines, rem);
	  		if (rd->imgsz - rd->nread < KAF8300_MAX_X * 5) {
	  			pad = 1;
	  		}
	    }	
	    lastread = down;
	    	   // IDLog("foop\n");

	    if (zero_reads > 1) {
	    	rb = rdd;
	    	retrBuf = &rb;
	    	rd->buffer = NULL;
	    }
	    //IDLog("retr %p buf %p \n", retrBuf, rb.buffer);
	    if(write_it) writedownload(pad, 0);
			
	  	do_download = 0;
	  	in_download = 0;
	  }
	  if (!in_download && !do_download) {
			initdownload();  	
	  	purgedownload ();
	  }
  } 	while(ctx->nexp >= ctx->imgseq && !interrupted);
  		DO_DBG("%s\n", "thread done");

}


void NsDownload::startThread(void) {
			interrupted = 0;
			sched_param sch_params;
      sch_params.sched_priority = 3;
    	downthread = new std::thread (&NsDownload::trun, this); //(&NsDownload::trun, this);
			pthread_setschedparam(downthread->native_handle(), SCHED_FIFO, &sch_params);		

}

void NsDownload::stopThread(void) {
	setInterrupted();
	downthread->join();
}
