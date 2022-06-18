#if 0
INDI Driver Functions

Copyright (C) 2022 by Ludovic Pollet

This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

/* A driverio struct is valid only for sending one xml message */
typedef struct driverio{
    struct userio userio;
    void * user;
    void ** joins;
    size_t * joinSizes;
    int joinCount;
    int locked;
    char * outBuff;
    unsigned int outPos;
} driverio;

void driverio_init(driverio * dio);
void driverio_finish(driverio * dio);
