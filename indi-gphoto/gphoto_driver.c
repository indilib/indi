/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.
  
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
*******************************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <sys/time.h>
#include <time.h>


#include <gphoto2/gphoto2.h>
#include <pthread.h>

#include "gphoto_driver.h"

typedef struct {
	CameraWidget		*parent;
	CameraWidget		*child;
	CameraWidgetType	type;
	union {
		int		bool;
		char		num;
	} value;
	int			choice_cnt;
	const char		**choices;
} gphoto_widget;

struct _gphoto_driver {
	Camera			*camera;
	GPContext		*context;
	CameraFile		*camerafile;
	CameraFilePath		camerapath;
	int			command;
	struct timeval		bulb_end;

	char			filename[80];
	int			width;
	int			height;
	gphoto_widget		*format_widget;
	gphoto_widget		*iso_widget;
	gphoto_widget		*exposure_widget;
	gphoto_widget		*bulb_widget;
	char			bulb_port[256];
	int			bulb_fd;

	int			exposure_cnt;
	double			*exposure;

	int			iso;
	int			format;

	pthread_mutex_t		mutex;
	pthread_t		thread;
	pthread_cond_t		signal;
};

enum {
	DSLR_CMD_BULB_CAPTURE = 0x01,
	DSLR_CMD_CAPTURE      = 0x02,
	DSLR_CMD_DONE         = 0x04,
	DSLR_CMD_THREAD_EXIT  = 0x08,
};

static int debug = 0;
#define dprintf if(debug) printf

static void errordumper(GPLogLevel level, const char *domain, const char *format,
                 va_list args, void *data) {
	if (debug) {
		vfprintf(stdout, format, args);
		fprintf(stdout, "\n");
	}
}

static GPContext* create_context() {
	GPContext *context;

	/* This is the mandatory part */
	context = gp_context_new();

	/* All the parts below are optional! */
	//gp_context_set_error_func (context, ctx_error_func, NULL);
	//gp_context_set_status_func (context, ctx_status_func, NULL);

	/* also:
	gp_context_set_cancel_func    (p->context, ctx_cancel_func,  p);
	gp_context_set_message_func   (p->context, ctx_message_func, p);
	if (isatty (STDOUT_FILENO))
		gp_context_set_progress_funcs (p->context,
			ctx_progress_start_func, ctx_progress_update_func,
			ctx_progress_stop_func, p);
	*/
	return context;
}

/*
 * This function looks up a label or key entry of
 * a configuration widget.
 * The functions descend recursively, so you can just
 * specify the last component.
 */

static int lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child) {
	int ret;
	ret = gp_widget_get_child_by_name (widget, key, child);
	if (ret < GP_OK)
		ret = gp_widget_get_child_by_label (widget, key, child);
	return ret;
}

static int read_widget(gphoto_widget *widget)
{
	const char *ptr;
	int i;
	int ret;

	switch(widget->type) {
	case GP_WIDGET_TOGGLE:
		ret = gp_widget_get_value (widget->child, &widget->value.bool);
		break;
	case GP_WIDGET_RADIO:
	case GP_WIDGET_MENU:
		ret = gp_widget_get_value (widget->child, &ptr);
                if (ret != GP_OK)
			return ret;
		if (! widget->choices) {
			widget->choice_cnt = gp_widget_count_choices (widget->child);
			widget->choices = calloc(sizeof(char *), widget->choice_cnt + 1);
			for ( i=0; i<widget->choice_cnt; i++) {
				const char *choice;
				ret = gp_widget_get_choice (widget->child, i, &choice);
				if (strcmp(choice, ptr) == 0)
					widget->value.num = i;
				widget->choices[i] = choice;
			}
		}
		break;
	default:
		fprintf(stderr, "WARNING: Widget type: %d is unsupported\n", widget->type);
	}
	return 0;
}

