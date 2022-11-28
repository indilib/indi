#pragma once

#include "indiapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _IGeneric
{
    /** Helper info */
    void *aux;
    /** Pointer to parent */
    void *parent;
    /** Index name */
    char name[MAXINDINAME];
    /** Short description */
    char label[MAXINDILABEL];
    /** Allocated text string */

    union
    {
        struct
        {
            /** Allocated text string */
            char *text;
        } itext;

        struct
        {
            /** GUI display format, see above */
            char format[MAXINDIFORMAT];
            /** Range min, ignored if min == max */
            double min;
            /** Range max, ignored if min == max */
            double max;
            /** Step size, ignored if step == 0 */
            double step;
            /** Current value */
            double value;
        } inumber;

        struct
        {
            /** Switch state */
            ISState s;
        } iswitch;

        struct
        {
            /** Light state */
            IPState s;
        } ilight;

        struct
        {
            /** Format attr */
            char format[MAXINDIBLOBFMT];
            /** Allocated binary large object bytes - maybe a shared buffer: use IDSharedBlobFree to free*/
            void *blob;
            /** Blob size in bytes */
            int bloblen;
            /** N uncompressed bytes */
            int size;
        } iblob;
    };
} IGeneric;

typedef struct _IGenericVectorProperty
{
    /** Helper info */
    void *aux;
    /** Device name */
    char device[MAXINDIDEVICE];
    /** Property name */
    char name[MAXINDINAME];
    /** Short description */
    char label[MAXINDILABEL];
    /** GUI grouping hint */
    char group[MAXINDIGROUP];
    /** Current property state */
    IPState s;
    /** Widgets comprising this vector */
    void *widgets;
    /** Dimension of widgets[] */
    int count;
    /** ISO 8601 timestamp of this event */
    char timestamp[MAXINDITSTAMP];
    /** Current max time to change, secs */
    double timeout;
    /** Client accessibility hint */
    IPerm p;

    union
    {
        struct
        {
            /** Switch behavior hint */
            ISRule r;
        } iswitch;
    };
} IGenericVectorProperty;

#ifdef __cplusplus
}
#endif
