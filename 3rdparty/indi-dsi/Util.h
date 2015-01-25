/*
 * Copyright (c) 2009, Interactive Data Corporation
 */

#ifndef __Util_hh
#define __Util_hh

#include <memory>
#include <usb.h>

#include <vector>
#include <string>

std::vector<std::string> tokenize_str(const std::string & str, const std::string &delims=", \t");

#endif /* __Util_hh */

