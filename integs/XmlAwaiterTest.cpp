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

#include <stdexcept>

#include "gtest/gtest.h"

#include "XmlAwaiter.h"

std::string parseXmlFragmentFromString(const std::string & str) {
    ssize_t pos = 0;
    auto lambda = [&pos, &str]()->char {
        return str[pos++];
    };
    return parseXmlFragment(lambda);
}


TEST(XmlAwaiter, SimpleFragment) {
    EXPECT_EQ( parseXmlFragmentFromString("< toto >"),                "<toto>");
    EXPECT_EQ( parseXmlFragmentFromString("<toto\n>"),                "<toto>");
    EXPECT_EQ( parseXmlFragmentFromString("<\ntoto\n>"),              "<toto>");
    EXPECT_EQ( parseXmlFragmentFromString("\n\n<\ntoto\n>"),          "<toto>");

    EXPECT_EQ( parseXmlFragmentFromString("< toto />"),               "<toto/>");
    EXPECT_EQ( parseXmlFragmentFromString("<toto\n/>"),               "<toto/>");
    EXPECT_EQ( parseXmlFragmentFromString("<\ntoto\n/>"),             "<toto/>");
    EXPECT_EQ( parseXmlFragmentFromString("\n\n<\ntoto\n/>"),         "<toto/>");
}

TEST(XmlAwaiter, SimpleAttribute) {
    EXPECT_EQ( parseXmlFragmentFromString("< toto   value='1' >"),    "<toto value='1'>");
    EXPECT_EQ( parseXmlFragmentFromString("<toto\nvalue='1'>"),       "<toto value='1'>");
    EXPECT_EQ( parseXmlFragmentFromString("< toto   value='1' />"),   "<toto value='1'/>");
    EXPECT_EQ( parseXmlFragmentFromString("<toto\nvalue='1'/>"),      "<toto value='1'/>");

    EXPECT_EQ( parseXmlFragmentFromString("< toto   value >"),        "<toto value>");
    EXPECT_EQ( parseXmlFragmentFromString("<toto\nvalue>"),           "<toto value>");
    EXPECT_EQ( parseXmlFragmentFromString("< toto   value />"),       "<toto value/>");
    EXPECT_EQ( parseXmlFragmentFromString("<toto\nvalue/>"),          "<toto value/>");

}

