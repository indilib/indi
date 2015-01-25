/*
 * Copyright © 2008, Roland Roberts
 *
 * References
 *
 *    [TRM] EZ-USB Technical Reference Manual, Document #001-13670 Rev. *A
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "DsiTypes.h"
#include "DsiException.h"
#include "DsiDevice.h"

#include "Util.h"

/* Convenient mnemonic for libusb timeouts which are always in this unit. */
#ifndef MILLISEC
#  define MILLISEC 2
#endif

using namespace std;
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <memory>

#include <sys/time.h>

static unsigned int last_time;

static unsigned int
get_sysclock_ms()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (tv.tv_sec * 1000 + tv.tv_usec/1000);
}

static std::auto_ptr<std::string>
format_buffer(unsigned char data[], size_t length)
{
    ostringstream buffer;
    for (int i = 0; i < length; i++) {
        if (i > 0 && (i % 8) == 0) {
            break;
            buffer << endl << "    " << setfill('0') << setw(8) << hex << i;
        }
        buffer << " " << setfill('0') << setw(2) << hex << (unsigned int) data[i];
    }
    for (int i = 8-length; i > 0; i--) {
        buffer << "   ";
    }
    std::string *s = new std::string(buffer.str());
    std::auto_ptr<std::string> result(s);
    return result;
}

static void
log_command_info(bool iswrite, const char *prefix, unsigned int length, char *buffer, unsigned int *result)
{
    unsigned int now = get_sysclock_ms();
    stringstream tmp;
    tmp <<  prefix << " " << hex << (unsigned int) length;
    cerr << setfill(' ') << setw(40) << left << tmp.str();
    cerr << "[dt=" << dec << now-last_time << "]" << endl;
    last_time = now;

    tmp.str("");
    if (iswrite) {
        tmp << "    00000000:" << *format_buffer((unsigned char *) buffer, length);
        const DSI::DeviceCommand *command = DSI::DeviceCommand::find((int) buffer[2]);
        cerr << setfill(' ') << setw(60) << left << tmp.str() << command->name();
        if (result)
            cerr << " " << *result;
        cerr << endl;
    } else {
        if (strcmp(prefix, "r 86") != 0) {
            tmp << "    00000000:" << *format_buffer((unsigned char *) buffer, buffer[0]);
            cerr << setfill(' ') << setw(60) << left << tmp.str() << "ACK";
            if (result)
                cerr << " " << *result;
            cerr << endl;
        }
    }
}

/**
 * Initialize a generic (base class) DSI device.
 *
 * The generic device will initialize the image size parameters to correspond
 * to a test pattern as well as set the test pattern flag to true.  The
 * generic device has no way of knowing the correct CCD size parameters.
 * Derived classes are responsible for overriding these settings.  If they do
 * not, all you will be able to retrieve is a test pattern image.
 *
 * @param devname
 *
 * @return
 */
DSI::Device::Device(const char *devname) :
    usb_speed((UsbSpeed)UsbSpeed::FULL),
    readout_mode(ReadoutMode::DUAL)
{
    command_sequence_number = 0;
    eeprom_length    = -1;
    log_commands     = true;
    test_pattern     = true;
    exposure_time    = 10;
    read_width       = 540;
    read_height_even = 253;
    read_height_odd  = 252;
    read_height      = read_height_even + read_height_odd;
    read_bpp         = 2;
    image_width      = read_width;
    image_height     = read_height;
    image_offset_x   = 0;
    image_offset_y   = 0;

    initImager(devname);
}

DSI::Device::~Device()
{
    cerr << "in DSI::Device::~Device" << endl;
    int result;
    if (handle != 0) {
        result = usb_release_interface(handle, 0);
        cerr << "usb_release_interface(handle, 0) -> " << result << endl;
        result = usb_close(handle);
        cerr << "usb_close(handle) -> " << result << endl;
    }
    handle = 0;
    dev = 0;
    command_sequence_number = 0;
}

std::string
DSI::Device::getCameraName()
{
    if (camera_name.empty()) {
        loadCameraName();
    }
    return camera_name;
}

void
DSI::Device::setCameraName(std::string &newname)
{
    setString(newname, 0x1c, 0x20);
    loadCameraName();
    return;
}

