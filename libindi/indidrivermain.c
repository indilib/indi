#if 0
    INDI
    Copyright (C) 2003-2006 Elwood C. Downey

                        Updated by Jasem Mutlaq (2003-2010)

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

#endif

/* main() for one INDI driver process.
 * Drivers define IS*() functions we call to deliver INDI XML arriving on stdin.
 * Drivers call ID*() functions to send INDI XML commands to stdout.
 * Drivers call IE*() functions to build an event-driver program.
 * Drivers call IU*() functions to perform various common utility tasks.
 * Troubles are reported on stderr then we exit.
 *
 * This requires liblilxml.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lilxml.h"
#include "base64.h"
#include "eventloop.h"
#include "indidevapi.h"
#include "indicom.h"
#include "indidrivermain.h"

#define MAXRBUF 4096

static void usage(void);
static int dispatch (XMLEle *root, char msg[]);
static void clientMsgCB(int fd, void *arg);

static int nroCheck;			/* # of elements in roCheck */
static int verbose;			/* chatty */
char *me;				/* a.out name */
static LilXML *clixml;			/* XML parser context */

/* insure RO properties are never modified. RO Sanity Check */
typedef struct
{
  char propName[MAXINDINAME];
  IPerm perm;
} ROSC;

static ROSC *roCheck;

int
main (int ac, char *av[])
{
#ifndef _WIN32
        setgid( getgid() );
        setuid( getuid() );

	if (geteuid() != getuid())
            exit(255);
#endif

	/* save handy pointer to our base name */
	for (me = av[0]; av[0][0]; av[0]++)
	    if (av[0][0] == '/')
		me = &av[0][1];

	/* crack args */
	while (--ac && (*++av)[0] == '-')
	    while (*++(*av))
		switch (*(*av)) {
		case 'v':	/* verbose */
		    verbose++;
		    break;
		default:
		    usage();
		}

	/* ac remaining args starting at av[0] */
	if (ac > 0)
	    usage();

	/* init */
	clixml =  newLilXML();
	addCallback (0, clientMsgCB, NULL);
	
	nroCheck = 0;
	roCheck = NULL;

	/* service client */
	eventLoop();

	/* eh?? */
	fprintf (stderr, "%s: inf loop ended\n", me);
	return (1);
}

/* Return 1 is property is already cached, 0 otherwise */
int isPropDefined(const char *property_name)
{
  int i=0;
  
  for (i=0; i < nroCheck; i++)
    if (!strcmp(property_name, roCheck[i].propName))
	return 1;

  return 0;

}

/* functions we define that drivers may call */

/* tell client to create a text vector property */
void
IDDefText (const ITextVectorProperty *tvp, const char *fmt, ...)
{
	int i;
	ROSC *SC;

	xmlv1();
	printf ("<defTextVector\n");
	printf ("  device='%s'\n", tvp->device);
	printf ("  name='%s'\n", tvp->name);
	printf ("  label='%s'\n", tvp->label);
	printf ("  group='%s'\n", tvp->group);
	printf ("  state='%s'\n", pstateStr(tvp->s));
	printf ("  perm='%s'\n", permStr(tvp->p));
	printf ("  timeout='%g'\n", tvp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < tvp->ntp; i++) {
	    IText *tp = &tvp->tp[i];
	    printf ("  <defText\n");
	    printf ("    name='%s'\n", tp->name);
	    printf ("    label='%s'>\n", tp->label);
	    printf ("      %s\n", tp->text ? tp->text : "");
	    printf ("  </defText>\n");
	}

	printf ("</defTextVector>\n");

	if (!isPropDefined(tvp->name))
	{	
		/* Add this property to insure proper sanity check */
		roCheck = roCheck ? (ROSC *) realloc ( roCheck, sizeof(ROSC) * (nroCheck+1))
	                  	: (ROSC *) malloc  ( sizeof(ROSC));
		SC      = &roCheck[nroCheck++];
	
		strcpy(SC->propName, tvp->name);
		SC->perm = tvp->p;
	}
	
	fflush (stdout);
}

/* tell client to create a new numeric vector property */
void
IDDefNumber (const INumberVectorProperty *n, const char *fmt, ...)
{
	int i;
	ROSC *SC;

	xmlv1();
	printf ("<defNumberVector\n");
	printf ("  device='%s'\n", n->device);
	printf ("  name='%s'\n", n->name);
	printf ("  label='%s'\n", n->label);
	printf ("  group='%s'\n", n->group);
	printf ("  state='%s'\n", pstateStr(n->s));
	printf ("  perm='%s'\n", permStr(n->p));
	printf ("  timeout='%g'\n", n->timeout);
	printf ("  timestamp='%s'\n", timestamp());


	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");


	for (i = 0; i < n->nnp; i++) {

	    INumber *np = &n->np[i];

	    printf ("  <defNumber\n");
	    printf ("    name='%s'\n", np->name);
	    printf ("    label='%s'\n", np->label);
	    printf ("    format='%s'\n", np->format);
	    printf ("    min='%.20g'\n", np->min);
	    printf ("    max='%.20g'\n", np->max);
	    printf ("    step='%.20g'>\n", np->step);
	    printf ("      %.20g\n", np->value);

	    printf ("  </defNumber>\n");
	}

	printf ("</defNumberVector>\n");
	
	if (!isPropDefined(n->name))
	{	
		/* Add this property to insure proper sanity check */
		roCheck = roCheck ? (ROSC *) realloc ( roCheck, sizeof(ROSC) * (nroCheck+1))
	                  	: (ROSC *) malloc  ( sizeof(ROSC));
		SC      = &roCheck[nroCheck++];
	
		strcpy(SC->propName, n->name);
		SC->perm = n->p;

	}
	
	fflush (stdout);
}

/* tell client to create a new switch vector property */
void
IDDefSwitch (const ISwitchVectorProperty *s, const char *fmt, ...)

{
	int i;
	ROSC *SC;

	xmlv1();
	printf ("<defSwitchVector\n");
	printf ("  device='%s'\n", s->device);
	printf ("  name='%s'\n", s->name);
	printf ("  label='%s'\n", s->label);
	printf ("  group='%s'\n", s->group);
	printf ("  state='%s'\n", pstateStr(s->s));
	printf ("  perm='%s'\n", permStr(s->p));
	printf ("  rule='%s'\n", ruleStr (s->r));
	printf ("  timeout='%g'\n", s->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < s->nsp; i++) {
	    ISwitch *sp = &s->sp[i];
	    printf ("  <defSwitch\n");
	    printf ("    name='%s'\n", sp->name);
	    printf ("    label='%s'>\n", sp->label);
	    printf ("      %s\n", sstateStr(sp->s));
	    printf ("  </defSwitch>\n");
	}

	printf ("</defSwitchVector>\n");
	
	if (!isPropDefined(s->name))
	{	
		/* Add this property to insure proper sanity check */
		roCheck = roCheck ? (ROSC *) realloc ( roCheck, sizeof(ROSC) * (nroCheck+1))
	                  	: (ROSC *) malloc  ( sizeof(ROSC));
		SC      = &roCheck[nroCheck++];
	
		strcpy(SC->propName, s->name);
		SC->perm = s->p;
	}
	
	fflush (stdout);
}

