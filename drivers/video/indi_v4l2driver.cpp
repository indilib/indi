#if 0
V4L Philips LX INDI Driver
INDI Interface for V4L devices (Philips)
    Copyright (C) 2003 - 2005 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

#include "v4l2driver.h"

static class Loader
{
        std::unique_ptr<V4L2_Driver> MainCam;
        // Map of Common Name (as detected from query camera capability cap.card) and driver name (used by INDI driver label).
        // V42L Name : Driver Name
        std::map<std::string, std::string> driverMap =
        {
            {"NexImage 5", "NexImage 5"},
            {"UVC Camera (046d:0809)", "Logitech Webcam Pro 9000"},
            {"SVBONY SV105: SVBONY SV105", "SVBONY SV105"},
            {"SVBONY SV205: SVBONY SV205", "SVBONY SV205"},
            {"NexImage 10", "NexImage 10"},
            {"NexImage Burst Color", "NexImage Burst Color"},
            {"NexImage Burst Mono", "NexImage Burst Mono"},
            {"Skyris 132C", "Skyris 132C"},
            {"Skyris 132M", "Skyris 132M"},
            {"Skyris 236C", "Skyris 236C"},
            {"Skyris 236M", "Skyris 236M"},
            {"iOptron iPolar: iOptron iPolar", "iOptron iPolar"},
            {"iOptron iGuider: iOptron iGuide", "iOptron iGuider"},
            {"mmal service 16.1", "Raspberry Pi High Quality Camera"},
            {"UVC Camera (046d:0825)", "Logitech HD C270"},
            {"USB 2.0 Camera: USB Camera", "IMX290 Camera"},
            {"0c45:6366 Microdia", "IMX290 H264 Camera"},
            {"Microsoft® LifeCam Cinema(TM):", "Microsoft LifeCam Cinema"}
        };
    public:
        Loader()
            {
                // We have two structures
                // driverMap is a map of common_name:driver_label
                // devices is a map of common_name:device_path as detected by INDI.
                // Goal is to create a driver with driver_label that opens a device at device_path

                // Enumerate all video devices, we get a map of common_name:device_path
                std::map<std::string, std::string> devices = INDI::V4L2_Base::enumerate();
                // Get environment device name and let's see if this is a generic driver or meant for a specific device.
                const char *envDevice = getenv("INDIDEV");
                auto isDefaultCamera = envDevice == nullptr || !strcmp(envDevice, "V4L2 CCD");

                auto targetDriver = std::make_pair(std::string("V4L2 CCD"), std::string("/dev/video0"));
                // If we are not using default camera, find if any of enumerated devices matches any device in our known
                // driver map
                if (!isDefaultCamera)
                {
                    // Check if the driver is supported.
                    for (const auto &oneDriver : driverMap)
                    {
                        // If we get a match, check if it exists in enumerated devices.
                        if (envDevice == oneDriver.second)
                        {
                            auto match = devices.find(oneDriver.first);
                            if (match != devices.end())
                            {
                                targetDriver = std::make_pair(oneDriver.second, (*match).second);
                                break;
                            }
                        }
                    }
                }

                if (targetDriver.first == "V42L CCD")
                    MainCam.reset(new V4L2_Driver());
                else
                    MainCam.reset(new V4L2_Driver(targetDriver.first.c_str(), targetDriver.second.c_str()));

                MainCam->initCamBase();
            }
    } loader;
