//#define SHOW_ALL_OPTIONS
#define MYDEV "GPHOTO DRIVER"			/* Device name we call ourselves */

#define COMM_GROUP	"Communication"
#define EXPOSE_GROUP	"Expose"
#define IMAGE_GROUP	"Image Settings"
#define DATA_GROUP      "Data Channel"

#define	MAXEXPERR	10		/* max err in exp time we allow, secs */
#define	OPENDT		5		/* open retry delay, secs */




enum { ON_S, OFF_S };
enum { FITS_S, NATIVE_S };

/********************************************
 Property: CCD Image BLOB
*********************************************/

/* Pixels BLOB parameter */
typedef enum {	/* N.B. order must match array below */
    IMG_B, N_B
} PixelsIndex;

class GPhotoCam
{
public:
	GPhotoCam();
	~GPhotoCam();
	void ISGetProperties(void);
	void ISNewSwitch (const char *name, ISState *states, char *names[], int n);
	void ISNewNumber (const char *name, double *doubles, char *names[], int n);
	void ISNewText   (const char *name, char *texts[], char *names[], int n);

	static void ExposureUpdate(void *vp);
	void ExposureUpdate();
private:
	void InitVars(void);
	void getStartConditions(void);	
	void uploadFile(const void *fitsData, size_t totalBytes, const char *ext);

	ISwitch *create_switch(const char *basestr, const char **options, int max_opts, int setidx);
	int Connect();
	void Reset();
	void AddWidget(gphoto_widget *widget);
private:
	gphoto_driver *gphotodrv;
	int expTID;			/* exposure callback timer id, if any */

	/* info when exposure started */
	struct timeval exp0;		/* when exp started */

	ISwitch mConnectS[2];
	ISwitchVectorProperty mConnectSP;
	IText mPortT[1];
	ITextVectorProperty mPortTP;

	INumber mExposureN[1];
	INumberVectorProperty mExposureNP;

	ISwitch *mIsoS;
	ISwitchVectorProperty mIsoSP;
	ISwitch *mFormatS;
	ISwitchVectorProperty mFormatSP;
	ISwitch mTransferS[2];
	ISwitchVectorProperty mTransferSP;

	IBLOB mFitsB[1];
	IBLOBVectorProperty mFitsBP;
};
