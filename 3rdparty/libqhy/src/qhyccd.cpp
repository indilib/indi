/*
 QHYCCD SDK
 
 Copyright (c) 2014 QHYCCD.
 All Rights Reserved.
 
 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.
 
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.
 
 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 
 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

/*! @file qhyccdstruct.h
    
    @brief QHYCCD SDK struct define
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <opencv/cv.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "qhyccdhex2cam.h"
#include "qhybase.h"
#include "qhy5lii_c.h"
#include "ic8300.h"
#include "qhy21.h"
#include "qhy22.h"
#include "qhy8l.h"
#include "qhy10.h"
#include "qhy12.h"
#include "qhy9s.h"
#include "qhy8pro.h"
#include "qhy11.h"
#include "qhy23.h"
#include "img2p.h"
#include "qhy6.h"
#include "qhy5ii.h"
#include "qhy5lii_m.h"
#include "qhy8.h"
#include "qhyxxx.h"
#include "simu.h"

/* 
  This is the maximum number of qhyccd cams.
 */
#define MAXDEVICES (100)

/*
  This is the length for camera ID
 */
#define ID_STR_LEN (0x20)


/**
 * @brief sturct cydef define
 *
 * Global struct for camera's class and some information
 */
struct cydev
{
    qhyccd_device *dev;          //!< Camera deivce
    qhyccd_handle *handle;       //!< Camera control handle
    unsigned short vid;          //!< Vendor ID
    unsigned short pid;          //!< Product ID
    unsigned char is_open;       //!< When device is opened, val = 1
    char id[64];                 //!< The camera's id
    QHYBASE *qcam;               //!< Camera class pointer
};

/* Global struct for camera's vendor id */
unsigned short camvid[MAXDEVICES] = 
{
    0x1618,0x1618,0x1618,0x1618,0x1618,0x1618,0x1618,0x1618,0x1618,
    0x1618,0x1618,0x1618,0x1618,0X1618,0x1618,0x1618
};

/* Global struct for camera io product id */
unsigned short campid[MAXDEVICES] =
{
    0x0921,0x8311,0x6741,0x6941,0x6005,0x1001,0x1201,0x8301,0x6003,
    0x1101,0x8141,0x2851,0x025a,0x6001,0x0931,0xffff
};

/* Global struct for camera'firmware product id */
unsigned short fpid[MAXDEVICES] =
{
    0x0920,0x8310,0x6740,0x6940,0x6004,0x1000,0x1200,0x8300,0x6002,
    0x1100,0x8140,0x2850,0x0259,0x6000,0x0930,0xffff
};

/* Global var for include vid,pid,qhybase clase... */
static struct cydev cydev[MAXDEVICES];

/* Global var for Number of cams */
static int numdev;

/* Global var for conected camera list */
static libusb_device **list;


static int qhyccd_handle2index(qhyccd_handle *h)	
{
    int index = QHYCCD_ERROR_INDEX;
    int i;
    for ( i = 0; i < numdev; ++i ) 
    {
        if(h == cydev[i].handle)
        {
            index = i;
            break;
        }
    }
    return index;
}

static int ReleaseQHYCCDClass(int index)
{  
    if(cydev[index].qcam)
    {
        delete(cydev[index].qcam);
        return QHYCCD_SUCCESS;
    }
    return QHYCCD_ERROR;
}

int InitQHYCCDResource()
{
    libusb_init(NULL);
    return QHYCCD_SUCCESS;
}

int ReleaseQHYCCDResource()
{  
    int i = 0;
    int ret = QHYCCD_ERROR;
    for(;i < numdev;i++)
    {
        ret = ReleaseQHYCCDClass(i);
        if(ret != QHYCCD_SUCCESS)
        {
            break;
        }
    }
 
    libusb_exit(NULL);
    return ret;
}

static int cyusb_get_device_descriptor(qhyccd_handle *h, struct libusb_device_descriptor *desc)
{
    qhyccd_device *tdev = libusb_get_device(h);
    return ( libusb_get_device_descriptor(tdev,desc));
}

