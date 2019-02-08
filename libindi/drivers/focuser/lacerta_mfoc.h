/*******************************************************************************
  Copyright(c) 2018 Franck Le Rhun. All rights reserved.
  Copyright(c) 2018 Christian Liska. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include "indifocuser.h"

/**
 * @brief The FocusSim class provides a simple Focuser simulator that can simulator the following devices:
 * + Absolute Focuser with encoders.
 * + Relative Focuser.
 * + Simple DC Focuser.
 *
 * The focuser type must be selected before establishing connection to the focuser.
 *
 * The driver defines FWHM property that is used in the @ref CCDSim "CCD Simulator" driver to simulate the fuzziness of star images.
 * It can be used to test AutoFocus routines among other applications.
 */
class lacerta_mfoc : public INDI::Focuser
{
  public:
    lacerta_mfoc();
    virtual ~lacerta_mfoc() = default;
    const char *getDefaultName();

    bool initProperties();
    void ISGetProperties(const char *dev);
    bool updateProperties();

    bool Connect();
    bool Disconnect();

    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool SetFocuserSpeed(int speed) override;
    virtual bool saveConfigItems(FILE *fp) override;
    virtual uint32_t GetAbsFocuserPosition();
  private:
    double internalTicks { 0 };
    double initTicks { 0 };
    virtual bool SetBacklash(double values[], char *names[], int n);
    virtual bool SetTempComp(double values[], char *names[], int n);
    virtual bool SetPositionMax(double values[], char *names[], int n);

    // MFOC specific parameters
    // 
    INumberVectorProperty BacklashNP;
    INumber BacklashN[1];
    
    INumberVectorProperty TempCompNP;
    INumber TempCompN[1];
    
    INumberVectorProperty PositionMaxNP;
    INumber PositionMaxN[1];
    
    enum
    {
        MODE_TDIR_BOTH,
        MODE_TDIR_IN,
        MODE_TDIR_OUT,
        MODE_COUNT_TEMP_DIR
    };
    ISwitchVectorProperty TempTrackDirSP;
    ISwitch TempTrackDirS[MODE_COUNT_TEMP_DIR];
    
    enum
    {
        MODE_SAVED_ON,
        MODE_SAVED_OFF,
        MODE_COUNT_SAVED
    };
    ISwitchVectorProperty StartSavedPositionSP;
    ISwitch StartSavedPositionS[MODE_COUNT_SAVED];

    enum
    {
        MODE_ALL,
        MODE_ABSOLUTE,
        MODE_RELATIVE,
        MODE_COUNT
    };
    ISwitchVectorProperty ModeSP;
    ISwitch ModeS[MODE_COUNT];
};
