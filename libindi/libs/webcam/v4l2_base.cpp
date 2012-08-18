/*
    Copyright (C) 2005 by Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on V4L 2 Example
    http://v4l2spec.bytesex.org/spec-single/v4l2.html#CAPTURE-EXAMPLE

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/ 

#include <iostream>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <asm/types.h>          /* for videodev2.h */

#include "ccvt.h"
#include "v4l2_base.h"
#include "eventloop.h"
#include "indidevapi.h"

#define ERRMSGSIZ	1024

#define CLEAR(x) memset (&(x), 0, sizeof (x))

using namespace std;

V4L2_Base::V4L2_Base()
{
   frameRate=10;
   selectCallBackID = -1;
   dropFrame = false;
  
   xmax = xmin = 160;
   ymax = ymin = 120;

   io		= IO_METHOD_MMAP;
   fd           = -1;
   buffers      = NULL;
   n_buffers    = 0;

   YBuf         = NULL;
   UBuf         = NULL;
   VBuf         = NULL;
   colorBuffer  = NULL;
   rgb24_buffer = NULL;
   callback     = NULL;

}

V4L2_Base::~V4L2_Base()
{

  delete (YBuf);
  delete (UBuf);
  delete (VBuf);
  delete (colorBuffer);
  delete (rgb24_buffer);

}

int V4L2_Base::xioctl(int fd, int request, void *arg)
{
        int r;

        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);

        return r;
}

int V4L2_Base::errno_exit(const char *s, char *errmsg)
{
        fprintf (stderr, "%s error %d, %s\n",
                 s, errno, strerror (errno));
     
        snprintf(errmsg, ERRMSGSIZ, "%s error %d, %s\n", s, errno, strerror (errno));

        return -1;
} 

int V4L2_Base::connectCam(const char * devpath, char *errmsg , int pixelFormat , int width , int height )
{
   frameRate=10;
   selectCallBackID = -1;
   dropFrame = false;

    if (open_device (devpath, errmsg) < 0)
      return -1;

    if (init_device(errmsg, pixelFormat, width, height) < 0)
      return -1;

   cerr << "V4L 2 - All successful, returning\n";
   return fd;
}

void V4L2_Base::disconnectCam()
{
   char errmsg[ERRMSGSIZ];
   delete YBuf;
   delete UBuf;
   delete VBuf;
   YBuf = UBuf = VBuf = NULL;
   
   if (selectCallBackID != -1)
     rmCallback(selectCallBackID);
     
   stop_capturing (errmsg);

   uninit_device (errmsg);

   close_device ();
     
   fprintf(stderr, "Disconnect cam\n");
}

int V4L2_Base::read_frame(char *errmsg)
{
        struct v4l2_buffer buf;
	unsigned int i;
        //cerr << "in read Frame" << endl;

	switch (io) {
	case IO_METHOD_READ:
    		if (-1 == read (fd, buffers[0].start, buffers[0].length)) {
            		switch (errno) {
            		case EAGAIN:
                    		return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				return errno_exit ("read", errmsg);
			}
		}

    		//process_image (buffers[0].start);

		break;

	case IO_METHOD_MMAP:
		CLEAR (buf);

            	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            	buf.memory = V4L2_MEMORY_MMAP;

    		if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
            		switch (errno) {
            		case EAGAIN:
                    		return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				return errno_exit ("VIDIOC_DQBUF", errmsg);
			}
		}

                assert (buf.index < n_buffers);

                switch (fmt.fmt.pix.pixelformat)
                {
                  case V4L2_PIX_FMT_YUV420:
                    memcpy(YBuf,((unsigned char *) buffers[buf.index].start), fmt.fmt.pix.width * fmt.fmt.pix.height);
                    memcpy(UBuf,((unsigned char *) buffers[buf.index].start) + fmt.fmt.pix.width * fmt.fmt.pix.height, (fmt.fmt.pix.width/2) * (fmt.fmt.pix.height/2));
                    memcpy(VBuf,((unsigned char *) buffers[buf.index].start) + fmt.fmt.pix.width * fmt.fmt.pix.height + (fmt.fmt.pix.width/2) * (fmt.fmt.pix.height/2), (fmt.fmt.pix.width/2) * (fmt.fmt.pix.width/2));
		    break;

                  case V4L2_PIX_FMT_YUYV:
                     ccvt_yuyv_420p( fmt.fmt.pix.width , fmt.fmt.pix.height, buffers[buf.index].start, YBuf, UBuf, VBuf);
		     break;

                 case V4L2_PIX_FMT_RGB24:
			RGB2YUV(fmt.fmt.pix.width, fmt.fmt.pix.height, buffers[buf.index].start, YBuf, UBuf, VBuf, 0);
                        break;

                 case V4L2_PIX_FMT_SBGGR8:
                         bayer2rgb24(rgb24_buffer, ((unsigned char *) buffers[buf.index].start), fmt.fmt.pix.width, fmt.fmt.pix.height);
                         break;
                }
                  
                /*if (dropFrame)
                {
                  dropFrame = false;
                  return 0;
                } */

                /* Call provided callback function if any */
                 if (callback)
                	(*callback)(uptr);

		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			return errno_exit ("VIDIOC_QBUF", errmsg);

		break;

	case IO_METHOD_USERPTR:
		CLEAR (buf);

    		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit ("VIDIOC_DQBUF", errmsg);
			}
		}

		for (i = 0; i < n_buffers; ++i)
			if (buf.m.userptr == (unsigned long) buffers[i].start
			    && buf.length == buffers[i].length)
				break;

		assert (i < n_buffers);

    		//process_image ((void *) buf.m.userptr);

		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF", errmsg);

		break;
	}

	return 0;
}

