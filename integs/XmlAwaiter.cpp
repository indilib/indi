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
#include "XmlAwaiter.h"

enum XmlStatus { PRE, WAIT_TAGNAME, TAGNAME, WAIT_ATTRIB, ATTRIB, QUOTE, WAIT_CLOSE };

std::string parseXmlFragment(std::function<char()> readchar) {

    XmlStatus state = PRE;
    std::string received = "";
    std::string xmlFragment = "";
    char lastQuote = '\0';
    while(true) {
        char c = readchar();
        received += c;

        bool isSpace = c == '\n' || c == '\r' || c== '\t' || c == ' ';
        switch (state) {
            case PRE:
                if (isSpace) {
                    continue;
                }
                if (c == '<') {
                    xmlFragment += c;
                    state = WAIT_TAGNAME;
                    continue;
                }
                throw std::runtime_error("Invalid xml fragment: " + received);
                break;
            case WAIT_TAGNAME:
                if (c == '/') {
                    xmlFragment += c;
                    continue;
                }
                if (isSpace) {
                    continue;
                }
                xmlFragment += c;
                state = TAGNAME;
                continue;
            case TAGNAME:
                if (isSpace) {
                    state = WAIT_ATTRIB;
                    continue;
                }
                if (c == '/') {
                    state = WAIT_CLOSE;
                    xmlFragment += c;
                    continue;
                }
                if (c == '>') {
                    xmlFragment += c;
                    break;
                }
                xmlFragment += c;
                continue;
            case WAIT_ATTRIB:
                if (isSpace) {
                    continue;
                }
                if (c == '/') {
                    state = WAIT_CLOSE;
                    xmlFragment += c;
                    continue;
                }
                if (c == '>') {
                    xmlFragment += c;
                    break;
                }
                xmlFragment += ' ';
                xmlFragment += c;
                state = ATTRIB;
                continue;
            case ATTRIB:
                if (isSpace) {
                    state = WAIT_ATTRIB;
                    continue;
                }
                if (c == '/') {
                    xmlFragment += "/";
                    state = WAIT_CLOSE;
                    continue;
                }
                if (c == '>') {
                    xmlFragment += '>';
                    break;
                }

                if (c == '"' || c=='\'') {
                    lastQuote = c;
                    state = QUOTE;
                    xmlFragment += "'";
                    continue;
                }
                xmlFragment += c;
                continue;
            case QUOTE:
                if (c == lastQuote) {
                    xmlFragment += "'";
                    state = WAIT_ATTRIB;
                    continue;
                }
                xmlFragment += c;
                continue;
            case WAIT_CLOSE:
                if (c == '>') {
                    xmlFragment += c;
                    break;
                }
                throw std::runtime_error("Invalid xml fragment: " + received);
        }
        return xmlFragment;
    }
}

