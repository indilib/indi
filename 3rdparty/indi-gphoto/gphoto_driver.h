#ifndef _GPHOTO_DRIVER_H_
#define _GPHOTO_DRIVER_H_

#include <gphoto2/gphoto2.h>

typedef struct
{
	CameraWidget		*widget;
	CameraWidgetType	type;
	const char		*name;
	const char		*parent;
	int			readonly;
	union {
		int		toggle;
		char		index;
		char		*text;
		float		num;
		int		date;
	} value;
	int			choice_cnt;
	const char		**choices;
	float			min;
	float			max;
	float			step;
} gphoto_widget;

struct _gphoto_driver;
typedef struct _gphoto_driver gphoto_driver;

struct _gphoto_widget_list;
typedef struct _gphoto_widget_list gphoto_widget_list;

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
extern void gphoto_free_buffer(gphoto_driver *gphoto);
extern const char *gphoto_get_file_extension(gphoto_driver *gphoto);
extern void gphoto_show_options(gphoto_driver *gphoto);
extern gphoto_widget_list *gphoto_find_all_widgets(gphoto_driver *gphoto);
extern gphoto_widget *gphoto_get_widget_info(gphoto_driver *gphoto, gphoto_widget_list **iter);
extern int gphoto_set_widget_num(gphoto_driver *gphoto, gphoto_widget *widget, float value);
extern int gphoto_set_widget_text(gphoto_driver *gphoto, gphoto_widget *widget, const char *str);
extern int gphoto_read_widget(gphoto_widget *widget);
extern int gphoto_widget_changed(gphoto_widget *widget);
extern int gphoto_get_dimensions(gphoto_driver *gphoto, int *width, int *height);
extern int gphoto_auto_focus(gphoto_driver *gphoto);
extern int gphoto_manual_focus (gphoto_driver *gphoto, int xx, char *errMsg);

#endif