static int QHY5IISeriesMatch(qhyccd_handle *handle)
{
    /* qhy5ii series model */
    int model;
    /* color cam or mono cam */
    int color;
    int ret;
    /* mem for cam info */
    unsigned char data[16];

    if(handle)
    {
        ret = libusb_control_transfer(handle,0xC0,0xCA,0,0x10,data,0x10,2000);

        if(ret == 0x10)
        {
            model = data[0];
            color = data[1];
       
            if(model == 1)
            {
                return DEVICETYPE_QHY5II;
            }
            else if(model == 6 && color == 0)
            {
                return DEVICETYPE_QHY5LII_M;
            }
            else if(model == 6 && color == 1)
            {
                return DEVICETYPE_QHY5LII_C;
            }
        }
	return ret;
    }
    return QHYCCD_ERROR;
}

static int QHYCCDSeriesMatch(int index,qhyccd_handle *handle)
{
    int ret = QHYCCD_ERROR_NOTSUPPORT;

    switch(cydev[index].pid)
    {
        case 0x0921:
        {
            ret = QHY5IISeriesMatch(handle);
            break;
        }
        case 0x8311:
        {
            ret = DEVICETYPE_IC8300;
            break;
        }
        case 0x6741:
        {
            ret = DEVICETYPE_QHY21;
            break;
        }
        case 0x6941:
        {
            ret = DEVICETYPE_QHY22;
            break;
        }
        case 0x6005:
        {
            ret = DEVICETYPE_QHY8L;
            break;
        }
        case 0x1001:
        {
            ret = DEVICETYPE_QHY10;
            break;
        }
        case 0x1201:
        {
            ret = DEVICETYPE_QHY12;
            break;
        }
        case 0x8301:
        {
            ret = DEVICETYPE_QHY9S;
            break;
        }
        case 0x6003:
        {
            ret = DEVICETYPE_QHY8PRO;
            break;
        }
        case 0x1101:
        {
            ret = DEVICETYPE_QHY11;
            break;
        }
        case 0x8141:
        {
            ret = DEVICETYPE_QHY23;
            break;
        }
        case 0x2851:
        {
            ret = DEVICETYPE_IMG2P;
            break;
        }
        case 0x025a:
        {
            ret = DEVICETYPE_QHY6;
            break;
        }
        case 0x6001:
        {
            ret = DEVICETYPE_QHY8;
            break;
        }
        case 0x0931:
        {
            ret = DEVICETYPE_QHYXXX;
            break;
        }
        case 0xffff:
        {
            ret = DEVICETYPE_SIMULATOR;
            break;
        }
        default:
        {
            fprintf(stderr,"current pid is 0x%x\n",cydev[index].pid);
            ret = DEVICETYPE_UNKNOW;
        }
        break;
    }

    return ret;
}

static int DeviceIsQHYCCD(int index,qhyccd_device *d)
{
    int i;
    int found = 0;
    struct libusb_device_descriptor desc;
    int vid;
    int pid;

    libusb_get_device_descriptor(d, &desc);
    vid = desc.idProduct;
    pid = desc.idProduct;

    for ( i = 0; i < MAXDEVICES; ++i ) 
    {
        if ( (camvid[i] == desc.idVendor) && (campid[i] == desc.idProduct) )
	{
            cydev[index].vid = desc.idVendor;
            cydev[index].pid = desc.idProduct;
            found = 1;
	    break;
        }
    }
    return found;
}

static int GetIdFromCam(qhyccd_handle *handle,char *id)
{
    int model,color,i;
    int ret = QHYCCD_ERROR;
    unsigned char data[ID_STR_LEN];
    char str[17];
    
    /* init the str with '\0',for some old camera */
    memset(str,'\0',17);

    if(handle)
    {
        ret = libusb_control_transfer(handle,0xC0, 0xca,0x10,0x10,data,0x10,2000);

        if(ret == 0x10)
        {         
            for(i = 0;i < 16;i++)
                sprintf(str+i,"%x",data[i]);
            id[16] = '\0';
            strcat(id,str);
        }
        else
        {
            fprintf(stderr,"get info from camera failure\n");
        }
    }
    return ret;
}

