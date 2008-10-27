#include "ApnCamera.h"
#include "libapogee.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#include "ApnCamera.h"
#include "libapogee.h"

/* master object */
static CApnCamera *alta;

static void checkalta();

/* connect to the camera and perform any one-time setup.
 * return 0 if ok else -1
 */
int
ApnGlueOpen(unsigned int id)
{
	char *ipaddress;
	unsigned int uaddr=0;
	unsigned int ip[4];

	switch (id)
	{
		// USB
		case APOGEE_USB_ONLY:
			uaddr = 1;
			break;

		// Ethernet
		case APOGEE_ETH_ONLY:
			ipaddress = getenv("APOGEE_ALTA_IP");
        		if (ipaddress == NULL) 
				return (-1);

		// Try Ehternet, if not then USB
		default:
			ipaddress = getenv("APOGEE_ALTA_IP");
        		if (ipaddress == NULL) 
			{
            			uaddr = 1;
        		}
			else 
			{
            			sscanf(ipaddress,"%d.%d.%d.%d",ip,ip+1,ip+2,ip+3);
            			uaddr = ip[0]*256*256*256 + ip[1]*256*256 + ip[2]*256 + ip[3];
        		}
			break;
	}

        alta = (CApnCamera *)new CApnCamera();
        if (!alta->InitDriver(uaddr,80,0) || !alta->ResetSystem())
            return (-1);

	alta->write_LedState (0, Apn_LedState_Expose);
	alta->write_LedState (1, Apn_LedState_AtTemp);

	return (0);
}

/* return the max values for the camera.
 */
void
ApnGlueGetMaxValues (double * /*exptime*/, int *roiw, int *roih, int *osw,
     int *osh, int *binw, int *binh, int *shutter, double *mintemp)
{
	checkalta();

	*roiw = alta->m_ApnSensorInfo->m_ImagingColumns;
	*roih = alta->m_ApnSensorInfo->m_ImagingRows;
	*osw =  alta->m_ApnSensorInfo->m_OverscanColumns;
	*osh =  alta->m_ApnSensorInfo->m_OverscanRows;
	*binw = alta->read_MaxBinningH();
	*binh = alta->read_MaxBinningV();

	*shutter = 1;		/* TODO */

	*mintemp = alta->m_ApnSensorInfo->m_CoolingSupported ? -30 : 0; /*TODO*/
}

/* set parameters for subsequent exposures.
 * if anything can't be done return -1 with explanation in whynot[], else 0.
 * also return final image size in net binned pixels, allowing for overscan,
 * binning etc.
 */
int
ApnGlueSetExpGeom (int roiw, int roih, int osw, int osh, int binw, int binh, int roix, int roiy, int *impixw, int *impixh, char whynot[])
{
	int maxw, maxh;

	checkalta();

	maxw = alta->m_ApnSensorInfo->m_ImagingColumns;
	maxh = alta->m_ApnSensorInfo->m_ImagingRows;

	/* be kind */
	if (!binw) binw = 1;
	if (!binh) binh = 1;
	if (!roiw) roiw = maxw;
	if (!roih) roih = maxh;

	/* check maxes */
	if (roiw > maxw) {
	    sprintf (whynot, "Max width is %d", maxw);
	    return (-1);
	}
	if (roih > maxh) {
	    sprintf (whynot, "Max height is %d", maxh);
	    return (-1);
	}

	/* check overscan */
	if (osw > 0 || osh > 0) {
	    int maxosw = alta->read_OverscanColumns();
	    int maxosh = alta->m_ApnSensorInfo->m_OverscanRows;

	    if (osw > maxosw) {
		sprintf (whynot, "Max overscan columns is %d", maxosw);
		return (-1);
	    }
	    if (osh > maxosh) {
		sprintf (whynot, "Max overscan rows is %d", maxosh);
		return (-1);
	    }
	    if (roix > 0 || roiw < maxw || roiy > 0 || roih < maxh) {
		sprintf (whynot, "Can not overscan with windowing");
		return (-1);
	    }

	    roiw += osw;
	    roih += osh;
	    alta->write_DigitizeOverscan(1);
	} else
	    alta->write_DigitizeOverscan(0);


	/* ok */
	alta->write_RoiStartX (roix);
	alta->write_RoiStartY (roiy);
	alta->write_RoiPixelsH (roiw/binw);
	alta->write_RoiPixelsV (roih/binh);

	alta->write_RoiBinningH (binw);
	alta->write_RoiBinningV (binh);

	*impixw = roiw/binw;
	*impixh = roih/binh;

	return (0);
}

/* start an exposure with the given duration (secs) and whether to open shutter.
 * update *exptime with the actual exposure time (may have been bumped to min).
 * return 0 if ok, else -1
 */
int
ApnGlueStartExp (double *exptime, int shutter)
{
	double minsecs;

	checkalta();

	minsecs = alta->m_ApnSensorInfo->m_MinSuggestedExpTime/1000.0;
	if (*exptime < minsecs)
	    *exptime = minsecs;

	alta->write_ImageCount(1);
	return (alta->Expose(*exptime, shutter) ? 0 : -1);
}

/* abort an exposure */
void
ApnGlueExpAbort(void)
{
	checkalta();

	alta->StopExposure(0);

}

/* return 1 if the current exposure is complete, else 0
 */
int
ApnGlueExpDone(void)
{
	checkalta();

	return (alta->read_ImagingStatus() == Apn_Status_ImageReady);
}


/* read pixels from the camera into the given buffer of length nbuf.
 * return 0 if ok else -1 with reason in whynot[].
 */
int
ApnGlueReadPixels (unsigned short *buf, int nbuf, char whynot[])
{
	unsigned short w, h;
	unsigned long c, r;

	checkalta();

	r = alta->GetImageData (buf, w, h, c);

	if (r != CAPNCAMERA_SUCCESS) 
	{
	    sprintf (whynot, "GetImageData returned %ld\n", r);
	    return (-1);
	}

	if (w*h != nbuf) 
	{
	    sprintf (whynot, "Expecting %d pixels but found %d", nbuf, w*h);
	    return (-1);
	}

	return (0);
}

/* set cooler target to given value, C
 * turn cooler off it target is 0.
 */
void
ApnGlueSetTemp (double C)
{
	checkalta();

	if (C == 0) {
	    alta->write_CoolerEnable(0);
	} else {
	    alta->write_CoolerEnable(1);
	    alta->write_CoolerSetPoint(C);
	}
}


/* fetch current cooler temp, C, and return status:
 *   0 = off
 *   1 = ramping to set point
 *   2 = at set point
 */
int
ApnGlueGetTemp (double *Cp)
{
	int status;

	checkalta();

	*Cp = alta->read_TempCCD();
	status = alta->read_CoolerStatus();
	if (status > 2)
	    status = 2;
	return (status);
}

/* set fan speed: 0..3 */
void
ApnGlueSetFan (int speed)
{
	alta->write_FanMode(0);		/* helps to do this first */
	alta->write_FanMode(speed&3);
}

/* return pointers to sensor and camera names.
 * N.B. caller should neither modify nor free these strings.
 */
void
ApnGlueGetName(char **sensor, char **camera)
{
	*sensor = alta->m_ApnSensorInfo->m_Sensor;
	*camera = alta->m_ApnSensorInfo->m_CameraModel;
}

/* check that the alta object has been created, else complain and exit.
 */
static void
checkalta()
{
	if (!alta) {
	    fprintf (stderr, "Bug! Alta used before open\n");
	    exit(1);
	}
}