int V4L2_Base::stop_capturing(char *errmsg)
{
        enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

                IERmCallback(selectCallBackID);
                selectCallBackID = -1;

                // N.B. I used this as a hack to solve a problem with capturing a frame
		// long time ago. I recently tried taking this hack off, and it worked fine!
		//dropFrame = true;

		if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
			return errno_exit ("VIDIOC_STREAMOFF", errmsg);

                

		break;
	}

   return 0;
}

int V4L2_Base::start_capturing(char * errmsg)
{
        unsigned int i;
        enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i) {
            		struct v4l2_buffer buf;

        		CLEAR (buf);

        		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        		buf.memory      = V4L2_MEMORY_MMAP;
        		buf.index       = i;

        		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    		return errno_exit ("VIDIOC_QBUF", errmsg);

		}
		
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
			return errno_exit ("VIDIOC_STREAMON", errmsg);

                

                selectCallBackID = IEAddCallback(fd, newFrame, this);

		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i) {
            		struct v4l2_buffer buf;

        		CLEAR (buf);

        		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        		buf.memory      = V4L2_MEMORY_USERPTR;
        		buf.m.userptr	= (unsigned long) buffers[i].start;
			buf.length      = buffers[i].length;

        		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    		return errno_exit ("VIDIOC_QBUF", errmsg);
		}


		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
			return errno_exit ("VIDIOC_STREAMON", errmsg);

		break;
	}

  return 0;

}

void V4L2_Base::newFrame(int /*fd*/, void *p)
{
  char errmsg[ERRMSGSIZ];

  ( (V4L2_Base *) (p))->read_frame(errmsg);

}