int InitQHYCCDClass(int camtype,int index)
{
    int ret;
    
    /* just for some old camera*/
    memset(cydev[index].id,'\0',32);

    switch(camtype)
    {
        case DEVICETYPE_QHY5LII_C:
        {
            cydev[index].qcam = new QHY5LII_C();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY5LII-C-",10);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY5LII_M:
        {
            cydev[index].qcam = new QHY5LII_M();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY5LII-M-",10);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY5II:
        {
            cydev[index].qcam = new QHY5II();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY5II-M-",9);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_IC8300:
        {
            cydev[index].qcam = new IC8300();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"IC8300-M-",9);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY21:
        {
            cydev[index].qcam = new QHY21();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY21-M-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY22:
        {
            cydev[index].qcam = new QHY22();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY22-M-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY8L:
        {
            cydev[index].qcam = new QHY8L();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY8L-C-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY10:
        {
            cydev[index].qcam = new QHY10();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY10-C-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY12:
        {
            cydev[index].qcam = new QHY12();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY12-C-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY9S:
        {
            cydev[index].qcam = new QHY9S();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY9S-M-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY8PRO:
        {
            cydev[index].qcam = new QHY8PRO();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY8PRO-C-",10);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY11:
        {
            cydev[index].qcam = new QHY11();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY11-M-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY23:
        {
            cydev[index].qcam = new QHY23();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY23-M-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_IMG2P:
        {
            cydev[index].qcam = new IMG2P();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"IMG2P-M-",8);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY6:
        {
            cydev[index].qcam = new QHY6();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY6-M-",7);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHY8:
        {
            cydev[index].qcam = new QHY8();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHY8-C-",7);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_QHYXXX:
        {
            cydev[index].qcam = new QHYXXX();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"QHYXXX-",7);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        case DEVICETYPE_SIMULATOR:
        {
            cydev[index].qcam = new SIMU();
            if(cydev[index].qcam != NULL)
            {
                memcpy(cydev[index].id,"SIMULATOR",9);
                ret = QHYCCD_SUCCESS;
            }
            else
            {
                ret = QHYCCD_ERROR_INITCLASS;
            }
            break;
        }
        default:
        {
            fprintf(stderr,"The camtype %d is not correct\n",camtype);
            ret = QHYCCD_ERROR_NOTSUPPORT;
        }
    }
    return ret;
}

int ScanQHYCCD(void)
{
    qhyccd_device *tdev;
    qhyccd_handle *handle = NULL;
    int found = 0;
    int i;
    int ret;
    int camret;
    int nid;

    numdev = libusb_get_device_list(NULL, &list);
    if ( numdev < 0 ) 
    {
        return QHYCCD_ERROR_NO_DEVICE;
    }
    nid = 0;

    for ( i = 0; i < numdev; ++i ) 
    {
        tdev = list[i];
        ret = DeviceIsQHYCCD(nid,tdev);
        if (ret > 0) 
        {
            cydev[nid].dev = tdev;
	        ret = libusb_open(tdev, &cydev[nid].handle);

	        if (ret)
	        {
                fprintf(stderr,"Open QHYCCD error\n");
                return QHYCCD_ERROR;
	        }

           handle = cydev[nid].handle;

           camret = QHYCCDSeriesMatch(nid,handle);
           if(camret == QHYCCD_ERROR_NOTSUPPORT)
           {
               fprintf(stderr,"SDK not support this camera now\n");
               libusb_close(handle);
               continue;
           }
         
           ret = InitQHYCCDClass(camret,nid);
           if(ret != QHYCCD_SUCCESS)
           {
               fprintf(stderr,"Init QHYCCD class error\n");
               libusb_close(handle);
               continue;
           }

	       GetIdFromCam(handle,cydev[nid].id);

	       libusb_close(handle);
               cydev[nid].handle = NULL;
	       cydev[nid].is_open = 0;
	       ++nid;
	    }
    }

    numdev = nid;
    return nid;
}

int GetQHYCCDId(int index,char *id)
{
    if(numdev > 0)
    {
        if(index < numdev)
        {   
            /* just copy from global var*/
            memcpy(id,cydev[index].id,ID_STR_LEN);
            return QHYCCD_SUCCESS;
        }
    }
    return QHYCCD_ERROR;
}

qhyccd_handle *OpenQHYCCD(char *id)
{
    int i;
    /* for return value */
    int ret;

    for(i = 0;i < numdev;i++)
    {
        ret = strcmp(cydev[i].id,id);

        if(!ret)
        {
            ret = cydev[i].qcam->ConnectCamera(cydev[i].dev,&cydev[i].handle);
               
            if(ret == QHYCCD_SUCCESS)
            { 
                cydev[i].is_open = 1;
                return cydev[i].handle;
            } 
        }
        else
        {
            fprintf(stderr,"lock is %s and key is %s\n,not match\n",cydev[i].id,id);
        }
    }
    return NULL;
}

int CloseQHYCCD(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        ret = cydev[index].qcam->DisConnectCamera(handle);
        if(ret == QHYCCD_SUCCESS)
        {
            cydev[index].is_open = 0;       
        }
    }
    return ret;
}

int InitQHYCCD(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->InitChipRegs(handle);
        }
    }
    return ret;
}

int IsQHYCCDControlAvailable(qhyccd_handle *handle,CONTROL_ID controlId)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->IsChipHasFunction(controlId);
        }
    }
    return ret;
}

int IsQHYCCDColor(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->IsColorCam();
        }
    }
    return ret;
}

int IsQHYCCDCool(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->IsCoolCam();
        }
    }
    return ret;
}

int SetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId, double value)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            switch(controlId)
            {
                case CONTROL_WBR:
	        { 
                    ret = cydev[index].qcam->SetChipWBRed(handle,value);
		    break;
	        }
	        case CONTROL_WBG:
	        {
                    ret = cydev[index].qcam->SetChipWBGreen(handle,value);
		    break;
	        }
                case CONTROL_WBB:
                {
                    ret = cydev[index].qcam->SetChipWBBlue(handle,value);
                    break;
	        }
	        case CONTROL_EXPOSURE:
	        {
		    ret = cydev[index].qcam->SetChipExposeTime(handle,value);
		    break;
	        }
	        case CONTROL_GAIN:
	        {
		    ret = cydev[index].qcam->SetChipGain(handle,value);
		    break;
	        }
	        case CONTROL_OFFSET:
	        {
		    ret = cydev[index].qcam->SetChipOffset(handle,value);
		    break;
	        }
	        case CONTROL_SPEED:
	        {
                    ret = cydev[index].qcam->SetChipSpeed(handle,value);
                    break;
                }
                case CONTROL_USBTRAFFIC:
                {
                    ret = cydev[index].qcam->SetChipUSBTraffic(handle,value);
                    break;
                }  
                case CONTROL_TRANSFERBIT:
                {  
                    ret = cydev[index].qcam->SetChipBitsMode(handle,value);
                    break;
                }
                case CONTROL_ROWNOISERE:
                {
                    ret = cydev[index].qcam->DeChipRowNoise(handle,value);
                    break;
                }
                case CONTROL_MANULPWM:
                {
                    ret = cydev[index].qcam->SetChipCoolPWM(handle,value);
                    break;
                }
                default:
                {
                    ret = QHYCCD_ERROR_NOTSUPPORT;
                }
            }
        }
    }
    return ret;
}

