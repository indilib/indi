#ifndef _QHY5_DRIVER_H_
#define _QHY5_DRIVER_H_

#include <libindi/indiccd.h>
#include <libindi/indiusbdevice.h>

#define USHORT unsigned short int
#define BYTE unsigned char

#define IMAGE_CCD 0
#define GUIDE_CCD 1

enum
{
    QHY_NORTH = 0x20,
    QHY_SOUTH = 0x40,
    QHY_EAST  = 0x10,
    QHY_WEST  = 0x80,
};


/*struct _qhy5_driver {
    usb_dev_handle *handle;

    void *image;
    size_t imagesize;
};*/


class QHY5Driver :  public INDI::USBDevice
{
    public:
        QHY5Driver();
        //  variables that we inherit from the indi ccd stuff
        //  and we need a local copy for stand-alone use

        int width;
        int height;
        int binw;
        int binh;
        int gain;
        int offw;
        int offh;
        int bpp;

        int impixw,impixh;

        bool hasGuide;
        bool hasST4;

        void *imageBuffer;
        int imageBufferSize;


        bool Connect();
        bool Disconnect();


/*    int ResetCamera();
 *    int ClearPixels(int flags,int cam);
        int LatchPixels(int flags,int cam,int xoffset,int yoffset,int width,int height,int xbin,int ybin);
        int ExposePixels(int flags,int cam,int xoffset,int yoffset,int width,int height,int xbin,int ybin, int ms);
        int ReadPixels(char *,int);

        int pulseGuide();

        virtual int SetParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);
        virtual int SetGuideParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);
        virtual int SetInterlaced(bool);*/

        int SetParams(int in_width, int in_height, int in_binw, int in_binh, int in_offw, int in_offh, int in_gain, int *pixw, int *pixh);
        int GetDefaultParam(int *width, int *height, int *binw, int *binh, int *gain);

        void *GetRow(int row);
        int ReadExposure();
        int StartExposure(unsigned int exposure);

        int Pulse(int direction, int duration_msec);
        int ResetCamera();


};

#endif