void
DSI::Device::initImager(const char *devname) {

    string bus_name, device_name;
    if (devname != 0) {
        vector<string> foo = tokenize_str(devname, ":,");
        if ((  (foo.size() != 3))
            || (foo[0] != "usb"))
            throw dsi_exception(string("invalid device specifier, ") + devname);
        bus_name = foo[1];
        device_name = foo[2];
    }

    int retcode;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    /* All DSI devices appear to present as the same USB vendor:device values.
     * There does not seem to be any better way to find the device other than
     * to iterate over and find the match.  Fortunately, this is fast.
     */

    for (struct usb_bus *bus = usb_get_busses(); bus != 0; bus = bus->next) {
        if (!bus_name.empty() && (bus_name != bus->dirname))
            continue;
        for (dev = bus->devices; dev != 0; dev = dev->next) {
            if (!device_name.empty() && (device_name != dev->filename))
                continue;
            if ((  (dev->descriptor.idVendor = 0x156c))
                && (dev->descriptor.idProduct == 0x0101)) {
                cerr << "Found device "
                     << setw(4) << setfill('0') << hex
                     << dev->descriptor.idVendor << ":" << dev->descriptor.idProduct
                     << " on usb:"
                     << bus->dirname << "," << dev->filename << endl;
                if ((handle = usb_open(dev)) == 0) {
                    throw dsi_exception("Failed to open device, aborting");
                }
            }
            break;
        }
        if (handle != 0)
            break;
    }

    if (handle == 0)
        throw dsi_exception("no DSI device found");

    const size_t size = 0x400;
    unsigned char data[size];

    /* This is monkey code.  SniffUSB shows that the Meade driver is doing
     * this, but I can think of no reason why.  It does the equivalent of the
     * following sequence
     *
     *    - usb_get_descriptor 1
     *    - usb_get_descriptor 1
     *    - usb_get_descriptor 2
     *    - usb_set_configuration 1
     *    - get the serial number
     *    - get the chip name
     *    - ping the device
     *    - reset the device
     *    - load the firmware information
     *    - load the bus speed status
     *
     * libusb says I'm supposed to claim the interface before doing anything to
     * the device.  I'm not clear what "anything" includes, but I can't do it
     * before the usb_set_configuration command or else I get EBUSY.  So I
     * stick it in the middle of the above sequence at what appears to be the
     * first workable point.
     */

    /* According the the libusb 1.0 documentation (but remember this is libusb
     * 0.1 code), the "safe" way to set the configuration on a device is to
     *
     *  1. Query the configuration.
     *  2. If it is not the desired on, set the desired configuration.
     *  3. Claim the interface.
     *  4. Check the configuration to make sure it is what you selected.  If
     *     not, it means someone else got it.
     *
     * However, that does not seem to be what the USB trace is showing from
     * the Windows driver.  It shows the sequence below (sans the claim
     * interface call, but I'm not sure that actually sends data over the
     * wire).
     *
     */
    if ((retcode = usb_get_descriptor(handle, 0x01, 0x00, data, size)) < 0)
        throw dsi_exception(std::string("failed to get descriptor, ") + strerror(-retcode));

    if ((retcode = usb_get_descriptor(handle, 0x01, 0x00, data, size)) < 0)
        throw dsi_exception(std::string("failed to get descriptor, ") + strerror(-retcode));

    if ((retcode = usb_get_descriptor(handle, 0x02, 0x00, data, size)) < 0)
        throw dsi_exception(std::string("failed to get descriptor, ") + strerror(-retcode));

    if ((retcode = usb_set_configuration(handle, 1)) < 0)
        throw dsi_exception(std::string("failed to set configuration, ") + strerror(-retcode));

    if ((retcode = usb_claim_interface(handle, 0)) < 0)
        throw dsi_exception(std::string("failed to claim interface, ") + strerror(-retcode));

    /* This is included out of desperation, but it works :-|
     *
     * After running once, an attempt to run a second time appears, for some
     * unknown reason, to leave us unable to read from EP 0x81.  At the very
     * least, we need to clear this EP.  However, believing in the power of
     * magic, we clear them all.
     */
    if ((retcode = usb_clear_halt(handle, 0x01)) < 0)
        throw dsi_exception(std::string("failed to clear EP 0x01, ") + strerror(-retcode));

    if ((retcode = usb_clear_halt(handle, 0x81)) < 0)
        throw dsi_exception(std::string("failed to clear EP 0x81, ") + strerror(-retcode));

    if ((retcode = usb_clear_halt(handle, 0x86)) < 0)
        throw dsi_exception(std::string("failed to clear EP 0x86, ") + strerror(-retcode));


    command(DeviceCommand::PING);
    command(DeviceCommand::RESET);

    loadVersion();
    loadStatus();

    command(DeviceCommand::GET_READOUT_MODE);

    /* I thought this is what the Meade driver was doing but, while it
       appears to be retrieving EEPROM data, it is not this region of the
       EEPROM.  */
    loadCcdChipName();
    loadCameraName();
    // loadSerialNumber();

    // command(DeviceCommand::SET_EXP_MODE, ExposureMode::NORMAL.value());

}

void
DSI::Device::sendRegister(AdRegister adr, unsigned int arg)
{
    unsigned int val = (adr.value() << 9) | (arg & 0x1ff);
    command(DeviceCommand::AD_WRITE, val);
}


void
DSI::Device::loadCameraName()
{
    std::string *ptr = getString(0x1c, 0x20);
    camera_name = *ptr;
    delete ptr;
}

/**
 * Initialized DSI device version information.  This queries the device to
 * determine the device family, model, version, and revision.  However, the
 * device does not actually identify itself at all.  All DSI devices claim to
 * be family 10, model 1, revision 1.  If you really want to know what you
 * have, you need to @see getCCDNumber().
 *
 */
void
DSI::Device::loadVersion()
{
    unsigned int result = command(DeviceCommand::GET_VERSION);
    dsi_family   = (result >> 0x00) & 0xff;
    dsi_model    = (result >> 0x08) & 0xff;
    dsi_firmware_version  = (result >> 0x10) & 0xff;
    dsi_firmware_revision = (result >> 0x18) & 0xff;

    if (dsi_family != 10 || dsi_model != 1 || dsi_firmware_version != 1) {
        ostringstream msg;
        msg << "unsupported imager ("
            << dsi_family << "," << dsi_model << ","
            << dsi_firmware_version << "," << dsi_firmware_revision
            << ") should be (1,1,1,any)";
        throw out_of_range(msg.str());
    }
}

/**
 * Return the value of the specified A-D register.
 *
 * @param reg which register to query.
 *
 * @return register value.
 */
unsigned int
DSI::Device::getAdRegister(DSI::AdRegister reg)
{
    return command(DeviceCommand::AD_READ, reg.value());
}

/**
 * Set the value of the specified A-D register.
 *
 * @param reg which register to set.
 * @param newval new value to be assigned.
 *
 * XXX: What does the value mean?
 */
void
DSI::Device::setAdRegister(DSI::AdRegister reg, unsigned int newval)
{
    unsigned int key = reg.value();
    key = (key << 9) | (newval & 0x1ff);
    command(DeviceCommand::AD_READ, key);
}

/**
 * Retrieve the DSI device current readout mode.
 *
 * @return current readout mode.
 */
DSI::ReadoutMode
DSI::Device::getReadoutMode()
{
    int result = command(DeviceCommand::GET_READOUT_MODE);
    if (!ReadoutMode::isValidValue(result)) {
        ostringstream msg;
        msg << "ReadoutMode value (" << result << ") not recognized";
        throw out_of_range(msg.str());
    }
    return *ReadoutMode::find(result);
}

/**
 * Set the DSI device readout mode.
 *
 * @param rm
 */