static gphoto_widget *find_widget (gphoto_driver *gphoto, const char *name) {
	gphoto_widget *widget = calloc(sizeof(gphoto_widget), 1);
	int			ret;

	ret = gp_camera_get_config (gphoto->camera, &widget->parent, gphoto->context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		free(widget);
		return NULL;
	}
	ret = lookup_widget (widget->parent, name, &widget->child);
	if (ret < GP_OK) {
		/*fprintf (stderr, "lookup widget failed: %d\n", ret);*/
		goto out;
	}

	ret = gp_widget_get_type (widget->child, &widget->type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	read_widget(widget);
	return widget;
out:
	gp_widget_free (widget->parent);
	free(widget);
	return NULL;
}

static int set_widget_num(gphoto_driver *gphoto, gphoto_widget *widget, int value)
{
	int			ret;
	const char		*ptr;

	if (! widget) {
		fprintf(stderr, "Invalid widget specified to set_widget_num\n");
		return 1;
	}
	switch(widget->type) {
	case GP_WIDGET_TOGGLE:
		ret = gp_widget_set_value (widget->child, &value);
		break;
	case GP_WIDGET_RADIO:
	case GP_WIDGET_MENU:
		ret = gp_widget_get_choice (widget->child, value, &ptr);
		ret = gp_widget_set_value (widget->child, ptr);
		break;
	default:
		fprintf(stderr, "Widget type: %d is unsupported\n", widget->type);
		return 1;
	}
        if (ret == GP_OK) {
                ret = gp_camera_set_config (gphoto->camera, widget->parent, gphoto->context);
                if (ret != GP_OK)
                        fprintf(stderr, "Failed to set new configuration value\n");
        }

	return ret;
}

static void widget_free(gphoto_widget *widget)
{
	if(widget->choices)
		free(widget->choices);
	free(widget);
}

static double * parse_shutterspeed(const char **choices, int count)
{
	double *exposure, val;
	int i, num, denom;

	if(count <= 0)
		return NULL;
	exposure = calloc(sizeof(double), count);
	for(i = 0; i <  count; i++) {
		if (sscanf(choices[i], "%d/%d", &num, &denom) == 2) {
			exposure[i] = 1.0 * num / (double)denom;
		} else if ((val = strtod(choices[i], NULL))) {
			exposure[i] = val;
		} else if (strncmp(choices[i], "bulb", 4) == 0) {
			exposure[i] = -1;
		} else {	
			// unknown
			exposure[i] = -2;
		}
	}
	return exposure;
}

static void *stop_bulb(void *arg)
{
	int timeout_set = 0;
	gphoto_driver *gphoto = arg;
	CameraEventType event;
	void    *data = NULL;
	long timeleft;
	struct timespec timeout;
	struct timeval curtime;
	int result;

	pthread_mutex_lock(&gphoto->mutex);
	pthread_cond_signal(&gphoto->signal);

	while(1) {
		if (! timeout_set) {
			// 5 second timeout
			gettimeofday(&curtime, NULL);
			timeout.tv_sec  = curtime.tv_sec + 5;
			timeout.tv_nsec = curtime.tv_usec * 1000;
		}
		timeout_set = 0;
		// All camera opertions take place with the mutex held, so we are thread-safe
		pthread_cond_timedwait(&gphoto->signal, &gphoto->mutex, &timeout);
		dprintf("timeout expired\n");
		if (! (gphoto->command & DSLR_CMD_DONE) && (gphoto->command & DSLR_CMD_BULB_CAPTURE)) {
			result = gp_camera_wait_for_event(gphoto->camera, 1, &event, &data, gphoto->context);
			switch (event) {
				//TODO: Handle unexpected events
				default: break;
			}
			gettimeofday(&curtime, NULL);
			timeleft = ((gphoto->bulb_end.tv_sec - curtime.tv_sec)*1000)+((gphoto->bulb_end.tv_usec - curtime.tv_usec)/1000);
			dprintf("Time left: %ld\n", timeleft);
			if (timeleft < 0) {
				//shut off bulb mode
				dprintf("Closing shutter\n");
				if (gphoto->bulb_widget) {
					set_widget_num(gphoto, gphoto->bulb_widget, FALSE);
				} else {
					close(gphoto->bulb_fd);
				}
				gphoto->command |= DSLR_CMD_DONE;
				pthread_cond_signal(&gphoto->signal);
			} else if (timeleft < 5000) {
				timeout_set = 1;
				timeout.tv_sec = gphoto->bulb_end.tv_sec;
				timeout.tv_nsec =  gphoto->bulb_end.tv_usec * 1000;
			}
		}
		if (! (gphoto->command & DSLR_CMD_DONE) && (gphoto->command & DSLR_CMD_CAPTURE)) {
			gp_camera_capture(gphoto->camera, GP_CAPTURE_IMAGE, &gphoto->camerapath, gphoto->context);
			gphoto->command |= DSLR_CMD_DONE;
			pthread_cond_signal(&gphoto->signal);
		}
		if (gphoto->command & DSLR_CMD_THREAD_EXIT) {
			break;
		}
	}
	pthread_mutex_unlock(&gphoto->mutex);
	return NULL;
}

static void reset_settings(gphoto_driver *gphoto)
{
	if (gphoto->iso >= 0)
		set_widget_num(gphoto, gphoto->iso_widget, gphoto->iso_widget->value.num);

	if (gphoto->format >= 0)
		set_widget_num(gphoto, gphoto->format_widget, gphoto->format_widget->value.num);

	set_widget_num(gphoto, gphoto->exposure_widget, gphoto->exposure_widget->value.num);
}

int find_bulb_exposure(gphoto_driver *gphoto, gphoto_widget *widget)
{
	int i;
	for (i = 0; i < widget->choice_cnt; i++) {
		if (gphoto->exposure[i] == -1) {
			return i;
		}
	}
	return -1;
}

int find_exposure_setting(gphoto_driver *gphoto, gphoto_widget *widget, int exptime_msec)
{
	int i;
	double exptime = exptime_msec / 1000.0;
	int best_idx = 0;
	double delta;
	double best_match = 99999;

	for (i = 0; i < widget->choice_cnt; i++) {
		if (gphoto->exposure[i]  <= 0)
			continue;
		delta = exptime - gphoto->exposure[i];
		if (delta < 0)
			delta = -delta;
		// printf("%d: Comparing %f <> %f (delta: %f)\n", i, exptime, gphoto->exposure[i], delta);
		if (delta < best_match) {
			best_match = delta;
			best_idx = i;
		}
	}
	return best_idx;
}

static void download_image(gphoto_driver *gphoto, CameraFilePath *fn, int fd)
{
	int result;
	CameraFileInfo info;

	strncpy(gphoto->filename, fn->name, sizeof(gphoto->filename));


	gp_camera_file_get_info (gphoto->camera, fn->folder, fn->name, &info, gphoto->context);
	gphoto->width = info.file.width;
	gphoto->height = info.file.height;

	if (fd < 0) {
		result = gp_file_new(&gphoto->camerafile);
	} else {
		result = gp_file_new_from_fd(&gphoto->camerafile, fd);
	}
	dprintf("  Retval: %d\n", result);
	dprintf("Downloading %s/%s\n", fn->folder, fn->name);
	result = gp_camera_file_get(gphoto->camera, fn->folder, fn->name,
		     GP_FILE_TYPE_NORMAL, gphoto->camerafile, gphoto->context);
	dprintf("  Retval: %d\n", result);
	dprintf("Deleting.\n");
	result = gp_camera_file_delete(gphoto->camera, fn->folder, fn->name,
			gphoto->context);
	dprintf("  Retval: %d\n", result);
	if(fd >= 0)
		close(fd);
}

int gphoto_start_exposure(gphoto_driver *gphoto, unsigned int exptime_msec)
{
	int idx;
	if (! gphoto->exposure_widget) {
		fprintf(stderr, "No exposure widget.  Can't expose\n");
		return 1;
	}
	dprintf("Starting exposure\n");
	pthread_mutex_lock(&gphoto->mutex);
	dprintf("  Mutex locked\n");

	if (gphoto->iso >= 0)
		set_widget_num(gphoto, gphoto->iso_widget, gphoto->iso);

	if (gphoto->format >= 0)
		set_widget_num(gphoto, gphoto->format_widget, gphoto->format);

	if (exptime_msec > 5000) {
		if ((! gphoto->bulb_port[0] && ! gphoto->bulb_widget) ||
  		    (idx = find_bulb_exposure(gphoto, gphoto->exposure_widget)) == -1)
		{
			fprintf(stderr, "Warning: Bulb mode isn't supported.  exposure limited to maximum camera exposure\n");
		} else {
			//Bulb mode is supported
			dprintf("Using bulb mode\n");
		
			set_widget_num(gphoto, gphoto->exposure_widget, idx);
		
			gettimeofday(&gphoto->bulb_end, NULL);
			gphoto->bulb_end.tv_usec += exptime_msec * 1000;
			while(gphoto->bulb_end.tv_usec > 1000000) {
				gphoto->bulb_end.tv_sec ++;
				gphoto->bulb_end.tv_usec -= 1000000;
			}
			if (gphoto->bulb_port[0]) {
				gphoto->bulb_fd = open(gphoto->bulb_port, O_RDWR, O_NONBLOCK);
				if(gphoto->bulb_fd < 0) {
					fprintf(stderr, "Failed to open serial port: %s\n", gphoto->bulb_port);
					pthread_mutex_unlock(&gphoto->mutex);
					return 1;
				}
			} else {
				set_widget_num(gphoto, gphoto->bulb_widget, TRUE);
			}
			gphoto->command = DSLR_CMD_BULB_CAPTURE;
			pthread_cond_signal(&gphoto->signal);
			pthread_mutex_unlock(&gphoto->mutex);
			dprintf("Exposure started\n");
			return 0;
		}
	}
	// Not using bulb mode
	idx = find_exposure_setting(gphoto, gphoto->exposure_widget, exptime_msec);
	dprintf("Using exposure time: %s\n", gphoto->exposure_widget->choices[idx]);
	set_widget_num(gphoto, gphoto->exposure_widget, idx);
	gphoto->command = DSLR_CMD_CAPTURE;
	pthread_cond_signal(&gphoto->signal);
	pthread_mutex_unlock(&gphoto->mutex);
	dprintf("Exposure started\n");
	return 0;
}

int gphoto_read_exposure_fd(gphoto_driver *gphoto, int fd)
{
	CameraFilePath  *fn;
	CameraEventType event;
	void    *data = NULL;
	int result;

	// Wait for exposure to complete
	dprintf("Reading exposure\n");
	pthread_mutex_lock(&gphoto->mutex);
	if (gphoto->camerafile) {
		gp_file_free(gphoto->camerafile);
		gphoto->camerafile = NULL;
	}
	if(! gphoto->command & DSLR_CMD_DONE)
		pthread_cond_wait(&gphoto->signal, &gphoto->mutex);
	dprintf("Exposure complete\n");
	if (gphoto->command & DSLR_CMD_CAPTURE) {
		download_image(gphoto, &gphoto->camerapath, fd);
		gphoto->command = 0;
		//Set exposure back to original value
		reset_settings(gphoto);
		pthread_mutex_unlock(&gphoto->mutex);
		return 0;
	}

	//Bulb mode
	gphoto->command = 0;
	while (1) {
		// Wait for image to be ready to download
		result = gp_camera_wait_for_event(gphoto->camera, 500, &event, &data, gphoto->context);
		if (result != GP_OK) {
			fprintf(stderr, "WARNING: Could not wait for event.\n");
			return (result);
		}
		switch (event) {
		case GP_EVENT_FILE_ADDED:
			dprintf("Captured an image\n");
			fn = (CameraFilePath*)data;
			download_image(gphoto, fn, fd);
			//Set exposure back to original value
			reset_settings(gphoto);

			pthread_mutex_unlock(&gphoto->mutex);
			return 0;
			break;
		default:
			dprintf("Got unexpected message: %d\n", event);
		}
		pthread_mutex_unlock(&gphoto->mutex);
		usleep(500 * 1000);
		pthread_mutex_lock(&gphoto->mutex);
	}
	pthread_mutex_unlock(&gphoto->mutex);
	return 0;
}

int gphoto_read_exposure(gphoto_driver *gphoto)
{
	return gphoto_read_exposure_fd(gphoto, -1);
}

const char **gphoto_get_formats(gphoto_driver *gphoto, int *cnt)
{
	if (!gphoto->format_widget) {
		if(cnt)
			*cnt = 0;
		return NULL;
	}
	if(cnt)
		*cnt = gphoto->format_widget->choice_cnt;
	return gphoto->format_widget->choices;
}

const char **gphoto_get_iso(gphoto_driver *gphoto, int *cnt)
{
	if (!gphoto->iso_widget) {
		if(cnt)
			*cnt = 0;
		return NULL;
	}
	if(cnt)
		*cnt = gphoto->iso_widget->choice_cnt;
	return gphoto->iso_widget->choices;
}

void gphoto_set_iso(gphoto_driver *gphoto, int iso)
{
	if (gphoto->iso_widget)
		gphoto->iso = iso;
	else
		fprintf(stderr, "WARNING: Could not set iso\n");
}

void gphoto_set_format(gphoto_driver *gphoto, int format)
{
	if (gphoto->format_widget)
		gphoto->format = format;
	else
		fprintf(stderr, "WARNING: Could not set format\n");
}

int gphoto_get_format_current(gphoto_driver *gphoto)
{
	if (! gphoto->format_widget)
		return 0;
	return gphoto->format_widget->value.num;
}

int gphoto_get_iso_current(gphoto_driver *gphoto)
{
	if (! gphoto->iso_widget)
		return 0;
	return gphoto->iso_widget->value.num;
}

gphoto_driver *gphoto_open(const char *shutter_release_port)
{
	gphoto_driver *gphoto;
	gphoto_widget *widget;
	Camera  *canon;
	GPContext *canoncontext = create_context();
	int result;

	dprintf("Opening gphoto\n");
	gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
	gp_camera_new(&canon);

	/* When I set GP_LOG_DEBUG instead of GP_LOG_ERROR above, I noticed that the
	 * init function seems to traverse the entire filesystem on the camera.  This
	 * is partly why it takes so long.
	 * (Marcus: the ptp2 driver does this by default currently.)
	 */
	dprintf("Camera init.  Takes about 10 seconds.\n");
	result = gp_camera_init(canon, canoncontext);
	if (result != GP_OK) {
		dprintf("  Retval: %d\n", result);
		return NULL;
	}

	gphoto = calloc(sizeof(gphoto_driver), 1);
	gphoto->camera = canon;
	gphoto->context = canoncontext;

	// Set 'capture=1' for Canon DSLRs.  Won't harm other cameras
	if ((widget = find_widget(gphoto, "capture"))) {
		set_widget_num(gphoto, widget, TRUE);
		widget_free(widget);
	}

	if ( (gphoto->exposure_widget = find_widget(gphoto, "shutterspeed")) ||
             (gphoto->exposure_widget = find_widget(gphoto, "eos-shutterspeed")) )
	{
		gphoto->exposure = parse_shutterspeed(gphoto->exposure_widget->choices, gphoto->exposure_widget->choice_cnt);
        } else {
		fprintf(stderr, "WARNING: Didn't find an exposure widget!\n");
		fprintf(stderr, "Are you sure the camera is set to 'Manual' mode?\n");
	}

	gphoto->format_widget = find_widget(gphoto, "imageformat");
	gphoto->format = -1;
	gphoto->iso_widget = find_widget(gphoto, "iso");
	if (! gphoto->iso_widget) {
		gphoto->iso_widget = find_widget(gphoto, "eos-iso");
	}
	gphoto->iso = -1;
	gphoto->bulb_widget = find_widget(gphoto, "bulb");

	if(shutter_release_port) {
		strncpy(gphoto->bulb_port, shutter_release_port, sizeof(gphoto->bulb_port));
		dprintf("Using external shutter-release cable\n");
	}
	gphoto->bulb_fd = -1;

	dprintf("Gphoto initialized\n");
	pthread_mutex_init(&gphoto->mutex, NULL);
	pthread_cond_init(&gphoto->signal, NULL);
	pthread_mutex_lock(&gphoto->mutex);
	pthread_create(&gphoto->thread, NULL, stop_bulb, gphoto);
	pthread_cond_wait(&gphoto->signal, &gphoto->mutex);
	dprintf("Blub-stop thread enabled\n");
	pthread_mutex_unlock(&gphoto->mutex);
	return gphoto;
}

int gphoto_close(gphoto_driver *gphoto)
{
	int result;
	if(! gphoto)
		return 0;
	if(gphoto->thread) {
		pthread_mutex_lock(&gphoto->mutex);
		gphoto->command |= DSLR_CMD_THREAD_EXIT;
		pthread_cond_signal(&gphoto->signal);
		pthread_mutex_unlock(&gphoto->mutex);
		pthread_join(gphoto->thread, NULL);
	}

	if (gphoto->exposure)
		free(gphoto->exposure);
	if (gphoto->exposure_widget)
		widget_free(gphoto->exposure_widget);
	if (gphoto->format_widget)
		widget_free(gphoto->format_widget);
	if (gphoto->iso_widget)
		widget_free(gphoto->iso_widget);
	if (gphoto->bulb_widget)
		widget_free(gphoto->bulb_widget);

	result = gp_camera_exit (gphoto->camera, gphoto->context);
	if (result != GP_OK) {
		fprintf(stderr, "WARNING: Could not close camera connection.\n");
	}
	// We seem to leak the context here.  libgphoto supplies no way to free it?
	free(gphoto);
	return 0;
}

void gphoto_get_buffer(gphoto_driver *gphoto, const char **buffer, size_t *size)
{
	gp_file_get_data_and_size(gphoto->camerafile, buffer, (unsigned long *)size);
}

const char *gphoto_get_file_extension(gphoto_driver *gphoto)
{
	return strchr(gphoto->filename, '.')+1;
}

int gphoto_get_dimensions(gphoto_driver *gphoto, int *width, int *height)
{
	*width = gphoto->width;
	*height = gphoto->height;
	return 0;
}

#ifdef GPHOTO_TEST
#include <getopt.h>

void write_image(gphoto_driver *gphoto, const char *basename) {
	FILE *fh;
	const char *buffer;
	size_t size;
	const char *ext;
	char filename[256];
	gphoto_get_buffer(gphoto, &buffer, &size);
	ext = gphoto_get_file_extension(gphoto);
	sprintf(filename, "%s.%s", basename, ext);
	fh = fopen(filename, "w+");
	fwrite(buffer, size, 1, fh);
	fclose(fh);
}

void show_help()
{
	printf("gphoto_driver [options]\n");
	printf("\t\t-e/--exposure <exposure>          specify exposure in msec (default: 100)\n");
	printf("\t\t-f/--file <filename>              specify filename to write to\n");
	printf("\t\t-c/--count <count>                specify how many sequential images to take\n");
	printf("\t\t-i/--iso <iso>                    choose iso (use --list to query values)\n");
	printf("\t\t-m/--format <format #>            choose format (use --list to query values)\n");
	printf("\t\t-p/--port <path to serial port>   choose a serial port to use for shutter release control\n");
	printf("\t\t-l/--list                         show available iso and format values\n");
	printf("\t\t-d/--debug                        enable debugging\n");
	printf("\t\t-h//-help                         show this message\n");
	exit(0);
}

int main (int argc,char **argv)
{
	gphoto_driver *gphoto;
	char image_name[80];
	int i;
	int count = 0;
	int list = 0;
	char *iso = NULL;
	char *port = NULL;
	int format = -1;
	unsigned int exposure = 100;
	char basename[256] = "image";

	struct option long_options[] = {
		{"count", required_argument, NULL, 'c'},
		{"debug", required_argument, NULL, 'd'},
		{"exposure" ,required_argument, NULL, 'e'},
		{"file", required_argument, NULL, 'f'},
		{"help", no_argument , NULL, 'h'},
		{"iso", required_argument, NULL, 'i'},
		{"list", no_argument, NULL, 'l'},
		{"format", required_argument, NULL, 'm'},
		{"port", required_argument, NULL, 'p'},
		{0, 0, 0, 0}
	};

	while (1) {
		char c;
		c = getopt_long (argc, argv, 
                     "c:de:f:hi:lm:p:",
                     long_options, NULL);
		if(c == EOF)
			break;
		switch(c) {
		case 'c':
			count = strtol(optarg, NULL, 0);
			break;
		case 'd':
			debug = 1;
			break;
		case 'e':
			exposure = strtol(optarg, NULL, 0);
			break;
		case 'f':
			strncpy(basename, optarg, 255);
			break;
		case 'h':
			show_help();
			break;
		case 'i':
			iso = optarg;
			break;
		case 'l':
			list = 1;
			break;
		case 'm':
			format = strtol(optarg, NULL, 0);
			break;
		case 'p':
			port = optarg;
			break;
		}
	}


	if(! (gphoto = gphoto_open(port))) {
		printf("Could not open the DSLR device\n");
		return -1;
	}
	if (list) {
		int max;
		const char **values = gphoto_get_formats(gphoto, &max);
		printf("Available image formats:\n");
		for(i = 0; i < max; i++) {
			printf("\t%3d: %s\n", i, values[i]);
		}
		printf("Available ISO:\n");
		values = gphoto_get_iso(gphoto, NULL);
		for(i = 0; values[i]; i++) {
			printf("\t%s\n", values[i]);
		}
		gphoto_close(gphoto);
		return 0;
	}

	if (iso) {
		int max;
		const char **values = gphoto_get_iso(gphoto, &max);
		for(i = 0; i < max; i++) {
			if (strcmp(values[i], iso) == 0) {
				gphoto_set_iso(gphoto, i);
				break;
			}
		}
	}
	if (format != -1) {
		gphoto_set_format(gphoto, format);
	}
	if(! gphoto->exposure_widget) {
		printf("No exposure widget.  Aborting...\n");
		gphoto_close(gphoto);
		return 1;
	}
	printf("Exposing for %f sec\n", exposure / 1000.0);
	if(count == 0) {
		if(gphoto_start_exposure(gphoto, exposure)) {
			printf("Exposure failed!\n");
			gphoto_close(gphoto);
			return 1;
		}
		usleep(exposure * 1000);
		gphoto_read_exposure(gphoto);
		write_image(gphoto, basename);
	}
	for(i = 0; i < count; i++) {
		sprintf(image_name, "%s%d", basename, i);
		if(gphoto_start_exposure(gphoto, exposure)) {
			printf("Exposure failed!\n");
			gphoto_close(gphoto);
			return 1;
		}
		usleep(exposure * 1000);
		gphoto_read_exposure(gphoto);
		write_image(gphoto, image_name);
	}
	gphoto_close(gphoto);
	return 0;
}
#endif
