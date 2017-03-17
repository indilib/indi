#ifndef MGENAUTOGUIDER_H
#define MGENAUTOGUIDER_H

/*
   INDI Developers Manual
   Tutorial #1

   "Hello INDI"

   We construct a most basic (and useless) device driver to illustate INDI.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file mgenautoguider.h
    \brief Construct a basic INDI device for the Lacerta MGen Autoguider.
    \author TallFurryMan

    \example mgenautoguider.h
    A very minimal autoguider! It connects/disconnects to the autoguider, and manages operational modes.
*/

#include "indidevapi.h"
#include "indiccd.h"

class MGenAutoguider : public INDI::CCD
{
public:
    MGenAutoguider();
    virtual ~MGenAutoguider();

public:
    static MGenAutoguider & instance();

protected:
    class MGenDevice * device;

public:
    struct version
    {
        int timer;
        struct timespec timestamp;
        unsigned short uploaded_firmware;
        unsigned short camera_firmware;
        IText textFirmware;
        ITextVectorProperty propVersions;
    } version;

public:
    struct voltage
    {
        int timer;
        struct timespec timestamp;
        float logic;
        float input;
        float reference;
        INumber numVoltages[3];
        INumberVectorProperty propVoltages;
    } voltage;

public:
    struct ui
    {
        int timer;
        struct timespec timestamp;
        struct framerate
        {
            INumber number;
            INumberVectorProperty property;
        } framerate;
        struct buttons
        {
            ISwitch switches[6];
            ISwitchVectorProperty properties[2];
        } buttons;
    } ui;

public:
    struct heartbeat
    {
        int timer;
        struct timespec timestamp;
        unsigned int no_ack_count;
    } heartbeat;

protected:
    bool initProperties();
    bool updateProperties();
    void TimerHit();

public:
    virtual bool ISNewNumber(char const * dev, char const * name, double values[], char * names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

protected:
    bool Connect();
    bool Disconnect();

protected:
    const char *getDefaultName();

public:
    /* @brief Returning the current operational mode of the device.
     * @return the current operational mode of the device.
     */
    IOMode getOpMode() const;

protected:
    bool getHeartbeat();
};

#endif // MGENAUTOGUIDER_H