int V4L2_Base::uninit_device(char *errmsg)
{

	switch (io) {
	case IO_METHOD_READ:
		free (buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		for (unsigned int i = 0; i < n_buffers; ++i)
			if (-1 == munmap (buffers[i].start, buffers[i].length))
				return errno_exit ("munmap", errmsg);
		break;

	case IO_METHOD_USERPTR:
		for (unsigned int i = 0; i < n_buffers; ++i)
			free (buffers[i].start);
		break;
	}

	free (buffers);

   return 0;
}

void V4L2_Base::init_read(unsigned int buffer_size)
{
        buffers = (buffer *) calloc (1, sizeof (*buffers));

        if (!buffers) {
                fprintf (stderr, "Out of memory\n");
                exit (EXIT_FAILURE);
        }

	buffers[0].length = buffer_size;
	buffers[0].start = malloc (buffer_size);

	if (!buffers[0].start) {
    		fprintf (stderr, "Out of memory\n");
            	exit (EXIT_FAILURE);
	}
}

int V4L2_Base::init_mmap(char *errmsg)
{
	struct v4l2_requestbuffers req;

        CLEAR (req);

        req.count               = 4;
        //req.count               = 1;
        req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory              = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf (stderr, "%s does not support "
                                 "memory mapping\n", dev_name);
                        snprintf(errmsg, ERRMSGSIZ, "%s does not support "
                                 "memory mapping\n", dev_name);
                        return -1;
                } else {
                        return errno_exit ("VIDIOC_REQBUFS", errmsg);
                }
        }

        if (req.count < 2) {
                fprintf (stderr, "Insufficient buffer memory on %s\n",
                         dev_name);
                snprintf(errmsg, ERRMSGSIZ, "Insufficient buffer memory on %s\n",
                         dev_name);
		return -1;
        }

        buffers = (buffer *) calloc (req.count, sizeof (*buffers));

        if (!buffers)
        {
                fprintf (stderr, "buffers. Out of memory\n");
                strncpy(errmsg, "buffers. Out of memory\n", ERRMSGSIZ);
                return -1;
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
        {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
                        return errno_exit ("VIDIOC_QUERYBUF", errmsg);

                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start =
                        mmap (NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

                if (MAP_FAILED == buffers[n_buffers].start)
                        return errno_exit ("mmap", errmsg);
        }

   return 0;
}

void V4L2_Base::init_userp(unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;
	char errmsg[ERRMSGSIZ];

        CLEAR (req);

        req.count               = 4;
        req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory              = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf (stderr, "%s does not support "
                                 "user pointer i/o\n", dev_name);
                        exit (EXIT_FAILURE);
                } else {
                        errno_exit ("VIDIOC_REQBUFS", errmsg);
                }
        }

        buffers = (buffer *) calloc (4, sizeof (*buffers));

        if (!buffers) {
                fprintf (stderr, "Out of memory\n");
                exit (EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
                buffers[n_buffers].length = buffer_size;
                buffers[n_buffers].start = malloc (buffer_size);

                if (!buffers[n_buffers].start) {
    			fprintf (stderr, "Out of memory\n");
            		exit (EXIT_FAILURE);
		}
        }
}

int V4L2_Base::init_device(char *errmsg, int pixelFormat , int width, int height)
{
	unsigned int min;

        if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap))
        {
                if (EINVAL == errno) {
                        fprintf (stderr, "%s is no V4L2 device\n",
                                 dev_name);
                        snprintf(errmsg, ERRMSGSIZ, "%s is no V4L2 device\n", dev_name);
                        return -1;
                } else {
                        return errno_exit ("VIDIOC_QUERYCAP", errmsg);
                }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        {
                fprintf (stderr, "%s is no video capture device\n",
                         dev_name);
                snprintf(errmsg, ERRMSGSIZ, "%s is no video capture device\n", dev_name);
                return -1;
        }

	switch (io) 
        {
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
			fprintf (stderr, "%s does not support read i/o\n",
				 dev_name);
                        snprintf(errmsg, ERRMSGSIZ, "%s does not support read i/o\n",
				 dev_name);
			return -1;
		}

		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING))
                {
			fprintf (stderr, "%s does not support streaming i/o\n",
				 dev_name);
                        snprintf(errmsg, ERRMSGSIZ, "%s does not support streaming i/o\n",
				 dev_name);
			return -1;
		}

		break;
	}

        /* Select video input, video standard and tune here. */

        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (-1 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
                /* Errors ignored. */
        }

        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
                switch (errno) {
                case EINVAL:
                        /* Cropping not supported. */
                        break;
                default:
                        /* Errors ignored. */
                        break;
                }
        }

        CLEAR (fmt);

       fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;

       if (-1 == xioctl (fd, VIDIOC_G_FMT, &fmt))
            return errno_exit ("VIDIOC_G_FMT", errmsg);

        fmt.fmt.pix.width       = (width == -1)       ? fmt.fmt.pix.width : width; 
        fmt.fmt.pix.height      = (height == -1)      ? fmt.fmt.pix.height : height;
        fmt.fmt.pix.pixelformat = (pixelFormat == -1) ? fmt.fmt.pix.pixelformat : pixelFormat;
        //fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

        if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
                return errno_exit ("VIDIOC_S_FMT", errmsg);

        /* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

        /* Let's get the actual size */
        CLEAR(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (fd, VIDIOC_G_FMT, &fmt))
                return errno_exit ("VIDIOC_G_FMT", errmsg);

        cerr << "width: " << fmt.fmt.pix.width << " - height: " << fmt.fmt.pix.height << endl;

       switch (pixelFormat)
       {
        case V4L2_PIX_FMT_YUV420:
         cerr << "pixel format: V4L2_PIX_FMT_YUV420" << endl;
        break;

        case V4L2_PIX_FMT_YUYV:
          cerr << "pixel format: V4L2_PIX_FMT_YUYV" << endl;
         break;

        case V4L2_PIX_FMT_RGB24:
          cerr << "pixel format: V4L2_PIX_FMT_RGB24" << endl;
         break;

        case V4L2_PIX_FMT_SBGGR8:
         cerr << "pixel format: V4L2_PIX_FMT_SBGGR8" << endl;
         break;

       }

        findMinMax();

        allocBuffers();

	switch (io)
        {
	case IO_METHOD_READ:
		init_read (fmt.fmt.pix.sizeimage);
		break;

	case IO_METHOD_MMAP:
		return init_mmap(errmsg);
		break;

	case IO_METHOD_USERPTR:
		init_userp (fmt.fmt.pix.sizeimage);
		break;
	}

  return 0;
}

