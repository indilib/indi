//
// Created by not7cd on 06/12/18.
//

#include "indi_starbook.h"

#include <memory>

std::unique_ptr<Starbook> starbook(new Starbook());

void ISGetProperties (const char *dev)
{
    starbook->ISGetProperties(dev);
}

void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    starbook->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    starbook->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    starbook->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    starbook->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice (XMLEle *root)
{
    INDI_UNUSED(root);
}


Starbook::Starbook()
{
}

bool Starbook::Connect()
{
    IDMessage(getDeviceName(), "Starbook connected successfully!");
    return true;
}

bool Starbook::Disconnect()
{
    IDMessage(getDeviceName(), "Starbook disconnected successfully!");
    return true;
}

const char * Starbook::getDefaultName()
{
    return "Starbook mount controller";
}