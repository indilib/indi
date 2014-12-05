#include <stdio.h>
#include <libqhyccd/qhyccd.h>


int main(void)
{
    int num = 0;
    qhyccd_handle *camhandle;
    int ret;

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
 
    }

    return 0;
failure:
    printf("some fatal error happened\n");
    return 1;
}
