/*
 * Copyright (c) 2009, Roland Roberts <roland@astrofoto.org>
 *
 */

#ifndef __DsiPro_hh
#define __DsiPro_hh

#include "DsiDevice.h"

namespace DSI {

    class DsiPro : public Device {

      private:

      protected:

        void initImager(const char *devname = 0);

      public:

        DsiPro(const char *devname = 0);
        ~DsiPro();
    };
};

#endif /* __DsiPro_hh */

