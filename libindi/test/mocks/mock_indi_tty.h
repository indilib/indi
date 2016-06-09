
#ifndef MOCK_INDI_TTY_H
#define MOCK_INDI_TTY_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "indicompp.h"

class MOCK_TTY : public INDI::TTY
{
public:
    MOCK_TTY();
    virtual ~MOCK_TTY();
    
    MOCK_METHOD0(getPortFD, int());
    MOCK_METHOD5(connect, int(const char *device, int bit_rate, int word_size, int parity, int stop_bits));
    MOCK_METHOD0(disconnect, int());
    MOCK_METHOD4(read, int(char *, int, int, int*));
    MOCK_METHOD4(read_section, int(char *, char, int, int*));
    MOCK_METHOD3(write, int(const char *, int, int*));
    MOCK_METHOD2(write_string, int(const char *, int *));
    MOCK_METHOD3(error_msg, void(int, char *, int));
    MOCK_METHOD1(set_debug, void(int));
    MOCK_METHOD1(timeout, int(int));
    MOCK_METHOD1(tcflush, int(int));
};

#endif
