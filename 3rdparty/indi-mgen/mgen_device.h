#ifndef MGEN_DEVICE_H
#define MGEN_DEVICE_H

//#include <pthread.h>
#include <queue>

#include "mgen.h"

class MGenDevice
{
protected:
    pthread_mutex_t _lock;
    struct ftdi_context * ftdi;
    bool is_device_connected;
    bool tried_turn_on;
    IOMode mode;
    unsigned short vid, pid;
    std::queue<unsigned int> button_queue;

public:
    bool lock();
    void unlock();

public:
    /** @brief Connecting a device identified by VID:PID.
     * @param vid is the Vendor identifier of the device to connect (0x0403 for instance).
     * @param pid is the Product identifier of the device to connect (0x6001 for instance).
     * @return 0 if successful
     * @return the return code of the last ftdi failure, and disable() is called.
     * @note PID:VID=0:0 is used as default to connect to the first available FTDI device.
     * @note If PID:VID connection is not successful, enumerates and log devices to debug output.
     */
    int Connect(unsigned short vid = 0, unsigned short pid = 0);
    bool isConnected() const { return is_device_connected; }
    void enable();
    void disable();

public:
    /* @brief Writing the query field of a command to the device.
     * @return the number of bytes written, or -1 if the command is invalid or device is not accessible.
     * @throw IOError when device communication is malfunctioning.
     */
    int write(IOBuffer const &) throw (IOError);

    /* @brief Reading the answer part of a command from the device.
     * @return the number of bytes read, or -1 if the command is invalid or device is not accessible.
     * @throw IOError when device communication is malfunctioning.
     */
    int read(IOBuffer &) throw (IOError);

public:
    /** @brief
     * @note Switches operational mode to UNKNOWN if successful.
     */
    int TurnPowerOn();

public:
    IOMode getOpMode() const { return mode; }
    int setOpMode(IOMode);
    static char const * const DBG_OpModeString(IOMode);

public:
    void pushButton(unsigned int _button);
    unsigned int popButton();

public:
    MGenDevice();
    ~MGenDevice();
};

#endif
