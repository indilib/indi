/*
    Copyright (C) 2004 by Jasem Mutlaq

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

#ifndef _PPort_hpp_
#define _PPort_hpp_

class port_t;

/** To access and share the parallel port
    between severa objects. */
class PPort {
public:
   PPort();
   PPort(int ioPort);
   ~PPort();
   /** set the ioport associated to the // port.
       \return true if the binding was possible
   */
   bool setPort(int ioPort);
   /** set a data bit of the // port.
       \return false if IO port not set, or bit not registered by ID.
       \param ID the identifier used to register the bit
       \param stat the stat wanted for the given bit
       \param bit the bit to set. They are numbered from 0 to 7.

   */
   bool setBit(const void * ID,int bit,bool stat);
   /** register a bit for object ID.
       \param ID the identifier used to register the bit it should be the 'this' pointer.
       \param bit the bit of the // port to register
       \return false if the bit is already register with an
               other ID, or if the // port is not initialised.
   */
   bool registerBit(const void * ID,int bit);
   /** release a bit.
       \param ID the identifier used to register the bit
       \param bit the bit to register.
       \return false if the bit was not registered by ID or
               if the if the // port is not initialised.
   */
   bool unregisterBit(const void * ID,int bit);

   /** test if a bit is registerd.
       \param ID the identifier used to register the bit
       \param bit the bit to test.
       \return false if the bit was not registered by ID or
               if the if the // port is not initialised.
   */
   bool isRegisterBit(const void * ID,int bit) const;
   
   /** set the bits off the // port according to previous call to setBit().
    */
   bool commit();
private:
   void reset();
   unsigned char bitArray;
   const void * assignedBit[8];
   port_t * currentPort;
};
#endif
