/* 
 * jpegutils.h: Some Utility programs for dealing with
 *               JPEG encoded images
 *
 *  Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>
 *  Copyright (C) 2001 pHilipp Zabel  <pzabel@gmx.de>
 *  Copyright (C) 2008 Angel Carpintero <ack@telenfonica.net>
 *
 */

#ifndef __JPEGUTILS_H__
#define __JPEGUTILS_H__

/**
 * \defgroup jpegSpace Functions to encode and decode JPEG

  <b>jpeg_data</b>:       buffer with input / output jpeg\n
  <b>len</b>:             Length of jpeg buffer\n
  <b>itype</b>:           Y4M_ILACE_NONE: Not interlaced\n
                    Y4M_ILACE_TOP_FIRST: Interlaced, top-field-first\n
                    Y4M_ILACE_BOTTOM_FIRST: Interlaced, bottom-field-first\n
   <b>ctype</b>            Chroma format for decompression.\n
                    Currently always 420 and hence ignored.\n
   <b>raw0</b>             buffer with input / output raw Y channel\n
   <b>raw1</b>             buffer with input / output raw U/Cb channel\n
   <b>raw2</b>             buffer with input / output raw V/Cr channel\n
   <b>width</b>            width of Y channel (width of U/V is width/2)\n
   <b>height</b>           height of Y channel (height of U/V is height/2)\n
  */

/*@{*/

#define Y4M_ILACE_NONE          0  /** non-interlaced, progressive frame    */
#define Y4M_ILACE_TOP_FIRST     1  /** interlaced, top-field first          */
#define Y4M_ILACE_BOTTOM_FIRST  2  /** interlaced, bottom-field first       */
#define Y4M_ILACE_MIXED         3  /** mixed, "refer to frame header"       */

#define Y4M_CHROMA_420JPEG      0  /** 4:2:0, H/V centered, for JPEG/MPEG-1 */
#define Y4M_CHROMA_420MPEG2     1  /** 4:2:0, H cosited, for MPEG-2         */
#define Y4M_CHROMA_420PALDV     2  /** 4:2:0, alternating Cb/Cr, for PAL-DV */
#define Y4M_CHROMA_444          3  /** 4:4:4, no subsampling, phew.         */
#define Y4M_CHROMA_422          4  /** 4:2:2, H cosited                     */
#define Y4M_CHROMA_411          5  /** 4:1:1, H cosited                     */
#define Y4M_CHROMA_MONO         6  /** luma plane only                      */
#define Y4M_CHROMA_444ALPHA     7  /** 4:4:4 with an alpha channel          */



/**
 * @short decode JPEG buffer
 */
int decode_jpeg_raw(unsigned char *jpeg_data, int len,
                    int itype, int ctype, unsigned int width, 
                    unsigned int height, unsigned char *raw0, 
                    unsigned char *raw1, unsigned char *raw2);

/**
 * @short decode JPEG raw gray buffer
 */
int decode_jpeg_gray_raw(unsigned char *jpeg_data, int len,
                         int itype, int ctype, unsigned int width, 
                         unsigned int height, unsigned char *raw0, 
                         unsigned char *raw1, unsigned char *raw2);

/**
 * @short encode raw JPEG buffer
 */
int encode_jpeg_raw(unsigned char *jpeg_data, int len, int quality,
                    int itype, int ctype, unsigned int width, 
                    unsigned int height, unsigned char *raw0, 
                    unsigned char *raw1, unsigned char *raw2);

/*@}*/

#endif
