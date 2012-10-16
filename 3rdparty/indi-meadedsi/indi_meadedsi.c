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

#define MAIN_CONN_GROUP	"Main Connect"

typedef struct node node_t;

typedef struct indidsi_device indidsi_t;

struct node {
    node_t *next, *prev;
    indidsi_t *indidsi;
};

static node_t *first = 0;

/* Property: Connection */
enum { ON_S, OFF_S };
static ISwitch ConnectS[] = {
    {"CONNECT" , "Connect" , ISS_OFF, 0, 0},
    {"DISCONNECT", "Disconnect", ISS_ON, 0, 0}
};

ISwitchVectorProperty ConnectSP = {
    MYDEV, "CONNECTION" , "Connection", MAIN_CONN_GROUP, IP_RW,
    ISR_1OFMANY, 0, IPS_IDLE, ConnectS, NARRAY(ConnectS), "", 0
};

static int device_count = 0;

struct indidsi_device
{
    dsi_camera_t *dsi;
    char *group;
    ITextVectorProperty name;
    ITextVectorProperty desc;

};

void
init_itextvector(ITextVectorProperty *itvp, const char *device,
                 const char *name, const char *label,
                 const char *group, IPerm perm, double timeout,
                 IPState state, IText *itext, int itext_length,
                 const char *timestamp, void *aux);

/* FIXME: Should be abstracted out as list management with simpler
 * add/remove/find opererations.  Do I want to do the whole glib thing as that
 * adds a dependence that may be undesirable for INDI.
 */

/**
 * Create and insert a new node into a double-linked list after the given
 * node.  Helper function for linked-list management.
 *
 * @param node The node to be inserted after.
 *
 * @return Pointer to the newly allocated node.
 *
 */
static node_t *
node_create_and_insert_after(node_t *node)
{
    node_t *new_node = calloc(1, sizeof(node_t));
    new_node->next = 0;
    new_node->prev = node;
    if (node)
        node->next = new_node;
    return new_node;
}

/**
 * Removed linked-list node from list and deallocates its storage.  Helper
 * function on linked-list management.
 *
 * @param node The node to be removed.
 *
 * @return Pointer to next node.
 */
static node_t *
node_destroy_and_relink(node_t *node)
{
    node_t *next;
    if (!node)
        return 0;
    if (node->next)
        node->next->prev = node->prev;
    if (node->prev)
        node->prev->next = node->next;
    next = node->next;
    node->next = node->prev = 0;
    free(node);
    return next;
}

/**
 * Create a new INDI DSI "group" which references a single physical device.
 *
 * @param dsi Pointer to an already connected DSI device which will be
 * presented to the client.
 *
 * @return Pointer to the INDI DSI device for communicating with the client.
 */
IText *
init_itext(IText *itext,
           const char *name, const char *label, const char *text,
           ITextVectorProperty *tvp, void *aux0, void *aux1)
{
    strncpy(itext->name, name, MAXINDINAME);
    strncpy(itext->label, label, MAXINDILABEL);
    itext->text = strdup(text);
    itext->tvp  = tvp;
    itext->aux0 = aux0;
    itext->aux1 = aux1;
    return itext;
}

indidsi_t *
dsi_create(dsi_camera_t *dsi)
{
    indidsi_t *indidsi;
    int bytes_needed;

    indidsi = calloc(1, sizeof(indidsi_t));
    indidsi->dsi = dsi;

    /* Okay, figure out the camera type and make it human readable. */
    bytes_needed = snprintf(0, 0, "%d: %s", device_count, dsi_get_camera_name(indidsi->dsi));
    indidsi->group = malloc(bytes_needed+1);
    snprintf(indidsi->group, bytes_needed, "%d: %s",
             device_count, dsi_get_camera_name(indidsi->dsi));

    IDLog("new physical device, %s\n", indidsi->group);

    indidsi->desc.tp = calloc(3, sizeof(IText));
    /* JM 2010-11-20: Changing to dsi_get_camera_name */
    init_itext(indidsi->desc.tp+0,
               "CAMERA_TYPE", "Camera Type",
               dsi_get_camera_name(indidsi->dsi),
               &(indidsi->desc), 0, 0);
    init_itext(indidsi->desc.tp+1,
               "CHIP_NAME", "Chip Name",
               dsi_get_chip_name(indidsi->dsi),
               &(indidsi->desc), 0, 0);
    init_itext(indidsi->desc.tp+2,
               "SERIAL_NO", "Serial No.",
               dsi_get_serial_number(indidsi->dsi),
               &(indidsi->desc), 0, 0);

    init_itextvector(&(indidsi->desc), MYDEV, "DESCRIPTION", "Description",
                     indidsi->group, IP_RO, 0, IPS_IDLE, indidsi->desc.tp, 3, 0, 0);

    return indidsi;
}