/* tell client to create a new lights vector property */
void
IDDefLight (const ILightVectorProperty *lvp, const char *fmt, ...)
{
	int i;

	xmlv1();
	printf ("<defLightVector\n");
	printf ("  device='%s'\n", lvp->device);
	printf ("  name='%s'\n", lvp->name);
	printf ("  label='%s'\n", lvp->label);
	printf ("  group='%s'\n", lvp->group);
	printf ("  state='%s'\n", pstateStr(lvp->s));
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < lvp->nlp; i++) {
	    ILight *lp = &lvp->lp[i];
	    printf ("  <defLight\n");
	    printf ("    name='%s'\n", lp->name);
	    printf ("    label='%s'>\n", lp->label);
	    printf ("      %s\n", pstateStr(lp->s));
	    printf ("  </defLight>\n");
	}

	printf ("</defLightVector>\n");
	fflush (stdout);
}

/* tell client to create a new BLOB vector property */
void
IDDefBLOB (const IBLOBVectorProperty *b, const char *fmt, ...)
{
  int i;
  ROSC *SC;

	xmlv1();
	printf ("<defBLOBVector\n");
	printf ("  device='%s'\n", b->device);
	printf ("  name='%s'\n", b->name);
	printf ("  label='%s'\n", b->label);
	printf ("  group='%s'\n", b->group);
	printf ("  state='%s'\n", pstateStr(b->s));
	printf ("  perm='%s'\n", permStr(b->p));
	printf ("  timeout='%g'\n", b->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

  for (i = 0; i < b->nbp; i++) {
    IBLOB *bp = &b->bp[i];
    printf ("  <defBLOB\n");
    printf ("    name='%s'\n", bp->name);
    printf ("    label='%s'\n", bp->label);
    printf ("  />\n");
  }

	printf ("</defBLOBVector>\n");

	if (!isPropDefined(b->name))
	{	
		/* Add this property to insure proper sanity check */
		roCheck = roCheck ? (ROSC *) realloc ( roCheck, sizeof(ROSC) * (nroCheck+1))
	                  	: (ROSC *) malloc  ( sizeof(ROSC));
		SC      = &roCheck[nroCheck++];
	
		strcpy(SC->propName, b->name);
		SC->perm = b->p;
	}

	fflush (stdout);
}

/* tell client to update an existing text vector property */
void
IDSetText (const ITextVectorProperty *tvp, const char *fmt, ...)
{
	int i;

	xmlv1();
	printf ("<setTextVector\n");
	printf ("  device='%s'\n", tvp->device);
	printf ("  name='%s'\n", tvp->name);
	printf ("  state='%s'\n", pstateStr(tvp->s));
	printf ("  timeout='%g'\n", tvp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < tvp->ntp; i++) {
	    IText *tp = &tvp->tp[i];
	    printf ("  <oneText name='%s'>\n", tp->name);
	    printf ("      %s\n", tp->text ? tp->text : "");
	    printf ("  </oneText>\n");
	}

	printf ("</setTextVector>\n");
	fflush (stdout);
}

/* tell client to update an existing numeric vector property */
void
IDSetNumber (const INumberVectorProperty *nvp, const char *fmt, ...)
{
	int i;

	xmlv1();
	printf ("<setNumberVector\n");
	printf ("  device='%s'\n", nvp->device);
	printf ("  name='%s'\n", nvp->name);
	printf ("  state='%s'\n", pstateStr(nvp->s));
	printf ("  timeout='%g'\n", nvp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < nvp->nnp; i++) {
	    INumber *np = &nvp->np[i];
	    printf ("  <oneNumber name='%s'>\n", np->name);
	    printf ("      %.20g\n", np->value);
	    printf ("  </oneNumber>\n");
	}

	printf ("</setNumberVector>\n");
	fflush (stdout);
}

/* tell client to update an existing switch vector property */
void
IDSetSwitch (const ISwitchVectorProperty *svp, const char *fmt, ...)
{
	int i;

	xmlv1();
	printf ("<setSwitchVector\n");
	printf ("  device='%s'\n", svp->device);
	printf ("  name='%s'\n", svp->name);
	printf ("  state='%s'\n", pstateStr(svp->s));
	printf ("  timeout='%g'\n", svp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < svp->nsp; i++) {
	    ISwitch *sp = &svp->sp[i];
	    printf ("  <oneSwitch name='%s'>\n", sp->name);
	    printf ("      %s\n", sstateStr(sp->s));
	    printf ("  </oneSwitch>\n");
	}

	printf ("</setSwitchVector>\n");
	fflush (stdout);
}

/* tell client to update an existing lights vector property */
void
IDSetLight (const ILightVectorProperty *lvp, const char *fmt, ...)
{
	int i;

	xmlv1();
	printf ("<setLightVector\n");
	printf ("  device='%s'\n", lvp->device);
	printf ("  name='%s'\n", lvp->name);
	printf ("  state='%s'\n", pstateStr(lvp->s));
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < lvp->nlp; i++) {
	    ILight *lp = &lvp->lp[i];
	    printf ("  <oneLight name='%s'>\n", lp->name);
	    printf ("      %s\n", pstateStr(lp->s));
	    printf ("  </oneLight>\n");
	}

	printf ("</setLightVector>\n");
	fflush (stdout);
}

/* tell client to update an existing BLOB vector property */
void
IDSetBLOB (const IBLOBVectorProperty *bvp, const char *fmt, ...)
{
	int i;

	xmlv1();
	printf ("<setBLOBVector\n");
	printf ("  device='%s'\n", bvp->device);
	printf ("  name='%s'\n", bvp->name);
	printf ("  state='%s'\n", pstateStr(bvp->s));
	printf ("  timeout='%g'\n", bvp->timeout);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf (">\n");

	for (i = 0; i < bvp->nbp; i++) {
	    IBLOB *bp = &bvp->bp[i];
	    unsigned char *encblob;
	    int j, l;

	    printf ("  <oneBLOB\n");
	    printf ("    name='%s'\n", bp->name);
	    printf ("    size='%d'\n", bp->size);
	    printf ("    format='%s'>\n", bp->format);

	    encblob = malloc (4*bp->bloblen/3+4);
	    l = to64frombits(encblob, bp->blob, bp->bloblen);
	    for (j = 0; j < l; j += 72)
		printf ("%.72s\n", encblob+j);
	    free (encblob);

	    printf ("  </oneBLOB>\n");
	}

  printf ("</setBLOBVector>\n");
  fflush (stdout);
}