void V4L2_Base::close_device(void)
{
        char errmsg[ERRMSGSIZ];

        if (-1 == close (fd))
	        errno_exit ("close", errmsg);

        fd = -1;
}

int V4L2_Base::open_device(const char *devpath, char *errmsg)
{
        struct stat st; 

        strncpy(dev_name, devpath, 64);

        if (-1 == stat (dev_name, &st)) {
                fprintf (stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror (errno));
                snprintf(errmsg, ERRMSGSIZ, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror (errno));
                return -1;
        }

        if (!S_ISCHR (st.st_mode))
        {
                fprintf (stderr, "%s is no device\n", dev_name);
                snprintf(errmsg, ERRMSGSIZ, "%s is no device\n", dev_name);
                return -1;
        }

        fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd)
        {
                fprintf (stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror (errno));
                snprintf(errmsg, ERRMSGSIZ, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror (errno));
                return -1;
        }

  return 0;
}



int V4L2_Base::getWidth()
{
  return fmt.fmt.pix.width;
}

int V4L2_Base::getHeight()
{
 return fmt.fmt.pix.height;
}

void V4L2_Base::setFPS(int fps)
{
  frameRate = 15;//fps;
}

int V4L2_Base::getFPS()
{
  return 15;
}

char * V4L2_Base::getDeviceName()
{
  return ((char *) cap.card);
}

void V4L2_Base::allocBuffers()
{
   delete (YBuf); YBuf = NULL;
   delete (UBuf); UBuf = NULL;
   delete (VBuf); VBuf = NULL;
   delete (colorBuffer); colorBuffer = NULL;
   delete (rgb24_buffer); rgb24_buffer = NULL;
   
   YBuf= new unsigned char[ fmt.fmt.pix.width * fmt.fmt.pix.height];
   UBuf= new unsigned char[ fmt.fmt.pix.width * fmt.fmt.pix.height];
   VBuf= new unsigned char[ fmt.fmt.pix.width * fmt.fmt.pix.height];
   colorBuffer = new unsigned char[fmt.fmt.pix.width * fmt.fmt.pix.height * 4];

   if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_SBGGR8)
     rgb24_buffer = new unsigned char[fmt.fmt.pix.width * fmt.fmt.pix.height * 3];
}

void V4L2_Base::getMaxMinSize(int & x_max, int & y_max, int & x_min, int & y_min)
{
  x_max = xmax; y_max = ymax; x_min = xmin; y_min = ymin;
}

int V4L2_Base::setSize(int x, int y)
{
   char errmsg[ERRMSGSIZ];
   int oldW, oldH;
 
   oldW = fmt.fmt.pix.width;
   oldH = fmt.fmt.pix.height;

   fmt.fmt.pix.width  = x;
   fmt.fmt.pix.height = y;

   if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
   {
        fmt.fmt.pix.width  = oldW;
        fmt.fmt.pix.height = oldH;
        return errno_exit ("VIDIOC_S_FMT", errmsg);
   }

   /* PWC bug? It seems that setting the "wrong" width and height will mess something in the driver.
      Only 160x120, 320x280, and 640x480 are accepted. If I try to set it for example to 300x200, it wii
      get set to 320x280, which is fine, but then the video information is messed up for some reason. */
      xioctl (fd, VIDIOC_S_FMT, &fmt);
 
  
   allocBuffers();

  return 0;
}

