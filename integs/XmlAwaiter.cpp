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

