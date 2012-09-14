#include <iostream>

using namespace std;

#define STAND_ALONE_CCD

#include "sxccd.h"

#define PROGRESSIVE_TEST 0
#define GUIDER_TEST 1
#define INTERLACE_TEST 2

int write_ppm(const char *name,char *buffer,int xres,int yres,int depth)
{
    int x;

   	FILE *h = fopen(name, "w");
	fprintf(h, "P5\n");

	fprintf(h, "%d %d\n", xres, yres);

	fprintf(h, "%d\n",depth);

    if(depth <= 255) {
        for(x=0; x<xres*yres; x++) {
            fprintf(h,"%c",buffer[x]);
        }

    } else {
        for(x=0; x<xres*yres*2; x++) {
            fprintf(h,"%c",buffer[x]);
        }
    }

	fclose(h);

    return 0;
}

int main()
{
    bool rc;

    int SubX,SubY,SubW,SubH,BinX,BinY;
    char *FrameBuffer=NULL;
    int FrameBufferSize;
    char *GuiderFrame=NULL;
    int GuiderFrameSize=1;
    int exptime;

    SxCCD ccd;


    int TestCase = GUIDER_TEST;

    //TestCase = PROGRESSIVE_TEST;

    exptime=1000000;

    rc=ccd.Connect();
    if(rc) {
        printf("connected ok\n");
        ccd.GetCameraModel();
        ccd.GetFirmwareVersion();

        SubX=0;
        SubY=0;
        BinX=1;
        BinY=1;
        SubW=ccd.xres;
        SubH=ccd.yres;

        FrameBufferSize=ccd.xres*ccd.yres;
        if(ccd.bits_per_pixel==16) FrameBufferSize*=2;

        if (TestCase == PROGRESSIVE_TEST)
            FrameBuffer=new char[FrameBufferSize];
        else if (TestCase == INTERLACE_TEST)
            FrameBuffer=new char[FrameBufferSize*2];


        if(ccd.sx_hasguide) {
            GuiderFrameSize=ccd.gxres*ccd.gyres;
            if(ccd.gbits_per_pixel==16) GuiderFrameSize*=2;
        }

        GuiderFrame=new char[GuiderFrameSize*2];

        switch (TestCase)
        {
           case PROGRESSIVE_TEST:
            //  Do a quick exposure on a progressive ccd
            ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD);
            ccd.LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            ccd.ReadPixels(FrameBuffer,FrameBufferSize);

            write_ppm("test.ppm",FrameBuffer,SubW,SubH,65535);
            break;

        case GUIDER_TEST:
            SubX=0;
            SubY=0;
            BinX=1;
            BinY=1;
            SubW=ccd.gxres;
            SubH=ccd.gyres;

            //  Experiment with the guide ccd

            ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,GUIDE_CCD);
            usleep(exptime);
            ccd.LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH ,GUIDE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            ccd.ReadPixels(GuiderFrame,GuiderFrameSize);
            //for(int x=0; x<GuiderFrameSize; x++) GuiderFrame[x]=x%100;
            write_ppm("test.ppm",GuiderFrame,SubW,SubH,255);
            break;

        case INTERLACE_TEST:
            SubX=0;
            SubY=0;
            BinX=1;
            BinY=1;
            SubW=ccd.xres;
            SubH=ccd.yres;

            //  Experiment with the interlaced ccd

            ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_EVEN,IMAGE_CCD);
            //  a delay here, equal to readout time for a half frame
            //  will make even and odd frames get the same exposure time
            //  Assuming exposure time is longer than readout time


            ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_ODD,IMAGE_CCD);
            //  A delay here for exposure time, less any delays introduced between the halves
            usleep(exptime);
            //  and now lets try an interlaced readout
            //  first the even lines
            ccd.LatchPixels(SXCCD_EXP_FLAGS_FIELD_EVEN | SXCCD_EXP_FLAGS_NOBIN_ACCUM,IMAGE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            ccd.ReadPixels(FrameBuffer,FrameBufferSize);


            //  and now the odd lines
            //ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_ODD,GUIDE_CCD);
            //usleep(exptime);

            ccd.LatchPixels(SXCCD_EXP_FLAGS_FIELD_ODD | SXCCD_EXP_FLAGS_NOBIN_ACCUM,IMAGE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            ccd.ReadPixels(FrameBuffer+FrameBufferSize,FrameBufferSize);

            //for(int x=0; x<GuiderFrameSize*2; x++) GuiderFrame[x]+=100;
            write_ppm("test.ppm",FrameBuffer,SubW,SubH*2,65535);
            break;


        }

        if(FrameBuffer != NULL) delete FrameBuffer;
        if(GuiderFrame != NULL) delete GuiderFrame;
        ccd.Disconnect();
    }

    return 0;
}
