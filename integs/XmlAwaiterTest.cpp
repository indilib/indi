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

