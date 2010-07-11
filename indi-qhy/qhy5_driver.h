#ifndef _QHY5_DRIVER_H_
#define _QHY5_DRIVER_H_
struct _qhy5_driver;
typedef struct _qhy5_driver qhy5_driver;
enum {
	QHY_NORTH = 0x20,
	QHY_SOUTH = 0x40,
	QHY_EAST  = 0x10,
	QHY_WEST  = 0x80,
};

extern int qhy5_start_exposure(qhy5_driver *handle, unsigned int exposure);
extern int qhy5_read_exposure(qhy5_driver *handle);
extern void *qhy5_get_row(qhy5_driver *handle, int row);
extern int qhy5_set_params(qhy5_driver *handle, int width, int height, int binw, int binh, int offw, int offh, int gain, int *pixw, int *pixh);
extern qhy5_driver *qhy5_open();
extern int qhy5_close(qhy5_driver *handle);
extern int qhy5_query_capabilities(qhy5_driver *handle, int *width, int *height, int *binw, int *binh, int *gain);
extern int qhy5_timed_move(qhy5_driver *qhy5, int direction, int duration_msec);
#endif