void V4L2_Base::setContrast(int val)
{
   /*picture_.contrast=val;
   updatePictureSettings();*/
}

int V4L2_Base::getContrast()
{
   return 255;//picture_.contrast;
}

void V4L2_Base::setBrightness(int val)
{
   /*picture_.brightness=val;
   updatePictureSettings();*/
}

int V4L2_Base::getBrightness()
{
   return 255;//picture_.brightness;
}

void V4L2_Base::setColor(int val)
{
   /*picture_.colour=val;
   updatePictureSettings();*/
}

int V4L2_Base::getColor()
{
   return 255; //picture_.colour;
}

void V4L2_Base::setHue(int val)
{
   /*picture_.hue=val;
   updatePictureSettings();*/
}

int V4L2_Base::getHue()
{
   return 255;//picture_.hue;
}

void V4L2_Base::setWhiteness(int val)
{
   /*picture_.whiteness=val;
   updatePictureSettings();*/
}

int V4L2_Base::getWhiteness() 
{
   return 255;//picture_.whiteness;
}

void V4L2_Base::setPictureSettings()
{
   /*if (ioctl(device_, VIDIOCSPICT, &picture_) ) {
      cerr << "updatePictureSettings" << endl;
   }
   ioctl(device_, VIDIOCGPICT, &picture_);*/
}

void V4L2_Base::getPictureSettings()
{
   /*if (ioctl(device_, VIDIOCGPICT, &picture_) )
   {
      cerr << "refreshPictureSettings" << endl;
   }*/
}

unsigned char * V4L2_Base::getY()
{
  if (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_SBGGR8)
      RGB2YUV(fmt.fmt.pix.width, fmt.fmt.pix.height, rgb24_buffer, YBuf, UBuf, VBuf, 0);

  return YBuf;
}

unsigned char * V4L2_Base::getU()
{
 return UBuf;
}

unsigned char * V4L2_Base::getV()
{
 return VBuf;
}

unsigned char * V4L2_Base::getColorBuffer()
{
  //cerr << "in get color buffer " << endl;

  switch (fmt.fmt.pix.pixelformat) 
  {
      case V4L2_PIX_FMT_YUV420:
        ccvt_420p_bgr32(fmt.fmt.pix.width, fmt.fmt.pix.height,
                      buffers[0].start, (void*)colorBuffer);
      break;

    case V4L2_PIX_FMT_YUYV:
         ccvt_yuyv_bgr32(fmt.fmt.pix.width, fmt.fmt.pix.height,
                      buffers[0].start, (void*)colorBuffer);
         break;

    case V4L2_PIX_FMT_RGB24:
         ccvt_rgb24_bgr32(fmt.fmt.pix.width, fmt.fmt.pix.height,
                      buffers[0].start, (void*)colorBuffer);
         break;

    case V4L2_PIX_FMT_SBGGR8:
         ccvt_rgb24_bgr32(fmt.fmt.pix.width, fmt.fmt.pix.height,
                      rgb24_buffer, (void*)colorBuffer);
         break;

   default:
    break;
  }

  return colorBuffer;
}

void V4L2_Base::registerCallback(WPF *fp, void *ud)
{
  callback = fp;
  uptr = ud;
}

void V4L2_Base::findMinMax()
{
    char errmsg[ERRMSGSIZ];
    struct v4l2_format tryfmt;
    CLEAR(tryfmt);

    xmin = xmax = fmt.fmt.pix.width;
    ymin = ymax = fmt.fmt.pix.height;

    tryfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tryfmt.fmt.pix.width       = 10;
    tryfmt.fmt.pix.height      = 10;
    tryfmt.fmt.pix.pixelformat = fmt.fmt.pix.pixelformat;
    tryfmt.fmt.pix.field       = fmt.fmt.pix.field;

    if (-1 == xioctl (fd, VIDIOC_TRY_FMT, &tryfmt))
    {
        errno_exit ("VIDIOC_TRY_FMT 1", errmsg);
        return;
    }
   
    xmin = tryfmt.fmt.pix.width;
    ymin = tryfmt.fmt.pix.height;

    tryfmt.fmt.pix.width       = 1600;
    tryfmt.fmt.pix.height      = 1200;

    if (-1 == xioctl (fd, VIDIOC_TRY_FMT, &tryfmt))
    {
                errno_exit ("VIDIOC_TRY_FMT 2", errmsg);
                return;
    }

    xmax = tryfmt.fmt.pix.width;
    ymax = tryfmt.fmt.pix.height;

    cerr << "Min X: " << xmin << " - Max X: " << xmax << " - Min Y: " << ymin << " - Max Y: " << ymax << endl;
}

