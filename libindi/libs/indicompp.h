
#ifndef INDICOMPP_H
#define INDICOMPP_H

#include <memory>
#include "indicom.h"

namespace INDI {   
    

class TTY
{    
protected:
        
    int _tty_fd;
        
public:

    typedef std::shared_ptr<TTY> ShPtr;
    
    typedef enum { 
          OK=0
        , READ_ERROR = -1
        , WRITE_ERROR = -2
        , SELECT_ERROR = -3
        , TIME_OUT=-4
        , PORT_FAILURE = -5
        , PARAM_ERROR = -6
        , ERRNO = -7
    }
    ERROR;

    TTY();
    virtual ~TTY();
       
    virtual int 
    getPortFD();
      
    virtual int 
    connect(const char *device, int bit_rate, int word_size, int parity, int stop_bits);
        
    virtual int 
    disconnect();
        
    virtual int 
    read(char *buf, int nbytes, int timeout, int *nbytes_read);
        
    virtual int 
    read_section(char *buf, char stop_char, int timeout, int *nbytes_read);
        
    virtual int 
    write(const char * buffer, int nbytes, int *nbytes_written);
        
    virtual int 
    write_string(const char * buffer, int *nbytes_written);
        
    virtual void 
    error_msg(int err_code, char *err_msg, int err_msg_len);
        
    virtual void 
    set_debug(int debug);
        
    virtual int 
    timeout(int timeout);
    
    int
    tcflush(int queue_selector);
    
    static void
    s_error_msg(int err_code, char *err_msg, int msg_len) {
        ::tty_error_msg(err_code, err_msg, msg_len);
    }
        
};

} /* end namespace INDI */

#endif
