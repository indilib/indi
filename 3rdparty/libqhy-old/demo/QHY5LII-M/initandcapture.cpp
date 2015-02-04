#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libqhyccd/qhyccd.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

int main(void)
{
    int num = 0;
    qhyccd_handle *camhandle;
    int ret;
    char id[32];
    char camtype[16];
    int found = 0;
    int w,h,bpp,channels;
    unsigned char *ImgData;
    
    ret = InitQHYCCDResource();
    if(ret == QHYCCD_SUCCESS)
    {
        printf("Init SDK success!\n");
    }
    else
    {
        goto failure;
    }
    num = ScanQHYCCD();
    if(num > 0)
    {
        printf("Yes!Found QHYCCD,the num is %d \n",num);
    }
    else
    {
        printf("Not Found QHYCCD,please check the usblink or the power\n");
        goto failure;
    }

    for(int i = 0;i < num;i++)
    {
        ret = GetQHYCCDId(i,id);
        if(ret == QHYCCD_SUCCESS)
        {
            strncpy(camtype,id,9);
            camtype[9] = '\0';
            if(!strcmp(camtype,"QHY5LII-M"))
            {
                printf("camid is :%s\n",id);
                found = 1;
                break;
            }
        }
    }

    if(found == 1)
    {
        camhandle = OpenQHYCCD(id);
        if(camhandle != NULL)
        {
            printf("Open QHY5LII-M success!\n");
        }
        else
        {
            printf("Open QHY5LII-M failue \n");
            goto failure;
        }

        ret = InitQHYCCD(camhandle);
        if(ret == QHYCCD_SUCCESS)
        {
            printf("Init QHY5LII-M success!\n");
        }
        else
        {
            printf("Init QHY5LII-M failue code:%d\n",ret);
            goto failure;
        }

        ret = SetQHYCCDResolution(camhandle,1280,960);
        if(ret == QHYCCD_SUCCESS)
        {
            printf("Set QHY5LII-M resolution success!\n");
        }
        else
        {
            printf("Set QHY5LII-M resolution failure\n");
            goto failure;
        }
        
        ret = BeginQHYCCDLive(camhandle);
        if(ret == QHYCCD_SUCCESS)
        {
            printf("Begin Live Video success!\n");
        }
        else
        {
            printf("Begin Live Video failure\n");
            goto failure;
        }
        
        int length = GetQHYCCDMemLength(camhandle);
        
        if(length > 0)
        {
            ImgData = (unsigned char *)malloc(length);
            memset(ImgData,0,length);
        }
        else
        {
            printf("Get the min memory space length failure \n");
            goto failure;
        }
        
        ret = GetQHYCCDLiveFrame(camhandle,&w,&h,&bpp,&channels,ImgData);
        if(ret == QHYCCD_SUCCESS)
        {
            printf("Get Image Frame succeess! \n");
        
            IplImage *img = cvCreateImage(cvSize(w,h),bpp,channels);
            img->imageData = (char *)ImgData;
        
            cvNamedWindow("show",CV_WINDOW_AUTOSIZE);
        
            printf("Focus on the image press any key to exit \n");
            
            cvShowImage("show",img);
            
            cvWaitKey(0);
        
            cvDestroyWindow("show");
        
            cvReleaseImage(&img);
        }
    }
    else
    {
        printf("The camera is not QHY5LII-M or other error \n");
        goto failure;
    }
    
    if(camhandle)
    {
        ret = CloseQHYCCD(camhandle);
        if(ret == QHYCCD_SUCCESS)
        {
            printf("Close QHY5LII-M success!\n");
        }
        else
        {
            goto failure;
        }
    }

    ret = ReleaseQHYCCDResource();
    if(ret == QHYCCD_SUCCESS)
    {
        printf("Rlease SDK Resource  success!\n");
    }
    else
    {
        goto failure;
    }

    return 0;

failure:
    printf("some fatal error happened\n");
    return 1;
}