double GetQHYCCDParam(qhyccd_handle *handle,CONTROL_ID controlId)
{
    int index = QHYCCD_ERROR_INDEX;
    double ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            switch(controlId)
            {
                case CONTROL_WBR:
                {
                    ret = cydev[index].qcam->GetChipWBRed();
                    break;
                }
                case CONTROL_WBG:
                {
                    ret = cydev[index].qcam->GetChipWBGreen();
                    break;
                }
                case CONTROL_WBB:
                {
                    ret = cydev[index].qcam->GetChipWBBlue();
                    break;
                }
                case CONTROL_EXPOSURE:
                {
                    ret = cydev[index].qcam->GetChipExposeTime();
                    break;
                }
                case CONTROL_GAIN:
                {
                    ret = cydev[index].qcam->GetChipGain();
                    break;
                }
                case CONTROL_OFFSET:
                {
                    ret = cydev[index].qcam->GetChipOffset();
                    break;
                }
                case CONTROL_SPEED:
                {
                    ret = cydev[index].qcam->GetChipSpeed();
                    break;
                }
                case CONTROL_USBTRAFFIC:
                {
                    ret = cydev[index].qcam->GetChipUSBTraffic();
                    break;
                } 
                case CONTROL_TRANSFERBIT:
                {
                    ret = cydev[index].qcam->GetChipBitsMode();
                    break;
                }
                case CONTROL_CURTEMP:
                {
                    ret = cydev[index].qcam->GetChipCoolTemp(handle);
                    break;
                }
                case CONTROL_CURPWM:
                {
                    ret = cydev[index].qcam->GetChipCoolPWM();
                    break;
                }
                default:
                {
                    ret = QHYCCD_ERROR_NOTSUPPORT;
                }
            }
        }
    }
    return ret;
}

