#if 0
INDI
Copyright (C) 2003 Elwood C. Downey
2022 Ludovic Pollet

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

#pragma once

#include <stdio.h>
#include "lilxml.h"

/** \mainpage Instrument Neutral Distributed Interface INDI
 *
\section Introduction

<p>INDI is a simple XML-like communications protocol described for interactive and automated remote control of diverse instrumentation. INDI is small, easy to parse, and stateless.</p>
<p>A full description of the INDI protocol is detailed in the INDI <a href="http://www.clearskyinstitute.com/INDI/INDI.pdf">white paper</a></p>

<p>Under INDI, any number of clients can connect to any number of drivers running one or more devices. It is a true N-to-N server/client architecture topology
allowing for reliable deployments of several INDI server, client, and driver instances distrubted across different systems in different physical and logical locations.</p>

<p>The basic premise of INDI is this: Drivers are responsible for defining their functionality in terms of <b>Properties</b>. Clients are not aware of such properties until they establish connection with the driver
and start receiving streams of defined properties. Each property encompases some functionality or information about the device. These include number, text, switch, light, and BLOB properties.</p>

<p>For example, all devices define the <i>CONNECTION</i> vector switch property, which is compromised of two switches:</p>
<ol>
<li><strong>CONNECT</strong>: Connect to the device.</li>
<li><strong>DISCONNECT</strong>: Disconnect to the device.</li>
</ol>

<p>Therefore, a client, whether it is a GUI client that represents such property as buttons, or a Python script that parses the properties, can change the state
of the switch to cause the desired action.</p>
<p>Not all properties are equal. A few properties are reserved to ensure interoperality among different clients that want to target a specific functionality.
These <i>Standard Properties</i> ensure that different clients agree of a common set of properties with specific meaning since INDI does not impose any specific meaning on the properties themselves.</p>

<p>INDI server acts as a convenient hub to route communications between clients and drivers. While it is not strictly required for controlling driver, it offers many queue and routing capabilities.</p>

\section Audience Intended Audience

INDI is intended for developers seeking to add support for their devices in INDI. Any INDI driver can be operated from numerous cross-platform cross-architecture <a href="http://indilib.org/about/clients.html">clients</a>.

\section Development Developing under INDI

<p>Please refere to the <a href="http://www.indilib.org/develop/developer-manual">INDI Developers Manual</a> for a complete guide on INDI's driver developemnt framework.</p>

<p>The INDI Library API is divided into the following main sections:</p>
<ul>
<li><a href="indidevapi_8h.html">INDI Device API</a></li>
<li><a href="classINDI_1_1BaseClient.html">INDI Client API</a></li>
<li><a href="namespaceINDI.html">INDI Base Drivers</a>: Base classes for all INDI drivers. Current base drivers include:
 <ul>
<li><a href="classINDI_1_1Telescope.html">Telescope</a></li>
<li><a href="classINDI_1_1CCD.html">CCD</a></li>
<li><a href="classINDI_1_1GuiderInterface.html">Guider</a></li>
<li><a href="classINDI_1_1FilterWheel.html">Filter Wheel</a></li>
<li><a href="classINDI_1_1Focuser.html">Focuser</a></li>
<li><a href="classINDI_1_1Rotator.html">Rotator</a></li>
<li><a href="classINDI_1_1Detector.html">Detector</a></li>
<li><a href="classINDI_1_1Dome.html">Dome</a></li>
<li><a href="classLightBoxInterface.html">Light Panel</a></li>
<li><a href="classINDI_1_1Weather.html">Weather</a></li>
<li><a href="classINDI_1_1GPS.html">GPS</a></li>
<li><a href="classINDI_1_1USBDevice.html">USB</a></li>
</ul>
<li>@ref Connection "INDI Connection Interface"</li>
<li>@ref INDI::SP "INDI Standard Properties"</li>
<li><a href="md_libs_indibase_alignment_alignment_white_paper.html">INDI Alignment Subsystem</a></li>
<li><a href="structINDI_1_1Logger.html">INDI Debugging & Logging API</a></li>
<li>@ref DSP "Digital Signal Processing API"</li>
<li><a href="indicom_8h.html">INDI Common Routine Library</a></li>
<li><a href="lilxml_8h.html">INDI LilXML Library</a></li>
<li><a href="classStreamManager.html">INDI Stream Manager for video encoding, streaming, and recording.</a></li>
<li><a href="group__configFunctions.html">Configuration</a></li>
</ul>

\section Tutorials

INDI Library includes a number of tutorials to illustrate development of INDI drivers. Check out the <a href="examples.html">examples</a> provided with INDI library.

\section Simulators

Simulators provide a great framework to test drivers and equipment alike. INDI Library provides the following simulators:
<ul>
<li><b>@ref ScopeSim "Telescope Simulator"</b>: Offers GOTO capability, motion control, guiding, and ability to set Periodic Error (PE) which is read by the CCD simulator when generating images.</li>
<li><b>@ref CCDSim "CCD Simulator"</b>: Offers a very flexible CCD simulator with a primary CCD chip and a guide chip. The simulator generate images based on the RA & DEC coordinates it
 snoops from the telescope driver using General Star Catalog (GSC). Please note that you must install GSC for the CCD simulator to work properly. Furthermore,
 The simulator snoops FWHM from the focuser simulator which affects the generated images focus. All images are generated in standard FITS format.</li>
<li><b>@ref GuideSim "Guide Simulator"</b>: Simple dedicated Guide Simulator.
<li><b>@ref FilterSim "Filter Wheel Simulator"</b>: Offers a simple simulator to change filter wheels and their corresponding designations.</li>
<li><b>@ref FocusSim "Focuser Simulator"</b>: Offers a simple simualtor for an absolute position focuser. It generates a simulated FWHM value that may be used by other simulator such as the CCD simulator.</li>
<li><b>@ref DomeSim "Dome Simulator"</b>: Offers a simple simulator for an absolute position dome with shutter.
<li><b>@ref GPSSimulator "GPS Simulator"</b>: Offers a simple simulator for GPS devices that send time and location data to the client and other drivers.
</ul>

\section Help

You can find information on INDI development in the <a href="http://www.indilib.org">INDI Library</a> site. Furthermore, you can discuss INDI related issues on the <a href="http://sourceforge.net/mail/?group_id=90275">INDI development mailing list</a>.

\author Jasem Mutlaq
\author Elwood Downey

For a full list of contributors, please check <a href="https://github.com/indilib/indi/graphs/contributors">Contributors page</a> on Github.

*/

