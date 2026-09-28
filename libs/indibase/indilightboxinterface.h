/*
    Light Box / Switch Interface
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#pragma once

#include "indibase.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertytext.h"

#include <stdint.h>

/**
 * \class LightBoxInterface
   \brief Provides interface to implement controllable light box/switch device.

   Filter durations preset can be defined if the active filter name is set. Once the filter names are retrieved, the duration in seconds can be set for each filter.
   When the filter wheel changes to a new filter, the duration is set accordingly.

   The child class is expected to call the following functions from the INDI frameworks standard functions:

   \e IMPORTANT: initLightBoxProperties() must be called before any other function to initialize the Light device properties.
   \e IMPORTANT: isGetLightBoxProperties() must be called in your driver ISGetProperties function
   \e IMPORTANT: processLightBoxSwitch() must be called in your driver ISNewSwitch function.
   \e IMPORTANT: processLightBoxNumber() must be called in your driver ISNewNumber function.
   \e IMPORTANT: processLightBoxText() must be called in your driver ISNewText function.
\author Jasem Mutlaq
*/

// Alias
using LI = INDI::LightBoxInterface;

namespace INDI
{

class LightBoxInterface
{
    public:
        enum
        {
            FLAT_LIGHT_ON,
            FLAT_LIGHT_OFF
        };

        enum
        {
            CAN_DIM = 1 << 0,             /** Does it support dimming? */
            CAN_BRIGHTNESS_MODE = 1 << 1, /** Does it support a device-side Low/High brightness mode? */
        } LightBoxCapability;

        // Brightness mode presets used by BrightnessModeSP/FilterBrightnessModeSP
        enum
        {
            BRIGHTNESS_MODE_LOW,
            BRIGHTNESS_MODE_HIGH
        };

    protected:
        LightBoxInterface(DefaultDevice *device);
        virtual ~LightBoxInterface();

        /** \brief Initialize light box properties. It is recommended to call this function within initProperties() of your primary device
                \param group Group or tab name to be used to define light box properties.
                \param capabilities Light box capabilities
            */
        void initProperties(const char *group, uint32_t capabilities);

        /**
             * @brief isGetLightBoxProperties Get light box properties
             * @param deviceName parent device name
             */
        void ISGetProperties(const char *deviceName);

        /** \brief Process light box switch properties */
        bool processSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

        /** \brief Process light box number properties */
        bool processNumber(const char *dev, const char *name, double values[], char *names[], int n);

        /** \brief Process light box text properties */
        bool processText(const char *dev, const char *name, char *texts[], char *names[], int n);

        bool updateProperties();
        bool saveConfigItems(FILE *fp);
        bool snoop(XMLEle *root);

        /**
             * @brief GetCapability returns the light box capabilities, see LightBoxCapability.
             */
        uint32_t GetCapability() const
        {
            return m_Capabilities;
        }

        /**
             * @brief SetCapability updates the light box capabilities. Useful when a capability (e.g.
             * CAN_BRIGHTNESS_MODE) is only known once the device has been connected to, i.e. after the
             * initial initProperties() call.
             * @param cap capability bitmask, see LightBoxCapability.
             */
        void SetCapability(uint32_t cap)
        {
            m_Capabilities = cap;
        }

        /**
             * @brief setBrightness Set light level. Must be implemented in the child class, if supported.
             * @param value level of light box
             * @return True if successful, false otherwise.
             */
        virtual bool SetLightBoxBrightness(uint16_t value);

        /**
             * @brief EnableLightBox Turn on/off on a light box. Must be implemented in the child class.
             * @param enable If true, turn on the light, otherwise turn off the light.
             * @return True if successful, false otherwise.
             */
        virtual bool EnableLightBox(bool enable);

        /**
             * @brief SetLightBoxBrightnessMode Set the device-side Low/High brightness mode. Must be
             * implemented in the child class if CAN_BRIGHTNESS_MODE is supported.
             * @param mode BRIGHTNESS_MODE_LOW or BRIGHTNESS_MODE_HIGH
             * @return True if successful, false otherwise.
             */
        virtual bool SetLightBoxBrightnessMode(int mode);

        // Turn on/off light
        INDI::PropertySwitch LightSP {2};

        // Light Intensity
        INDI::PropertyNumber LightIntensityNP {1};

        // Active devices to snoop
        INDI::PropertyText ActiveDeviceTP {1};

        INDI::PropertyNumber FilterIntensityNP {0};

        // Device-side Low/High brightness mode, and one independent per-filter preset
        // switch (On = High, Off = Low), populated dynamically alongside FilterIntensityNP
        // once filter names are known. Only used when CAN_BRIGHTNESS_MODE is set.
        INDI::PropertySwitch BrightnessModeSP {2};
        INDI::PropertySwitch FilterBrightnessModeSP {0};

    private:
        void addFilterDuration(const char *filterName, uint16_t filterDuration);

        DefaultDevice *m_DefaultDevice {nullptr};
        uint8_t currentFilterSlot {0};
        // Set once a real FILTER_SLOT snoop has been received, so a FilterBrightnessModeSP
        // toggle isn't mistaken as applying to filter slot 0 before the active slot is known.
        bool m_FilterSlotKnown {false};
        uint32_t m_Capabilities {0};
};
}
