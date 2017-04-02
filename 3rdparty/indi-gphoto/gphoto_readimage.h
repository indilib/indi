#ifndef _GPHOTO_READIMAGE_H_
#define _GPHOTO_READIMAGE_H_
int read_dcraw(const char *filename, uint8_t **memptr, size_t *memsize, int *n_axis, int *w, int *h, int *bitsperpixel);
int read_libraw(const char *filename, uint8_t **memptr, size_t *memsize, int *n_axis, int *w, int *h, int *bitsperpixel, char *bayer_pattern);
int read_jpeg(const char *filename, uint8_t **memptr, size_t *memsize, int *n_axis, int *w, int *h);
int read_jpeg_mem(unsigned char *inBuffer, unsigned long inSize, uint8_t **memptr, size_t *memsize, int *naxis, int *w, int *h );
void gphoto_read_set_debug(const char *name);
#endif
