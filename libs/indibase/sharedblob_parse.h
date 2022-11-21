/*******************************************************************************
 Copyright(c) 2022 Ludovic Pollet. All rights reserved.

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

#include <vector>
#include <string>

namespace INDI
{

// Allocate a new uuid for this blob content
std::string allocateBlobUid(int fd);

// Release the blob for which uid has not been attached
void releaseBlobUids(const std::vector<std::string> &uids);

// Attach the given blob buffer and release it's uid
void * attachBlobByUid(const std::string &uid, size_t size);

}
