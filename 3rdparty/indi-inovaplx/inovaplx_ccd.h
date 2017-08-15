#ifndef SIMPLECCD_H
#define SIMPLECCD_H

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <memory>
#include <pthread.h>
#include <indiccd.h>

#include <inovasdk.h>

enum CameraProperty
{
    CCD_GAIN_N=0,
    CCD_BLACKLEVEL_N,
    NUM_PROPERTIES
};
int instanceN = 0;
class INovaCCD : public INDI::CCD
{
public:
    INovaCCD();

    bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    void ISGetProperties(const char *dev);
    //Guiding functions
    IPState GuideEast(float ms);
    IPState GuideWest(float ms);
    IPState GuideNorth(float ms);
    IPState GuideSouth(float ms);
    bool HasST4Port();
    bool HasBayer();
    bool CanSubFrame();
    bool CanBin();
    int maxW, maxH;
    int startX, startY, endX, endY;
    int binX, binY;
    void CaptureThread();
protected:
    // General device functions
    bool Connect();
    bool Disconnect();
    const char *getDeviceName();
    const char *getDefaultName();
    bool initProperties();
    bool updateProperties();

    // CCD specific functions
    bool StartExposure(float duration);
    bool AbortExposure();
    void TimerHit();
    void addFITSKeywords(fitsfile *fptr, CCDChip *targetChip);

private:
    char DriverName[150];
    // Utility functions
    float CalcTimeLeft();
    void  setupParams();
    void  grabImage();
    // Are we exposing?
    bool InExposure;
    bool threadsRunning;
    bool FrameReady;
    unsigned char *RawData;
    unsigned char *TmpData;
    // Struct to keep timing
    struct timeval ExpStart;
    float ExposureRequest;
    int GainRequest;
    int BlackLevelRequest;
    int timerID;
    pthread_t captureThread;
    // We declare the CCD properties
    IText iNovaInformationT[5];
    ITextVectorProperty iNovaInformationTP;
    INumber *CameraPropertiesN, GuideNS[2], GuideEW[2];
    INumberVectorProperty CameraPropertiesNP, GuideNSV, GuideEWV;

};

#endif // SIMPLECCD_H