void
DSI::Device::setReadoutMode(DSI::ReadoutMode rm)
{
    command(DeviceCommand::GET_READOUT_MODE, rm.value());
}

/**
 * Initialize the internal state to reflect the USB bus speed and whether or
 * not the DSI device has firmware debugging(?) enabled.
 *
 */
void
DSI::Device::loadStatus()
{
    int result = command(DeviceCommand::GET_STATUS);
    int _usbSpeed = (result & 0xff);
    int _fwDebug  = ((result << 8) & 0xff);

    if (!UsbSpeed::isValidValue(result)) {
        ostringstream msg;
        msg << "USB Speed value (" << _usbSpeed << ") not recognized";
        throw out_of_range(msg.str());
    }
    usb_speed = *UsbSpeed::find(result);
    firmware_debug  = (_fwDebug == 1);
}

/**
 * Retrieve the EEPROM length.
 *
 * @return EEPROM length, in bytes.
 */
unsigned int
DSI::Device::getEepromLength()
{
    if (eeprom_length < 0) {
        eeprom_length = command(DeviceCommand::GET_EEPROM_LENGTH);
    }
    return eeprom_length;
}

/**
 * Read one byte value from the EEPROM.
 *
 * @param __offset offset from EEPROM start of byte to read
 *
 * @return the value of the EEPROM byte at __offset.
 */
unsigned char
DSI::Device::getEepromByte(int __offset)
{
    return command(DeviceCommand::GET_EEPROM_BYTE, __offset);
}

unsigned char
DSI::Device::setEepromByte(unsigned char __val, int __offset) {
    return command(DeviceCommand::SET_EEPROM_BYTE, __offset | (__val << 8));
}

void
DSI::Device::setEepromData(unsigned char *__buffer, int __offset, int __length) {
    for (int i = 0; i < __length; i++) {
        setEepromByte(__buffer[i], __offset+i);
    }
}

/**
 * Retrieve a segment of the EEPROM data as a sequence of bytes.  The result
 * will be allocated on the heap as an array of unsigned bytes and must be
 * deleted by the caller.
 *
 *     unsigned char *eepromData = DSI::Device::getEepromData(0,8);
 *     ...
 *     delete [] eepromData;
 *
 * @param __offset EEPROM offset for start of data.
 * @param __length length of EEPROM region to read.
 *
 * @return
 */
unsigned char *
DSI::Device::getEepromData(int __offset, int __length)
{
    int eepromLength = getEepromLength();
    unsigned char *buffer = new unsigned char [__length];
    for (int i = 0; i < __length; i++) {
        if (__offset >= eepromLength)
            return buffer;
        buffer[i] = getEepromByte(__offset++);
    }
    return buffer;
}

/**
 * Initialize the DSI serial number by reading the EEPROM data.
 *
 * NB: the Meade driver appears to *write* to the EEPROM if the serial number
 * looks bogus.  It write a "serial number" generated from the current
 * date/time which is just plain weird.  I suspect that code was an artifact
 * of early engineering samples provided to the original developer that may
 * not have had serial number information burned in the EEPROM.  At least,
 * that would make some sense.
 */
void
DSI::Device::loadSerialNumber()
{
    unsigned char *eepromData = getEepromData(0, 8);
    serial_number = 0;
    for (int i = 0; i < 8; i++) {
        unsigned long long x = (unsigned int) eepromData[i];
        serial_number += x << (i * 8);
    }
    delete [] eepromData;
}

/**
 * Initialize the CCD chip name by reading the EEPROM data where it is stored.
 *
 */
void
DSI::Device::loadCcdChipName()
{
    std::string *ptr = getString(8, 20);
    ccd_chip_name = *ptr;
    delete ptr;
    if (ccd_chip_name == "None")
        ccd_chip_name = "ICX404AK";
}

/**
 * Return the CCD chip name for this device.
 *
 * @return string value for the CCD chip name.
 */
std::string
DSI::Device::getCcdChipName()
{
    return std::string(ccd_chip_name);
}

void
DSI::Device::setExposureTime(double exptime)
{
    exposure_time = 10000 * exptime;
};

double
DSI::Device::getExposureTime()
{
    return 0.0001 * exposure_time;
};


/**
 * Retrieve a string value from the specified EEPROM region.  The string will
 * be allocated on the heap and must be deleted by the caller.
 *
 *     std::string *foo = DSI::Device::getString(8, 20);
 *     ...
 *     delete [] foo;
 *
 * @param __offset EEPROM offset for start of string.
 * @param __length length of EEPROM region to read.
 *
 * @return allocated string formed from EEPROM data.
 */
std::string *
DSI::Device::getString(int __offset, int __length) {
    unsigned char *eepromData = getEepromData(__offset, __length);
    std::string result;
    if ((eepromData[0] == 0xff) || (eepromData[1] == 0xff) || (eepromData[2] == 0xff)) {
        result.assign("None");
    } else {
        result.assign((char *)eepromData+1, eepromData[0]);
    }
    delete [] eepromData;
    return new string(result);
}

void
DSI::Device::setString(std::string __value, int __offset, int __length) {
    unsigned char *value = new unsigned char[__length];
    memset(value, 0xff, __length);
    int n;
    if (__value.length() > __length - 2) {
        n = __length - 2;
    } else {
        n = __value.length();
    }
    value[0] = n;
    for (int i = 1; i < n+1; i++) {
        value[i] = __value[i-1];
    }
    setEepromData(value, __offset, __length);
}

unsigned char *
DSI::Device::getTestPattern()
{
    return getImage(DeviceCommand::TEST_PATTERN, 10);

}

unsigned char *
DSI::Device::getImage()
{
    if (test_pattern)
        return getTestPattern();
    else {
        // unsigned char *image = getImage(DeviceCommand::TRIGGER, 10);
        // delete [] image;
        return getImage(DeviceCommand::TRIGGER, exposure_time);
    }
}

unsigned char *
DSI::Device::getImage(int howlong)
{
    if (test_pattern)
        return getTestPattern();
    else {
        return getImage(DeviceCommand::TRIGGER, howlong);
    }
}

