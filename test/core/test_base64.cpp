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

#include <gtest/gtest.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <cstring>

#include "base64.h"

TEST(CORE_BASE64, Test_to64frombits)
{
    const char   inp_msg[] = "FOOBARBAZ";
    const size_t inp_len   = sizeof(inp_msg) - 1;

    const char   out_msg[] = "Rk9PQkFSQkFa";
    const size_t out_len   = sizeof(out_msg) - 1;

    char   res_msg[out_len + 1] = {0,};
    size_t res_len = 0;

    res_len = to64frombits_s(
        reinterpret_cast<unsigned char *>(res_msg),
        reinterpret_cast<const unsigned char *>(inp_msg),
        inp_len,
        out_len
    );
    ASSERT_EQ(out_len, res_len);
    ASSERT_STREQ(out_msg, res_msg);
}

TEST(CORE_BASE64, Test_from64tobits)
{
    const char   inp_msg[] = "Rk9PQkFSQkFa";
    // const size_t inp_len   = sizeof(inp_msg) - 1;

    const char   out_msg[] = "FOOBARBAZ";
    const size_t out_len   = sizeof(out_msg) - 1;

    char   res_msg[out_len + 1] = {0,};
    size_t res_len = 0;

    res_len = from64tobits(res_msg, inp_msg);
    ASSERT_EQ(out_len, res_len);
    ASSERT_STREQ(out_msg, res_msg);
}

TEST(CORE_BASE64, Test_from64tobits_fast)
{
    const char   inp_msg[] = "Rk9PQkFSQkFa";
    const size_t inp_len   = sizeof(inp_msg) - 1;

    const char   out_msg[] = "FOOBARBAZ";
    const size_t out_len   = sizeof(out_msg) - 1;

    char   res_msg[out_len + 1] = {0,};
    size_t res_len = 0;

    res_len = from64tobits_fast(res_msg, inp_msg, inp_len);
    ASSERT_EQ(out_len, res_len);
    ASSERT_STREQ(out_msg, res_msg);
}
