
#include <stdio.h>

#include "indilogger.h"

#include "mgen.h"
#include "mgenautoguider.h"
#include "mgen_device.h"

MGenDeviceState::MGenDeviceState():
    _lock(),
    ftdi(NULL),
    is_device_connected(false),
    tried_turn_on(false),
    no_ack_count(0),
    mode(OPM_UNKNOWN),
    button_queue()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_lock,&attr);
    pthread_mutexattr_destroy(&attr);
}

MGenDeviceState::~MGenDeviceState()
{
    pthread_mutex_destroy(&_lock);
}

bool MGenDeviceState::lock() { return !pthread_mutex_lock(&_lock); }
void MGenDeviceState::unlock() { pthread_mutex_unlock(&_lock); }

void MGenDeviceState::enable() { if(lock()) { is_device_connected = true; unlock(); } }
void MGenDeviceState::disable() { if(lock()) { is_device_connected = false; unlock(); } }

void MGenDeviceState::setOpMode(IOMode _mode) { if(lock()) { mode = _mode; unlock(); } }

void MGenDeviceState::pushButton(unsigned int _button) { _L("sending key %d to remote UI",_button); if(lock()) { button_queue.push(_button); unlock(); } }
unsigned int MGenDeviceState::popButton() { unsigned int b = -1; if(lock()) { b = button_queue.front(); button_queue.pop(); unlock(); } return b; }
