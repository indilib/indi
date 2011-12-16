#include <iostream>

using namespace std;

#define STAND_ALONE_CCD

#include "sxccd.h"

int write_ppm(char *name,char *buffer,int xres,int yres,int depth)
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
    char *FrameBuffer;
    int FrameBufferSize;
    char *GuiderFrame;
    int GuiderFrameSize=1;
    int exptime;

    SxCCD ccd;

    int TestGuider=0;
    exptime=100;

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
        SubH=ccd.xres;

        FrameBufferSize=ccd.xres*ccd.yres;
        if(ccd.bits_per_pixel==16) FrameBufferSize*=2;
        FrameBuffer=new char[FrameBufferSize];

        if(ccd.hasguide) {
            GuiderFrameSize=ccd.gxres*ccd.gyres;
            if(ccd.gbits_per_pixel==16) GuiderFrameSize*=2;
        }

        GuiderFrame=new char[GuiderFrameSize*2];

        if(TestGuider==0) {
            //  Do a quick exposure on a progressive ccd
            ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD);
            ccd.LatchPixels(SXCCD_EXP_FLAGS_FIELD_BOTH,IMAGE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            ccd.ReadPixels(FrameBuffer,FrameBufferSize);

            write_ppm("test.ppm",FrameBuffer,SubW,SubH,65535);

        } else {

            SubX=0;
            SubY=0;
            BinX=1;
            BinY=1;
            SubW=ccd.gxres;
            SubH=ccd.gyres;

            //  Experiment with the interlaced ccd

            ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_EVEN,GUIDE_CCD);
            //  a delay here, equal to readout time for a half frame
            //  will make even and odd frames get the same exposure time
            //  Assuming exposure time is longer than readout time


            ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_ODD,GUIDE_CCD);
            //  A delay here for exposure time, less any delays introduced between the halves
            usleep(exptime);
            //  and now lets try an interlaced readout
            //  first the even lines
            ccd.LatchPixels(SXCCD_EXP_FLAGS_FIELD_EVEN | SXCCD_EXP_FLAGS_NOBIN_ACCUM,GUIDE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            ccd.ReadPixels(GuiderFrame,GuiderFrameSize);


            //  and now the odd lines
            //ccd.ClearPixels(SXCCD_EXP_FLAGS_FIELD_ODD,GUIDE_CCD);
            //usleep(exptime);

            ccd.LatchPixels(SXCCD_EXP_FLAGS_FIELD_ODD | SXCCD_EXP_FLAGS_NOBIN_ACCUM,GUIDE_CCD,SubX,SubY,SubW,SubH,BinX,BinY);
            ccd.ReadPixels(GuiderFrame+GuiderFrameSize,GuiderFrameSize);

            for(int x=0; x<GuiderFrameSize*2; x++) GuiderFrame[x]+=100;
            write_ppm("test.ppm",GuiderFrame,SubW,SubH*2,255);

        }


        delete FrameBuffer;
        delete GuiderFrame;
        ccd.Disconnect();
    }

    return 0;
}
