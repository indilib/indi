/*******************************************************************************
  Copyright(c) 2025 Frank Wang & Jérémie Klein. All rights reserved.

  WandererRotator Pro V2

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

#include "wanderer_rotator_pro_v2.h"

// We declare an auto pointer to WandererRotatorProV2.
static std::unique_ptr<WandererRotatorProV2> wandererrotatorprov2(new WandererRotatorProV2());

WandererRotatorProV2::WandererRotatorProV2(){
    setVersion(1, 0);
}

const char *WandererRotatorProV2::getDefaultName(){
    return "WandererRotator Pro V2";
}

const char *WandererRotatorProV2::getRotatorHandshakeName() {
    return "WandererRotatorProV2";
}

int WandererRotatorProV2::getMinimumCompatibleFirmwareVersion() {
    return 20261025;
}

int WandererRotatorProV2::getLatestFirmwareVersion() {
    return 20261025;
}

int WandererRotatorProV2::getStepsPerDegree() {
    return 1349;
}

int WandererRotatorProV2::getMovePositionOffset() {
    return 0;
}
