/*******************************************************************************
 Copyright(c) 2016 Andy Kirkham. All rights reserved.

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

#include <string>
#include <vector>
#include <sstream>

#define INDI_CAP_STDERR_BEBIN \
    std::stringstream strerr_buffer;\
    std::streambuf *sbuf = std::cerr.rdbuf();\
    std::cerr.rdbuf(strerr_buffer.rdbuf())
    
#define INDI_CAP_STDERR_END \
    std::cerr.rdbuf(sbuf)

#define INDI_CAP_STDERR_PRINT \
    do {\
        std::string tok;\
        while(std::getline(strerr_buffer, tok, '\n')) {\
            std::cout << "[   stderr ] " << tok << std::endl;\
        };\
    } while(0)