int
DSI::Device::getGain()
{
    return command(DeviceCommand::GET_GAIN);
}

int
DSI::Device::setGain(int gain)
{
    if ((gain < 0) || (gain > 63)) {
        return -1;
    }
    return command(DeviceCommand::GET_GAIN, gain);
}

int
DSI::Device::startExposure(int howlong)
{
        // Monkey code.  Monkey see (SniffUSB), monkey do).  Some part of this
        // is required because w/o it, I get segfaults on the second attempt
        // to run the code.
        int status;

        // status = command(DeviceCommand::GET_TEMP);
        // status = command(DeviceCommand::GET_EXP_MODE);
        // status = command(DeviceCommand::SET_GAIN,     0x3f);
        // status = command(DeviceCommand::SET_OFFSET,   0x00);
        status = command(DeviceCommand::SET_EXP_TIME, howlong);
        if (howlong < 10000) {
            status = command(DeviceCommand::SET_READOUT_SPD, ReadoutSpeed::HIGH.value());
            status = command(DeviceCommand::SET_NORM_READOUT_DELAY, 3);
            status = command(DeviceCommand::SET_READOUT_MODE, ReadoutMode::DUAL.value());
        } else {
            status = command(DeviceCommand::SET_READOUT_SPD, ReadoutSpeed::NORMAL.value());
            status = command(DeviceCommand::SET_NORM_READOUT_DELAY, 7);
            status = command(DeviceCommand::SET_READOUT_MODE, ReadoutMode::SINGLE.value());
        }

        status = command(DeviceCommand::GET_READOUT_MODE);
        if (howlong < 10000) {
            status = command(DeviceCommand::SET_VDD_MODE, VddMode::ON.value());
        } else {
            status = command(DeviceCommand::SET_VDD_MODE, VddMode::AUTO.value());
        }
        status = command(DeviceCommand::SET_GAIN, 0);
        // status = command(DeviceCommand::GET_READOUT_MODE);
        status = command(DeviceCommand::SET_OFFSET, 0x0ff);
        status = command(DeviceCommand::SET_FLUSH_MODE, FlushMode::CONTINUOUS.value());
        status = command(DeviceCommand::GET_READOUT_MODE);
        status = command(DeviceCommand::GET_EXP_TIME);

        status = command(DeviceCommand::TRIGGER);

        // XXX: wait for exposure to complete. signal handler to set abort
        // flag then clean up connection.

        return 0;
}

unsigned char *
DSI::Device::downloadImage()
{
        int status;
        unsigned int t_read_width;
        unsigned int t_read_height_even;
        unsigned int t_read_height_odd;
        unsigned int t_read_height;
        unsigned int t_read_bpp;
        unsigned int t_image_width;
        unsigned int t_image_height;
        unsigned int t_image_offset_x;
        unsigned int t_image_offset_y;


        t_read_width       = ((read_bpp * read_width / 512) + 1) * 256;
        t_read_height_even = read_height_even;
        t_read_height_odd  = read_height_odd;
        t_read_height      = t_read_height_even + t_read_height_odd;
        t_read_bpp         = read_bpp;
        t_image_width      = image_width;
        t_image_height     = image_height;
        t_image_offset_x   = image_offset_x;
        t_image_offset_y   = image_offset_y;

        unsigned int odd_size  = t_read_bpp * t_read_width * t_read_height_odd;
        unsigned int even_size = t_read_bpp * t_read_width * t_read_height_even;
        unsigned int all_size  = t_read_bpp * t_read_width * t_read_height;

        unsigned char *odd_data  = new unsigned char[odd_size];
        unsigned char *even_data = new unsigned char[even_size];
        unsigned char *all_data  = new unsigned char[all_size];

        /* XXX: There has to be  a way to calculate a more optimal readout
           time here. */
        status = usb_bulk_read(handle, 0x86, (char *) even_data, even_size, 60000*MILLISEC);
        if (log_commands) {
            log_command_info(false, "r 86", (status > 0 ? status : 0), (char *) even_data, 0);

            cerr << dec
                 << "read even data, status = (" << status << ") "
                 << (status > 0 ? "" : strerror(-status)) << endl
                 << "    requested " << even_size << " bytes "
                 << t_read_width << " x " << t_read_height_even << " (even pixels)" << endl;

        }

        if (status < 0) {
            stringstream ss;
            ss << dec
               << "read even data, status = (" << status << ") "
               << strerror(-status);
            throw device_read_error(ss.str());
        }

        status = usb_bulk_read(handle, 0x86, (char *) odd_data,  odd_size,  60000*MILLISEC);
        if (log_commands) {
            log_command_info(false, "r 86", (status > 0 ? status : 0), (char *) even_data, 0);

            cerr << dec
                 << "read odd data, status = (" << status <<  ") "
                 << (status > 0 ? "" : strerror(-status)) << endl
                 << "    requested " << odd_size << " bytes "
                 << t_read_width << " x " << t_read_height_odd << " (odd pixels)" << endl;
        }

        if (status < 0) {
            stringstream ss;
            ss << dec
               << "read odd data, status = (" << status <<  ") "
               << strerror(-status);
            throw device_read_error(ss.str());
        }
	// roryt  - decode the data to an image
        // memcpy(all_data, even_data, even_size);
        // memcpy(all_data+even_size, odd_data, odd_size);
	// just the monochrome interlaced cameras for the moment

	unsigned char msb, lsb, is_odd;
	unsigned int x_ptr, line_start, y_ptr, read_ptr,write_ptr;
	char* even = (char *) even_data;
	char* odd  = (char *) odd_data;

        if (log_commands)
            cerr	<< "t_image_height  ="  << t_image_height
			<< endl
			<< "t_image_width   ="	<< t_image_width
			<< endl
			<< "t_image_offset_x="  << t_image_offset_x
			<< endl
			<< "t_image_offset_y="  << t_image_offset_y
			<< endl
			<< "t_read_width    ="  << t_read_width
			<< endl
			<< "t_read_height   ="  << t_read_height
			<< endl
			<< "t_read_bpp      ="  << t_read_bpp
			<< endl;

	for (write_ptr = y_ptr = 0; y_ptr < t_image_height; y_ptr++)
	{
            line_start = t_read_width * ((y_ptr + t_image_offset_y) /2);
            is_odd = (y_ptr + t_image_offset_y) % 2;

            cerr << "starting image row " << y_ptr
                 << ", write_ptr=" << write_ptr
                 << ", line_start=" << line_start
                 << ", is_odd=" << (is_odd == 0 ? 0 : 1)
                 << ", read_ptr=" << (line_start+t_image_offset_x)*2 << endl;
            for (x_ptr = 0; x_ptr < t_image_width ; x_ptr++)
            {
                read_ptr =  (line_start + x_ptr + t_image_offset_x) * 2;
                if (is_odd == 1) // odd line
                {
                    msb=odd[read_ptr];
                    lsb=odd[read_ptr + 1];
                } else {		// even line
                        msb=even[read_ptr];
                        lsb=even[read_ptr + 1];
                }
                all_data[write_ptr++] = msb;
                all_data[write_ptr++] = lsb;
            }
    }
    if (log_commands)
            cerr << "write_ptr=" << write_ptr << endl;

    delete [] odd_data;
    delete [] even_data;

    return all_data;


    throw dsi_exception("unsupported image command");

}

