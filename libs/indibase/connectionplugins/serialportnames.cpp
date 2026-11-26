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

#include "serialportnames.h"

#ifdef __linux__
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <regex>
#include <sys/stat.h>
#endif

namespace Connection
{
namespace SerialPorts
{
#ifdef __linux__
namespace
{
const char *BY_ID = "/dev/serial/by-id/";
const char *BY_PATH = "/dev/serial/by-path/";

// The USB device behind a tty.
struct UsbAdapter
{
    bool valid {false};
    std::string type;    // idVendor:idProduct
    std::string serial;  // empty if the adapter reports none
    std::string product;
    std::string socket;  // USB device path, e.g. 3-1.2.1
    std::string key;     // type, serial and interface: equal for adapters udev can't tell apart by id
};

std::string canonical(const std::string &path)
{
    char resolved[PATH_MAX];
    return realpath(path.c_str(), resolved) ? resolved : "";
}

std::string baseName(const std::string &path)
{
    return path.substr(path.find_last_of('/') + 1);
}

bool startsWith(const std::string &text, const std::string &prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}

std::string readLine(const std::string &path)
{
    std::ifstream in(path);
    std::string value;
    std::getline(in, value);
    value.erase(value.find_last_not_of(" \n\r\t") + 1);
    return value;
}

bool isNodeName(const std::string &name)
{
    static const std::regex nodeRe("^tty(USB|ACM)[0-9]+$");
    return std::regex_match(name, nodeRe);
}

std::vector<std::string> entries(const std::string &dir)
{
    std::vector<std::string> names;
    struct dirent **list;
    int count = scandir(dir.c_str(), &list, nullptr, alphasort);
    for (int i = 0; i < count; i++)
    {
        if (list[i]->d_name[0] != '.')
            names.push_back(list[i]->d_name);
        free(list[i]);
    }
    if (count >= 0)
        free(list);
    return names;
}

// Walk up from the tty device in sysfs to the USB device that owns it.
UsbAdapter usbAdapter(const std::string &port, const std::string &root)
{
    UsbAdapter adapter;
    const std::string tty = baseName(canonical(port));
    const std::string sys = canonical(root + "/sys");
    if (!isNodeName(tty) || sys.empty())
        return adapter;

    std::string interface;
    for (std::string dir = canonical(root + "/sys/class/tty/" + tty + "/device");
            dir.size() > sys.size() && startsWith(dir, sys); dir = dir.substr(0, dir.find_last_of('/')))
    {
        if (interface.empty())
            interface = readLine(dir + "/bInterfaceNumber");
        const std::string vendor = readLine(dir + "/idVendor");
        if (vendor.empty())
            continue;
        adapter.valid = true;
        adapter.type = vendor + ":" + readLine(dir + "/idProduct");
        adapter.serial = readLine(dir + "/serial");
        adapter.product = readLine(dir + "/product");
        adapter.socket = baseName(dir);
        adapter.key = adapter.type + ":" + adapter.serial + ":" + interface;
        break;
    }
    return adapter;
}

// The kernel device nodes of all USB serial ports.
std::vector<std::string> nodes(const std::string &root)
{
    std::vector<std::string> result;
    for (const auto &name : entries(root + "/dev"))
        if (isNodeName(name) && isKernelNode(root + "/dev/" + name))
            result.push_back(root + "/dev/" + name);
    return result;
}

// A link in dir that points to target; links containing "avoid" only if there is no other.
std::string linkTo(const std::string &dir, const std::string &target, const std::string &avoid = "")
{
    std::string fallback;
    for (const auto &name : entries(dir))
    {
        if (canonical(dir + name) != target)
            continue;
        if (avoid.empty() || name.find(avoid) == std::string::npos)
            return dir + name;
        if (fallback.empty())
            fallback = dir + name;
    }
    return fallback;
}
}

std::vector<std::string> list(const std::string &root)
{
    std::vector<std::string> result;
    for (const auto &node : nodes(root))
        result.push_back(stableName(node, root));
    return result;
}

std::string stableName(const std::string &port, const std::string &root)
{
    const std::string target = canonical(port);
    const UsbAdapter adapter = usbAdapter(port, root);
    if (!adapter.valid)
        return port;

    if (!adapter.serial.empty())
    {
        const auto all = nodes(root);
        const auto twins = std::count_if(all.begin(), all.end(), [&](const std::string & node)
        {
            return usbAdapter(node, root).key == adapter.key;
        });
        const std::string byId = twins == 1 ? linkTo(root + BY_ID, target) : "";
        if (!byId.empty())
            return byId;
    }

    // udev creates by-path links with and without the USB revision ("-usbv2-"), prefer the plain one.
    const std::string byPath = linkTo(root + BY_PATH, target, "-usbv");
    return byPath.empty() ? port : byPath;
}

std::string nameToSave(const std::string &port, const std::string &root)
{
    return (isKernelNode(port) || startsWith(port, root + BY_ID)) ? stableName(port, root) : port;
}

std::string label(const std::string &port, const std::string &root)
{
    if (!startsWith(port, root + BY_PATH))
        return "";
    const UsbAdapter adapter = usbAdapter(port, root);
    return adapter.valid ? adapter.product + " @ USB " + adapter.socket : "";
}

std::string adapterType(const std::string &port, const std::string &root)
{
    return usbAdapter(port, root).type;
}

bool sameDevice(const std::string &a, const std::string &b)
{
    const std::string target = canonical(a);
    return !target.empty() && target == canonical(b);
}

bool isKernelNode(const std::string &port)
{
    struct stat st;
    return isNodeName(baseName(port)) && lstat(port.c_str(), &st) == 0 && !S_ISLNK(st.st_mode);
}

std::vector<std::string> searchOrder(const std::vector<std::string> &ports, const std::string &configuredPort,
                                     const std::string &knownType, const std::string &root)
{
    const std::string type = knownType.empty() ? adapterType(configuredPort, root) : knownType;
    const bool byPath = startsWith(configuredPort, root + BY_PATH);
    std::vector<std::string> ordered = ports;
    if (!type.empty())
        std::stable_partition(ordered.begin(), ordered.end(), [&](const std::string & port)
    {
        return adapterType(port, root) == type;
    });
    else if (byPath)
        std::stable_partition(ordered.begin(), ordered.end(), [&](const std::string & port)
    {
        return startsWith(port, root + BY_PATH);
    });
    return ordered;
}

#else

std::vector<std::string> list(const std::string &)
{
    return {};
}

std::string stableName(const std::string &port, const std::string &)
{
    return port;
}

std::string nameToSave(const std::string &port, const std::string &)
{
    return port;
}

std::string label(const std::string &, const std::string &)
{
    return "";
}

std::string adapterType(const std::string &, const std::string &)
{
    return "";
}

bool sameDevice(const std::string &a, const std::string &b)
{
    return a == b;
}

bool isKernelNode(const std::string &)
{
    return false;
}

std::vector<std::string> searchOrder(const std::vector<std::string> &ports, const std::string &, const std::string &,
                                     const std::string &)
{
    return ports;
}

#endif
}
}