int GetQHYCCDParamMinMaxStep(qhyccd_handle *handle,CONTROL_ID controlId,double *min,double *max,double *step)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->GetControlMinMaxStepValue(controlId,min,max,step);
        }
    }
    return ret;
}

int SetQHYCCDResolution(qhyccd_handle *handle,int width,int height)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->SetChipResolution(handle,width,height);
        }
    }
    return ret;
}

int GetQHYCCDMemLength(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->GetChipMemoryLength();
        }
    }
    return ret;
}

int ExpQHYCCDSingleFrame(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->BeginSingleExposure(handle);
        }
    }
    return ret;
}

int GetQHYCCDSingleFrame(qhyccd_handle *handle,int *w,int *h,int *bpp,int *channels,unsigned char *imgdata)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->GetSingleFrame(handle,w,h,bpp,channels,imgdata);
        }
    }
    return ret;
}

int StopQHYCCDExpSingle(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->StopSingleExposure(handle);
        }
    }
    return ret;
}

int BeginQHYCCDLive(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->BeginLiveExposure(handle);
        }
    }
    return ret;
}

int GetQHYCCDLiveFrame(qhyccd_handle *handle,int *w,int *h,int *bpp,int *channels,unsigned char *imgdata)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->GetLiveFrame(handle,w,h,bpp,channels,imgdata);
        }
    }
    return ret;
}

int StopQHYCCDLive(qhyccd_handle *handle)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->StopLiveExposure(handle);
        }
    }
    return ret;
}

int SetQHYCCDBinMode(qhyccd_handle *handle,int wbin,int hbin)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->SetChipBinMode(handle,wbin,hbin);
        }
    }
    return ret;
}

int SetQHYCCDBitsMode(qhyccd_handle *handle,int bits)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->SetChipBitsMode(handle,bits);
        }
    }
    return ret;
}

int ControlQHYCCDTemp(qhyccd_handle *handle,double targettemp)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->AutoTempControl(handle,targettemp);
        }
    }
    return ret;

}

int ControlQHYCCDGuide(qhyccd_handle *handle,int direction,int duration)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->Send2GuiderPort(handle,direction,duration);
        }
    }
    return ret;
}

int ControlQHYCCDCFW(qhyccd_handle *handle,int pos)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(handle);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->Send2CFWPort(handle,pos);
        }
    }
    return ret;
}

int SetQHYCCDTrigerMode(qhyccd_handle *handle,int trigerMode)
{
    return QHYCCD_ERROR;
}