/** \file indiapi.h
    \brief Constants and Data structure definitions for the interface to the reference INDI C API implementation.
    \author Elwood C. Downey
*/

/*******************************************************************************
 * INDI wire protocol version implemented by this API.
 * N.B. this is indepedent of the API itself.
 */

#define INDIV 1.7

/* INDI Library version */
#define INDI_VERSION_MAJOR   1
#define INDI_VERSION_MINOR   9
#define INDI_VERSION_RELEASE 9

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Manifest constants
 */

/** \typedef BLOBHandling
    \brief How drivers handle BLOBs incoming from snooping drivers */
typedef enum
{
    B_NEVER = 0, /*!< Never receive BLOBs */
    B_ALSO,      /*!< Receive BLOBs along with normal messages */
    B_ONLY       /*!< ONLY receive BLOBs from drivers, ignore all other traffic */
} BLOBHandling;

/**
 * @typedef ISState
 * @brief Switch state.
 */
typedef enum
{
ISS_OFF = 0, /*!< Switch is OFF */
ISS_ON       /*!< Switch is ON */
} ISState;

/**
 * @typedef IPState
 * @brief Property state.
 */
typedef enum
{
IPS_IDLE = 0, /*!< State is idle */
IPS_OK,       /*!< State is ok */
IPS_BUSY,     /*!< State is busy */
IPS_ALERT     /*!< State is alert */
} IPState;

/**
 * @typedef ISRule
 * @brief Switch vector rule hint.
 */
typedef enum
{
ISR_1OFMANY, /*!< Only 1 switch of many can be ON (e.g. radio buttons) */
ISR_ATMOST1, /*!< At most one switch can be ON, but all switches can be off. It is similar to ISR_1OFMANY with the exception that all switches can be off. */
ISR_NOFMANY  /*!< Any number of switches can be ON (e.g. check boxes) */
} ISRule;

