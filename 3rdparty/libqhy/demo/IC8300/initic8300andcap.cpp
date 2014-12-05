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
    IplImage *img;

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
            strncpy(camtype,id,6);
            if(!strcmp(camtype,"IC8300"))
            {
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
            printf("Open IC8300 success!\n");
        }
        else
        {
            printf("Open IC8300 failue \n");
            goto failure;
        }

        ret = InitQHYCCD(camhandle);
        if(ret == QHYCCD_SUCCESS)
        {
            printf("Init IC8300 success!\n");
        }
        else
        {
            printf("Init IC8300 failue code:%d\n",ret);
            goto failure;
        }
        
        ret = ExpQHYCCDSingleFrame(camhandle);
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

        ret = GetQHYCCDSingleFrame(camhandle,&w,&h,&bpp,&channels,ImgData);
        if(ret == QHYCCD_SUCCESS)
        {
            printf("Get Image Frame succeess! \n");

            img = cvCreateImage(cvSize(w,h),bpp,channels);
            img->imageData = (char *)ImgData;
          
            if(bpp == 8)
            {
                cvSaveImage("/tmp/test.bmp",img);
            }
            else
            {
                cvSaveImage("/tmp/test.tiff",img);
            }

            delete(ImgData);       
                 
            cvReleaseImage(&img);  
            img = NULL;   
             

        }
        else
        {
            printf("Get Image Data failed:%d\n",ret);
        }
    }
    else
    {
        printf("The camera is not IC8300 or other error \n");
        goto failure;
    }
    
    if(camhandle)
    {
        ret = CloseQHYCCD(camhandle);
        if(ret == QHYCCD_SUCCESS)
        {
            printf("Close IC8300 success!\n");
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
