/*******************************************************************************
  Copyright(c) 2026 Andreas R.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#pragma once

#include <string>
#include <vector>

/**
 * Names of USB serial ports that stay the same across reboots (Linux), used by Connection::Serial.
 *
 * The kernel numbers /dev/ttyUSB<n> and /dev/ttyACM<n> in the order the adapters are found, which can change at
 * every boot. udev adds /dev/serial/by-id/ links (vendor, product and serial number) and /dev/serial/by-path/
 * links (USB socket). Adapters without a unique serial number (e.g. two CH340 chips) share one by-id link, which
 * points to whichever was found last, so only by-path tells them apart.
 *
 * On other systems all functions return the port unchanged or nothing.
 * @a root is a prefix of /dev and /sys for unit tests, empty on a real system.
 */
namespace Connection
{
namespace SerialPorts
{
/** All USB serial ports, each under its stable name. */
std::vector<std::string> list(const std::string &root = "");

/**
 * The stable name of a USB serial port: its by-id link if the adapter reports a serial number that no other
 * connected adapter shares, else its by-path link, else @a port.
 */
std::string stableName(const std::string &port, const std::string &root = "");

/**
 * The name to save after a successful connection: the stable name for a kernel device node or a by-id link shared
 * by identical adapters, @a port itself for anything else (custom udev names, by-path links, Bluetooth, ...).
 */
std::string nameToSave(const std::string &port, const std::string &root = "");

/** Label for a by-path port, e.g. "USB Serial @ USB 3-1.2.1"; empty for any other port. */
std::string label(const std::string &port, const std::string &root = "");

/** USB vendor and product ID of the adapter behind a port, e.g. "1a86:7523"; empty if unknown. */
std::string adapterType(const std::string &port, const std::string &root = "");

/** True if both names lead to the same device. */
bool sameDevice(const std::string &a, const std::string &b);

/** True for a kernel device node like /dev/ttyUSB0 (not a symlink to one). */
bool isKernelNode(const std::string &port);

/**
 * Order in which an auto search tries @a ports: ports with the same adapter type first (@a knownType, else the
 * type of the adapter now at @a configuredPort). If neither is known and @a configuredPort is a by-path link (a
 * device without a unique serial number), the other by-path ports first. Each group keeps its order.
 */
std::vector<std::string> searchOrder(const std::vector<std::string> &ports, const std::string &configuredPort,
                                     const std::string &knownType, const std::string &root = "");
}
}
