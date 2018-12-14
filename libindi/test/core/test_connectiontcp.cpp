/*******************************************************************************
 Copyright(c) 2016 Norbert Szulc. All rights reserved.
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

#include <gtest/gtest.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../../libs/indibase/connectionplugins/connectiontcp.h"
#include "../../libs/indibase/defaultdevice.h"

TEST(CORE_CONNECTIONPLUGINS, Test_connectionTcpDefaults) {
    INDI::DefaultDevice dev = INDI::BaseDevice();
    Connection::TCP tcpConnection = Connection::TCP(&dev);

    tcpConnection.setDefaultHost("192.168.0.1");
    EXPECT_EQ("192.168.0.1", tcpConnection.host());
    tcpConnection.setDefaultPort(2000);
    EXPECT_EQ(2000, tcpConnection.port());
}

z