/**
 * @typedef IPerm
 * @brief Permission hint, with respect to client.
 */
typedef enum
{
IP_RO, /*!< Read Only */
IP_WO, /*!< Write Only */
IP_RW  /*!< Read & Write */
} IPerm;

// The XML strings for these attributes may be any length but implementations
// are only obligued to support these lengths for the various string attributes.
#define MAXINDINAME    64
#define MAXINDILABEL   64
#define MAXINDIDEVICE  64
#define MAXINDIGROUP   64
#define MAXINDIFORMAT  64
#define MAXINDIBLOBFMT 64
#define MAXINDITSTAMP  64
#define MAXINDIMESSAGE 255

/*******************************************************************************
 * Typedefs for each INDI Property type.
 *
 * INumber.format may be any printf-style appropriate for double
 * or style "m" to create sexigesimal using the form "%<w>.<f>m" where
 *   <w> is the total field width.
 *   <f> is the width of the fraction. valid values are:
 *      9  ->  :mm:ss.ss
 *      8  ->  :mm:ss.s
 *      6  ->  :mm:ss
 *      5  ->  :mm.m
 *      3  ->  :mm
 *
 * examples:
 *
 *   to produce     use
 *
 *    "-123:45"    %7.3m
 *  "  0:01:02"    %9.6m
 */

/**
 * @struct IText
 * @brief One text descriptor.
 */
typedef struct _IText
{
/** Index name */
char name[MAXINDINAME];
    /** Short description */
    char label[MAXINDILABEL];
    /** Allocated text string */
    char *text;
    /** Pointer to parent */
    struct _ITextVectorProperty *tvp;
    /** Helper info */
    void *aux0;
    /** Helper info */
    void *aux1;
} IText;

/**
 * @struct _ITextVectorProperty
 * @brief Text vector property descriptor.
 */
typedef struct _ITextVectorProperty
{
    /** Device name */
    char device[MAXINDIDEVICE];
    /** Property name */
    char name[MAXINDINAME];
    /** Short description */
    char label[MAXINDILABEL];
    /** GUI grouping hint */
    char group[MAXINDIGROUP];
    /** Client accessibility hint */
    IPerm p;
    /** Current max time to change, secs */
    double timeout;
    /** Current property state */
    IPState s;
    /** Texts comprising this vector */
    IText *tp;
    /** Dimension of tp[] */
    int ntp;
    /** ISO 8601 timestamp of this event */
    char timestamp[MAXINDITSTAMP];
    /** Helper info */
    void *aux;
} ITextVectorProperty;

/**
 * @struct INumber
 * @brief One number descriptor.
 */
typedef struct _INumber
{
    /** Index name */
    char name[MAXINDINAME];
    /** Short description */
    char label[MAXINDILABEL];
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
    /** Pointer to parent */
    struct _INumberVectorProperty *nvp;
    /** Helper info */
    void *aux0;
    /** Helper info */
    void *aux1;
} INumber;

/**
 * @struct _INumberVectorProperty
 * @brief Number vector property descriptor.
 *
 * INumber.format may be any printf-style appropriate for double or style
 * "m" to create sexigesimal using the form "%\<w\>.\<f\>m" where:\n
 * \<w\> is the total field width.\n
 * \<f\> is the width of the fraction. valid values are:\n
 *       9  ->  \<w\>:mm:ss.ss \n
 *       8  ->  \<w\>:mm:ss.s \n
 *       6  ->  \<w\>:mm:ss \n
 *       5  ->  \<w\>:mm.m \n
 *       3  ->  \<w\>:mm \n
 *
 * examples:\n
 *
 * To produce "-123:45", use \%7.3m \n
 * To produce "  0:01:02", use \%9.6m
 */
typedef struct _INumberVectorProperty
{
    /** Device name */
    char device[MAXINDIDEVICE];
    /** Property name */
    char name[MAXINDINAME];
    /** Short description */
    char label[MAXINDILABEL];
    /** GUI grouping hint */
    char group[MAXINDIGROUP];
    /** Client accessibility hint */
    IPerm p;
    /** Current max time to change, secs */
    double timeout;
    /** current property state */
    IPState s;
    /** Numbers comprising this vector */
    INumber *np;
    /** Dimension of np[] */
    int nnp;
    /** ISO 8601 timestamp of this event */
    char timestamp[MAXINDITSTAMP];
    /** Helper info */
    void *aux;
} INumberVectorProperty;

