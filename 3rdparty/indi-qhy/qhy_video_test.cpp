/*
 QHY Video Test CCD

 Copyright (C) 2019 Jasem Mutlaq

 Based on Single Frame Test by Jan Soldan

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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <qhyccd.h>

#include <iostream>
#include <thread>
#include <atomic>

#define VERSION 1.00

static std::atomic<bool> exit_thread { false };

int videoThread(qhyccd_handle *pCamHandle, unsigned char *pImgData)
{
    std::chrono::high_resolution_clock timer;
    using fsec = std::chrono::duration<float>;
    uint32_t frames = 0, w, h, bpp, channels;
    int rc = 0;

    rc = SetQHYCCDStreamMode(pCamHandle, 0x01);
    if (rc != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "SetQHYCCDStreamMode failed: %d", rc);
    }
    rc = BeginQHYCCDLive(pCamHandle);
    if (rc != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "BeginQHYCCDLive failed: %d", rc);
    }

    auto start = timer.now();

    while (!exit_thread)
    {
        if (GetQHYCCDLiveFrame(pCamHandle, &w, &h, &bpp, &channels, pImgData) == QHYCCD_SUCCESS)
            frames++;
    }

    auto stop = timer.now();

    fsec duration = (stop - start);

    fprintf(stderr, "Frames: %d Duration: %.3f seconds FPS: %.3f\n", frames, duration.count(), frames / duration.count());

    SetQHYCCDStreamMode(pCamHandle, 0x0);
    StopQHYCCDLive(pCamHandle);

    return 0;
}

int main(int, char **)
{

    int USB_TRAFFIC = 0;
    int USB_SPEED = 0;
    int CHIP_GAIN = 1;
    int CHIP_OFFSET = 180;
    int EXPOSURE_TIME = 40000;
    int camBinX = 1;
    int camBinY = 1;

    double chipWidthMM;
    double chipHeightMM;
    double pixelWidthUM;
    double pixelHeightUM;

    unsigned int roiStartX;
    unsigned int roiStartY;
    unsigned int roiSizeX;
    unsigned int roiSizeY;

    unsigned int overscanStartX;
    unsigned int overscanStartY;
    unsigned int overscanSizeX;
    unsigned int overscanSizeY;

    unsigned int effectiveStartX;
    unsigned int effectiveStartY;
    unsigned int effectiveSizeX;
    unsigned int effectiveSizeY;

    unsigned int maxImageSizeX;
    unsigned int maxImageSizeY;
    unsigned int bpp;
    //unsigned int channels;

    unsigned char *pImgData = nullptr;

    EnableQHYCCDLogFile(false);
    EnableQHYCCDMessage(false);

    fprintf(stderr, "QHY Video Test using VideoFrameMode, Version: %.2f\n", VERSION);

    // init SDK
    int retVal = InitQHYCCDResource();
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "SDK resources initialized.\n");
    }
    else
    {
        fprintf(stderr, "Cannot initialize SDK resources, error: %d\n", retVal);
        return 1;
    }

    // scan cameras
    int camCount = ScanQHYCCD();
    if (camCount > 0)
    {
        fprintf(stderr, "Number of QHYCCD cameras found: %d \n", camCount);
    }
    else
    {
        fprintf(stderr, "No QHYCCD camera found, please check USB or power.\n");
        return 1;
    }

    // iterate over all attached cameras
    bool camFound = false;
    char camId[32];

    for (int i = 0; i < camCount; i++)
    {
        retVal = GetQHYCCDId(i, camId);
        if (QHYCCD_SUCCESS == retVal)
        {
            fprintf(stderr, "Application connected to the following camera from the list: Index: %d,  cameraID = %s\n", (i + 1), camId);
            camFound = true;
            break;
        }
    }

    if (!camFound)
    {
        fprintf(stderr, "The detected camera is not QHYCCD or other error.\n");
        // release sdk resources
        retVal = ReleaseQHYCCDResource();
        if (QHYCCD_SUCCESS == retVal)
        {
            fprintf(stderr, "SDK resources released.\n");
        }
        else
        {
            fprintf(stderr, "Cannot release SDK resources, error %d.\n", retVal);
        }
        return 1;
    }

    // open camera
    qhyccd_handle *pCamHandle = OpenQHYCCD(camId);
    if (pCamHandle != nullptr)
    {
        fprintf(stderr, "Open QHYCCD success.\n");
    }
    else
    {
        fprintf(stderr, "Open QHYCCD failure.\n");
        return 1;
    }

    // initialize camera
    retVal = InitQHYCCD(pCamHandle);
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "InitQHYCCD success.\n");
    }
    else
    {
        fprintf(stderr, "InitQHYCCD faililure, error: %d\n", retVal);
        return 1;
    }

    // get overscan area
    retVal = GetQHYCCDOverScanArea(pCamHandle, &overscanStartX, &overscanStartY, &overscanSizeX, &overscanSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "GetQHYCCDOverScanArea:\n");
        fprintf(stderr, "Overscan Area startX x startY : %d x %d\n", overscanStartX, overscanStartY);
        fprintf(stderr, "Overscan Area sizeX  x sizeY  : %d x %d\n", overscanSizeX, overscanSizeY);
    }
    else
    {
        fprintf(stderr, "GetQHYCCDOverScanArea failure, error: %d\n", retVal);
        return 1;
    }

    // get effective area
    retVal = GetQHYCCDOverScanArea(pCamHandle, &effectiveStartX, &effectiveStartY, &effectiveSizeX, &effectiveSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "GetQHYCCDEffectiveArea:\n");
        fprintf(stderr, "Effective Area startX x startY: %d x %d\n", effectiveStartX, effectiveStartY);
        fprintf(stderr, "Effective Area sizeX  x sizeY : %d x %d\n", effectiveSizeX, effectiveSizeY);
    }
    else
    {
        fprintf(stderr, "GetQHYCCDOverScanArea failure, error: %d\n", retVal);
        return 1;
    }

    // get chip info
    retVal = GetQHYCCDChipInfo(pCamHandle, &chipWidthMM, &chipHeightMM, &maxImageSizeX, &maxImageSizeY, &pixelWidthUM, &pixelHeightUM, &bpp);
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "GetQHYCCDChipInfo:\n");
        fprintf(stderr, "Effective Area startX x startY: %d x %d\n", effectiveStartX, effectiveStartY);
        fprintf(stderr, "Chip  size width x height     : %.3f x %.3f [mm]\n", chipWidthMM, chipHeightMM);
        fprintf(stderr, "Pixel size width x height     : %.3f x %.3f [um]\n", pixelWidthUM, pixelHeightUM);
        fprintf(stderr, "Image size width x height     : %d x %d\n", maxImageSizeX, maxImageSizeY);
    }
    else
    {
        fprintf(stderr, "GetQHYCCDChipInfo failure, error: %d\n", retVal);
        return 1;
    }

    // set ROI
    roiStartX = 0;
    roiStartY = 0;
    roiSizeX = maxImageSizeX;
    roiSizeY = maxImageSizeY;

    // check color camera
    retVal = IsQHYCCDControlAvailable(pCamHandle, CAM_COLOR);
    if (retVal == BAYER_GB || retVal == BAYER_GR || retVal == BAYER_BG || retVal == BAYER_RG)
    {
        fprintf(stderr, "This is a color camera.\n");
        SetQHYCCDDebayerOnOff(pCamHandle, true);
        SetQHYCCDParam(pCamHandle, CONTROL_WBR, 20);
        SetQHYCCDParam(pCamHandle, CONTROL_WBG, 20);
        SetQHYCCDParam(pCamHandle, CONTROL_WBB, 20);
    }
    else
    {
        fprintf(stderr, "This is a mono camera.\n");
    }

    // check traffic
    retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_USBTRAFFIC);
    if (QHYCCD_SUCCESS == retVal)
    {
        retVal = SetQHYCCDParam(pCamHandle, CONTROL_USBTRAFFIC, USB_TRAFFIC);
        if (QHYCCD_SUCCESS == retVal)
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_USBTRAFFIC set to: %d, success.\n", USB_TRAFFIC);
        }
        else
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_USBTRAFFIC failure, error: %d\n", retVal);
            getchar();
            return 1;
        }
    }

    // check speed
    retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_SPEED);
    if (QHYCCD_SUCCESS == retVal)
    {
        retVal = SetQHYCCDParam(pCamHandle, CONTROL_SPEED, USB_SPEED);
        if (QHYCCD_SUCCESS == retVal)
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_SPEED set to: %d, success.\n", USB_SPEED);
        }
        else
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_SPEED failure, error: %d\n", retVal);
            getchar();
            return 1;
        }
    }

    // check gain
    retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_GAIN);
    if (QHYCCD_SUCCESS == retVal)
    {
        retVal = SetQHYCCDParam(pCamHandle, CONTROL_GAIN, CHIP_GAIN);
        if (retVal == QHYCCD_SUCCESS)
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_GAIN set to: %d, success\n", CHIP_GAIN);
        }
        else
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_GAIN failure, error: %d\n", retVal);
            getchar();
            return 1;
        }
    }

    // check offset
    retVal = IsQHYCCDControlAvailable(pCamHandle, CONTROL_OFFSET);
    if (QHYCCD_SUCCESS == retVal)
    {
        retVal = SetQHYCCDParam(pCamHandle, CONTROL_OFFSET, CHIP_OFFSET);
        if (QHYCCD_SUCCESS == retVal)
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_GAIN set to: %d, success.\n", CHIP_OFFSET);
        }
        else
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_GAIN failed.\n");
            getchar();
            return 1;
        }
    }

    // set exposure time
    retVal = SetQHYCCDParam(pCamHandle, CONTROL_EXPOSURE, EXPOSURE_TIME);
    fprintf(stderr, "SetQHYCCDParam CONTROL_EXPOSURE set to: %d us, success.\n", EXPOSURE_TIME);
    if (QHYCCD_SUCCESS == retVal)
    {
    }
    else
    {
        fprintf(stderr, "SetQHYCCDParam CONTROL_EXPOSURE failure, error: %d us\n", retVal);
        getchar();
        return 1;
    }

    // set image resolution
    retVal = SetQHYCCDResolution(pCamHandle, roiStartX, roiStartY, roiSizeX, roiSizeY);
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "SetQHYCCDResolution roiStartX x roiStartY: %d x %d\n", roiStartX, roiStartY);
        fprintf(stderr, "SetQHYCCDResolution roiSizeX  x roiSizeY : %d x %d\n", roiSizeX, roiSizeY);
    }
    else
    {
        fprintf(stderr, "SetQHYCCDResolution failure, error: %d\n", retVal);
        return 1;
    }

    // set binning mode
    retVal = SetQHYCCDBinMode(pCamHandle, camBinX, camBinY);
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "SetQHYCCDBinMode set to: binX: %d, binY: %d, success.\n", camBinX, camBinY);
    }
    else
    {
        fprintf(stderr, "SetQHYCCDBinMode failure, error: %d\n", retVal);
        return 1;
    }

    // get requested memory lenght
    uint32_t length = GetQHYCCDMemLength(pCamHandle);

    if (length > 0)
    {
        pImgData = new unsigned char[length];
        memset(pImgData, 0, length);
        fprintf(stderr, "Allocated memory for frame: %d [uchar].\n", length);
    }
    else
    {
        fprintf(stderr, "Cannot allocate memory for frame.\n");
        return 1;
    }

    fprintf(stderr, "Press any key to exit...\n");

    // Video Frame
    std::thread t(&videoThread, pCamHandle, pImgData);

    // wait for user key
    std::getchar();

    if (!exit_thread)
    {
        exit_thread = true;
        t.join();
    }

    // close camera handle
    retVal = CloseQHYCCD(pCamHandle);
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "Close QHYCCD success.\n");
    }
    else
    {
        fprintf(stderr, "Close QHYCCD failure, error: %d\n", retVal);
    }

    // release sdk resources
    retVal = ReleaseQHYCCDResource();
    if (QHYCCD_SUCCESS == retVal)
    {
        fprintf(stderr, "SDK resources released.\n");
    }
    else
    {
        fprintf(stderr, "Cannot release SDK resources, error %d.\n", retVal);
        return 1;
    }

    return 0;
}