void V4L2_Base::enumerate_ctrl (void)
{
  char errmsg[ERRMSGSIZ];
  CLEAR(queryctrl);

  for (queryctrl.id = V4L2_CID_BASE; queryctrl.id < V4L2_CID_LASTP1; queryctrl.id++)
  {
        if (0 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl))
        {
               cerr << "Control " << queryctrl.name << endl;

                if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                        continue;

                cerr << "Control " << queryctrl.name << endl;

                if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
                        enumerate_menu ();
        } else
        {
                if (errno == EINVAL)
                        continue;

                errno_exit("VIDIOC_QUERYCTRL", errmsg);
                return;
        }
  }

  for (queryctrl.id = V4L2_CID_PRIVATE_BASE;  ;  queryctrl.id++)
  {
        if (0 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl))
        {
              cerr << "Private Control " << queryctrl.name << endl;

                if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                        continue;

                if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
                        enumerate_menu ();
        } else {
                if (errno == EINVAL)
                        break;

                errno_exit ("VIDIOC_QUERYCTRL", errmsg);
                return;
        }

  }

}

void V4L2_Base::enumerate_menu (void)
{
  char errmsg[ERRMSGSIZ];
  cerr << "  Menu items:" << endl;

        CLEAR(querymenu);
        querymenu.id = queryctrl.id;

        for (querymenu.index = queryctrl.minimum;  querymenu.index <= queryctrl.maximum; querymenu.index++)
         {
                if (0 == xioctl (fd, VIDIOC_QUERYMENU, &querymenu))
                {
                        cerr << "  " <<  querymenu.name << endl;
                } else
                {
                        errno_exit("VIDIOC_QUERYMENU", errmsg);
                        return;
                }
         }
}

int  V4L2_Base::query_ctrl(unsigned int ctrl_id, double & ctrl_min, double & ctrl_max, double & ctrl_step, double & ctrl_value, char *errmsg)
{ 

    struct v4l2_control control;

    CLEAR(queryctrl);

    queryctrl.id = ctrl_id;

   if (-1 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl))
   {
        if (errno != EINVAL) 
                return errno_exit ("VIDIOC_QUERYCTRL", errmsg);
                
        else 
        {
                cerr << "#" << ctrl_id << " is not supported" << endl;
                snprintf(errmsg, ERRMSGSIZ, "# %d is not supported", ctrl_id);
		return -1;
        }
   } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
   {
        cerr << "#" << ctrl_id << " is disabled" << endl;
        snprintf(errmsg, ERRMSGSIZ, "# %d is disabled", ctrl_id);
        return -1;
   }

   ctrl_min   = queryctrl.minimum;
   ctrl_max   = queryctrl.maximum;
   ctrl_step  = queryctrl.step;
   ctrl_value = queryctrl.default_value;

   /* Get current value */
   CLEAR(control);
   control.id = ctrl_id;

   if (0 == xioctl(fd, VIDIOC_G_CTRL, &control))
      ctrl_value = control.value;

    cerr << queryctrl.name << " -- min: " << ctrl_min << " max: " << ctrl_max << " step: " << ctrl_step << " value: " << ctrl_value << endl;

    return 0;

}