unsigned char *
DSI::Device::getImage(DeviceCommand __command, int howlong)
{

    if ((  (__command == DeviceCommand::TRIGGER))
        || (__command == DeviceCommand::TEST_PATTERN)) {

        // Monkey code.  Monkey see (SniffUSB), monkey do).  Some part of this
        // is required because w/o it, I get segfaults on the second attempt
        // to run the code.
        int status;

        // status = command(DeviceCommand::GET_TEMP);
        // status = command(DeviceCommand::GET_EXP_MODE);
        // status = command(DeviceCommand::SET_GAIN,     0x3f);
        // status = command(DeviceCommand::SET_OFFSET,   0x00);
        status = command(DeviceCommand::SET_EXP_TIME, howlong);
        if (howlong < 10000) {
            status = command(DeviceCommand::SET_READOUT_SPD, ReadoutSpeed::HIGH.value());
            status = command(DeviceCommand::SET_NORM_READOUT_DELAY, 3);
            status = command(DeviceCommand::SET_READOUT_MODE, ReadoutMode::DUAL.value());
        } else {
            status = command(DeviceCommand::SET_READOUT_SPD, ReadoutSpeed::NORMAL.value());
            status = command(DeviceCommand::SET_NORM_READOUT_DELAY, 7);
            status = command(DeviceCommand::SET_READOUT_MODE, ReadoutMode::SINGLE.value());
        }

        status = command(DeviceCommand::GET_READOUT_MODE);
        if (howlong < 10000) {
            status = command(DeviceCommand::SET_VDD_MODE, VddMode::ON.value());
        } else {
            status = command(DeviceCommand::SET_VDD_MODE, VddMode::AUTO.value());
        }
        status = command(DeviceCommand::SET_GAIN, 0);
        // status = command(DeviceCommand::GET_READOUT_MODE);
        status = command(DeviceCommand::SET_OFFSET, 0x0ff);
        status = command(DeviceCommand::SET_FLUSH_MODE, FlushMode::CONTINUOUS.value());
        status = command(DeviceCommand::GET_READOUT_MODE);
        status = command(DeviceCommand::GET_EXP_TIME);

        status = command(__command);

        // XXX: wait for exposure to complete. signal handler to set abort
        // flag then clean up connection.

        unsigned int t_read_width;
        unsigned int t_read_height_even;
        unsigned int t_read_height_odd;
        unsigned int t_read_height;
        unsigned int t_read_bpp;
        unsigned int t_image_width;
        unsigned int t_image_height;
        unsigned int t_image_offset_x;
        unsigned int t_image_offset_y;

        /* XXX: I'm a bit confused about the test pattern.  It *looks* like
         * the camera always sends back the same amount of data, but the
         * interpretation is different for the test pattern.  So,
         */

        if (__command == DeviceCommand::TRIGGER) {

            t_read_width       = ((read_bpp * read_width / 512) + 1) * 256;
            t_read_height_even = read_height_even;
            t_read_height_odd  = read_height_odd;
            t_read_height      = t_read_height_even + t_read_height_odd;
            t_read_bpp         = read_bpp;
            t_image_width      = image_width;
            t_image_height     = image_height;
            t_image_offset_x   = image_offset_x;
            t_image_offset_y   = image_offset_y;

        } else {

            /* We ignore the actual CCD chip size information for the test
             * pattern image and use our temporary values.
             */

            t_read_width       = 540;
            t_read_height_even = 0xfd;
            t_read_height_odd  = 0xfc;
            t_read_height      = t_read_height_even + t_read_height_odd;
            t_read_bpp         = 2;
            t_image_width      = t_read_width;
            t_image_height     = t_read_height;
            t_image_offset_x   = 0;
            t_image_offset_y   = 0;
        }

        unsigned int odd_size  = t_read_bpp * t_read_width * t_read_height_odd;
        unsigned int even_size = t_read_bpp * t_read_width * t_read_height_even;
        unsigned int all_size  = t_read_bpp * t_read_width * t_read_height;

        unsigned char *odd_data  = new unsigned char[odd_size];
        unsigned char *even_data = new unsigned char[even_size];
        unsigned char *all_data  = new unsigned char[all_size];

        /* The Meade driver seems to only issue a GET_EXP_TIME_COUNT command
         * when the exposure is over about 2 seconds (count = 20,000).  From
         * testing, it looks like if I try to issue this command for exposures
         * shorter, the camera locks up and has to be physically disconnected
         * and reconnected.
         */
        int time_left = howlong;
        while (time_left > 5000) {
            int sleep_time = 100 * (time_left - 5000);
            usleep(sleep_time);
            time_left = command(DeviceCommand::GET_EXP_TIMER_COUNT);
        }

        if (last_time == 0)
            last_time = get_sysclock_ms();

        /* XXX: There has to be  a way to calculate a more optimal readout
           time here. */
        status = usb_bulk_read(handle, 0x86, (char *) even_data, even_size, 60000*MILLISEC);
        if (log_commands) {
            log_command_info(false, "r 86", (status > 0 ? status : 0), (char *) even_data, 0);

            cerr << dec
                 << "read even data, status = (" << status << ") "
                 << (status > 0 ? "" : strerror(-status)) << endl
                 << "    requested " << even_size << " bytes "
                 << t_read_width << " x " << t_read_height_even << " (even pixels)" << endl;

        }

        if (status < 0) {
            stringstream ss;
            ss << dec
               << "read even data, status = (" << status << ") "
               << strerror(-status);
            throw device_read_error(ss.str());
        }

        status = usb_bulk_read(handle, 0x86, (char *) odd_data,  odd_size,  60000*MILLISEC);
        if (log_commands) {
            log_command_info(false, "r 86", (status > 0 ? status : 0), (char *) even_data, 0);

            cerr << dec
                 << "read odd data, status = (" << status <<  ") "
                 << (status > 0 ? "" : strerror(-status)) << endl
                 << "    requested " << odd_size << " bytes "
                 << t_read_width << " x " << t_read_height_odd << " (odd pixels)" << endl;
        }

        if (status < 0) {
            stringstream ss;
            ss << dec
               << "read odd data, status = (" << status <<  ") "
               << strerror(-status);
            throw device_read_error(ss.str());
        }
	// roryt  - decode the data to an image
        // memcpy(all_data, even_data, even_size);
        // memcpy(all_data+even_size, odd_data, odd_size);
	// just the monochrome interlaced cameras for the moment

	unsigned char msb, lsb, is_odd;
	unsigned int x_ptr, line_start, y_ptr, read_ptr,write_ptr;
	char* even = (char *) even_data;
	char* odd  = (char *) odd_data;

        if (log_commands)
            cerr	<< "t_image_height  ="  << t_image_height
			<< endl
			<< "t_image_width   ="	<< t_image_width
			<< endl
			<< "t_image_offset_x="  << t_image_offset_x
			<< endl
			<< "t_image_offset_y="  << t_image_offset_y
			<< endl
			<< "t_read_width    ="  << t_read_width
			<< endl
			<< "t_read_height   ="  << t_read_height
			<< endl
			<< "t_read_bpp      ="  << t_read_bpp
			<< endl;

	for (write_ptr = y_ptr = 0; y_ptr < t_image_height; y_ptr++)
	{
            line_start = t_read_width * ((y_ptr + t_image_offset_y) /2);
            is_odd = (y_ptr + t_image_offset_y) % 2;

            cerr << "starting image row " << y_ptr
                 << ", write_ptr=" << write_ptr
                 << ", line_start=" << line_start
                 << ", is_odd=" << (is_odd == 0 ? 0 : 1)
                 << ", read_ptr=" << (line_start+t_image_offset_x)*2 << endl;
            for (x_ptr = 0; x_ptr < t_image_width ; x_ptr++)
            {
                read_ptr =  (line_start + x_ptr + t_image_offset_x) * 2;
                if (is_odd == 1) // odd line
                {
                    msb=odd[read_ptr];
                    lsb=odd[read_ptr + 1];
                } else {		// even line
                    msb=even[read_ptr];
                    lsb=even[read_ptr + 1];
                }
                all_data[write_ptr++] = msb;
                all_data[write_ptr++] = lsb;
            }
	}
        if (log_commands)
            cerr << "write_ptr=" << write_ptr << endl;

        delete [] odd_data;
        delete [] even_data;

        return all_data;

    }

    throw dsi_exception("unsupported image command");

}


