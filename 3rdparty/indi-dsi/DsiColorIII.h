/*
 * Copyright (c) 2009, Roland Roberts <roland@astrofoto.org>
 * Copyright (c) 2016, Ben Gilsrud <bgilsrud@gmail.com>
 *
 */

#ifndef __DsiColorIII_hh
#define __DsiColorIII_hh

#include "DsiDevice.h"

namespace DSI {

    class DsiColorIII : public Device {

      private:
        void initImager(const char *devname = 0);

      public:

        DsiColorIII(const char *devname);
        ~DsiColorIII();
    };
};

#endif /* __DsiColorIII_hh */

