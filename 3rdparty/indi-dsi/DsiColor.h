/*
 * Copyright (c) 2009, Roland Roberts <roland@astrofoto.org>
 * Copyright (c) 2015, Ben Gilsrud <bgilsrud@gmail.com>
 *
 */

#ifndef __DsiColor_hh
#define __DsiColor_hh

#include "DsiDevice.h"

namespace DSI {

    class DsiColor : public Device {

      private:

      protected:

        void initImager(const char *devname = 0);

      public:

        DsiColor(const char *devname = 0);
        ~DsiColor();
    };
};

#endif /* __DsiColor_hh */

