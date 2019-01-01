#ifndef __DEBUGVIEW_H__
#define __DEBUGVIEW_H__

#include "chatty.h"
#include "config.h"

#if (defined(__linux__ )&&!defined (__ANDROID__))
#include <functional>
#endif

#define QHYCCD_MSGL_FATAL 	1  		// will exit/abort
#define QHYCCD_MSGL_ERR 	2    		// continues
#define QHYCCD_MSGL_WARN 3   		// warning
#define QHYCCD_MSGL_INFO 	4
#define QHYCCD_MSGL_DBG0	5
#define QHYCCD_MSGL_DBG1	6
#define QHYCCD_MSGL_DBG2 	7
#define QHYCCD_MSGL_DBG3 	8
#define QHYCCD_MSGL_DBG4 	9
#define QHYCCD_MSGL_DBG5 	10
#define QHYCCD_MSGL_DISABLE 	11

void OutputDebugPrintf(int level,const char * strOutputString,...);
#if defined(__linux__ )&&!defined (__ANDROID__)
void SetDebugLogFunction(std::function<void(const std::string &message)> logFunction);
#endif

#endif
