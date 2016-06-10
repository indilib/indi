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

#include "mocks/mock_indi_tty.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "drivers/dome/baader_dome.h"

TEST(DOME_BAADER_getDefaultName, getDefaultName)
{
    MOCK_TTY  *p_mock_tty = new MOCK_TTY;
    std::unique_ptr<BaaderDome> dome(new BaaderDome(p_mock_tty));
    const char *p_actual = dome->getDefaultName();
    ASSERT_STREQ("Baader Dome", p_actual);
}