/**
 * @struct ISwitch
 * @brief One switch descriptor.
 */
typedef struct _ISwitch
{
    /** Index name */
    char name[MAXINDINAME];
    /** Switch label */
    char label[MAXINDILABEL];
    /** Switch state */
    ISState s;
    /** Pointer to parent */
    struct _ISwitchVectorProperty *svp;
    /** Helper info */
    void *aux;
} ISwitch;

/**
 * @struct _ISwitchVectorProperty
 * @brief Switch vector property descriptor.
 */
typedef struct _ISwitchVectorProperty
{
    /** Device name */
    char device[MAXINDIDEVICE];
    /** Property name */
    char name[MAXINDINAME];
    /** Short description */
    char label[MAXINDILABEL];
    /** GUI grouping hint */
    char group[MAXINDIGROUP];
    /** Client accessibility hint */
    IPerm p;
    /** Switch behavior hint */
    ISRule r;
    /** Current max time to change, secs */
    double timeout;
    /** Current property state */
    IPState s;
    /** Switches comprising this vector */
    ISwitch *sp;
    /** Dimension of sp[] */
    int nsp;
    /** ISO 8601 timestamp of this event */
    char timestamp[MAXINDITSTAMP];
    /** Helper info */
    void *aux;
} ISwitchVectorProperty;

/**
 * @struct ILight
 * @brief One light descriptor.
 */
typedef struct _ILight
{
    /** Index name */
    char name[MAXINDINAME];
    /** Light labels */
    char label[MAXINDILABEL];
    /** Light state */
    IPState s;
    /** Pointer to parent */
    struct _ILightVectorProperty *lvp;
    /** Helper info */
    void *aux;
} ILight;

/**
 * @struct _ILightVectorProperty
 * @brief Light vector property descriptor.
 */
typedef struct _ILightVectorProperty
{
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
    /** Lights comprising this vector */
    ILight *lp;
    /** Dimension of lp[] */
    int nlp;
    /** ISO 8601 timestamp of this event */
    char timestamp[MAXINDITSTAMP];
    /** Helper info */
    void *aux;
} ILightVectorProperty;

/**
 * @struct IBLOB
 * @brief One Blob (Binary Large Object) descriptor.
 */
typedef struct _IBLOB/* one BLOB descriptor */
{
    /** Index name */
    char name[MAXINDINAME];
    /** Blob label */
    char label[MAXINDILABEL];
    /** Format attr */
    char format[MAXINDIBLOBFMT];
    /** Allocated binary large object bytes - maybe a shared buffer: use IDSharedBlobFree to free*/
    void *blob;
    /** Blob size in bytes */
    int bloblen;
    /** N uncompressed bytes */
    int size;
    /** Pointer to parent */
    struct _IBLOBVectorProperty *bvp;
    /** Helper info */
    void *aux0;
    /** Helper info */
    void *aux1;
    /** Helper info */
    void *aux2;
} IBLOB;

/**
 * @struct _IBLOBVectorProperty
 * @brief BLOB (Binary Large Object) vector property descriptor.
 */
typedef struct _IBLOBVectorProperty /* BLOB vector property descriptor */
{
    /** Device name */
    char device[MAXINDIDEVICE];
    /** Property name */
    char name[MAXINDINAME];
    /** Short description */
    char label[MAXINDILABEL];
    /** GUI grouping hint */
    char group[MAXINDIGROUP];
    /** Client accessibility hint */
    IPerm p;
    /** Current max time to change, secs */
    double timeout;
    /** Current property state */
    IPState s;
    /** BLOBs comprising this vector */
    IBLOB *bp;
    /** Dimension of bp[] */
    int nbp;
    /** ISO 8601 timestamp of this event */
    char timestamp[MAXINDITSTAMP];
    /** Helper info */
    void *aux;
} IBLOBVectorProperty;

/**
 * @brief Handy macro to find the number of elements in array a[]. Must be used
 * with actual array, not pointer.
 */
#define NARRAY(a) (sizeof(a) / sizeof(a[0]))

/**
 * @brief Bails out if memory pointer is 0. Prints file and function.
 */
