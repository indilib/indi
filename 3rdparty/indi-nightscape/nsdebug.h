#ifndef __NS_DEBUG__H__
#define __NS_DEBUG__H__
#include <indilogger.h>


#define DO_INFO(a, ...) IDLog(a, __VA_ARGS__)
#define DO_DBG(a, ...) IDLog(a, __VA_ARGS__)
#define DO_ERR(a, ...) IDLog( a, __VA_ARGS__)

#endif