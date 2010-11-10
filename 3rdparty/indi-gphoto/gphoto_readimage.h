#ifndef _GPHOTO_READIMAGE_H_
#define _GPHOTO_READIMAGE_H_
extern int read_dcraw(const char *filename, void **memptr, size_t *memsize);
extern int read_jpeg(const char *filename, void **memptr, size_t *memsize);
#endif
