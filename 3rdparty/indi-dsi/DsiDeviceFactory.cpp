/*
 * Copyright © 2008, Roland Roberts
 *
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string>
#include <iostream>
#include <ostream>
#include <iomanip>

#include "DsiDeviceFactory.h"
#include "DsiPro.h"
#include "DsiProII.h"

using namespace std;

DSI::Device *
DSI::DeviceFactory::getInstance(const char *devname)
{

    // Yes, we open the device in order to find out what CCD it is, then we do
    // it all over again creating a specific subtype.
    DSI::Device *tmp = new DSI::Device(devname);

    cerr << "Found Camera " << tmp->getCameraName() << endl
         << "Found CCD " << tmp->getCcdChipName() << endl;

    std::string ccdChipName = tmp->getCcdChipName();
    delete tmp;

    if (ccdChipName == "ICX254AL")
        return new DSI::DsiPro(devname);

    if (ccdChipName == "ICX429ALL")
        return new DSI::DsiProII(devname);

    return 0;
}