/* tell client to update min/max elements of an existing number vector property */
void IUUpdateMinMax(const INumberVectorProperty *nvp)
{
  int i;

  xmlv1();
  printf ("<setNumberVector\n");
  printf ("  device='%s'\n", nvp->device);
  printf ("  name='%s'\n", nvp->name);
  printf ("  state='%s'\n", pstateStr(nvp->s));
  printf ("  timeout='%g'\n", nvp->timeout);
  printf ("  timestamp='%s'\n", timestamp());
  printf (">\n");

  for (i = 0; i < nvp->nnp; i++) {
    INumber *np = &nvp->np[i];
    printf ("  <oneNumber name='%s'\n", np->name);
    printf ("    min='%g'\n", np->min);
    printf ("    max='%g'\n", np->max);
    printf ("    step='%g'\n", np->step);
    printf(">\n");
    printf ("      %g\n", np->value);
    printf ("  </oneNumber>\n");
  }

  printf ("</setNumberVector>\n");
  fflush (stdout);
}

/* tell Client to delete the property with given name on given device, or
 * entire device if !name
 */
void
IDDelete (const char *dev, const char *name, const char *fmt, ...)
{
	xmlv1();
	printf ("<delProperty\n  device='%s'\n", dev);
	if (name)
	    printf (" name='%s'\n", name);
	printf ("  timestamp='%s'\n", timestamp());
	if (fmt) {
	    va_list ap;
	    va_start (ap, fmt);
	    printf ("  message='");
	    vprintf (fmt, ap);
	    printf ("'\n");
	    va_end (ap);
	}
	printf ("/>\n");
	fflush (stdout);
}

/* tell indiserver we want to snoop on the given device/property.
 * name ignored if NULL or empty.
 */
void
IDSnoopDevice (const char *snooped_device_name, char *snooped_property_name)
{
	xmlv1();
	if (snooped_property_name && snooped_property_name[0])
	    printf ("<getProperties device='%s' name='%s'/>\n",
				    snooped_device_name, snooped_property_name);
	else
	    printf ("<getProperties device='%s'/>\n", snooped_device_name);
	fflush (stdout);
}

/* tell indiserver whether we want BLOBs from the given snooped device.
 * silently ignored if given device is not already registered for snooping.
 */
void 
IDSnoopBLOBs (const char *snooped_device, BLOBHandling bh)
{
	const char *how;

	switch (bh) {
	case B_NEVER: how = "Never"; break;
	case B_ALSO:  how = "Also";  break;
	case B_ONLY:  how = "Only";  break;
	default: return;
	}

	xmlv1();
	printf ("<enableBLOB device='%s'>%s</enableBLOB>\n",
						snooped_device, how);
	fflush (stdout);
}

/* "INDI" wrappers to the more generic eventloop facility. */

int
IEAddCallback (int readfiledes, IE_CBF *fp, void *p)
{
	return (addCallback (readfiledes, (CBF*)fp, p));
}

void
IERmCallback (int callbackid)
{
	rmCallback (callbackid);
}

int
IEAddTimer (int millisecs, IE_TCF *fp, void *p)
{
	return (addTimer (millisecs, (TCF*)fp, p));
}

void
IERmTimer (int timerid)
{
	rmTimer (timerid);
}

int
IEAddWorkProc (IE_WPF *fp, void *p)
{
	return (addWorkProc ((WPF*)fp, p));
}

void
IERmWorkProc (int workprocid)
{
	rmWorkProc (workprocid);
}


int
IEDeferLoop (int maxms, int *flagp)
{
	return (deferLoop (maxms, flagp));
}

int
IEDeferLoop0 (int maxms, int *flagp)
{
	return (deferLoop0 (maxms, flagp));
}

/* find a member of an IText vector, else NULL */
IText *
IUFindText  (const ITextVectorProperty *tvp, const char *name)
{
	int i;

	for (i = 0; i < tvp->ntp; i++)
	    if (strcmp (tvp->tp[i].name, name) == 0)
		return (&tvp->tp[i]);
	fprintf (stderr, "No IText '%s' in %s.%s\n",name,tvp->device,tvp->name);
	return (NULL);
}

/* find a member of an INumber vector, else NULL */
INumber *
IUFindNumber(const INumberVectorProperty *nvp, const char *name)
{
	int i;

	for (i = 0; i < nvp->nnp; i++)
	    if (strcmp (nvp->np[i].name, name) == 0)
		return (&nvp->np[i]);
	fprintf(stderr,"No INumber '%s' in %s.%s\n",name,nvp->device,nvp->name);
	return (NULL);
}

/* find a member of an ISwitch vector, else NULL */
ISwitch *
IUFindSwitch(const ISwitchVectorProperty *svp, const char *name)
{
	int i;

	for (i = 0; i < svp->nsp; i++)
	    if (strcmp (svp->sp[i].name, name) == 0)
		return (&svp->sp[i]);
	fprintf(stderr,"No ISwitch '%s' in %s.%s\n",name,svp->device,svp->name);
	return (NULL);
}

/* find an ON member of an ISwitch vector, else NULL.
 * N.B. user must make sense of result with ISRule in mind.
 */
ISwitch *
IUFindOnSwitch(const ISwitchVectorProperty *svp)
{
	int i;

	for (i = 0; i < svp->nsp; i++)
	    if (svp->sp[i].s == ISS_ON)
		return (&svp->sp[i]);
	/*fprintf(stderr, "No ISwitch On in %s.%s\n", svp->device, svp->name);*/
	return (NULL);
}

/* Set all switches to off */
void 
IUResetSwitch(ISwitchVectorProperty *svp)
{
  int i;
  
  for (i = 0; i < svp->nsp; i++)
    svp->sp[i].s = ISS_OFF;
}

/* Update property switches in accord with states and names. */
int 
IUUpdateSwitch(ISwitchVectorProperty *svp, ISState *states, char *names[], int n)
{
 int i=0;
 ISwitch *sp;
 char sn[MAXINDINAME];

 /* store On switch name */
 if (svp->r == ISR_1OFMANY)
 {
 	sp = IUFindOnSwitch(svp);
 	if (sp) strncpy(sn, sp->name, MAXINDINAME);
 
	IUResetSwitch(svp);
 }
 
 for (i = 0; i < n ; i++)
 {
   sp = IUFindSwitch(svp, names[i]);
	 
   if (!sp)
   {
              svp->s = IPS_IDLE;
	      IDSetSwitch(svp, "Error: %s is not a member of %s property.", names[i], svp->name);
	      return -1;
   }
	 
   sp->s = states[i]; 
 }
 
 /* Consistency checks for ISR_1OFMANY after update. */
 if (svp->r == ISR_1OFMANY)
 {
	int t_count=0;
	for (i=0; i < svp->nsp; i++)
	{
		if (svp->sp[i].s == ISS_ON)
			t_count++;
	}
	if (t_count != 1)
	{
		IUResetSwitch(svp);
		sp = IUFindSwitch(svp, sn);
		if (sp) sp->s = ISS_ON;
		svp->s = IPS_IDLE;
		IDSetSwitch(svp, "Error: invalid state switch for property %s. %s.", svp->name, t_count == 0 ? "No switch is on" : "Too many switches are on");
		return -1;
	}
 }
		
 return 0;

}

