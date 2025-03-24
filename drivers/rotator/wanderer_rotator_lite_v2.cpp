/*******************************************************************************
  Copyright(c) 2025 Frank Wang & Jérémie Klein. All rights reserved.

  WandererRotator Lite V2

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

#include "wanderer_rotator_lite_v2.h"

// We declare an auto pointer to WandererRotatorLiteV2.
static std::unique_ptr<WandererRotatorLiteV2> wandererrotatorlitev2(new WandererRotatorLiteV2());

WandererRotatorLiteV2::WandererRotatorLiteV2(){
    setVersion(1, 0);
}

const char *WandererRotatorLiteV2::getDefaultName(){
    return "WandererRotator Lite V2";
}

const char *WandererRotatorLiteV2::getRotatorHandshakeName() {
    return "WandererRotatorLiteV2";
}

int WandererRotatorLiteV2::getMinimumCompatibleFirmwareVersion() {
    return 20240226;
}

int WandererRotatorLiteV2::getStepsPerDegree() {
    return 1199;
}
