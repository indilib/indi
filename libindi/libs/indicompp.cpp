

#include "indicompp.h"


INDI::TTY::TTY() :
    _tty_fd(0)
{}

INDI::TTY::~TTY()
{}
        
int 
INDI::TTY::connect(
    const char *device, int bit_rate, int word_size, 
    int parity, int stop_bits)
{
    return ::tty_connect(device, bit_rate, word_size, parity, stop_bits, &_tty_fd);
}

int
INDI::TTY::getPortFD()
{
    return _tty_fd;
}

int 
INDI::TTY::disconnect()
{
    return ::tty_disconnect(_tty_fd);
}
        
int 
INDI::TTY::read(char *buf, int nbytes, int timeout, int *nbytes_read)
{
    return ::tty_read(_tty_fd, buf, nbytes, timeout, nbytes_read);
}
        
int 
INDI::TTY::read_section(char *buf, char stop_char, 
        int timeout, int *nbytes_read)
{
    return ::tty_read_section(_tty_fd, buf, stop_char, timeout, nbytes_read);
}
        
int 
INDI::TTY::write(const char * buffer, int nbytes, int *nbytes_written)
{
    return ::tty_write(_tty_fd, buffer, nbytes, nbytes_written);
}
        
int 
INDI::TTY::write_string(const char * buffer, int *nbytes_written)
{
    return ::tty_write_string(_tty_fd, buffer, nbytes_written);
}
        
void 
INDI::TTY::error_msg(int err_code, char *err_msg, int err_msg_len)
{
    ::tty_error_msg(err_code, err_msg, err_msg_len);
}

void 
INDI::TTY::set_debug(int debug)
{
}
        
int 
INDI::TTY::timeout(int timeout)
{
    return ::tty_timeout(_tty_fd, timeout);
}

#include <termios.h>
#include <unistd.h>

int
INDI::TTY::tcflush(int queue_selector)
{
   return ::tcflush(_tty_fd, queue_selector); 
}
        
