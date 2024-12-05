/*******************************************************************
Creative Commons Attribution-NonCommercial License

Copyright © 2020-2024 Nathan Woelfle

This work is licensed under a Creative Commons Attribution-NonCommercial 4.0 International License.

You are free to:

    Share — copy and redistribute the material in any medium or format
    Adapt — remix, transform, and build upon the material

Under the following conditions:

    Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
    NonCommercial — You may not use the material for commercial purposes.
    No additional restrictions — You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.

Notices:

    You may not use this work for commercial purposes without written permission from the copyright holder.
    This work is provided "as is" without warranty of any kind, either express or implied, including but not limited to the warranties of merchantability, fitness for a particular purpose, and noninfringement. In no event shall the authors or copyright holders be liable for any claim, damages, or other liability, whether in an action of contract, tort, or otherwise, arising from, out of, or in connection with the software or the use or other dealings in the software.

Scope:

    This license applies to both the hardware and software components of the DarkLight Cover Calibrator.

Modified Versions:

    You are permitted to create modified versions of the DarkLight Cover Calibrator for non-commercial use, provided that you:
        Retain the original copyright notice and license terms.
        Include a clear reference to the original creator (Nathan Woelfle) and provide a link to the original work.

Jurisdiction:

    This license is governed by the laws of the United States of America, and by international copyright laws and treaties.

For more information, please refer to the full terms of the Creative Commons Attribution-NonCommercial 4.0 International License: https://creativecommons.org/licenses/by-nc/4.0/
*******************************************************************/

#pragma once

#include "defaultdevice.h"

namespace Connection
{
class Serial;
}

class DarkLight_CoverCalibrator : public INDI::DefaultDevice
{
    public:
        DarkLight_CoverCalibrator();
        virtual ~DarkLight_CoverCalibrator() override = default;

        virtual const char *getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual void TimerHit() override;

    private:

        //serial communications
        bool Handshake();
        bool sendCommand(const char *command, const char *response);
        int PortFD{-1};

        Connection::Serial *serialConnection{nullptr};

        bool mainValues();
        void setStabilizeTime();
        void setAutoOn();
        void setLightDisabled();
        void getCoverState();
        void getCalibratorState();
        void getBrightness();
        void setBrightness(double BrightnessValue);
        bool lightDisabled;
        bool coverIsMoving;
        bool lightIsReady;
        bool autoOn;

        //define properties
        INDI::PropertyNumber StabilizeTimeNP {1};
        INDI::PropertySwitch AutoOnSP {1};
        INDI::PropertySwitch DisableLightSP {1};
        INDI::PropertyText CoverStateTP {1};
        INDI::PropertySwitch MoveToSP {3};
        enum {Open, Close, Halt};
        INDI::PropertyText CalibratorStateTP {1};
        INDI::PropertySwitch TurnLightSP {2};
        enum {On, Off};
        INDI::PropertyNumber MaxBrightnessNP {1};
        INDI::PropertyNumber CurrentBrightnessNP {1};
        INDI::PropertyNumber GoToValueNP {1};
        INDI::PropertySwitch AdjustValueSP {2};
        enum {Decrease, Increase};
        INDI::PropertySwitch GoToSavedSP {2};
        enum {Broadband, Narrowband};
        INDI::PropertySwitch SetToSavedSP {2};
        enum {Set_Broadband, Set_Narrowband};

    protected:
        virtual bool saveConfigItems(FILE *fp) override;
};
