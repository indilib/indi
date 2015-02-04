#include <stdio.h>
#include <string.h>
#include <libqhyccd/qhyccd.h>


int main(void)
{
    int num = 0;
    qhyccd_handle *camhandle;
    int ret;
    char id[32];
    char camtype[16];
    int found = 0;
    char ch = '0';

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

    for(int i = 0;i < num;i++)
    {
        ret = GetQHYCCDId(i,id);
        if(ret == QHYCCD_SUCCESS)
        {
            strncpy(camtype,id,6);
            if(!strcmp(camtype,"QHYXXX"))
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
            printf("Open QHYXXX success!\n");
        }
        
        while(ch != 'e' && ch != 'E')
        {
            printf("Please enter the command you want to control:\n");
            printf("Choice is 0 - 8,it means the 9 hole in color filter wheel\n");
            printf("If you want to exit,enter e or E\n");
            scanf("%s",&ch);
            getchar();
            
            if(ch >= '0' && ch <= '8')
            {
                ret = ControlQHYCCDCFW(camhandle,ch);
                if(ret != QHYCCD_SUCCESS)
                {
                    printf("Control the color filter wheel failure \n");
                    goto failure;
                }
            }
        }
    }
    else
    {
        printf("The camera is not QHYXXX or other error \n");
        goto failure;
    }
    
        
    ret = CloseQHYCCD(camhandle);
    if(ret == QHYCCD_SUCCESS)
    {
        printf("Close QHYXXX success!\n");
    }
    else
    {
        goto failure;
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
