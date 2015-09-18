/* 
 * The MI CCD driver library.
 * Copyright (C) 2010 Petr Kubanek <petr@kubanek.net>
 * Copyright (C) 2010 Moravske pristroje, s.r.o. <http://www.mii.cz>
 *
 * This code cannot be distributed without prior agreement from MI.
 */

/**
 * @mainpage MI CCD Linux driver
 *
 * Copyright &copy; 2010 Petr Kubánek <petr@kubanek.net><br/>
 * Copyright &copy; 2010 Moravské přístroje s.r.o. (MI) <a href='http://www.mii.cz'>http://www.mii.cz</a>
 *
 * Driver and associated documentation cannot be distributed without prior agreement from MI.
 *
 * This is driver for MI CCD camera. You will need to link with libmiccd.a to get working binary, e.g. assuming 
 * libmiccd.a resides in /usr/local/lib, headers in /usr/local/include, and you are using cc for C compiler, 
 * link your code code.c with this library with:
 *
 * cc -o code -I/usr/local/include code.c /usr/local/lib/libmiccd.a
 *
 * With exception of miccd_open all functions takes as the first parameter
 * pointer to structure identifiing camera.  The return value is 0 for success,
 * negative for system errno value, and positive for error reply from camera itself.
 *
 * The library does not need any kernel driver - access to USB bus through
 * /dev/bus/usb, available in all recent kernels, is enough.
 *
 * @section function Function list
 *
 * All function are documented under <a href='globals_func.html'>global function list</a>. Following
 * links to their definition in order you will use them in the code.
 *
 * - miccd_open()
 * - miccd_info()
 * - miccd_open_shutter()
 * - miccd_close_shutter()
 * - miccd_read_frame()
 * - miccd_close()
 *
 * @author Petr Kubánek <petr@kubanek.net>
 */

/**
 * @file
 *
 * Prototypes for MI CCD library functions.
 */

#ifndef __MICCD_LIB__
#define __MICCD_LIB__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Camera information structure. Contains fields retrieved from
 * camera. Can be used to distinguish different cameras.
 */
typedef struct {
	/// camera unique identifier
	uint32_t id; 
	/// camera hardware revision
	uint16_t hwrevision;
	/// chip width [pixels]
	uint16_t w;
	/// chip height [pixels]
	uint16_t h;
	/// pixel width [nm]
	uint16_t pw;
	/// pixel height [nm]
	uint16_t ph;
	/// camera description
	char description[15];
	/// camera serial number
	char serial[15];
	/// camera chip identification
	char chip[14];
} camera_info_t;

typedef struct {
	/// camera unique identifier
	uint32_t id; 
	/// camera hardware revision
	uint8_t hwrevision;
	// number of filters present in intergated filter wheels
	uint8_t filters;
	// 
	uint16_t FIFOlines;
	/// chip width [pixels]
	uint16_t w;
	/// chip height [pixels]
	uint16_t h;
	/// pixel width [nm]
	uint16_t pw;
	/// pixel height [nm]
	uint16_t ph;
	/// camera description
	char description[15];
	/// camera serial number
	char serial[15];
	/// camera chip identification
	char chip[14];
} camera_info_h_t;

/**
 * Camera model.
 */
typedef	enum {G10300, G10400, G10800, G11200, G11400, G12000, G2, G3, G3_H} model_t;

/**
 * Camera support structure. Holds various camera variables which cannot be retrieved
 * from camera.
 */
typedef struct {
	/// file descriptor of the camera
	int fd;
	/// binning in x
	uint8_t binx;
	/// binning in y
	uint8_t biny;
	/// exposure width
	uint16_t w;
	/// exposure height
	uint16_t h;
	/// readout mode
	enum {NORMAL = 0, LOW, ULTRA_LOW} mode;
	model_t model;
} camera_t;

/**
 * Open connection to camera with given product ID.
 *
 * @param id camera product ID. If set to 0, the first found camera will be
 * connected. You can use miccd_info(camera,info) call to retrieve its product
 * ID
 *
 * @param camera camera support structure
 *
 * @return opened file descriptor for camera, negative error code on error.
 * Error code is negative errno value.
 * @see miccd_info(camera,info)
 */
int miccd_open (int32_t id, camera_t *camera);

/**
 * Close connection to camera.
 *
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure.
 */
int miccd_close (camera_t *camera);

/**
 * Retrieve camera information structure from camera.
 *
 * @param camera camera structure filled in miccd_open() call
 * @param info camera descriptor structure, filled on successfull return with camera data
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_info (camera_t *camera, camera_info_t *info);

/**
 * Set G1 camera mode.
 *
 * @param camera   camera structure filled in miccd_open() call
 * @param bit16    if 16 bit readout should be enabled
 * @param lownoise lownoise mode enabled 
 */
int miccd_g1_mode (camera_t *camera, int bit16, int lownoise);

