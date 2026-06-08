/*******************************************************************************
  Copyright(c) 2026 Makoto Kasahara All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file LICENSE.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Minimal serial helper for MX-HD protocol:
// - Most commands are LX200-style ":xxx#" with optional "#"-terminated ASCII response.
// - Status "@ST#" returns exactly 3 binary bytes (no terminator).
class MXHDSerial
{
public:
    explicit MXHDSerial(int fd);

    // Drop unread bytes in RX buffer to avoid stale-response desync.
    bool discardInput(std::string &err);

    // Write command bytes as-is (no extra terminators).
    bool writeRaw(const void *data, size_t len, std::string &err);
    bool writeString(const std::string &s, std::string &err);

    // Read until '#', return string without '#'. Timeout is in milliseconds.
    bool readHashTerminated(std::string &out, int timeoutMs, std::string &err);

    // Read exactly N bytes (binary). Timeout is in milliseconds for the whole read.
    bool readExact(std::vector<uint8_t> &out, size_t n, int timeoutMs, std::string &err);

    // Read a single byte. Timeout is in milliseconds.
    bool readChar(char &ch, int timeoutMs, std::string &err);

    // Convenience: send command (expects '#'-terminated response)
    bool queryHash(const std::string &cmd, std::string &response, int timeoutMs, std::string &err);

    // Convenience: send command and read one-char ACK response.
    bool queryAck(const std::string &cmd, char &ack, int timeoutMs, std::string &err);

    // Convenience: send status command (@ST#) and read 3 bytes.
    bool queryStatus3(const std::string &cmd, uint8_t &b1, uint8_t &b2, uint8_t &b3, int timeoutMs, std::string &err);

private:
    int m_fd;
};