#define assert_mem(p) if((p) == 0) { fprintf(stderr, "%s(%s): Failed to allocate memory\n", __FILE__, __func__); exit(1); }

/**
 * \defgroup convertFunctions Formatting Functions: Functions to perform handy formatting and conversion routines.
 */
/*@{*/

/** \brief Converts a sexagesimal number to a string.

   sprint the variable a in sexagesimal format into out[].

  \param out a pointer to store the sexagesimal number.
  \param a the sexagesimal number to convert.
  \param w the number of spaces in the whole part.
  \param fracbase is the number of pieces a whole is to broken into; valid options:\n
          \li 360000:	\<w\>:mm:ss.ss
      \li 36000:	\<w\>:mm:ss.s
      \li 3600:	\<w\>:mm:ss
      \li 600:	\<w\>:mm.m
      \li 60:	\<w\>:mm

  \return number of characters written to out, not counting final null terminator.
 */
int fs_sexa(char *out, double a, int w, int fracbase);

/** \brief convert sexagesimal string str AxBxC to double.

    x can be anything non-numeric. Any missing A, B or C will be assumed 0. Optional - and + can be anywhere.

    \param str0 string containing sexagesimal number.
    \param dp pointer to a double to store the sexagesimal number.
    \return return 0 if ok, -1 if can't find a thing.
 */
int f_scansexa(const char *str0, double *dp);

void getSexComponents(double value, int *d, int *m, int *s);
void getSexComponentsIID(double value, int *d, int *m, double *s);

/** \brief Fill buffer with properly formatted INumber string.
    \param buf to store the formatted string.
    \param format format in sprintf style.
    \param value the number to format.
    \return length of string.

    \note buf must be of length MAXINDIFORMAT at minimum
*/
int numberFormat(char *buf, const char *format, double value);

/*@}*/

/** \brief Create an ISO 8601 formatted time stamp. The format is YYYY-MM-DDTHH:MM:SS
    \return The formatted time stamp.
*/
const char *timestamp();

/**
 * \defgroup dutilFunctions IU Functions: Functions drivers call to perform handy utility routines.

<p>This section describes handy utility functions that are provided by the
framework for tasks commonly required in the processing of client
messages. It is not strictly necessary to use these functions, but it
both prudent and efficient to do so.</P>

<p>These do not communicate with the Client in any way.</p>
 */
/*@{*/

/** \brief Add a number vector property value to the configuration file
    \param fp file pointer to a configuration file.
    \param nvp pointer to a number vector property.
*/
extern void IUSaveConfigNumber(FILE *fp, const INumberVectorProperty *nvp);

/** \brief Add a text vector property value to the configuration file
    \param fp file pointer to a configuration file.
    \param tvp pointer to a text vector property.
*/
extern void IUSaveConfigText(FILE *fp, const ITextVectorProperty *tvp);

/** \brief Add a switch vector property value to the configuration file
    \param fp file pointer to a configuration file.
    \param svp pointer to a switch vector property.
*/
extern void IUSaveConfigSwitch(FILE *fp, const ISwitchVectorProperty *svp);

/** \brief Add a BLOB vector property value to the configuration file
    \param fp file pointer to a configuration file.
    \param bvp pointer to a BLOB vector property.
    \note If the BLOB size is large, this function will block until the BLOB contents are written to the file.
*/
extern void IUSaveConfigBLOB(FILE *fp, const IBLOBVectorProperty *bvp);

/** \brief Find an IText member in a vector text property.
*
* \param tvp a pointer to a text vector property.
* \param name the name of the member to search for.
* \return a pointer to an IText member on match, or NULL if nothing is found.
*/
extern IText *IUFindText(const ITextVectorProperty *tvp, const char *name);

/** \brief Find an INumber member in a number text property.
*
* \param nvp a pointer to a number vector property.
* \param name the name of the member to search for.
* \return a pointer to an INumber member on match, or NULL if nothing is found.
*/
extern INumber *IUFindNumber(const INumberVectorProperty *nvp, const char *name);

/** \brief Find an ISwitch member in a vector switch property.
*
* \param svp a pointer to a switch vector property.
* \param name the name of the member to search for.
* \return a pointer to an ISwitch member on match, or NULL if nothing is found.
*/
extern ISwitch *IUFindSwitch(const ISwitchVectorProperty *svp, const char *name);

