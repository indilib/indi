/* definitions for the SX camera control packets */

/*
 * CCD color representation.
 *  Packed colors allow individual sizes up to 16 bits.
 *  2X2 matrix bits are represented as:
 *      0 1
 *      2 3
 */
#define SXCCD_COLOR_PACKED_RGB          0x8000
#define SXCCD_COLOR_PACKED_BGR          0x4000
#define SXCCD_COLOR_PACKED_RED_SIZE     0x0F00
#define SXCCD_COLOR_PACKED_GREEN_SIZE   0x00F0
#define SXCCD_COLOR_PACKED_BLUE_SIZE    0x000F
#define SXCCD_COLOR_MATRIX_ALT_EVEN     0x2000
#define SXCCD_COLOR_MATRIX_ALT_ODD      0x1000
#define SXCCD_COLOR_MATRIX_2X2          0x0000
#define SXCCD_COLOR_MATRIX_RED_MASK     0x0F00
#define SXCCD_COLOR_MATRIX_GREEN_MASK   0x00F0
#define SXCCD_COLOR_MATRIX_BLUE_MASK    0x000F
#define SXCCD_COLOR_MONOCHROME          0x0FFF
/*
 * Caps bit definitions.
 */
#define SXCCD_CAPS_STAR2K               0x01
#define SXCCD_CAPS_COMPRESS             0x02
#define SXCCD_CAPS_EEPROM               0x04
#define SXCCD_CAPS_GUIDER               0x08
/*
 * CCD command options.
 */
#define SXCCD_EXP_FLAGS_FIELD_ODD     	1
#define SXCCD_EXP_FLAGS_FIELD_EVEN    	2
#define SXCCD_EXP_FLAGS_FIELD_BOTH    	(SXCCD_EXP_FLAGS_FIELD_EVEN|SXCCD_EXP_FLAGS_FIELD_ODD)
#define SXCCD_EXP_FLAGS_FIELD_MASK    	SXCCD_EXP_FLAGS_FIELD_BOTH
#define SXCCD_EXP_FLAGS_NOBIN_ACCUM   	4
#define SXCCD_EXP_FLAGS_NOWIPE_FRAME  	8
#define SXCCD_EXP_FLAGS_TDI             32
#define SXCCD_EXP_FLAGS_NOCLEAR_FRAME 	64
/*
 * Control request fields.
 */
#define USB_REQ_TYPE                0
#define USB_REQ                     1
#define USB_REQ_VALUE_L             2
#define USB_REQ_VALUE_H             3
#define USB_REQ_INDEX_L             4
#define USB_REQ_INDEX_H             5
#define USB_REQ_LENGTH_L            6
#define USB_REQ_LENGTH_H            7
#define USB_REQ_DATA                8
#define USB_REQ_DIR(r)              ((r)&(1<<7))
#define USB_REQ_DATAOUT             0x00
#define USB_REQ_DATAIN              0x80
#define USB_REQ_KIND(r)             ((r)&(3<<5))
#define USB_REQ_VENDOR              (2<<5)
#define USB_REQ_STD                 0
#define USB_REQ_RECIP(r)            ((r)&31)
#define USB_REQ_DEVICE              0x00
#define USB_REQ_IFACE               0x01
#define USB_REQ_ENDPOINT            0x02
#define USB_DATAIN                  0x80
#define USB_DATAOUT                 0x00
/*
 * CCD camera control commands.
 */
#define SXUSB_GET_FIRMWARE_VERSION  255
#define SXUSB_ECHO                  0
#define SXUSB_CLEAR_PIXELS          1
#define SXUSB_READ_PIXELS_DELAYED   2
#define SXUSB_READ_PIXELS           3
#define SXUSB_SET_TIMER             4
#define SXUSB_GET_TIMER             5
#define SXUSB_RESET                 6
#define SXUSB_SET_CCD               7
#define SXUSB_GET_CCD               8
#define SXUSB_SET_STAR2K            9
#define SXUSB_WRITE_SERIAL_PORT     10
#define SXUSB_READ_SERIAL_PORT      11
#define SXUSB_SET_SERIAL            12
#define SXUSB_GET_SERIAL            13
#define SXUSB_CAMERA_MODEL          14
#define SXUSB_LOAD_EEPROM           15

#define SX_GUIDE_EAST             0x08     /* RA+ */
#define SX_GUIDE_NORTH            0x04     /* DEC+ */
#define SX_GUIDE_SOUTH            0x02     /* DEC- */
#define SX_GUIDE_WEST             0x01     /* RA- */


#define SX_CLEAR_NS               0x09
#define SX_CLEAR_WE               0x06


#define USHORT unsigned short int
#define BYTE unsigned char

#define IMAGE_CCD 0
#define GUIDE_CCD 1

typedef struct t_sxccd_params
{
    USHORT hfront_porch;
    USHORT hback_porch;
    USHORT width;
    USHORT vfront_porch;
    USHORT vback_porch;
    USHORT height;
    float  pix_width;
    float  pix_height;
    USHORT color_matrix;
    BYTE   bits_per_pixel;
    BYTE   num_serial_ports;
    BYTE   extra_caps;
    BYTE   vclk_delay;
} CCDPARMS, *PCCDPARMS;

#include <libindi/indiccd.h>
#include <libindi/indiusbdevice.h>



class SxCCD :  public INDI::USBDevice
{
    public:
        SxCCD();
        //  variables that we inherit from the indi ccd stuff
        //  and we need a local copy for stand-alone use
        int CameraModel;
        int SubType;
        int ColorSensor;
        //int Interlaced;

        float pixwidth;
        float pixheight;
        int xres;
        int yres;
        int gxres;
        int gyres;
        bool sx_hasguide;
        bool sx_hasST4;

        float gpixwidth;
        float gpixheight;

        struct t_sxccd_params parms;
        struct t_sxccd_params gparms;

        double north_guide, south_guide, west_guide, east_guide;
        unsigned char guide_cmd;

        int bits_per_pixel;
        int gbits_per_pixel;

        bool Connect();
        bool Disconnect();

        void getDefaultParam();
        void getCapabilites();

        int ResetCamera();
        int GetCameraModel();
        int GetFirmwareVersion();
        int GetCameraParams(int,PCCDPARMS);
        //int SetCameraParams(int,PCCDPARMS);

        int ClearPixels(int flags,int cam);
        int LatchPixels(int flags,int cam,int xoffset,int yoffset,int width,int height,int xbin,int ybin);
        int ExposePixels(int flags,int cam,int xoffset,int yoffset,int width,int height,int xbin,int ybin, int ms);
        int ReadPixels(char *,int);

        int pulseGuide();

        virtual int SetParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);
        virtual int SetGuideParams(int XRes,int YRes,int CamBits,float pixwidth,float pixheight);
        virtual int SetInterlaced(bool);


};