/**
 * Internal helper for sending a command to the DSI device.  If the command is
 * one which requires no parameters, then the actual execution will be
 * delegated to command(DeviceCommand,int,int), otherwise a dsi_exception is
 * thrown.
 *
 * @param __command command to be executed.
 *
 * @return decoded command response.
 */
unsigned int
DSI::Device::command(DeviceCommand __command)
{

    // This is the one place where having class-based enums instead of
    // built-in enums is annoying: you can't use a switch statement here.
    if ((  (__command == DeviceCommand::PING))
        || (__command == DeviceCommand::RESET)
        || (__command == DeviceCommand::ABORT)
        || (__command == DeviceCommand::TRIGGER)
        || (__command == DeviceCommand::PS_ON)
        || (__command == DeviceCommand::PS_OFF)
        || (__command == DeviceCommand::CCD_VDD_ON)
        || (__command == DeviceCommand::CCD_VDD_OFF)
        || (__command == DeviceCommand::TEST_PATTERN)
        || (__command == DeviceCommand::ERASE_EEPROM)
        || (__command == DeviceCommand::GET_VERSION)
        || (__command == DeviceCommand::GET_STATUS)
        || (__command == DeviceCommand::GET_TIMESTAMP)
        || (__command == DeviceCommand::GET_EXP_TIME)
        || (__command == DeviceCommand::GET_EXP_TIMER_COUNT)
        || (__command == DeviceCommand::GET_EEPROM_VIDPID)
        || (__command == DeviceCommand::GET_EEPROM_LENGTH)
        || (__command == DeviceCommand::GET_GAIN)
        || (__command == DeviceCommand::GET_EXP_MODE)
        || (__command == DeviceCommand::GET_VDD_MODE)
        || (__command == DeviceCommand::GET_FLUSH_MODE)
        || (__command == DeviceCommand::GET_CLEAN_MODE)
        || (__command == DeviceCommand::GET_READOUT_SPD)
        || (__command == DeviceCommand::GET_READOUT_MODE)
        || (__command == DeviceCommand::GET_OFFSET)
        || (__command == DeviceCommand::GET_NORM_READOUT_DELAY)
        || (__command == DeviceCommand::GET_TEMP))
        return command(__command, 0, 3);

    throw dsi_exception("unsupported device command " + __command.name());
};

