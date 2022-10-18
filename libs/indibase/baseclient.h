/*******************************************************************************
  Copyright(c) 2011 Jasem Mutlaq. All rights reserved.
               2022 Ludovic Pollet

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

#ifndef SWIG
#include "abstractbaseclient.h"
#else
%include "abstractbaseclient.h"
#endif

/** @class INDI::BaseClient
 *  @brief Class to provide basic client functionality.
 *
 *  BaseClient enables accelerated development of INDI Clients by providing a framework that facilitates communication, device
 *  handling, and event notification. By subclassing BaseClient, clients can quickly connect to an INDI server, and query for
 *  a set of INDI::BaseDevice devices, and read and write properties seamlessly. Event driven programming is possible due to
 *  notifications upon reception of new devices or properties.
 *
 *  Upon connecting to an INDI server, it creates a dedicated thread to handle all incoming traffic. The thread is terminated
 *  when disconnectServer() is called or when a communication error occurs.
 *
 *  @attention All notifications functions defined in INDI::BaseMediator <b>must</b> be implemented in the client class even if
 *  they are not used because these are pure virtual functions.
 *
 *  @see <a href="http://indilib.org/develop/tutorials/107-client-development-tutorial.html">INDI Client Tutorial</a> for more details.
 *  @author Jasem Mutlaq
 *  @author Ludovic Pollet
 */

namespace INDI
{
class BaseClientPrivate;
}
class INDI::BaseClient : public INDI::AbstractBaseClient
{
        DECLARE_PRIVATE_D(d_ptr_indi, BaseClient)

    public:
        BaseClient();
        virtual ~BaseClient();

    public:
        /** @brief Connect to INDI server.
         *  @returns True if the connection is successful, false otherwise.
         *  @note This function blocks until connection is either successull or unsuccessful.
         */
        bool connectServer() override;

        /** @brief Disconnect from INDI server.
         *         Any devices previously created will be deleted and memory cleared.
         *  @return True if disconnection is successful, false otherwise.
         */
        bool disconnectServer(int exit_code = 0) override;
};
