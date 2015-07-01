/*
 * Copyright (c) 2009, Roland Roberts <roland@astrofoto.org>
 *
 */

#ifndef __DsiProIII_hh
#define __DsiProIII_hh

#include "DsiDevice.h"

namespace DSI {

    class DsiProIII : public Device {

      private:
        void initImager(const char *devname = 0);

      public:

        DsiProIII(const char *devname);
        ~DsiProIII();
    };
};

#endif /* __DsiProIII_hh */