void
init_itextvector(ITextVectorProperty *itvp, const char *device,
                const char *name, const char *label,
                const char *group, IPerm perm, double timeout,
                IPState state, IText *itext, int itext_length,
                const char *timestamp, void *aux)
{
    if (itvp == 0)
        return;

    if (device == 0)
        itvp->device[0] = 0;
    else
        strncpy(itvp->device, device, MAXINDIDEVICE);

    if (label == 0)
        itvp->name[0] = 0;
    else
        strncpy(itvp->name, name, MAXINDINAME);

    if (label == 0)
        itvp->label[0] = 0;
    else
        strncpy(itvp->label, label, MAXINDILABEL);

    if (group == 0)
        itvp->group[0] = 0;
    else
        strncpy(itvp->group, group, MAXINDIGROUP);

    itvp->p         = IP_RO;
    itvp->timeout   = 0;
    itvp->s         = IPS_IDLE;
    itvp->tp        = itext;
    itvp->ntp       = itext_length;

   if (timestamp == 0)
        itvp->timestamp[0] = 0;
    else
        strncpy(itvp->timestamp, timestamp, MAXINDITSTAMP);

   itvp->aux       = aux;
}

void
indidsi_destroy(indidsi_t *indidsi)
{
   free(indidsi->desc.tp[0].text);
   free(indidsi->desc.tp[1].text);
   free(indidsi->desc.tp);
   dsi_close(indidsi->dsi);
   free(indidsi);
   return;
}

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
    node_t *node;

    /* Now we check the state of CONNECT, the 1st member of
       the ConnectSP property we defined earliar */
    switch (ConnectS[0].s) {
        /* If CONNECT is on, then try to establish a connection to the device */
        case ISS_ON:

            /* The IDLog function is a very useful function that will log time-stamped
               messages to stderr. This function is invaluable to debug your drivers.
               It operates like printf */
            IDLog("Establishing a connection to %s...\n", MYDEV);

            /* Change the state of the ConnectSP (CONNECTION) property to OK */
            ConnectSP.s = IPS_OK;

            dsi_scanbus();

            /* Tell the client to update the states of the ConnectSP
               property, and send a message to inform successful connection */
            IDSetSwitch(&ConnectSP, "Connection to %s is successful.", MYDEV);

            IDLog("first = %p\n", first);

            for (node = first; node != 0; node = node->next) {
                IDLog("defining new description for %s\n", node->indidsi->desc.tp[0].text);
                IDDefText(&(node->indidsi->desc), "hello!");
            }

            break;



            /* If CONNECT is off (which is the same thing as DISCONNECT being on),
               then try to disconnect the device */
        case ISS_OFF:

            IDLog ("Terminating connection to %s...\n", MYDEV);

            /* The device is disconnected, change the state to IDLE */
            ConnectSP.s = IPS_IDLE;

            /* Tell the client to update the states of the ConnectSP property,
               and send a message to inform successful disconnection */
            IDSetSwitch(&ConnectSP, "%s has been disconnected.", MYDEV);

            /* FIXME: Alas, simple descriptions won't work because property
               names are effectively device-global.  So we really need to
               dynamically assign property names based on the device
               sequence.  We're back to arrays instead of lists. */
            node = first;
            while (node) {
                IDDelete(MYDEV, "DESCRIPTION", "bye, bye.");

                indidsi_destroy(node->indidsi);
                node = node_destroy_and_relink(node);
            }
            first = 0;
            break;
    }
}

static int
dsi_scanbus()
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
            indidsi_destroy(node->indidsi);
            node = node_destroy_and_relink(node);
        }
    }

    /* All DSI devices appear to present as the same USB vendor:device values.
     * There does not seem to be any better way to find the device other than
     * to iterate over and find the match.  Fortunately, this is fast. */
    for (bus = usb_get_busses(); bus != 0; bus = bus->next) {
        for (dev = bus->devices; dev != 0; dev = dev->next) {
            if ((  (dev->descriptor.idVendor = 0x156c))
                && (dev->descriptor.idProduct == 0x0101)) {

                IDLog("indi_meadedsi found device %04x:%04x at usb:%s,%s\n",
                      dev->descriptor.idVendor, dev->descriptor.idProduct,
                      bus->dirname, dev->filename);

                /* Found a DSI device, open it and add it to our list. */
                snprintf(device, 100, "usb:%s,%s", bus->dirname, dev->filename);
                IDLog("trying to open device %s\n", device);
                dsi_camera_t *dsi = dsi_open(device);
                IDLog("opened new DSI camera, dsi=%p\n", dsi);
                if (dsi != 0) {
                    node_t *node = first;
                    if (node == 0) {
                        IDLog("assigning to first\n");
                        first = node = node_create_and_insert_after(first);
                    } else {
                        /* Ugly way of "keeping track" of last! */
                        IDLog("walking node list from first to last\n");
                        while (node->next != 0) {
                            node = node->next;
                        }
                    }
                    node->indidsi = dsi_create(dsi);
                    IDLog("added new node %p with %p\n", node, node->indidsi);
                }
            }
        }
    }

    /* At this point, 'first' points to the first node in a list of DSI
       devices that we found and we should have found all of them that are
       plugged in. */
}