/** \brief Find an ILight member in a vector Light property.
*
* \param lvp a pointer to a Light vector property.
* \param name the name of the member to search for.
* \return a pointer to an ILight member on match, or NULL if nothing is found.
*/
extern ILight *IUFindLight(const ILightVectorProperty *lvp, const char *name);

/** \brief Find an IBLOB member in a vector BLOB property.
*
* \param bvp a pointer to a BLOB vector property.
* \param name the name of the member to search for.
* \return a pointer to an IBLOB member on match, or NULL if nothing is found.
*/
extern IBLOB *IUFindBLOB(const IBLOBVectorProperty *bvp, const char *name);

/** \brief Returns the first ON switch it finds in the vector switch property.

*   \note This is only valid for ISR_1OFMANY mode. That is, when only one switch out of many is allowed to be ON. Do not use this function if you can have multiple ON switches in the same vector property.
*
* \param sp a pointer to a switch vector property.
* \return a pointer to the \e first ON ISwitch member if found. If all switches are off, NULL is returned.
*/
extern ISwitch *IUFindOnSwitch(const ISwitchVectorProperty *sp);

/** \brief Returns the index of the string in a string array
*
* \param needle the string to match against each element in the hay
* \param hay a pointer to a string array to search in
* \param n the size of hay
* \return index of needle if found in the hay. Otherwise -1 if not found.
*/
extern int IUFindIndex(const char *needle, char **hay, unsigned int n);

/** \brief Returns the index of first ON switch it finds in the vector switch property.

*   \note This is only valid for ISR_1OFMANY mode. That is, when only one switch out of many is allowed to be ON. Do not use this function if you can have multiple ON switches in the same vector property.
*
* \param sp a pointer to a switch vector property.
* \return index to the \e first ON ISwitch member if found. If all switches are off, -1 is returned.
*/

extern int IUFindOnSwitchIndex(const ISwitchVectorProperty *sp);

/** \brief Returns the name of the first ON switch it finds in the supplied arguments.

*   \note This is only valid for ISR_1OFMANY mode. That is, when only one switch out of many is allowed to be ON. Do not use this function if you can have multiple ON switches in the same vector property.
*   \note This is a convience function intended to be used in ISNewSwitch(...) function to find out ON switch name without having to change actual switch state via IUUpdateSwitch(..)
*
* \param states list of switch states passed by ISNewSwitch()
* \param names list of switch names passed by ISNewSwitch()
* \param n number of switches passed by ISNewSwitch()
* \return name of the \e first ON ISwitch member if found. If all switches are off, NULL is returned.
*/
extern const char *IUFindOnSwitchName(ISState *states, char *names[], int n);

/** \brief Reset all switches in a switch vector property to OFF.
*
* \param svp a pointer to a switch vector property.
*/
extern void IUResetSwitch(ISwitchVectorProperty *svp);

/** \brief Function to save blob metadata in the corresponding blob.
    \param bp pointer to an IBLOB member.
    \param size size of the blob buffer encoded in base64
    \param blobsize actual size of the buffer after base64 decoding. This is the actual byte count used in drivers.
    \param blob pointer to the blob buffer
    \param format format of the blob buffer
    \note Do not call this function directly, it is called internally by IUUpdateBLOB.
    */
extern int IUSaveBLOB(IBLOB *bp, int size, int blobsize, char *blob, char *format);

/** \brief Function to reliably save new text in a IText.
    \param tp pointer to an IText member.
    \param newtext the new text to be saved
*/
extern void IUSaveText(IText *tp, const char *newtext);

/** \brief Assign attributes for a switch property. The switch's auxiliary elements will be set to NULL.
    \param sp pointer a switch property to fill
    \param name the switch name
    \param label the switch label
    \param s the switch state (ISS_ON or ISS_OFF)
*/
extern void IUFillSwitch(ISwitch *sp, const char *name, const char *label, ISState s);

/** \brief Assign attributes for a light property. The light's auxiliary elements will be set to NULL.
    \param lp pointer a light property to fill
    \param name the light name
    \param label the light label
    \param s the light state (IDLE, WARNING, OK, ALERT)
*/
extern void IUFillLight(ILight *lp, const char *name, const char *label, IPState s);

