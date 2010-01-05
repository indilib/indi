/* Driver for any Meade DSI camera.

    Copyright (C) 2010 Roland Roberts (roland@astrofoto.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <fitsio.h>

#include <indidevapi.h>
#include <eventloop.h>
#include <base64.h>
#include <zlib.h>

#include <dsi.h>
#include <usb.h>

/* operational info */
#define MYDEV "Meade DSI"			/* Device name we call ourselves */

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"

typedef struct node node_t;

struct node {
    node_t *next, *prev;
    dsi_camera_t *dsi;
};

static node_t *first = 0;

static int dsi_camera_count = 0;
static int dis_camera_list_size = 0;
static dsi_camera_t **dsi_camera_list = 0;

/**********************************************************************************************/
/************************************ GROUP: Communication ************************************/
/**********************************************************************************************/

/********************************************
 Property: Connection
*********************************************/
enum { ON_S, OFF_S };
static ISwitch ConnectS[] = {
    {"CONNECT" , "Connect" , ISS_OFF, 0, 0},
    {"DISCONNECT", "Disconnect", ISS_ON, 0, 0}
};
ISwitchVectorProperty ConnectSP = {
    MYDEV, "CONNECTION" , "Connection", COMM_GROUP, IP_RW,
    ISR_1OFMANY, 0, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0
};

/* Function prototypes */
void connectDevice(void);
static int dsi_scanbus();

/* send client definitions of all properties */
void
ISGetProperties (char const *dev)
{
    if (dev && strcmp(MYDEV, dev))
        return;

    /* Communication Group */
    IDDefSwitch(&ConnectSP, NULL);
}

void
ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    /* Let's check if the property the client wants to change is the ConnectSP
       (name: CONNECTION) property*/
    if (!strcmp(name, ConnectSP.name)) {
        /* We update the switches by sending their names and updated states
           IUUpdateSwitches function, if something goes wrong, we return */
        if (IUUpdateSwitch(&ConnectSP, states, names, n) < 0)
            return;

        /* We try to establish a connection to our device */
        connectDevice();
        return;
    }
}

void
ISNewNumber(const char * dev, const char *name, double *doubles, char *names[], int n)
{
}

void
ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
}
void
ISNewBLOB(const char *dev, const char *name, int sizes[],
          int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
}

void
ISSnoopDevice(XMLEle *root)
{
    INDI_UNUSED(root);
}

void
connectDevice(void)
{
    /* Now we check the state of CONNECT, the 1st member of
       the ConnectSP property we defined earliar */
    switch (ConnectS[0].s) {
        /* If CONNECT is on, then try to establish a connection to the device */
        case ISS_ON:

            /* The IDLog function is a very useful function that will log time-stamped
               messages to stderr. This function is invaluable to debug your drivers.
               It operates like printf */
            IDLog ("Establishing a connection to %s...\n", MYDEV);

            /* Change the state of the ConnectSP (CONNECTION) property to OK */
            ConnectSP.s = IPS_OK;

            dsi_scanbus();

            /* Tell the client to update the states of the ConnectSP
               property, and send a message to inform successful connection */
            IDSetSwitch(&ConnectSP, "Connection to %s is successful.", MYDEV);
            break;

            /* If CONNECT is off (which is the same thing as DISCONNECT being on),
               then try to disconnect the device */
        case ISS_OFF:

            IDLog ("Terminating connection to %s...\n", MYDEV);

            /* The device is disconnected, change the state to IDLE */
            ConnectSP.s = IPS_IDLE;

            /* Tell the client to update the states of the ConnectSP property,
               and send a message to inform successful disconnection */
            IDSetSwitch(&ConnectSP, "%s has been disconneced.", MYDEV);

            break;
    }
}

static int dsi_scanbus()
{
    struct usb_bus *bus;
    struct usb_device *dev;
    struct usb_dev_handle *handle = 0;

    char device[100];

    usb_init();
    usb_find_busses();
    usb_find_devices();

    /* Rescanning the bus for DSI devices automatically resets everything so
       we close all devices and clear our device list. */
    if (first != 0) {
        node_t *node = first;
        while (node != 0) {
            dsi_close(node->dsi);
            node->dsi = 0;
            node = node->next;
            node->prev->next = 0;
            node->prev = 0;
            first = node;
        }
    }

    /* All DSI devices appear to present as the same USB vendor:device values.
     * There does not seem to be any better way to find the device other than
     * to iterate over and find the match.  Fortunately, this is fast. */
    for (bus = usb_get_busses(); bus != 0; bus = bus->next) {
        for (dev = bus->devices; dev != 0; dev = dev->next) {
            if ((  (dev->descriptor.idVendor = 0x156c))
                && (dev->descriptor.idProduct == 0x0101)) {

                IDLog("Found device %04x:%04x at usb:%s,%s\n",
                      dev->descriptor.idVendor, dev->descriptor.idProduct,
                      bus->dirname, dev->filename);

                /* Found a DSI device, open it and add it to our list. */
                snprintf(device, 100, "usb:%s,%s", bus->dirname, dev->filename);
                dsi_camera_t *dsi = dsi_open(device);
                if (dsi != 0) {
                    if (!first) {
                        first = calloc(1, sizeof(node_t));
                        first->dsi = dsi;
                    } else {
                        node_t *node = first;
                        while (node->next != 0) {
                            node = node->next;
                        }
                        node->next = calloc(1, sizeof(node_t));
                        node->next->prev = node;
                        node->next->dsi = dsi;
                    }
                }
            }
        }
    }
    /* At this point, 'first' points to the first node in a list of DSI
       devices that we found and we should have found all of them that are
       plugged in. */
}