int  V4L2_Base::queryINTControls(INumberVectorProperty *nvp)
{
   struct v4l2_control control;
   char errmsg[ERRMSGSIZ];
   CLEAR(queryctrl);
   INumber *numbers = NULL;
   unsigned int *num_ctrls = NULL;
   int nnum=0;

  for (queryctrl.id = V4L2_CID_BASE; queryctrl.id < V4L2_CID_LASTP1; queryctrl.id++)
  {
        if (0 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl))
        {
                if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                {
                    cerr << queryctrl.name << " is disabled." << endl;
                    continue;
                }

                if (queryctrl.type == V4L2_CTRL_TYPE_INTEGER)
                {
                   numbers = (numbers == NULL) ? (INumber *) malloc (sizeof(INumber)) :
                                                 (INumber *) realloc (numbers, (nnum+1) * sizeof (INumber));

                   num_ctrls = (num_ctrls == NULL) ? (unsigned int *) malloc  (sizeof (unsigned int)) :
                                                     (unsigned int *) realloc (num_ctrls, (nnum+1) * sizeof (unsigned int));

                   strncpy(numbers[nnum].name, ((char *) queryctrl.name) , MAXINDINAME);
                   strncpy(numbers[nnum].label, ((char *) queryctrl.name), MAXINDILABEL);
                   strncpy(numbers[nnum].format, "%0.f", MAXINDIFORMAT);
                   numbers[nnum].min    = queryctrl.minimum;
                   numbers[nnum].max    = queryctrl.maximum;
                   numbers[nnum].step   = queryctrl.step;
                   numbers[nnum].value  = queryctrl.default_value;
     
                   /* Get current value if possible */
                   CLEAR(control);
                   control.id = queryctrl.id;
                   if (0 == xioctl(fd, VIDIOC_G_CTRL, &control))
                            numbers[nnum].value = control.value;

                   /* Store ID info in INumber. This is the first time ever I make use of aux0!! */
                   num_ctrls[nnum] = queryctrl.id;

                   cerr << queryctrl.name << " -- min: " << queryctrl.minimum << " max: " << queryctrl.maximum << " step: " << queryctrl.step << " value: " << numbers[nnum].value << endl;

                   nnum++;
                  
                }
        } else if (errno != EINVAL) {
		if(numbers) free(numbers);
                return errno_exit ("VIDIOC_QUERYCTRL", errmsg);
        }

   }

  for (queryctrl.id = V4L2_CID_PRIVATE_BASE;  ;  queryctrl.id++)
  {
     if (0 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl))
        {
                if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                {
                    cerr << queryctrl.name << " is disabled." << endl;
                    continue;
                }

                if (queryctrl.type == V4L2_CTRL_TYPE_INTEGER)
                {
                   numbers = (numbers == NULL) ? (INumber *) malloc (sizeof(INumber)) :
                                                 (INumber *) realloc (numbers, (nnum+1) * sizeof (INumber));

                   num_ctrls = (num_ctrls == NULL) ? (unsigned int *) malloc  (sizeof (unsigned int)) :
                                                     (unsigned int *) realloc (num_ctrls, (nnum+1) * sizeof (unsigned int));

                   strncpy(numbers[nnum].name, ((char *) queryctrl.name) , MAXINDINAME);
                   strncpy(numbers[nnum].label, ((char *) queryctrl.name), MAXINDILABEL);
                   strncpy(numbers[nnum].format, "%0.f", MAXINDIFORMAT);
                   numbers[nnum].min    = queryctrl.minimum;
                   numbers[nnum].max    = queryctrl.maximum;
                   numbers[nnum].step   = queryctrl.step;
                   numbers[nnum].value  = queryctrl.default_value;
     
                   /* Get current value if possible */
                   CLEAR(control);
                   control.id = queryctrl.id;
                   if (0 == xioctl(fd, VIDIOC_G_CTRL, &control))
                            numbers[nnum].value = control.value;

                   /* Store ID info in INumber. This is the first time ever I make use of aux0!! */
                   num_ctrls[nnum] = queryctrl.id;
     
                   nnum++;
                  
                }
         }
         else break;
   } 

  /* Store numbers in aux0 */
  for (int i=0; i < nnum; i++)
      numbers[i].aux0 = &num_ctrls[i];

  nvp->np  = numbers;
  nvp->nnp = nnum;

  return nnum;

}

int  V4L2_Base::setINTControl(unsigned int ctrl_id, double new_value, char *errmsg)
{
   struct v4l2_control control;

   CLEAR(control);

   //cerr << "The id is " << ctrl_id << " new value is " << new_value << endl;

   control.id = ctrl_id;
   control.value = (int) new_value;

   if (-1 == xioctl(fd, VIDIOC_S_CTRL, &control))
         return errno_exit ("VIDIOC_S_CTRL", errmsg);

   return 0;
}