/* Update property numbers in accord with values and names */
int IUUpdateNumber(INumberVectorProperty *nvp, double values[], char *names[], int n)
{
  int i=0;
  
  INumber *np;
  
  for (i = 0; i < n; i++)
  {
    np = IUFindNumber(nvp, names[i]);
    if (!np)
    {
    	nvp->s = IPS_IDLE;
	IDSetNumber(nvp, "Error: %s is not a member of %s property.", names[i], nvp->name);
	return -1;
    }
    
    if (values[i] < np->min || values[i] > np->max)
    {
       nvp->s = IPS_IDLE;
       IDSetNumber(nvp, "Error: Invalid range. Valid range is from %g to %g", np->min, np->max);
       return -1;
    }
      
  }

  /* First loop checks for error, second loop set all values atomically*/
  for (i=0; i < n; i++)
  {
    np = IUFindNumber(nvp, names[i]);
    np->value = values[i];
  }

  return 0;

}

/* Update property text in accord with texts and names */
int IUUpdateText(ITextVectorProperty *tvp, char * texts[], char *names[], int n)
{
  int i=0;
  
  IText *tp;
  
  for (i = 0; i < n; i++)
  {
    tp = IUFindText(tvp, names[i]);
    if (!tp)
    {
    	tvp->s = IPS_IDLE;
	IDSetText(tvp, "Error: %s is not a member of %s property.", names[i], tvp->name);
	return -1;
    }
  }

  /* First loop checks for error, second loop set all values atomically*/
  for (i=0; i < n; i++)
  {
    tp = IUFindText(tvp, names[i]);
    IUSaveText(tp, texts[i]);
  }

  return 0;

}

/* save malloced copy of newtext in tp->text, reusing if not first time */
void
IUSaveText (IText *tp, const char *newtext)
{
	/* seed for realloc */
	if (tp->text == NULL)
	    tp->text = malloc (1);

	/* copy in fresh string */
	tp->text = strcpy (realloc (tp->text, strlen(newtext)+1), newtext);
}

void IUFillSwitch(ISwitch *sp, const char *name, const char * label, ISState s)
{
  strcpy(sp->name, name);
  strcpy(sp->label, label);
  sp->s = s;
  sp->svp = NULL;
  sp->aux = NULL;
}

void IUFillLight(ILight *lp, const char *name, const char * label, IPState s)
{
  strcpy(lp->name, name);
  strcpy(lp->label, label);
  lp->s = s;
  lp->lvp = NULL;
  lp->aux = NULL;
}


void IUFillNumber(INumber *np, const char *name, const char * label, const char *format, double min, double max, double step, double value)
{

  strcpy(np->name, name);
  strcpy(np->label, label);
  strcpy(np->format, format);
  
  np->min	= min;
  np->max	= max;
  np->step	= step;
  np->value	= value;
  np->nvp	= NULL;
  np->aux0	= NULL;
  np->aux1	= NULL;
}

void IUFillText(IText *tp, const char *name, const char * label, const char *initialText)
{

  strcpy(tp->name, name);
  strcpy(tp->label, label);
  tp->text = NULL;
  tp->tvp  = NULL;
  tp->aux0 = NULL;
  tp->aux1 = NULL;

  IUSaveText(tp, initialText);

}

void IUFillSwitchVector(ISwitchVectorProperty *svp, ISwitch *sp, int nsp, const char * dev, const char *name, const char *label, const char *group, IPerm p, ISRule r, double timeout, IPState s)
{
  strcpy(svp->device, dev);
  strcpy(svp->name, name);
  strcpy(svp->label, label);
  strcpy(svp->group, group);
  strcpy(svp->timestamp, "");
  
  svp->p	= p;
  svp->r	= r;
  svp->timeout	= timeout;
  svp->s	= s;
  svp->sp	= sp;
  svp->nsp	= nsp;
}

void IUFillLightVector(ILightVectorProperty *lvp, ILight *lp, int nlp, const char * dev, const char *name, const char *label, const char *group, IPState s)
{
  strcpy(lvp->device, dev);
  strcpy(lvp->name, name);
  strcpy(lvp->label, label);
  strcpy(lvp->group, group);
  strcpy(lvp->timestamp, "");
  
  lvp->s	= s;
  lvp->lp	= lp;
  lvp->nlp	= nlp;
}
 
void IUFillNumberVector(INumberVectorProperty *nvp, INumber *np, int nnp, const char * dev, const char *name, const char *label, const char* group, IPerm p, double timeout, IPState s)
{
  
  strcpy(nvp->device, dev);
  strcpy(nvp->name, name);
  strcpy(nvp->label, label);
  strcpy(nvp->group, group);
  strcpy(nvp->timestamp, "");
  
  nvp->p	= p;
  nvp->timeout	= timeout;
  nvp->s	= s;
  nvp->np	= np;
  nvp->nnp	= nnp;
  
}

void IUFillTextVector(ITextVectorProperty *tvp, IText *tp, int ntp, const char * dev, const char *name, const char *label, const char* group, IPerm p, double timeout, IPState s)
{
  strcpy(tvp->device, dev);
  strcpy(tvp->name, name);
  strcpy(tvp->label, label);
  strcpy(tvp->group, group);
  strcpy(tvp->timestamp, "");
  
  tvp->p	= p;
  tvp->timeout	= timeout;
  tvp->s	= s;
  tvp->tp	= tp;
  tvp->ntp	= ntp;

}

/*****************************************************************************
 * convenience functions for use in your implementation of ISSnoopDevice().
 */

/* crack the snooped driver setNumberVector or defNumberVector message into
 * the given INumberVectorProperty.
 * return 0 if type, device and name match and all members are present, else
 * return -1
 */
