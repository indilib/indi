/*
    OnStepX INDI Driver — driver version
    Copyright (C) 2026 Ed Lee

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

// Reported in DRIVER_INFO by every OnStepX device: the mount, the aux
// controller and their focuser, rotator and power children.  drivers.xml
// lists indi_onstepx and indi_onstepx_aux with the same version; keep them
// in step (onstepx_conformance.py --check-source fails if they differ).
constexpr int ONSTEPX_VERSION_MAJOR = 0;
constexpr int ONSTEPX_VERSION_MINOR = 1;
