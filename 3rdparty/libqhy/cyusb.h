#ifndef __CYUSB_H
#define __CYUSB_H

/*********************************************************************************\
 * This is the main header file for the cyusb suite for Linux/Mac, called cyusb.h *
 *                                                                                *
 * Author              :        V. Radhakrishnan ( rk@atr-labs.com )              *
 * License             :        GPL Ver 2.0                                       *
 * Copyright           :        Cypress Semiconductors Inc. / ATR-LABS            *
 * Date written        :        March 12, 2012                                    *
 * Modification Notes  :                                                          *
 *    1. Cypress Semiconductor, January 23, 2013                                  *
 *       Added function documentation.                                            *
 *       Added new constant to specify number of device ID entries.               *
 *                                                                                *
 \********************************************************************************/

#ifdef LINUX
#include <libusb.h>
#include <mutex>  // std::mutex
#include "qhyccd.h"
#endif

#define MAX_ID_PAIRS    (100)
#define MAX_DEVICE_TYPES (100)
#define MAX_OPEN_DEVICES (5)
#define ID_STR_LEN (0x20)
#define TRANSFER_COUNT (16)
#define TRANSFER_SIZE (76800)

struct cydev {
    qhyccd_device *dev;
#ifdef WIN32
    void *handle; 
#else
    qhyccd_handle *handle;
#endif
    uint16_t vid; 
    uint16_t pid; 
    uint8_t is_open;
    char id[64]; 
    QHYBASE *qcam;
    
    // added stuff for libusb async functions
    struct libusb_transfer *p_libusb_transfer_array[TRANSFER_COUNT];

    UnlockImageQueue *p_image_queue;
    uint32_t image_queue_len = 0;
    
    volatile bool raw_exit = false;
    volatile int event_count;

    pthread_t raw_handle;
    std::mutex raw_exit_mutex; 
    std::mutex event_count_mutex; 

    //pthread_mutex_t raw_exit_mutex;
    //pthread_mutex_t event_count_mutex;
    
    uint8_t sig[16];
    uint8_t sigcrc[16];
    uint8_t raw_data_cache[7400 * 5000 * 4];
    uint8_t img_buffer[TRANSFER_COUNT * TRANSFER_SIZE];

    uint32_t header_len;
    uint32_t frame_len;
    uint32_t ending_len;
    
    uint32_t sig_len;
    uint32_t header_type;
    uint32_t raw_frame_width;
    uint32_t raw_frame_height;
    uint32_t raw_frame_bpp;
    
    uint32_t received_raw_data_len; // if unsigned QHY5IIL crashes!!!
};

/****************************************************************************************
  Prototype    : void cyusb_download_fx2(cyusb_handle *h, char *filename,
                     unsigned char vendor_command);
  Description  : Performs firmware download on FX2.
  Parameters   :
                 cyusb_handle *h              : Device handle
                 char * filename              : Path where the firmware file is stored
                 unsigned char vendor_command : Vendor command that needs to be passed during download
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ****************************************************************************************/
int fx2_ram_download(qhyccd_handle *h, char *filename, unsigned char vendor_command);

/****************************************************************************************
  Prototype    : void cyusb_download_fx3(cyusb_handle *h, char *filename);
  Description  : Performs firmware download on FX3.
  Parameters   :
                 cyusb_handle *h : Device handle
                 char *filename  : Path where the firmware file is stored
  Return Value : 0 on success, or an appropriate LIBUSB_ERROR.
 ***************************************************************************************/
int fx3_usbboot_download(qhyccd_handle *h, char *filename);

#endif