/**
 * Internal helper for sending a command to the DSI device.  This determines
 * what the length of the actual command will be and then delgates to
 * command(DeviceCommand,int,int) or command(DeviceCommand).
 *
 * @param __command command to be executed.
 * @param __option command argument, ignored for SOME commands.
 *
 * @return decoded command response.
 */
unsigned int
DSI::Device::command(DeviceCommand __command, int __option)
{
    // This is the one place where having class-based enums instead of
    // built-in enums is annoying: you can't use a switch statement here.
    if ((   __command == DeviceCommand::GET_EEPROM_BYTE)
        || (__command == DeviceCommand::SET_GAIN)
        || (__command == DeviceCommand::SET_EXP_MODE)
        || (__command == DeviceCommand::SET_VDD_MODE)
        || (__command == DeviceCommand::SET_FLUSH_MODE)
        || (__command == DeviceCommand::SET_CLEAN_MODE)
        || (__command == DeviceCommand::SET_READOUT_SPD)
        || (__command == DeviceCommand::SET_READOUT_MODE)
        || (__command == DeviceCommand::AD_READ)
        || (__command == DeviceCommand::GET_DEBUG_VALUE)) {
        return command(__command, __option, 4);
    } else if ((   __command == DeviceCommand::SET_EEPROM_BYTE)
               || (__command == DeviceCommand::SET_OFFSET)
               || (__command == DeviceCommand::SET_NORM_READOUT_DELAY)
               || (__command == DeviceCommand::SET_ROW_COUNT_ODD)
               || (__command == DeviceCommand::SET_ROW_COUNT_EVEN)
               || (__command == DeviceCommand::AD_WRITE)) {
        return command(__command, __option, 5);
    } else if ((   __command == DeviceCommand::SET_EXP_TIME)
               || (__command == DeviceCommand::SET_EEPROM_VIDPID)) {
        return command(__command, __option, 7);
    } else {
        return command(__command);
    }

    throw dsi_exception("unsupported device command " + __command.name());
}

/**
 * Internal helper for sending a command to the DSI device.  This determines
 * what the expected response length is and then delegates actually processing
 * to command(DeviceCommand,int,int,int).
 *
 * @param __command command to be executed.
 * @param __option
 * @param __length
 *
 * @return decoded command response.
 */
unsigned int
DSI::Device::command(DeviceCommand __command, int __option, int __length)
{
    // This is the one place where having class-based enums instead of
    // built-in enums is annoying: you can't use a switch statement here.
    if ((  (__command == DeviceCommand::PING))
        || (__command == DeviceCommand::RESET)
        || (__command == DeviceCommand::ABORT)
        || (__command == DeviceCommand::TRIGGER)
        || (__command == DeviceCommand::TEST_PATTERN)
        || (__command == DeviceCommand::SET_EEPROM_BYTE)
        || (__command == DeviceCommand::SET_GAIN)
        || (__command == DeviceCommand::SET_OFFSET)
        || (__command == DeviceCommand::SET_EXP_TIME)
        || (__command == DeviceCommand::SET_VDD_MODE)
        || (__command == DeviceCommand::SET_FLUSH_MODE)
        || (__command == DeviceCommand::SET_CLEAN_MODE)
        || (__command == DeviceCommand::SET_READOUT_SPD)
        || (__command == DeviceCommand::SET_READOUT_MODE)
        || (__command == DeviceCommand::SET_NORM_READOUT_DELAY)
        || (__command == DeviceCommand::SET_ROW_COUNT_ODD)
        || (__command == DeviceCommand::SET_ROW_COUNT_EVEN)
        || (__command == DeviceCommand::PS_ON)
        || (__command == DeviceCommand::PS_OFF)
        || (__command == DeviceCommand::CCD_VDD_ON)
        || (__command == DeviceCommand::CCD_VDD_OFF)
        || (__command == DeviceCommand::AD_WRITE)
        || (__command == DeviceCommand::SET_EEPROM_VIDPID)
        || (__command == DeviceCommand::ERASE_EEPROM)) {
        return command(__command, __option, __length, 0);
    }

    if ((  (__command == DeviceCommand::GET_EEPROM_LENGTH))
        || (__command == DeviceCommand::GET_EEPROM_BYTE)
        || (__command == DeviceCommand::GET_GAIN)
        || (__command == DeviceCommand::GET_EXP_MODE)
        || (__command == DeviceCommand::GET_VDD_MODE)
        || (__command == DeviceCommand::GET_FLUSH_MODE)
        || (__command == DeviceCommand::GET_CLEAN_MODE)
        || (__command == DeviceCommand::GET_READOUT_SPD)
        || (__command == DeviceCommand::GET_READOUT_MODE)) {
        return command(__command, __option, __length, 1);
    }

    if ((  (__command == DeviceCommand::GET_VERSION))
        || (__command == DeviceCommand::GET_STATUS)
        || (__command == DeviceCommand::GET_TIMESTAMP)
        || (__command == DeviceCommand::GET_EXP_TIME)
        || (__command == DeviceCommand::GET_EXP_TIMER_COUNT)
        || (__command == DeviceCommand::GET_EEPROM_VIDPID)) {
        return command(__command, __option, __length, 4);
    }

    if ((  (__command == DeviceCommand::GET_OFFSET))
        || (__command == DeviceCommand::GET_NORM_READOUT_DELAY)
        || (__command == DeviceCommand::SET_EXP_MODE)
        || (__command == DeviceCommand::GET_ROW_COUNT_ODD)
        || (__command == DeviceCommand::GET_ROW_COUNT_EVEN)
        || (__command == DeviceCommand::GET_TEMP)
        || (__command == DeviceCommand::AD_READ)
        || (__command == DeviceCommand::GET_DEBUG_VALUE)) {
        return command(__command, __option, __length, 2);
    }

    throw dsi_exception("unsupported device command " + __command.name());
}

/**
 * Internal helper for sending a command to the DSI device.  This formats the
 * command as a sequence of bytes and delegates to command(unsigned char *,int,int)
 *
 * @param __command command to be executed.
 * @param __option
 * @param __length
 * @param __expected
 *
 * @return decoded command response.
 */
