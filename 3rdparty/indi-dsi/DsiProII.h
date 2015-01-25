/*
 * Copyright (c) 2009, Roland Roberts <roland@astrofoto.org>
 *
 */

#ifndef __DsiProII_hh
#define __DsiProII_hh

#include "DsiDevice.h"

namespace DSI {

    class DsiProII : public Device {

      private:
        void initImager(const char *devname = 0);

      public:

        DsiProII(const char *devname);
        ~DsiProII();
    };
};

#endif /* __DsiProII_hh */

