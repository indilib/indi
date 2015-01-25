/*
 * Copyright © 2009, Roland Roberts
 *
 */

#ifndef __DsiDeviceFactory_hh
#define __DsiDeviceFactory_hh

#include "DsiDevice.h"

namespace DSI {
    class DeviceFactory {
      public:
        static Device *getInstance(const char *devpath = 0);
    };
};

#endif /* __DsiDeviceFactory_hh */