unsigned int
DSI::Device::command(DeviceCommand __command, int __option, int __length, int __expected)
{
    const size_t __size = 0x40;
    unsigned char buffer[__size];
    command_sequence_number++;
    // XXX: Or do I omit this and shift things down for libusb?
    buffer[0] = __length;
    buffer[1] = command_sequence_number;
    buffer[2] = __command.value();
    switch (__length) {
        case 3:
            break;
        case 4:
            buffer[3] = __option;
            break;
        case 5:
            buffer[3] = 0x0ff & __option;
            buffer[4] = 0x0ff & (__option >> 8);
            break;
        case 7:
            buffer[3] = 0x0ff & __option;
            buffer[4] = 0x0ff & (__option >>  8);
            buffer[5] = 0x0ff & (__option >> 16);
            buffer[6] = 0x0ff & (__option >> 24);
            break;
        default:
            throw dsi_exception("unsupported command length");
    }
    return command(buffer, __length, __expected);
}

/**
 * Write a command buffer to the DSI device and decode the return buffer.
 *
 * @param __buffer raw command buffer.
 * @param __length length of the command buffer, in bytes.
 * @param __expected expected length of the response buffer, in bytes.
 *
 * @return decoded response as appropriate for the command.
 *
 * DSI commands return either 0, 1, 2, or 4 byte results.  The results are
 * nominally unsigned integers although in some cases (e.g. GET_VERSION),
 * the 4 bytes are actually 4 separate bytes.  However, all 4-byte responses
 * are treated as 32-bit unsigned integers and are decoded and returned that
 * way.  Similarly, 2-byte responses are treated as 16-bit unsigned integers
 * and are decoded and returned that way.
 */
unsigned int
DSI::Device::command(unsigned char *__buffer, int __length, int __expected)
{

    const DeviceCommand *command = DeviceCommand::find((int) __buffer[2]);
    //    if (command == 0)
    //        throw dsi_exception(std::string("unrecognized command in buffer: ") + __buffer[2]);

    unsigned int value;
    switch (__length) {
        case 3:
            break;
        case 4:
            value = getByteResult((unsigned char *) __buffer);
            break;
        case 5:
            value = getShortResult((unsigned char *) __buffer);
            break;
        case 7:
            value = getIntResult((unsigned char *) __buffer);
            break;
        default:
            throw dsi_exception("unsupported command length");
            break;
    }


    if (last_time == 0) {
        last_time = get_sysclock_ms();
    }

    int retcode = usb_bulk_write(handle, 0x01, (char *) __buffer, __buffer[0], 100*MILLISEC);
    if (retcode < 0) {
        throw device_write_error(std::string("usb_bulk_read error ") + strerror(-retcode));
    }

    if (log_commands)
        log_command_info(true, "w 1", __buffer[0], (char *) __buffer, (__length > 3 ? &value : 0));

    const size_t size = 0x40;
    char buffer[size];
    retcode = usb_bulk_read(handle, 0x81, buffer, size, 100*MILLISEC);

    if (retcode < 0) {
        throw device_read_error(std::string("usb_bulk_read error ") + strerror(-retcode));
    }

    if (buffer[0] != retcode) {
        ostringstream msg;
        msg << "response length " << dec << (int) buffer[0]
            << " does not match bytes read " << retcode << endl;
        throw bad_length(msg.str());
    }

    if ((unsigned char) buffer[1] != command_sequence_number) {
        if (log_commands) {
            log_command_info(false, "r 81", buffer[0], (char *) buffer, 0);
        }
        ostringstream msg;
        msg << "response sequence number (" << (unsigned int) buffer[1]
            << ") does not match request (" << command_sequence_number
            << ") for command " << command->name();
        throw bad_command(msg.str());
    }

    if (buffer[2] != (&DeviceResponse::ACK)->value()) {
        ostringstream msg;
        msg << "command " << command->name() << " did not get ACK (was "
            << setfill('0') << setw(2) << buffer[2] << ")";
        throw bad_response(msg.str());
    }

    unsigned int result = 0;
    switch (__expected) {
        case 0:
            break;
        case 1:
            result = getByteResult((unsigned char *) buffer);
            break;
        case 2:
            result = getShortResult((unsigned char *) buffer);
            break;
        case 4:
            result = getIntResult((unsigned char *) buffer);
            break;
        default:
            throw dsi_exception("unsupported result length");
            break;
    }

    if (log_commands)
        log_command_info(false, "r 81", 0x40, buffer, (__expected > 0 ? &result : 0));


    return result;
}

/**
 * Decode byte 4 of the buffer as an 8-bit unsigned integer.
 *
 * @param __buffer raw query result buffer from DSI.
 *
 * @return decoded value as an unsigned integer.
 */
unsigned int
DSI::Device::getByteResult(unsigned char *__buffer)
{
    return (unsigned int) __buffer[3];
}

/**
 * Decode bytes 4-5 of the buffer as a 16-bit big-endian unsigned integer.
 *
 * @param __buffer raw query result buffer from DSI.
 *
 * @return decoded value as an unsigned integer.
 */
unsigned int
DSI::Device::getShortResult(unsigned char *__buffer)
{
    return (unsigned int) ((__buffer[4] << 8) | __buffer[3]);
}

/**
 * Decode bytes 4-7 of the buffer as a 32-bit big-endian unsigned integer.
 *
 * @param __buffer raw query result buffer from DSI.
 *
 * @return decoded value as an unsigned integer.
 */
unsigned int
DSI::Device::getIntResult(unsigned char *__buffer)
{
    unsigned int result;
    result =                 __buffer[6];
    result = (result << 8) | __buffer[5];
    result = (result << 8) | __buffer[4];
    result = (result << 8) | __buffer[3];
    return result;
}

/**
 * Set the device state to "abort exposure requested." The exposure will not
 * be aborted as soon as possible
 *
 */
void
DSI::Device::abortExposure()
{
    abort_requested = true;
}