void Bits16ToBits8(qhyccd_handle *h,unsigned char *InputData16,unsigned char *OutputData8,int imageX,int imageY,unsigned short B,unsigned short W)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(h);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            cydev[index].qcam->Bit16To8_Stretch(InputData16,OutputData8,imageX,imageY,B,W);
        }
    }
}

void HistInfo192x130(qhyccd_handle *h,int x,int y,unsigned char *InBuf,unsigned char *OutBuf)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(h);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            cydev[index].qcam->HistInfo(x,y,InBuf,OutBuf);
        }
    }
}

/*
struct BINRESOLUTION *SupportBIN(int bin)
{
    if(mainClass->camOpen)
    {
        return mainClass->IsSupportBIN(bin);
    }
}
*/

static int qhyccd_reset(libusb_device_handle *h,int cpu_enable)
{
	unsigned char reset = (cpu_enable) ? 0 : 1;
	int r;
    
	r = libusb_control_transfer(h, 0x40, 0xA0, FX2_CPUCS_ADDR, 0x00, &reset, 0x01, VENDORCMD_TIMEOUT);
	if ( r != 1 ) {
		fprintf (stderr, "ERROR: FX2 reset command failed\n");
		return -1;
	}
    
	return 0;
}

static int qhyccd_load_vendax(libusb_device_handle *h)
{
	int r, j;
    unsigned int i;
	unsigned char *fw_p;
	unsigned char  databuf[MAX_BYTES_PER_LINE];
	unsigned char  num_bytes = 0;
	unsigned short address = 0;
    
	for ( i = 0; ((i < FX2_VENDAX_SIZE) && (fx2_vendax[i][8] == 0x30)); i++ ) {
		fw_p = (unsigned char *)&fx2_vendax[i][1];
		num_bytes = GET_HEX_BYTE(fw_p);
		fw_p += 2;
		address   = GET_HEX_WORD(fw_p);
		fw_p += 6;
		for ( j = 0; j < num_bytes; j++ ) {
			databuf[j] = GET_HEX_BYTE(fw_p);
			fw_p += 2;
		}
        
		r = libusb_control_transfer(h, 0x40, 0xA0, address, 0x00, databuf, num_bytes, VENDORCMD_TIMEOUT);
		if ( r != num_bytes ) {
			fprintf (stderr, "Error in control_transfer\n");
			return -2;
		}
	}
    
	r = qhyccd_reset (h, 1);
	if ( r != 0 ) {
		fprintf (stderr, "Error: Failed to get FX2 out of reset\n");
		return -3;
	}
    
	return 0;
}

