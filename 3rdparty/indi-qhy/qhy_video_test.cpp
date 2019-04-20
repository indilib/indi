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

    auto start = timer.now();

    while (!exit_thread)
    {
        if (GetQHYCCDLiveFrame(pCamHandle, &w, &h, &bpp, &channels, pImgData) == QHYCCD_SUCCESS)
        {
            frames++;

            auto stop = timer.now();
            fsec duration = (stop - start);
            if (duration.count() >= 3)
            {
                fprintf(stderr, "Frames: %d Duration: %.3f seconds FPS: %.3f\n", frames, duration.count(), frames / duration.count());
                start = timer.now();
                frames = 0;
            }
            usleep(3000);
        }
        else
            usleep(1000);
    }

    return 0;
}

int main(int, char **)
{
    int USB_TRAFFIC = 20;
    int USB_SPEED = 2;
    int CHIP_GAIN = 1;
    int CHIP_OFFSET = 180;
    int EXPOSURE_TIME = 1;
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
    int rc = InitQHYCCDResource();
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "SDK resources initialized.\n");
    }
    else
    {
        fprintf(stderr, "Cannot initialize SDK resources, error: %d\n", rc);
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
        rc = GetQHYCCDId(i, camId);
        if (QHYCCD_SUCCESS == rc)
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
        rc = ReleaseQHYCCDResource();
        if (QHYCCD_SUCCESS == rc)
        {
            fprintf(stderr, "SDK resources released.\n");
        }
        else
        {
            fprintf(stderr, "Cannot release SDK resources, error %d.\n", rc);
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
    rc = InitQHYCCD(pCamHandle);
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "InitQHYCCD success.\n");
    }
    else
    {
        fprintf(stderr, "InitQHYCCD faililure, error: %d\n", rc);
        return 1;
    }

    // get overscan area
    rc = GetQHYCCDOverScanArea(pCamHandle, &overscanStartX, &overscanStartY, &overscanSizeX, &overscanSizeY);
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "GetQHYCCDOverScanArea:\n");
        fprintf(stderr, "Overscan Area startX x startY : %d x %d\n", overscanStartX, overscanStartY);
        fprintf(stderr, "Overscan Area sizeX  x sizeY  : %d x %d\n", overscanSizeX, overscanSizeY);
    }
    else
    {
        fprintf(stderr, "GetQHYCCDOverScanArea failure, error: %d\n", rc);
        return 1;
    }

    // get effective area
    rc = GetQHYCCDOverScanArea(pCamHandle, &effectiveStartX, &effectiveStartY, &effectiveSizeX, &effectiveSizeY);
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "GetQHYCCDEffectiveArea:\n");
        fprintf(stderr, "Effective Area startX x startY: %d x %d\n", effectiveStartX, effectiveStartY);
        fprintf(stderr, "Effective Area sizeX  x sizeY : %d x %d\n", effectiveSizeX, effectiveSizeY);
    }
    else
    {
        fprintf(stderr, "GetQHYCCDOverScanArea failure, error: %d\n", rc);
        return 1;
    }

    // get chip info
    rc = GetQHYCCDChipInfo(pCamHandle, &chipWidthMM, &chipHeightMM, &maxImageSizeX, &maxImageSizeY, &pixelWidthUM, &pixelHeightUM, &bpp);
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "GetQHYCCDChipInfo:\n");
        fprintf(stderr, "Effective Area startX x startY: %d x %d\n", effectiveStartX, effectiveStartY);
        fprintf(stderr, "Chip  size width x height     : %.3f x %.3f [mm]\n", chipWidthMM, chipHeightMM);
        fprintf(stderr, "Pixel size width x height     : %.3f x %.3f [um]\n", pixelWidthUM, pixelHeightUM);
        fprintf(stderr, "Image size width x height     : %d x %d\n", maxImageSizeX, maxImageSizeY);
    }
    else
    {
        fprintf(stderr, "GetQHYCCDChipInfo failure, error: %d\n", rc);
        return 1;
    }

    // set ROI
    roiStartX = 0;
    roiStartY = 0;
    roiSizeX = maxImageSizeX;
    roiSizeY = maxImageSizeY;

    // check color camera
    rc = IsQHYCCDControlAvailable(pCamHandle, CAM_COLOR);
    if (rc == BAYER_GB || rc == BAYER_GR || rc == BAYER_BG || rc == BAYER_RG)
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

    // set exposure time
    rc = SetQHYCCDParam(pCamHandle, CONTROL_EXPOSURE, EXPOSURE_TIME);
    fprintf(stderr, "SetQHYCCDParam CONTROL_EXPOSURE set to: %d us, success.\n", EXPOSURE_TIME);
    if (QHYCCD_SUCCESS == rc)
    {
    }
    else
    {
        fprintf(stderr, "SetQHYCCDParam CONTROL_EXPOSURE failure, error: %d us\n", rc);
        getchar();
        return 1;
    }

    // N.B. SetQHYCCDStreamMode must be called immediately after CONTROL_EXPOSURE is SET
    // 1. Exposure
    // 2. Stream Mode
    // 3. Speed
    // 4. Traffic
    // 5. 8-bit
    rc = SetQHYCCDStreamMode(pCamHandle, 1);
    if (rc != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "SetQHYCCDStreamMode failed: %d", rc);
    }

    // check traffic
    rc = IsQHYCCDControlAvailable(pCamHandle, CONTROL_USBTRAFFIC);
    if (QHYCCD_SUCCESS == rc)
    {
        rc = SetQHYCCDParam(pCamHandle, CONTROL_USBTRAFFIC, USB_TRAFFIC);
        if (QHYCCD_SUCCESS == rc)
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_USBTRAFFIC set to: %d, success.\n", USB_TRAFFIC);
        }
        else
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_USBTRAFFIC failure, error: %d\n", rc);
            getchar();
            return 1;
        }
    }

    // check speed
    rc = IsQHYCCDControlAvailable(pCamHandle, CONTROL_SPEED);
    if (QHYCCD_SUCCESS == rc)
    {
        rc = SetQHYCCDParam(pCamHandle, CONTROL_SPEED, USB_SPEED);
        if (QHYCCD_SUCCESS == rc)
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_SPEED set to: %d, success.\n", USB_SPEED);
        }
        else
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_SPEED failure, error: %d\n", rc);
            getchar();
            return 1;
        }
    }

    // check gain
    rc = IsQHYCCDControlAvailable(pCamHandle, CONTROL_GAIN);
    if (QHYCCD_SUCCESS == rc)
    {
        rc = SetQHYCCDParam(pCamHandle, CONTROL_GAIN, CHIP_GAIN);
        if (rc == QHYCCD_SUCCESS)
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_GAIN set to: %d, success\n", CHIP_GAIN);
        }
        else
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_GAIN failure, error: %d\n", rc);
            getchar();
            return 1;
        }
    }

    // check offset
    rc = IsQHYCCDControlAvailable(pCamHandle, CONTROL_OFFSET);
    if (QHYCCD_SUCCESS == rc)
    {
        rc = SetQHYCCDParam(pCamHandle, CONTROL_OFFSET, CHIP_OFFSET);
        if (QHYCCD_SUCCESS == rc)
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

    // set image resolution
    rc = SetQHYCCDResolution(pCamHandle, roiStartX, roiStartY, roiSizeX, roiSizeY);
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "SetQHYCCDResolution roiStartX x roiStartY: %d x %d\n", roiStartX, roiStartY);
        fprintf(stderr, "SetQHYCCDResolution roiSizeX  x roiSizeY : %d x %d\n", roiSizeX, roiSizeY);
    }
    else
    {
        fprintf(stderr, "SetQHYCCDResolution failure, error: %d\n", rc);
        return 1;
    }

    rc = IsQHYCCDControlAvailable(pCamHandle, CONTROL_TRANSFERBIT);
    if(rc == QHYCCD_SUCCESS)
    {
        rc = SetQHYCCDBitsMode(pCamHandle, 8);
        if(rc != QHYCCD_SUCCESS)
        {
            fprintf(stderr, "SetQHYCCDParam CONTROL_TRANSFERBIT failed\n");
            return 1;
        }
    }

    // set binning mode
    rc = SetQHYCCDBinMode(pCamHandle, camBinX, camBinY);
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "SetQHYCCDBinMode set to: binX: %d, binY: %d, success.\n", camBinX, camBinY);
    }
    else
    {
        fprintf(stderr, "SetQHYCCDBinMode failure, error: %d\n", rc);
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

    rc = BeginQHYCCDLive(pCamHandle);
    if (rc != QHYCCD_SUCCESS)
    {
        fprintf(stderr, "BeginQHYCCDLive failed: %d", rc);
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

    StopQHYCCDLive(pCamHandle);
    SetQHYCCDStreamMode(pCamHandle, 0x0);

    // close camera handle
    rc = CloseQHYCCD(pCamHandle);
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "Close QHYCCD success.\n");
    }
    else
    {
        fprintf(stderr, "Close QHYCCD failure, error: %d\n", rc);
    }

    // release sdk resources
    rc = ReleaseQHYCCDResource();
    if (QHYCCD_SUCCESS == rc)
    {
        fprintf(stderr, "SDK resources released.\n");
    }
    else
    {
        fprintf(stderr, "Cannot release SDK resources, error %d.\n", rc);
        return 1;
    }

    return 0;
}