/** \brief Assign attributes for a number property. The number's auxiliary elements will be set to NULL.
    \param np pointer a number property to fill
    \param name the number name
    \param label the number label
    \param format the number format in printf style (e.g. "%02d")
    \param min  the minimum possible value
    \param max the maximum possible value
    \param step the step used to climb from minimum value to maximum value
    \param value the number's current value
*/
extern void IUFillNumber(INumber *np, const char *name, const char *label, const char *format, double min, double max,
                         double step, double value);

/** \brief Assign attributes for a text property. The text's auxiliary elements will be set to NULL.
    \param tp pointer a text property to fill
    \param name the text name
    \param label the text label
    \param initialText the initial text
*/
extern void IUFillText(IText *tp, const char *name, const char *label, const char *initialText);

/** \brief Assign attributes for a BLOB property. The BLOB's data and auxiliary elements will be set to NULL.
    \param bp pointer a BLOB property to fill
    \param name the BLOB name
    \param label the BLOB label
    \param format the BLOB format.
*/
extern void IUFillBLOB(IBLOB *bp, const char *name, const char *label, const char *format);

/** \brief Assign attributes for a switch vector property. The vector's auxiliary elements will be set to NULL.
    \param svp pointer a switch vector property to fill
    \param sp pointer to an array of switches
    \param nsp the dimension of sp
    \param dev the device name this vector property belongs to
    \param name the vector property name
    \param label the vector property label
    \param group the vector property group
    \param p the vector property permission
    \param r the switches behavior
    \param timeout vector property timeout in seconds
    \param s the vector property initial state.
*/
extern void IUFillSwitchVector(ISwitchVectorProperty *svp, ISwitch *sp, int nsp, const char *dev, const char *name,
                               const char *label, const char *group, IPerm p, ISRule r, double timeout, IPState s);

/** \brief Assign attributes for a light vector property. The vector's auxiliary elements will be set to NULL.
    \param lvp pointer a light vector property to fill
    \param lp pointer to an array of lights
    \param nlp the dimension of lp
    \param dev the device name this vector property belongs to
    \param name the vector property name
    \param label the vector property label
    \param group the vector property group
    \param s the vector property initial state.
*/
extern void IUFillLightVector(ILightVectorProperty *lvp, ILight *lp, int nlp, const char *dev, const char *name,
                              const char *label, const char *group, IPState s);

/** \brief Assign attributes for a number vector property. The vector's auxiliary elements will be set to NULL.
    \param nvp pointer a number vector property to fill
    \param np pointer to an array of numbers
    \param nnp the dimension of np
    \param dev the device name this vector property belongs to
    \param name the vector property name
    \param label the vector property label
    \param group the vector property group
    \param p the vector property permission
    \param timeout vector property timeout in seconds
    \param s the vector property initial state.
*/
extern void IUFillNumberVector(INumberVectorProperty *nvp, INumber *np, int nnp, const char *dev, const char *name,
                               const char *label, const char *group, IPerm p, double timeout, IPState s);

/** \brief Assign attributes for a text vector property. The vector's auxiliary elements will be set to NULL.
    \param tvp pointer a text vector property to fill
    \param tp pointer to an array of texts
    \param ntp the dimension of tp
    \param dev the device name this vector property belongs to
    \param name the vector property name
    \param label the vector property label
    \param group the vector property group
    \param p the vector property permission
    \param timeout vector property timeout in seconds
    \param s the vector property initial state.
*/
extern void IUFillTextVector(ITextVectorProperty *tvp, IText *tp, int ntp, const char *dev, const char *name,
                             const char *label, const char *group, IPerm p, double timeout, IPState s);

/** \brief Assign attributes for a BLOB vector property. The vector's auxiliary elements will be set to NULL.
    \param bvp pointer a BLOB vector property to fill
    \param bp pointer to an array of BLOBs
    \param nbp the dimension of bp
    \param dev the device name this vector property belongs to
    \param name the vector property name
    \param label the vector property label
    \param group the vector property group
    \param p the vector property permission
    \param timeout vector property timeout in seconds
    \param s the vector property initial state.
*/
extern void IUFillBLOBVector(IBLOBVectorProperty *bvp, IBLOB *bp, int nbp, const char *dev, const char *name,
                             const char *label, const char *group, IPerm p, double timeout, IPState s);