int
IUSnoopNumber (XMLEle *root, INumberVectorProperty *nvp)
{
	char *dev, *name;
	XMLEle *ep;
	int i;

	/* check and crack type, device, name and state */
	if (strcmp (tagXMLEle(root)+3, "NumberVector") ||
					crackDN (root, &dev, &name, NULL) < 0)
	    return (-1);
	if (strcmp (dev, nvp->device) || strcmp (name, nvp->name))
	    return (-1);	/* not this property */
	(void) crackIPState (findXMLAttValu (root,"state"), &nvp->s);

	/* match each INumber with a oneNumber */
	for (i = 0; i < nvp->nnp; i++) {
	    for (ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
		if (!strcmp (tagXMLEle(ep), "oneNumber") &&
			!strcmp (nvp->np[i].name, findXMLAttValu(ep, "name"))) {
		    if (f_scansexa (pcdataXMLEle(ep), &nvp->np[i].value) < 0)
			return (-1);	/* bad number format */
		    break;
		}
	    }
	    if (!ep)
		return (-1);	/* element not found */
	}

	/* ok */
	return (0);
}

/* crack the snooped driver setTextVector or defTextVector message into
 * the given ITextVectorProperty.
 * return 0 if type, device and name match and all members are present, else
 * return -1
 */
int
IUSnoopText (XMLEle *root, ITextVectorProperty *tvp)
{
	char *dev, *name;
	XMLEle *ep;
	int i;

	/* check and crack type, device, name and state */
	if (strcmp (tagXMLEle(root)+3, "TextVector") ||
					crackDN (root, &dev, &name, NULL) < 0)
	    return (-1);
	if (strcmp (dev, tvp->device) || strcmp (name, tvp->name))
	    return (-1);	/* not this property */
	(void) crackIPState (findXMLAttValu (root,"state"), &tvp->s);

	/* match each IText with a oneText */
	for (i = 0; i < tvp->ntp; i++) {
	    for (ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
		if (!strcmp (tagXMLEle(ep), "oneText") &&
			!strcmp (tvp->tp[i].name, findXMLAttValu(ep, "name"))) {
		    IUSaveText (&tvp->tp[i], pcdataXMLEle(ep));
		    break;
		}
	    }
	    if (!ep)
		return (-1);	/* element not found */
	}

	/* ok */
	return (0);
}

/* crack the snooped driver setLightVector or defLightVector message into
 * the given ILightVectorProperty. it is not necessary that all ILight names
 * be found.
 * return 0 if type, device and name match, else return -1.
 */
int
IUSnoopLight (XMLEle *root, ILightVectorProperty *lvp)
{
	char *dev, *name;
	XMLEle *ep;
	int i;

	/* check and crack type, device, name and state */
	if (strcmp (tagXMLEle(root)+3, "LightVector") ||
					crackDN (root, &dev, &name, NULL) < 0)
	    return (-1);
	if (strcmp (dev, lvp->device) || strcmp (name, lvp->name))
	    return (-1);	/* not this property */

	(void) crackIPState (findXMLAttValu (root,"state"), &lvp->s);

	/* match each oneLight with one ILight */
	for (ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
	    if (!strcmp (tagXMLEle(ep), "oneLight")) {
		const char *name = findXMLAttValu (ep, "name");
		for (i = 0; i < lvp->nlp; i++) {
		    if (!strcmp (lvp->lp[i].name, name)) {
			if (crackIPState(pcdataXMLEle(ep), &lvp->lp[i].s) < 0) {
			    return (-1);	/* unrecognized state */
			}
			break;
		    }
		}
	    }
	}

	/* ok */
	return (0);
}

/* crack the snooped driver setSwitchVector or defSwitchVector message into the
 * given ISwitchVectorProperty. it is not necessary that all ISwitch names be
 * found.
 * return 0 if type, device and name match, else return -1.
 */
int
IUSnoopSwitch (XMLEle *root, ISwitchVectorProperty *svp)
{
	char *dev, *name;
	XMLEle *ep;
	int i;

	/* check and crack type, device, name and state */
	if (strcmp (tagXMLEle(root)+3, "SwitchVector") ||
					crackDN (root, &dev, &name, NULL) < 0)
	    return (-1);
	if (strcmp (dev, svp->device) || strcmp (name, svp->name))
	    return (-1);	/* not this property */
	(void) crackIPState (findXMLAttValu (root,"state"), &svp->s);

	/* match each oneSwitch with one ISwitch */
	for (ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
	    if (!strcmp (tagXMLEle(ep), "oneSwitch")) {
		const char *name = findXMLAttValu (ep, "name");
		for (i = 0; i < svp->nsp; i++) {
		    if (!strcmp (svp->sp[i].name, name)) {
			if (crackISState(pcdataXMLEle(ep), &svp->sp[i].s) < 0) {
			    return (-1);	/* unrecognized state */
			}
			break;
		    }
		}
	    }
	}

	/* ok */
	return (0);
}

/* crack the snooped driver setBLOBVector message into the given
 * IBLOBVectorProperty. it is not necessary that all IBLOB names be found.
 * return 0 if type, device and name match, else return -1.
 * N.B. we assume any existing blob in bvp has been malloced, which we free
 *   and replace with a newly malloced blob if found.
 */
int
IUSnoopBLOB (XMLEle *root, IBLOBVectorProperty *bvp)
{
	char *dev, *name;
	XMLEle *ep;
	int i;

	/* check and crack type, device, name and state */
	if (strcmp (tagXMLEle(root), "setBLOBVector") ||
					crackDN (root, &dev, &name, NULL) < 0)
	    return (-1);
	if (strcmp (dev, bvp->device) || strcmp (name, bvp->name))
	    return (-1);	/* not this property */
	(void) crackIPState (findXMLAttValu (root,"state"), &bvp->s);

	/* match each oneBLOB with one IBLOB */
	for (ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
	    if (!strcmp (tagXMLEle(ep), "oneBLOB")) {
		const char *name = findXMLAttValu (ep, "name");
		for (i = 0; i < bvp->nbp; i++) {
		    IBLOB *bp = &bvp->bp[i];
		    if (!strcmp (bp->name, name)) {
			strcpy (bp->format, findXMLAttValu (ep,"format"));
			bp->size = atof (findXMLAttValu (ep,"size"));
			bp->bloblen = pcdatalenXMLEle(ep)+1;
			if (bp->blob)
			    free (bp->blob);
			bp->blob = strcpy(malloc(bp->bloblen),pcdataXMLEle(ep));
			break;
		    }
		}
	    }
	}

	/* ok */
	return (0);
}

/* print usage message and exit (1) */
static void
usage(void)
{
	fprintf (stderr, "Usage: %s [options]\n", me);
	fprintf (stderr, "Purpose: INDI Device driver framework.\n");
	fprintf (stderr, "Options:\n");
	fprintf (stderr, " -v    : more verbose to stderr\n");

	exit (1);
}

/* callback when INDI client message arrives on stdin.
 * collect and dispatch when see outter element closure.
 * exit if OS trouble or see incompatable INDI version.
 * arg is not used.
 */
