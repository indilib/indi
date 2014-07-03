#ifndef _GPHOTO_READIMAGE_H_
#define _GPHOTO_READIMAGE_H_
extern int read_dcraw(const char *filename, char **memptr, size_t *memsize, int *n_axis, int *w, int *h, int *bitsperpixel);
extern int read_jpeg(const char *filename, char **memptr, size_t *memsize, int *n_axis, int *w, int *h);
extern void gphoto_read_set_debug(int enable);
#endif
