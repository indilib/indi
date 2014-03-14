
#ifndef ASICAMERA_H
#define ASICAMERA_H

#ifdef _WINDOWS
	#define ASICAMERA_API __declspec(dllexport)
#elif defined _LIN
	#define ASICAMERA_API 
#endif

enum CAMERA_TYPE{ //Camera Type
	CAMERA_ASI120MM = 0,
	CAMERA_ASI120MC,
	CAMERA_ASI034MC,
	CAMERA_ASI130MM,
	CAMERA_ASI035MM,
	CAMERA_ASI035MC,
	CAMERA_ASI030MC,
	CAMERA_UNKNOWN
};

enum Control_TYPE{ //Control ID
	CONTROL_GAIN = 0,
	CONTROL_EXPOSURE,
	CONTROL_GAMMA,
	CONTROL_WB_R,
	CONTROL_WB_B,
	CONTROL_BRIGHTNESS,
	CONTROL_BANDWIDTHOVERLOAD,
};


enum IMG_TYPE{ //Supported image type
	IMG_RAW8=0,
	IMG_RGB24,
	IMG_RAW16,
	IMG_Y8,
};

enum GuideDirections{ //Guider Direction
	guideNorth=0,
	guideSouth,
	guideEast,
	guideWest
};

enum BayerPattern{
	BayerRG=0,
	BayerBG,
	BayerGR,
	BayerGB
};

#ifndef __cplusplus
#define bool char
#define BayerPattern int
#define CAMERA_TYPE int
#define Control_TYPE int
#define IMG_TYPE int
#define GuideDirections int
#endif

#ifdef __cplusplus
extern "C" {
#endif

// get number of Connected ASI cameras.
ASICAMERA_API  int getNumberOfConnectedCameras(); 
//open Camera, camIndex 0 means the first one.
ASICAMERA_API  bool openCamera(int camIndex);
// init the  camera after Open
ASICAMERA_API  bool initCamera();
//don't forget to closeCamera if you opened one
ASICAMERA_API  void closeCamera();
//Is it a color camera?
ASICAMERA_API  bool isColorCam();
//get the pixel size of the camera
ASICAMERA_API  double getPixelSize();
// what is the bayer pattern
ASICAMERA_API  BayerPattern getColorBayer();
//get the camera name. camIndex 0 means the first one.
ASICAMERA_API  char* getCameraModel(int camIndex);
//get the camera menu type. camIndex 0 means the first one.
ASICAMERA_API  CAMERA_TYPE getCameraType(int camIndex);
//Subtract Dark using bmp file
ASICAMERA_API int EnableDarkSubtract(char *BMPPath);
//Disable Subtracting Dark 
ASICAMERA_API void DisableDarkSubtract();

// is control supported by current camera
ASICAMERA_API bool isAvailable(Control_TYPE control) ;   
// is control supported auto adjust
ASICAMERA_API bool isAutoSupported(Control_TYPE control) ;		
// get control current value and auto status
ASICAMERA_API int getValue(Control_TYPE control, bool *pbAuto)  ;    
// get minimal value of control
ASICAMERA_API int getMin(Control_TYPE control) ;  
// get maximal  value of control
ASICAMERA_API int getMax(Control_TYPE control) ;  
// set current value and auto states of control
ASICAMERA_API void setValue(Control_TYPE control, int value, bool autoset); 
// set auto parameter
ASICAMERA_API void setAutoPara(int iMaxGain, int iMaxExp, int iDestBrightness);
// get auto parameter
ASICAMERA_API void getAutoPara(int *pMaxGain, int *pMaxExp, int *pDestBrightness);

ASICAMERA_API  int getMaxWidth();  // max image width
ASICAMERA_API  int getMaxHeight(); // max image height
ASICAMERA_API  int getWidth(); // get current width
ASICAMERA_API  int getHeight(); // get current heigth
ASICAMERA_API  int getStartX(); // get ROI start X
ASICAMERA_API  int getStartY(); // get ROI start Y

ASICAMERA_API  float getSensorTemp(); //get the temp of sensor ,only ASI120 support
ASICAMERA_API  unsigned long getDroppedFrames(); //get Dropped frames 
ASICAMERA_API  bool SetMisc(bool bFlipRow, bool bFlipColumn);	//Flip x and y
ASICAMERA_API  void GetMisc(bool * pbFlipRow, bool * pbFlipColumn); //Get Flip setting	

//whether the camera support bin2 or bin3
ASICAMERA_API  bool isBinSupported(int binning); 
//whether the camera support this img_type
ASICAMERA_API  bool isImgTypeSupported(IMG_TYPE img_type); 
//get the current binning method
ASICAMERA_API  int getBin(); 

//call this function to change ROI area after setImageFormat
//return true when success false when failed
ASICAMERA_API  bool setStartPos(int startx, int starty); 
// set new image format - 
//ASI120's data size must be times of 1024 which means width*height%1024=0
ASICAMERA_API  bool setImageFormat(int width, int height,  int binning, IMG_TYPE img_type);  
//get the image type current set
ASICAMERA_API  IMG_TYPE getImgType(); 

//start capture image
ASICAMERA_API  void startCapture(); 
//stop capture image
ASICAMERA_API  void stopCapture();


// wait waitms capture a single frame -1 means wait forever, success return true, failed return false
ASICAMERA_API bool getImageData(unsigned char* buffer, int bufSize, int waitms);

//ST4 guide support. only the module with ST4 port support this
ASICAMERA_API void pulseGuide(GuideDirections direction, int timems);

#ifdef __cplusplus
}
#endif

#endif
