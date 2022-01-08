#ifndef XML_FRAGMENT_READER_H_
#define XML_FRAGMENT_READER_H_ 1

#include <string>
#include <functional>

std::string parseXmlFragment(std::function<char()> readchar);

#endif // XML_FRAGMENT_READER_H_
