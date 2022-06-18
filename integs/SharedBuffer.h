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

#ifndef SHAREDBUFFER_H_
#define SHAREDBUFFER_H_ 1

class SharedBuffer
{
        int fd;
        ssize_t size;

    public:
        SharedBuffer();
        ~SharedBuffer();

        int getFd() const;

        void allocate(ssize_t size);
        void write(const void * buffer, ssize_t offset, ssize_t size);
        void release();

        void attach(int fd);

        ssize_t getSize() const
        {
            return size;
        }
};

#endif // SHAREDBUFFER_H_