/** \brief Update a snooped number vector property from the given XML root element.
    \param root XML root elememnt containing the snopped property content
    \param nvp a pointer to the number vector property to be updated.
    \return 0 if cracking the XML element and updating the property proceeded without errors, -1 if trouble.
*/
extern int IUSnoopNumber(XMLEle *root, INumberVectorProperty *nvp);

/** \brief Update a snooped text vector property from the given XML root element.
    \param root XML root elememnt containing the snopped property content
    \param tvp a pointer to the text vector property to be updated.
    \return 0 if cracking the XML element and updating the property proceeded without errors, -1 if trouble.
*/
extern int IUSnoopText(XMLEle *root, ITextVectorProperty *tvp);

/** \brief Update a snooped light vector property from the given XML root element.
    \param root XML root elememnt containing the snopped property content
    \param lvp a pointer to the light vector property to be updated.
    \return 0 if cracking the XML element and updating the property proceeded without errors, -1 if trouble.
*/
extern int IUSnoopLight(XMLEle *root, ILightVectorProperty *lvp);

/** \brief Update a snooped switch vector property from the given XML root element.
    \param root XML root elememnt containing the snopped property content
    \param svp a pointer to the switch vector property to be updated.
    \return 0 if cracking the XML element and updating the property proceeded without errors, -1 if trouble.
*/
extern int IUSnoopSwitch(XMLEle *root, ISwitchVectorProperty *svp);

/** \brief Update a snooped BLOB vector property from the given XML root element.
    \param root XML root elememnt containing the snopped property content
    \param bvp a pointer to the BLOB vector property to be updated.
    \return 0 if cracking the XML element and updating the property proceeded without errors, -1 if trouble.
*/
extern int IUSnoopBLOB(XMLEle *root, IBLOBVectorProperty *bvp);

/*@}*/

/** \brief Extract dev and name attributes from an XML element.
    \param root The XML element to be parsed.
    \param dev pointer to an allocated buffer to save the extracted element device name attribute.
           The buffer size must be at least MAXINDIDEVICE bytes.
    \param name pointer to an allocated buffer to save the extracted elemented name attribute.
           The buffer size must be at least MAXINDINAME bytes.
    \param msg pointer to an allocated char buffer to store error messages. The minimum buffer size is MAXRBUF.
    \return 0 if successful, -1 if error is encountered and msg is set.
*/
extern int crackDN(XMLEle *root, char **dev, char **name, char msg[]);

/** \brief Extract property state (Idle, OK, Busy, Alert) from the supplied string.
    \param str A string representation of the state.
    \param ip Pointer to IPState structure to store the extracted property state.
    \return 0 if successful, -1 if error is encountered.
*/
extern int crackIPState(const char *str, IPState *ip);

/** \brief Extract switch state (On or Off) from the supplied string.
    \param str A string representation of the switch state.
    \param ip Pointer to ISState structure to store the extracted switch state.
    \return 0 if successful, -1 if error is encountered.
*/
extern int crackISState(const char *str, ISState *ip);

/** \brief Extract property permission state (RW, RO, WO) from the supplied string.
    \param str A string representation of the permission state.
    \param ip Pointer to IPerm structure to store the extracted permission state.
    \return 0 if successful, -1 if error is encountered.
*/
extern int crackIPerm(const char *str, IPerm *ip);

/** \brief Extract switch rule (OneOfMany, OnlyOne..etc) from the supplied string.
    \param str A string representation of the switch rule.
    \param ip Pointer to ISRule structure to store the extracted switch rule.
    \return 0 if successful, -1 if error is encountered.
*/
extern int crackISRule(const char *str, ISRule *ip);

/** \return Returns a string representation of the supplied property state. */
extern const char *pstateStr(IPState s);
/** \return Returns a string representation of the supplied switch status. */
extern const char *sstateStr(ISState s);
/** \return Returns a string representation of the supplied switch rule. */
extern const char *ruleStr(ISRule r);
/** \return Returns a string representation of the supplied permission value. */
extern const char *permStr(IPerm p);

#ifdef __cplusplus
}
#endif
