
#pragma once

#include <string>
#include <vector>
#include <sstream>

#define INDI_CAP_STDERR_BEBIN \
    std::stringstream strerr_buffer;\
    std::streambuf *sbuf = std::cerr.rdbuf();\
    std::cerr.rdbuf(strerr_buffer.rdbuf())
    
#define INDI_CAP_STDERR_END \
    std::cerr.rdbuf(sbuf)

#define INDI_CAP_STDERR_PRINT \
    do {\
        std::string tok;\
        while(std::getline(strerr_buffer, tok, '\n')) {\
            std::cout << "[   stderr ] " << tok << std::endl;\
        };\
    } while(0)

