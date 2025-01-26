/*******************************************************************************
  Copyright(c) 2025 Frank Wang & Jérémie Klein. All rights reserved.

  WandererRotator Mini V1/V2

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "wanderer_rotator_mini.h"
#include "indicom.h"
#include "connectionplugins/connectionserial.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <sstream>
#include <iostream>


// We declare an auto pointer to WandererRotatorMini.
static std::unique_ptr<WandererRotatorMini> wandererrotatormini(new WandererRotatorMini());

WandererRotatorMini::WandererRotatorMini(){
    setVersion(1, 1);
}

const char *WandererRotatorMini::getDefaultName(){
    return "WandererRotator Mini";
}

const char *WandererRotatorMini::getRotatorHandshakeName() {
    return "WandererRotatorMini";
}

int WandererRotatorMini::getMinimumCompatibleFirmwareVersion() {
    return 20240226;
}

int WandererRotatorMini::getStepsPerDegree() {
    return 1142;
}
