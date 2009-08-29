#ifndef _GPHOTO_DRIVER_H_
#define _GPHOTO_DRIVER_H_

struct _gphoto_driver;
typedef struct _gphoto_driver gphoto_driver;

extern int gphoto_start_exposure(gphoto_driver *gphoto, unsigned int exptime_msec);
extern int gphoto_read_exposure(gphoto_driver *gphoto);
extern int gphoto_read_exposure_fd(gphoto_driver *gphoto, int fd);
extern const char **gphoto_get_formats(gphoto_driver *gphoto, int *cnt);
extern const char **gphoto_get_iso(gphoto_driver *gphoto, int *cnt);
extern void gphoto_set_iso(gphoto_driver *gphoto, int iso);
extern void gphoto_set_format(gphoto_driver *gphoto, int format);
extern int gphoto_get_format_current(gphoto_driver *gphoto);
extern int gphoto_get_iso_current(gphoto_driver *gphoto);
extern gphoto_driver *gphoto_open(const char *shutter_release_port);
extern int gphoto_close(gphoto_driver *gphoto);
extern void gphoto_get_buffer(gphoto_driver *gphoto, const char **buffer, size_t *size);
extern const char *gphoto_get_file_extension(gphoto_driver *gphoto);
extern void gphoto_show_options(gphoto_driver *gphoto);
#endif
