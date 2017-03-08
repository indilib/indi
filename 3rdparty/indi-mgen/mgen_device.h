#ifndef MGEN_DEVICE_H
#define MGEN_DEVICE_H

#include <pthread.h>
#include <queue>

#include "mgen.h"

class MGenDeviceState
{
public:
    pthread_mutex_t _lock;
    struct ftdi_context * ftdi;
    bool is_device_connected;
    bool tried_turn_on;
    unsigned int no_ack_count;
    IOMode mode;
    std::queue<unsigned int> button_queue;

public:
    bool lock();
    void unlock();

public:
    bool isConnected() const { return is_device_connected; }
    void enable();
    void disable();

public:
    IOMode getOpMode() const { return mode; }
    void setOpMode(IOMode _mode);

public:
    void pushButton(unsigned int _button);
    unsigned int popButton();

public:
    MGenDeviceState();
    ~MGenDeviceState();
};

#endif
