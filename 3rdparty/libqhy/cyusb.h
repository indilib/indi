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
//#include <libusb-1.0/libusb.h>
#include <libusb.h>
#include <mutex>  // std::mutex
#include "qhyccd.h"
#endif

/* This is the maximum number of VID/PID pairs that this library will consider. This limits
   the number of valid VID/PID entries in the configuration file.
 */
#define MAX_ID_PAIRS    100

/*
 This is the maximum number of qhyccd camera types.
 */
#define MAX_DEVICE_TYPES (100)

/*
 This is the maximum number of opened qhyccd cameras.
 */
#define MAX_OPEN_DEVICES (5)

/*
 This is the length for camera ID
 */
#define ID_STR_LEN (0x20)

/**
 * @brief struct cydef define
 *
 * Global struct for camera's class and some information
 */

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

    int32_t header_len;
    int32_t frame_len;
    int32_t ending_len;
    int32_t sig_len;
    int32_t header_type;
    int32_t raw_frame_width;
    int32_t raw_frame_height;
    int32_t raw_frame_bpp;
    int32_t received_raw_data_len;
};

/* Global struct for camera's vendor id */
static uint16_t camvid[MAX_DEVICE_TYPES] = {
    0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x1618, 0x1618, 0x1618, 0X1618, 0x1618, 0x1618, 0x16c0, 0x1618,
    0x16c0, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x04b4, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x1618, 0x04b4, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618,
    0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x04b4, 0x1618,
    0x1618, 0x1618, 0x1618, 0x1618, 0x1618, 0x1618
};

/* Global struct for camera'io product id */
static uint16_t campid[MAX_DEVICE_TYPES] = {
    0x0921, 0x8311, 0x6741, 0x6941, 0x6005, 0x1001, 0x1201, 0x8301, 0x6003,
    0x1111, 0x8141, 0x2851, 0x025a, 0x6001, 0x0931, 0x1611, 0x296d, 0x4023,
    0x2971, 0xa618, 0x1501, 0x1651, 0x8321, 0x1621, 0x1671, 0x8303, 0x1631,
    0x2951, 0x00f1, 0x296d, 0x0941, 0x0175, 0x8323, 0x0179, 0x1623, 0x0237,
    0x0186, 0x6953, 0x8614, 0x1601, 0x1633, 0xC401, 0x0225, 0xC175, 0x0291,
    0xC179, 0xC225, 0xC291, 0xC164, 0xC166, 0xC368, 0xC184, 0x8614, 0xF368,
    0xA815, 0x5301, 0x1633, 0xC248, 0xC168, 0xC129
};

/* Global struct for camera'firmware product id */
static uint16_t fpid[MAX_DEVICE_TYPES] = {
    0x0920, 0x8310, 0x6740, 0x6940, 0x6004, 0x1000, 0x1200, 0x8300, 0x6002,
    0x1110, 0x8140, 0x2850, 0x0259, 0x6000, 0x0930, 0x1610, 0x0901, 0x4022,
    0x2970, 0xb618, 0x1500, 0x1650, 0x8320, 0x1620, 0x1670, 0x8302, 0x1630,
    0x2950, 0x00f1, 0x0901, 0x0940, 0x0174, 0x8322, 0x0178, 0x1622, 0x0236,
    0x0185, 0x6952, 0x8613, 0x1600, 0x1632, 0xC400, 0x0224, 0xC174, 0x0290,
    0xC178, 0xC224, 0xC290, 0xC163, 0xC165, 0xC367, 0xC183, 0x8613, 0xF367,
    0xA814, 0x5300, 0x1632, 0xC247, 0xC167, 0xC128
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
