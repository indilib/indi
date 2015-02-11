/*
 * Copyright © 2008, Roland Roberts
 * Copyright © 2014, Ben Gilsrud
 *
 */

#ifndef __DsiDevice_hh
#define __DsiDevice_hh


#include <libusb-1.0/libusb.h>
#include <string>

#include "DsiTypes.h"

using namespace std;

namespace DSI {

    class Device {

      private:
        bool log_commands;
        int eeprom_length;
        std::string camera_name;

      protected:

        /* These are chip-specific sizes required to parameterize the image
         * retrieval.
         */
        unsigned int read_width;
        unsigned int read_height_even;
        unsigned int read_height_odd;
        unsigned int read_height;
        unsigned int read_bpp;
        unsigned int image_width;
        unsigned int image_height;
        unsigned int image_offset_x;
        unsigned int image_offset_y;
        float pixel_size_x;
        float pixel_size_y;

        /* Exposure time, multiple of 100 microseconds. */
        unsigned int exposure_time;

        /* If true, return a test pattern regardless of whether or not a real
         * image was requested. */
        bool test_pattern;

        /* True if camera is one-shot color, false otherwise. */
        bool is_color;

        /* True if camera is high-gain. */
        bool is_high_gain;

        /* True if the chip can do 2x2 binning.  No Meade DSI cameras can do
         * more than 2x2 binning. */
        bool is_binnable;

        /* Pixel aspect ratio */
        double aspect_ratio;

        libusb_device *dev;
        libusb_device_handle *handle;
        unsigned char command_sequence_number;

        ReadoutMode readout_mode;
        UsbSpeed usb_speed;
        bool firmware_debug;

        /* Helper function for low-level diagnostics.  I use this to compare
         * what I think I'm sending with what a USB sniffer tells me my
         * command translates into.
         */
        void print_data(std::string command, unsigned char buffer[], size_t length);

        /* You might think these tell you what DSI camera you have (I did),
         * but you would be mistaken.  I haven't found any camera that reports
         * anything different for these other than family 10, model 1. */
        unsigned int dsi_family;
        unsigned int dsi_model;

        /* DSI firmware version information.  It would appear that all DSI
         * firmware is version 1.  I know Meade has released updated drivers,
         * so it is possible that some of the releases had different versions
         * and revision numbers, but I haven't seen it to check. */
        unsigned int dsi_firmware_version;
        unsigned int dsi_firmware_revision;

        /* I don't know why we treat this as a number, may as well be a
         * string.  What I don't have is  */
        unsigned long long serial_number;
        std::string ccd_chip_name;

        bool abort_requested;

        unsigned int timeout_response;
        unsigned int timeout_request;
        unsigned int timeout_image;

        /* XXX: What are the units on these?  Milliseconds?  microseconds?
         * Communications timeout values.
         */
        static const unsigned int TIMEOUT_FULL_MAX_IMAGE    = 0x1770;
        static const unsigned int TIMEOUT_HIGH_MAX_IMAGE    = 0x0fa0;
        static const unsigned int TIMEOUT_FULL_MAX_RESPONSE = 0x03e8;
        static const unsigned int TIMEOUT_HIGH_MAX_RESPONSE = 0x03e8;
        static const unsigned int TIMEOUT_FULL_MAX_REQUEST  = 0x03e8;
        static const unsigned int TIMEOUT_HIGH_MAX_REQUEST  = 0x03e8;

        // Methods with "load" mean read and initialize the information from
        // the camera.
        void loadSerialNumber();
        void loadCcdChipName();
        void loadStatus();
        void loadVersion();
        void loadEepromLength();
        void loadCameraName();

        virtual ReadoutMode getReadoutMode();
        virtual void setReadoutMode(DSI::ReadoutMode rm);

        /* These are semi-generic and arguable belong in a helper class.
         * However, they know two things about the DSI.  First is the USB
         * handle (easily factored out into the call signature).  Second is
         * the command set.  This latter is DSI specific, so I'm leaving them
         * as protected members so derived classes can access them.
         */
        unsigned int command(DeviceCommand __command);
        unsigned int command(DeviceCommand __command, int __option);
        unsigned int command(DeviceCommand __command, int __option, int __length);
        unsigned int command(DeviceCommand __command, int __option, int __length, int __expected);

        /* These all have to do with reading from the EEPROM. */
        std::string *getString(int __offset, int __length);
        void setString(std::string __value, int __offset, int __length);
        unsigned int getEepromLength();
        unsigned char getEepromByte(int __offset);
        unsigned char *getEepromData(int ___offset, int __length);
        unsigned char setEepromByte(unsigned char __byte, int __offset);
        void setEepromData(unsigned char *__buffer, int ___offset, int __length);

        /* These arguably do not belong as part of the device specification,
         * but as a utility class of some sort.  They are in principle
         * applicable to anything which uses the the Cyrus EZ-USB FX2 chip.
         */
        unsigned int command(unsigned char *__buffer, int __length, int __expected);

        unsigned int getByteResult(unsigned char *__buffer);
        unsigned int getShortResult(unsigned char *__buffer);
        unsigned int getIntResult(unsigned char *__buffer);

        virtual void initImager(const char *devname = 0);

        virtual unsigned char *getImage(int howlong);
        virtual unsigned char *getImage(DeviceCommand __command, int howlong);

        void sendRegister(AdRegister adr, unsigned int arg);

      public:

        Device(const char *devname = 0);
        virtual ~Device();

        virtual unsigned int getReadWidth() { return read_width; };
        virtual unsigned int getReadHeightEven() { return read_height_even; };
        virtual unsigned int getReadHeightOdd() { return read_height_odd; };
        virtual unsigned int getReadHeight() { return read_height; };
        virtual unsigned int getReadBpp() { return read_bpp; };
        virtual unsigned int getImageWidth() { return image_width; };
        virtual unsigned int getImageHeight() { return image_height; };
        virtual unsigned int getImageOffsetX() { return image_offset_x; };
        virtual unsigned int getImageOffsetY() { return image_offset_y; };
        virtual float getPixelSizeX() { return pixel_size_x; };
        virtual float getPixelSizeY() { return pixel_size_y; };

        virtual void abortExposure();
        virtual unsigned int getAdRegister(DSI::AdRegister reg);
        virtual void setAdRegister(DSI::AdRegister reg, unsigned int newval);
        virtual std::string getCcdChipName();
        virtual std::string getCameraName();
        virtual void setCameraName(std::string &);

        virtual unsigned char *getTestPattern();
        virtual unsigned char *getImage();

        virtual void setExposureTime(double exptime);
        virtual double getExposureTime();
        virtual unsigned char *downloadImage();
        virtual int startExposure(int howlong);

        virtual int getGain();
        virtual int setGain(int gain);
        void setDebug(bool turnOn) { log_commands = turnOn; };
        bool isDebug() { return log_commands; }

    };
};

#endif /* __DsiDevice_hh */