static int SetQHYCCDFirmware(libusb_device_handle *h,const char *filename,int extended)
{
	FILE *fp1 = NULL;
	int i, r, len = 0;
	char hexdata[MAX_LINE_LENGTH];
	char *fw_p;
	unsigned char databuf[MAX_BYTES_PER_LINE];
	unsigned char num_bytes = 0;
	unsigned short address = 0;
    
	struct stat filbuf;
	int filsz;
    
	fp1 = fopen(filename, "rb");
	if ( !fp1 ) {
		fprintf(stderr, "Error: File does not exist\n");
		return -1;
	}
    
	stat(filename, &filbuf);
	filsz = filbuf.st_size;
    
	r = qhyccd_reset (h, 0);
	if ( r != 0 ) {
		fprintf (stderr, "Error: Failed to force FX2 into reset\n");
		fclose(fp1);
		return -1;
	}
	sleep(1);
    
	if ( extended ) {
		r = qhyccd_load_vendax(h);
		if ( r != 0 ) {
			fprintf (stderr, "Failed to download Vend_Ax firmware to aid programming\n");
			fclose(fp1);
			return -2;
		}
	}
    
LoadRam:
	while ( fgets(hexdata, MAX_LINE_LENGTH, fp1) != NULL ) {
		len += strlen(hexdata);
		if ( hexdata[8] == '1' )
			break;
        
		fw_p       = &hexdata[1];
		num_bytes  = GET_HEX_BYTE(fw_p);
		fw_p      += 2;
		address    = GET_HEX_WORD(fw_p);
		if (((extended) && (address >= FX2_INT_RAMSIZE)) || ((!extended) && (address < FX2_INT_RAMSIZE))) {
			fw_p += 6;
			for (i = 0; i < num_bytes; i++) {
				databuf[i] = GET_HEX_BYTE(fw_p);
				fw_p += 2;
			}
            
			r = libusb_control_transfer(h, 0x40, ((extended) ? 0xA3 : 0xA0), address, 0x00, databuf, num_bytes, VENDORCMD_TIMEOUT);
			if ( r != num_bytes ) {
				fprintf (stderr, "Vendor write to RAM failed\n");
				fclose(fp1);
				return -3;
			}
		}
	}
    
	if ( extended ) {
		/* All data has been loaded on external RAM. Now halt the CPU and load the internal RAM. */
		r = qhyccd_reset(h, 0);
		if ( r != 0 ) {
			fprintf(stderr, "Error: Failed to halt FX2 CPU\n");
			fclose(fp1);
			return -4;
		}
        
		extended = 0;
		fseek(fp1, 0, SEEK_SET);
		sleep(1);
		goto LoadRam;
	}
    
	fclose(fp1);
    
	/* Now release CPU from reset. */
	r = qhyccd_reset(h, 1);
	if ( r != 0 ) {
		fprintf(stderr, "Error: Failed to release FX2 from reset\n");
		return -5;
	}
    
	return 0;
}

int OSXInitQHYCCDFirmware()
{
    int ret = QHYCCD_ERROR;
    int i;
    libusb_device_handle *h;
    
    libusb_init(NULL);
    
    for(i = 0;i < 20;i++)
    {
        h = libusb_open_device_with_vid_pid(NULL,camvid[i],fpid[i]);
    
        if(h)
        {
            libusb_kernel_driver_active(h,0);
            libusb_claim_interface(h,0);
            switch(fpid[i])
            {
                case 0x0920:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY5II.HEX",1);
                    break;
                }
                case 0x8310:
                {
                    SetQHYCCDFirmware(h,"firmware/IC8300.HEX",1);
                    break;
                }
                case 0x6740:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY21.HEX",1);
                    break;
                }
                case 0x6940:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY22.HEX",1);
                    break;
                }
                case 0x6004:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY8L.HEX",1);
                    break;
                }
                case 0x1000:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY10.HEX",1);
                    break;
                }
                case 0x1200:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY12.HEX",1);
                    break;
                }
                case 0x8300:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY9S.HEX",1);
                    break;
                }
                case 0x1100:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY11.HEX",1);
                    break;
                }
                case 0x8140:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY23.HEX",1);
                    break;
                }
                case 0x2850:
                {
                    SetQHYCCDFirmware(h,"firmware/IMG2P.HEX",1);
                    break;
                }
                case 0x0259:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY6.HEX",1);
                    break;
                }
                case 0x6000:
                {
                    SetQHYCCDFirmware(h,"firmware/QHY8.HEX",1);
                    break;
                }
                case 0x0930:
                {
                    SetQHYCCDFirmware(h,"firmware/QHYXXX.HEX",1);
                    break;
                }
            }

            libusb_close(h);
            ret = QHYCCD_SUCCESS;
        }
    }
    
    libusb_exit(NULL);
    return ret;
}

int GetQHYCCDChipInfo(qhyccd_handle *h,double *chipw,double *chiph,int *imagew,int *imageh,double *pixelw,double *pixelh,int *bpp)
{
    int index = QHYCCD_ERROR_INDEX;
    int ret = QHYCCD_ERROR;

    index = qhyccd_handle2index(h);

    if(index != QHYCCD_ERROR_INDEX)
    {
        if(cydev[index].is_open)
        {
            ret = cydev[index].qcam->GetChipInfo(chipw,chiph,imagew,imageh,pixelw,pixelh,bpp);
        }
    }

    return ret;
}
