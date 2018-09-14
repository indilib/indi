#ifndef _QHYDEVICE_H_
#define _QHYDEVICE_H_

// written by Jan Soldan, 2018

#ifdef  __APPLE__
    #include <libusb.h>
    #include <mutex>  // std::mutex
#elif __linux__
    #include <libusb-1.0/libusb.h>
    #include <libusb.h>
    #include <mutex>  // std::mutex
#endif

#include "qhyccd.h"
#include "qhybase.h"

#define MAX_ID_PAIRS    (100)
#define MAX_DEVICE_TYPES (100)
// we can open up to MAX_OPEN_DEVICES QHYCCD cameras
#define MAX_OPEN_DEVICES (8) 
#define ID_STR_LEN (0x20)

#define TRANSFER_COUNT (32)
#define TRANSFER_SIZE (76800)

#define DATA_CACHE_WIDTH (7400)
#define DATA_CACHE_HEIGHT (5000)
#define DATA_CACHE_CHANNELS (4)

class UnlockImageQueue;
/*
 ****************************************************************************************
 * class QhyDevice
 ****************************************************************************************
 */
class QhyDevice {
	public:
    	libusb_device *dev;
#ifdef WIN32
    	void *handle; 
#else
    	libusb_device_handle *handle;
#endif
    	uint16_t vid; 
    	uint16_t pid; 
    	uint8_t is_open;
    	char id[64]; 
    	QHYBASE *qcam;
    
    	// added stuff for libusb async functions
    	struct libusb_transfer *libusb_transfer_array[TRANSFER_COUNT];

    	UnlockImageQueue *p_image_queue;
    	uint32_t image_queue_len;
    
    	int event_count;
    	bool thread_exit_flag;
    	bool first_exposure_flag;

    	std::mutex event_count_mutex; 
    	std::mutex thread_exit_flag_mutex; 
    	std::mutex first_exposure_flag_mutex; 

    	pthread_t thread_id;

    	uint8_t sig[16];
    	uint8_t sigcrc[16];
    
    	uint8_t *p_raw_data_cache;
    	uint8_t *p_img_buffer;
    
    	uint32_t header_len;
    	uint32_t frame_len;
    	uint32_t ending_len;
    
    	uint32_t sig_len;
    	uint32_t header_type;
    	uint32_t raw_frame_width;
    	uint32_t raw_frame_height;
    	uint32_t raw_frame_bpp;
        int32_t received_raw_data_len;
        
	public:
        QhyDevice();
        QhyDevice(int idx);
		virtual ~QhyDevice();
		void dump(int idx);
		void clear();
};

#endif