/**
 * Set camera mode.
 *
 * @param camera camera structure filled in miccd_open() call
 * @param mode   camera mode (0 - normal, 1 = low noise; 2 = ultra low noise; 2 is not available on G1)
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_mode (camera_t *camera, uint8_t mode);

/**
 * Clear camera CCD.
 *
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_clear (camera_t *camera);

/** 
 * Clear horizontal register.
 *
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_hclear (camera_t *camera);

/** 
 * Shifts rows till first imagine row. This jump over prescan region. After
 * calling this function serial register holds first pixel of first non-prescan
 * (image) row.
 *
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_shift_to0 (camera_t *camera);

/** 
 * Shift camera rows.
 *
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_shift (camera_t *camera);

/** 
 * Shift camera rows. This is equal to paraller shift - full rows of camera
 * data are read out and discarded.
 *
 * @param camera camera structure filled in miccd_open() call
 * @param v number of rows for paraller shift
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_vshift_clear (camera_t *camera, uint16_t v);

/** 
 * Shift camera columns. This is equal to serial shift - ony serial register
 * is shifted by given number of pixels.
 *
 * @param camera camera structure filled in miccd_open() call
 * @param h number of pixels for serial shift
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_hshift_clear (camera_t *camera, uint16_t h);

/**
 * Readout camera chip.
 *
 * @brief Read data from camera and stored them in provided buffer. You can call this function
 * anytime, but most probably you will prefer to call miccd_open_shutter() and miccd_close_shutter() before
 * to accumulate some light on the camera.
 *
 * @param camera camera_t structure filled in miccd_open() call
 * @param hbinning frame horizontal binning
 * @param vbinning frame vertical binning
 * @param x ROI x start
 * @param y ROI y start
 * @param w ROI width
 * @param h ROI height
 * @param data pointer to buffer allocated to hold data. Must be large enough to hold all data from camera (e.g. at least w/hbinning * h/vbinning * 2 bytes).
 * 
 * @return 0 on success, negative error code on (-errno) on failure
 *
 * @see miccd_open_shutter
 * @see miccd_close_shutter
 */
int miccd_read_frame (camera_t *camera, uint8_t hbinning, uint8_t vbinning, uint16_t x, uint16_t y, uint16_t w, uint16_t h, char *data);

int miccd_read_data (camera_t *camera, uint32_t data_size, char *data, uint16_t w, uint16_t h);

/** 
 * Open camera shutter.
 *
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_open_shutter (camera_t *camera);

/** 
 * Close camera shutter.
 *
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_close_shutter (camera_t *camera);

/**
 * Start exposure. Works only with G1 camera.
 *
 * @param camera    camera structure filled in miccd_open() call
 * @param exposure  exposure length in seconds. If it is < 0, then the frame will be read out (no exposure will occurs). Start of the exposure should be triggered by call to miccd_clear.
 *
 * @return 0 on success, negative error code on (-errno) on failure
 *
 * @see miccd_clear
 */
int32_t miccd_start_exposure (camera_t *camera, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float exposure);

/**
 * Abort exposure on G1 camera.
 *
 * @param camera    camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_abort_exposure (camera_t *camera);

/** 
 * Change filter in camera filter wheel.
 *
 * @brief Change filter in camera filter wheel. Filters are counted from 0.
 *
 * @param filter filter number. Filters are counted from 0.
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_filter (camera_t *camera, uint8_t filter);

/**
 * Set camera cooling temperature.
 *
 * @param temp target cooling temperature in degrees C. Please consult camera documentation for details
 * of its cooling capabilities. 
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_set_cooltemp (camera_t *camera, float temp);

/**
 * Retrieve chip temperature.
 *
 * @param temp pointer to float to store chip temperature (in degrees C).
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_chip_temperature (camera_t *camera, float *temp);

/**
 * Retrieve environmental (surrounding) temperature.
 *
 * @param temp pointer to float to store environmental temperature (in degrees C).
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_environment_temperature (camera_t *camera, float *temp);

/**
 * Retrieve camera voltage.
 *
 * @param voltage pointer to integer to store voltage level
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_power_voltage (camera_t *camera, uint16_t *voltage);

/**
 * Retrieve camera gain.
 *
 * @param gain pointer to integer to store gain level
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_gain (camera_t *camera, uint16_t *gain);

/**
 * Switch on/off camera fan.
 *
 * @param fan    turn fan on/off (boolean)
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_fan (camera_t *camera, int8_t fan);

/**
 * Read EEPROM data from the camera.
 *
 * @param offset read EEPROM from this offset
 * @param size   size of buffer to read
 * @param camera camera structure filled in miccd_open() call
 *
 * @return 0 on success, negative error code on (-errno) on failure
 */
int miccd_read_eeprom (camera_t *camera, uint8_t offset, uint8_t size, void *buf);

#ifdef __cplusplus
};
#endif

#endif // !__MICCD_LIB__