static void
clientMsgCB (int fd, void *arg)
{
	char buf[1024], msg[1024], *bp;
	int nr;
	arg=arg;

	/* one read */
	nr = read (fd, buf, sizeof(buf));
	if (nr < 0) {
	    fprintf (stderr, "%s: %s\n", me, strerror(errno));
	    exit(1);
	}
	if (nr == 0) {
	    fprintf (stderr, "%s: EOF\n", me);
	    exit(1);
	}

	/* crack and dispatch when complete */
	for (bp = buf; nr-- > 0; bp++) {
	    XMLEle *root = readXMLEle (clixml, *bp, msg);
	    if (root) {
		if (dispatch (root, msg) < 0)
		    fprintf (stderr, "%s dispatch error: %s\n", me, msg);
		delXMLEle (root);
	    } else if (msg[0])
		fprintf (stderr, "%s XML error: %s\n", me, msg);
	}

}

/* crack the given INDI XML element and call driver's IS* entry points as they
 *   are recognized.
 * return 0 if ok else -1 with reason in msg[].
 * N.B. exit if getProperties does not proclaim a compatible version.
 */
static int
dispatch (XMLEle *root, char msg[])
{
	char *rtag = tagXMLEle(root);
	XMLEle *ep;
	int n,i=0;

	if (verbose)
	    prXMLEle (stderr, root, 0);

	/* check tag in surmised decreasing order of likelyhood */

	if (!strcmp (rtag, "newNumberVector")) {
	    static double *doubles;
	    static char **names;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* seed for reallocs */
	    if (!doubles) {
		doubles = (double *) malloc (1);
		names = (char **) malloc (1);
	    }

	    /* pull out each name/value pair */
	    for (n = 0, ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
		if (strcmp (tagXMLEle(ep), "oneNumber") == 0) {
		    XMLAtt *na = findXMLAtt (ep, "name");
		    if (na) {
			if (n >= maxn) {
			    /* grow for this and another */
			    int newsz = (maxn=n+1)*sizeof(double);
			    doubles = (double *) realloc(doubles,newsz);
			    newsz = maxn*sizeof(char *);
			    names = (char **) realloc (names, newsz);
			}
			if (f_scansexa (pcdataXMLEle(ep), &doubles[n]) < 0)
			    IDMessage (dev,"%s: Bad format %s", name,
							    pcdataXMLEle(ep));
			else
			    names[n++] = valuXMLAtt(na);
		    }
		}
	    }
	    
	    /* insure property is not RO */
	    for (i=0; i < nroCheck; i++)
	    {
	      if (!strcmp(roCheck[i].propName, name))
	      {
	       if (roCheck[i].perm == IP_RO)
	         return -1;
	      }
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewNumber (dev, name, doubles, names, n);
	    else
		IDMessage(dev,"%s: newNumberVector with no valid members",name);
	    return (0);
	}

	if (!strcmp (rtag, "newSwitchVector")) {
	    static ISState *states;
	    static char **names;
	    static int maxn;
	    char *dev, *name;
	    XMLEle *ep;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* seed for reallocs */
	    if (!states) {
		states = (ISState *) malloc (1);
		names = (char **) malloc (1);
	    }

	    /* pull out each name/state pair */
	    for (n = 0, ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
		if (strcmp (tagXMLEle(ep), "oneSwitch") == 0) {
		    XMLAtt *na = findXMLAtt (ep, "name");
		    if (na) {
			if (n >= maxn) {
			    int newsz = (maxn=n+1)*sizeof(ISState);
			    states = (ISState *) realloc(states, newsz);
			    newsz = maxn*sizeof(char *);
			    names = (char **) realloc (names, newsz);
			}
			if (strcmp (pcdataXMLEle(ep),"On") == 0) {
			    states[n] = ISS_ON;
			    names[n] = valuXMLAtt(na);
			    n++;
			} else if (strcmp (pcdataXMLEle(ep),"Off") == 0) {
			    states[n] = ISS_OFF;
			    names[n] = valuXMLAtt(na);
			    n++;
			} else 
			    IDMessage (dev, "%s: must be On or Off: %s", name,
							    pcdataXMLEle(ep));
		    }
		}
	    }

	    /* insure property is not RO */
	    for (i=0; i < nroCheck; i++)
	    {
	      if (!strcmp(roCheck[i].propName, name))
	      {
	       if (roCheck[i].perm == IP_RO)
	         return -1;
	      }
	    }
	    
	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewSwitch (dev, name, states, names, n);
	    else
		IDMessage(dev,"%s: newSwitchVector with no valid members",name);
	    return (0);
	}

	if (!strcmp (rtag, "newTextVector")) {
	    static char **texts;
	    static char **names;
	    static int maxn;
	    char *dev, *name;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* seed for reallocs */
	    if (!texts) {
		texts = (char **) malloc (1);
		names = (char **) malloc (1);
	    }

	    /* pull out each name/text pair */
	    for (n = 0, ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
		if (strcmp (tagXMLEle(ep), "oneText") == 0) {
		    XMLAtt *na = findXMLAtt (ep, "name");
		    if (na) {
			if (n >= maxn) {
			    int newsz = (maxn=n+1)*sizeof(char *);
			    texts = (char **) realloc (texts, newsz);
			    names = (char **) realloc (names, newsz);
			}
			texts[n] = pcdataXMLEle(ep);
			names[n] = valuXMLAtt(na);
			n++;
		    }
		}
	    }
	    
	    /* insure property is not RO */
	    for (i=0; i < nroCheck; i++)
	    {
	      if (!strcmp(roCheck[i].propName, name))
	      {
	       if (roCheck[i].perm == IP_RO)
	         return -1;
	      }
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0)
		ISNewText (dev, name, texts, names, n);
	    else
		IDMessage (dev, "%s: set with no valid members", name);
	    return (0);
	}

	if (!strcmp (rtag, "newBLOBVector")) {
	    static char **blobs;
	    static char **names;
	    static char **formats;
	    static int *blobsizes;
	    static int *sizes;
	    static int maxn;
	    char *dev, *name;
	    int i;

	    /* pull out device and name */
	    if (crackDN (root, &dev, &name, msg) < 0)
		return (-1);

	    /* seed for reallocs */
	    if (!blobs) {
		blobs = (char **) malloc (1);
		names = (char **) malloc (1);
		formats = (char **) malloc (1);
		blobsizes = (int *) malloc (1);
		sizes = (int *) malloc (1);
	    }

	    /* pull out each name/BLOB pair, decode */
	    for (n = 0, ep = nextXMLEle(root,1); ep; ep = nextXMLEle(root,0)) {
		if (strcmp (tagXMLEle(ep), "oneBLOB") == 0) {
		    XMLAtt *na = findXMLAtt (ep, "name");
		    XMLAtt *fa = findXMLAtt (ep, "format");
		    XMLAtt *sa = findXMLAtt (ep, "size");
		    if (na && fa && sa) {
			if (n >= maxn) {
			    int newsz = (maxn=n+1)*sizeof(char *);
			    blobs = (char **) realloc (blobs, newsz);
			    names = (char **) realloc (names, newsz);
			    formats = (char **) realloc(formats,newsz);
			    newsz = maxn*sizeof(int);
			    sizes = (int *) realloc(sizes,newsz);
			    blobsizes = (int *) realloc(blobsizes,newsz);
			}
			blobs[n] = malloc (3*pcdatalenXMLEle(ep)/4);
			blobsizes[n] = from64tobits(blobs[n], pcdataXMLEle(ep));
			names[n] = valuXMLAtt(na);
			formats[n] = valuXMLAtt(fa);
			sizes[n] = atoi(valuXMLAtt(sa));
			n++;
		    }
		}
	    }

	    /* invoke driver if something to do, but not an error if not */
	    if (n > 0) {
		ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats,names,n);
		for (i = 0; i < n; i++)
		    free (blobs[i]);
	    } else
		IDMessage (dev, "%s: newBLOBVector with no valid members",name);
	    return (0);
	}

	if (!strcmp (rtag, "getProperties")) {
	    XMLAtt *ap;
	    double v;

	    /* check version */
	    ap = findXMLAtt (root, "version");
	    if (!ap) {
		fprintf (stderr, "%s: getProperties missing version\n", me);
		exit(1);
	    }
	    v = atof (valuXMLAtt(ap));
	    if (v > INDIV) {
		fprintf (stderr, "%s: client version %g > %g\n", me, v, INDIV);
		exit(1);
	    }

	    /* ok */
	    ap = findXMLAtt (root, "device");
	    ISGetProperties (ap ? valuXMLAtt(ap) : NULL);
	    return (0);
	}

	/* other commands might be from a snooped device.
	 * we don't know here which devices are being snooped so we send
	 * all remaining valid messages
	 */
	if (        !strcmp (rtag, "setNumberVector") ||
		    !strcmp (rtag, "setTextVector") ||
		    !strcmp (rtag, "setLightVector") ||
		    !strcmp (rtag, "setSwitchVector") ||
		    !strcmp (rtag, "setBLOBVector") ||
		    !strcmp (rtag, "defNumberVector") ||
		    !strcmp (rtag, "defTextVector") ||
		    !strcmp (rtag, "defLightVector") ||
		    !strcmp (rtag, "defSwitchVector") ||
		    !strcmp (rtag, "defBLOBVector") ||
		    !strcmp (rtag, "message") ||
		    !strcmp (rtag, "delProperty")) {
	    ISSnoopDevice (root);
	    return (0);
	}

	sprintf (msg, "Unknown command: %s", rtag);
	return(1);
}

int readConfig(const char *filename, const char *dev, char errmsg[])
{
    char configFileName[MAXRBUF], configDefaultFileName[MAXRBUF];
    char *rname, *rdev;
    XMLEle *root = NULL, *fproot = NULL;
    LilXML *lp = newLilXML();

    FILE *fp = NULL;

    if (filename)
         strncpy(configFileName, filename, MAXRBUF);
     else
    {
        if (getenv("INDICONFIG"))
            strncpy(configFileName, getenv("INDICONFIG"), MAXRBUF);
        else
           snprintf(configFileName, MAXRBUF, "%s/.indi/%s_config.xml", getenv("HOME"), dev);

    }

    fp = fopen(configFileName, "r");
    if (fp == NULL)
    {
         snprintf(errmsg, MAXRBUF, "Unable to read config file. Error loading file %s: %s\n", filename, strerror(errno));
         return -1;
    }

    fproot = readXMLFile(fp, lp, errmsg);

    if (fproot == NULL)
    {
        snprintf(errmsg, MAXRBUF, "Unable to parse config XML: %s", errmsg);
        return -1;
    }

    for (root = nextXMLEle (fproot, 1); root != NULL; root = nextXMLEle (fproot, 0))
    {

        /* pull out device and name */
        if (crackDN (root, &rdev, &rname, errmsg) < 0)
            return -1;

        // It doesn't belong to our device??
        if (strcmp(dev, rdev))
            continue;

        dispatch(root, errmsg);

    }

    fclose(fp);
    delXMLEle(fproot);
    delXMLEle(root);
    delLilXML(lp);

    return (0);

}


void IUSaveDefaultConfig(const char *source_config, const char *dest_config, const char *dev)
{

    char configFileName[MAXRBUF], configDefaultFileName[MAXRBUF];

    if (source_config)
         strncpy(configFileName, source_config, MAXRBUF);
     else
    {
        if (getenv("INDICONFIG"))
           strncpy(configFileName, getenv("INDICONFIG"), MAXRBUF);
        else
           snprintf(configFileName, MAXRBUF, "%s/.indi/%s_config.xml", getenv("HOME"), dev);

    }

     if (dest_config)
         strncpy(configDefaultFileName, dest_config, MAXRBUF);
     else if (getenv("INDICONFIG"))
         snprintf(configDefaultFileName, MAXRBUF, "%s.default", getenv("INDICONFIG"));
     else
        snprintf(configDefaultFileName, MAXRBUF, "%s/.indi/%s_config.xml.default", getenv("HOME"), dev);

  // If the default doesn't exist, create it.
  if (access(configDefaultFileName, F_OK))
  {
    FILE *fpin = fopen(configFileName, "r");
      if(fpin != NULL)
      {
        FILE *fpout = fopen(configDefaultFileName, "w");
        if(fpout != NULL)
        {
          int ch = 0;
          while((ch = getc(fpin)) != EOF)
            putc(ch, fpout);

          fclose(fpin);
          fclose(fpout);
       }
    }
  }

}


/* return static string corresponding to the given property or light state */
const char *
pstateStr (IPState s)
{
        switch (s) {
        case IPS_IDLE:  return ("Idle");
        case IPS_OK:    return ("Ok");
        case IPS_BUSY:  return ("Busy");
        case IPS_ALERT: return ("Alert");
        default:
            fprintf (stderr, "Impossible IPState %d\n", s);
            exit(1);
        }
}

/* crack string into IPState.
 * return 0 if ok, else -1
 */
int
crackIPState (const char *str, IPState *ip)
{
             if (!strcmp (str, "Idle"))  *ip = IPS_IDLE;
        else if (!strcmp (str, "Ok"))    *ip = IPS_OK;
        else if (!strcmp (str, "Busy"))  *ip = IPS_BUSY;
        else if (!strcmp (str, "Alert")) *ip = IPS_ALERT;
        else return (-1);
        return (0);
}

/* crack string into ISState.
 * return 0 if ok, else -1
 */
int
crackISState (const char *str, ISState *ip)
{
             if (!strcmp (str, "On"))  *ip = ISS_ON;
        else if (!strcmp (str, "Off")) *ip = ISS_OFF;
        else return (-1);
        return (0);
}

int
crackIPerm (const char *str, IPerm *ip)
{
             if (!strcmp (str, "rw"))  *ip = IP_RW;
        else if (!strcmp (str, "ro")) *ip = IP_RO;
        else if (!strcmp (str, "wo")) *ip = IP_WO;
        else return (-1);
        return (0);
}

int crackISRule (const char *str, ISRule *ip)
{
    if (!strcmp (str, "OneOfMany"))  *ip = ISR_1OFMANY;
    else if (!strcmp (str, "AtMostOne")) *ip = ISR_ATMOST1;
    else if (!strcmp (str, "AnyOfMany")) *ip = ISR_NOFMANY;
    else return (-1);
  return (0);
}

/* return static string corresponding to the given switch state */
const char *
sstateStr (ISState s)
{
        switch (s) {
        case ISS_ON:  return ("On");
        case ISS_OFF: return ("Off");
        default:
            fprintf (stderr, "Impossible ISState %d\n", s);
            exit(1);
        }
}

/* return static string corresponding to the given Rule */
const char *
ruleStr (ISRule r)
{
        switch (r) {
        case ISR_1OFMANY: return ("OneOfMany");
        case ISR_ATMOST1: return ("AtMostOne");
        case ISR_NOFMANY: return ("AnyOfMany");
        default:
            fprintf (stderr, "Impossible ISRule %d\n", r);
            exit(1);
        }
}

/* return static string corresponding to the given IPerm */
const char *
permStr (IPerm p)
{
        switch (p) {
        case IP_RO: return ("ro");
        case IP_WO: return ("wo");
        case IP_RW: return ("rw");
        default:
            fprintf (stderr, "Impossible IPerm %d\n", p);
            exit(1);
        }
}

/* print the boilerplate comment introducing xml */
void
xmlv1()
{
        printf ("<?xml version='1.0'?>\n");
}

/* send client a message for a specific device or at large if !dev */
void
IDMessage (const char *dev, const char *fmt, ...)
{

        xmlv1();
        printf ("<message\n");
        if (dev)
            printf (" device='%s'\n", dev);
        printf ("  timestamp='%s'\n", timestamp());
        if (fmt) {
            va_list ap;
            va_start (ap, fmt);
            printf ("  message='");
            vprintf (fmt, ap);
            printf ("'\n");
            va_end (ap);
        }
        printf ("/>\n");
        fflush (stdout);
}

/* pull out device and name attributes from root.
 * return 0 if ok else -1 with reason in msg[].
 */
int
crackDN (XMLEle *root, char **dev, char **name, char msg[])
{
        XMLAtt *ap;

        ap = findXMLAtt (root, "device");
        if (!ap) {
            sprintf (msg, "%s requires 'device' attribute", tagXMLEle(root));
            return (-1);
        }
        *dev = valuXMLAtt(ap);

        ap = findXMLAtt (root, "name");
        if (!ap) {
            sprintf (msg, "%s requires 'name' attribute", tagXMLEle(root));
            return (-1);
        }
        *name = valuXMLAtt(ap);

        return (0);
}

FILE * IUGetConfigFP(const char *filename, const char *dev, char errmsg[])
{
    char configFileName[MAXRBUF];
    FILE *fp = NULL;

    if (filename)
         strncpy(configFileName, filename, MAXRBUF);
     else
    {
        if (getenv("INDICONFIG"))
            strncpy(configFileName, getenv("INDICONFIG"), MAXRBUF);
        else
           snprintf(configFileName, MAXRBUF, "%s/.indi/%s_config.xml", getenv("HOME"), dev);

    }

     fp = fopen(configFileName, "w");
     if (fp == NULL)
     {
          snprintf(errmsg, MAXRBUF, "Unable to read config file. Error loading file %s: %s\n", filename, strerror(errno));
          return NULL;
     }

     return fp;
}

void IUSaveConfigTag(FILE *fp, int ctag)
{
    if (!fp)
        return;

    /* Opening tag */
    if (ctag == 0)
        fprintf(fp, "<INDIDevice>\n");
    /* Closing tag */
    else
       fprintf(fp, "</INDIDevice>\n");
}

void IUSaveConfigNumber (FILE *fp, const INumberVectorProperty *nvp)
{
    int i;

    fprintf (fp, "<newNumberVector device='%s' name='%s'>\n", nvp->device, nvp->name);

    for (i = 0; i < nvp->nnp; i++)
    {
        INumber *np = &nvp->np[i];
        fprintf (fp, "  <oneNumber name='%s'>\n", np->name);
        fprintf (fp, "      %.20g\n", np->value);
        fprintf (fp, "  </oneNumber>\n");
    }

    fprintf (fp, "</newNumberVector>\n");
}

void IUSaveConfigText (FILE *fp, const ITextVectorProperty *tvp)
{
    int i;

    fprintf (fp, "<newTextVector device='%s' name='%s'>\n", tvp->device, tvp->name);

    for (i = 0; i < tvp->ntp; i++)
    {
        IText *tp = &tvp->tp[i];
        fprintf (fp, "  <oneText name='%s'>\n", tp->name);
        fprintf (fp, "      %s\n", tp->text ? tp->text : "");
        fprintf (fp, "  </oneText>\n");
    }

    fprintf (fp, "</newTextVector>\n");

}

void IUSaveConfigSwitch (FILE *fp, const ISwitchVectorProperty *svp)
{
    int i;

    fprintf (fp, "<newSwitchVector device='%s' name='%s'>\n", svp->device, svp->name);

    for (i = 0; i < svp->nsp; i++)
    {
        ISwitch *sp = &svp->sp[i];
        fprintf (fp, "  <oneSwitch name='%s'>\n", sp->name);
        fprintf (fp, "      %s\n", sstateStr(sp->s));
        fprintf (fp, "  </oneSwitch>\n");
    }

    fprintf (fp, "</newSwitchVector>\n");

}

void IUSaveConfigBLOB (FILE *fp, const IBLOBVectorProperty *bvp)
{
    int i;

    fprintf (fp, "<newBLOBVector device='%s' name='%s'>\n", bvp->device, bvp->name);

    for (i = 0; i < bvp->nbp; i++)
    {
        IBLOB *bp = &bvp->bp[i];
        unsigned char *encblob;
        int j, l;

        fprintf (fp, "  <oneBLOB\n");
        fprintf (fp, "    name='%s'\n", bp->name);
        fprintf (fp, "    size='%d'\n", bp->size);
        fprintf (fp, "    format='%s'>\n", bp->format);

        encblob = malloc (4*bp->bloblen/3+4);
        l = to64frombits(encblob, bp->blob, bp->bloblen);
        for (j = 0; j < l; j += 72)
            fprintf (fp, "%.72s\n", encblob+j);
        free (encblob);

        fprintf (fp, "  </oneBLOB>\n");
    }

    fprintf (fp, "</newBLOBVector>\n");

}

