/*
 * mgencommand.h
 *
 *  Created on: 21 janv. 2017
 *      Author: TallFurryMan
 */

#ifndef MGENCOMMAND_HPP_
#define MGENCOMMAND_HPP_

#include <iostream>
#include <vector>
#include <string>
#include <libftdi1/ftdi.h>

#include "mgenautoguider.h"

#define _L(msg, ...) INDI::Logger::getInstance().print(root.getDeviceName(), INDI::Logger::DBG_SESSION, __FILE__, __LINE__, "%s: " msg, __FUNCTION__, __VA_ARGS__)

#define CHK_W(bytes, call) { bytes = (call) ; if(bytes < 0) { _L("failed writing request to device (%d)", bytes); return CR_IO_ERROR; } }
#define CHK_R(bytes, call) { bytes = (call) ; if(bytes < 0) { _L("failed reading request from device (%d)", bytes); return CR_IO_ERROR; } }

class MGC
{
protected:
    MGenAutoguider& root;

public:
    enum CommandResult
    {
        CR_SUCCESS,
        CR_FAILURE,
        CR_IO_ERROR,
    };

    class IOError: std::exception
    {
    protected:
        std::string const _what;
    public:
        virtual const char * what() const noexcept { return _what.c_str(); }
        IOError(int code): std::exception(), _what(std::string("I/O error code ") + std::to_string(code)) {};
    };

public:
    virtual enum MGenAutoguider::CommandByte commandByte() const = 0;
    virtual enum MGenAutoguider::OpMode opMode() const = 0;
    virtual CommandResult ask(struct ftdi_context * const ftdi) throw (IOError) = 0;

public:
    typedef std::vector<unsigned char> IOBuffer;

public:
    IOBuffer query;
    IOBuffer answer;

public:
    unsigned char opCode() const { return root.getOpCode(commandByte()); }
    char const * name() const { return root.getOpCodeString(commandByte()); }

protected:
    MGenAutoguider::OpMode current_mode() const { return root.connectionStatus.mode; }

public:
    virtual int write(struct ftdi_context * const ftdi) throw (IOError)
    {
        if(!ftdi)
            return -1;

        if(opMode() != MGenAutoguider::OPM_UNKNOWN && opMode() != root.connectionStatus.mode)
        {
            _L("operating mode '%s' doesn't support command '%s'", root.getOpModeString(opMode()), name());
            return -1;
        }

        _L("writing '%s' - %d bytes to device: %02X %02X %02X %02X %02X ...", name(), query.size(), query.size()>0?query[0]:0, query.size()>1?query[1]:0, query.size()>2?query[2]:0, query.size()>3?query[3]:0, query.size()>4?query[4]:0);
        int const bytes_written = ftdi_write_data(ftdi, query.data(), query.size());

        /* FIXME: Adjust this to optimize how long device absorb the command for */
        usleep(10000);

        if(bytes_written < 0)
            throw IOError(bytes_written);

        return bytes_written;
    }

public:
    virtual int read(struct ftdi_context * const ftdi) throw (IOError)
    {
        if(!ftdi)
            return -1;

        if(opMode() != MGenAutoguider::OPM_UNKNOWN && opMode() != root.connectionStatus.mode)
        {
            _L("operating mode '%s' doesn't support command '%s'", root.getOpModeString(opMode()), name());
            return -1;
        }

        if(answer.size() > 0)
        {
            _L("reading %d bytes from device", answer.size());
            int const bytes_read = ftdi_read_data(ftdi, answer.data(), answer.size());

            if(bytes_read < 0)
                throw IOError(bytes_read);

            _L("read %d bytes from device: %02X %02X %02X %02X %02X ...", bytes_read, answer.size()>0?answer[0]:0, answer.size()>1?answer[1]:0, answer.size()>2?answer[2]:0, answer.size()>3?answer[3]:0, answer.size()>4?answer[4]:0);
            return bytes_read;
        }

        return 0;
    }

protected:
    MGC( MGenAutoguider& root, IOBuffer query, IOBuffer answer ):
        root(root),
        query(query),
        answer(answer) {};

    virtual ~MGC() {};
};

class MGCP_QUERY_DEVICE: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGCP_QUERY_DEVICE; }
    virtual enum MGenAutoguider::OpMode opMode() const { return MGenAutoguider::OPM_UNKNOWN; }

public:
    MGC::CommandResult ask(struct ftdi_context * const ftdi) throw (IOError)
    {
        write(ftdi);

        int const bytes_read = read(ftdi);
        if(answer[0] == (unsigned char) ~query[0] && 5 == bytes_read)
        {
            _L("device acknowledged identification, analyzing '%02X%02X%02X'", answer[2], answer[3], answer[4]);

            IOBuffer const app_mode_answer { ~query[0], 3, 0x01, 0x80, 0x02 };
            IOBuffer const boot_mode_answer { ~query[0], 3, 0x01, 0x80, 0x01 };

            if( answer != app_mode_answer && answer != boot_mode_answer )
            {
                _L("device identification returned unknown mode","");
                return CR_FAILURE;
            }

            _L("identified boot/compatible mode", "");
            /* TODO: indicate boot/compatible to caller */
            return CR_SUCCESS;
        }

        _L("invalid identification response from device (%d bytes read)", bytes_read);
        return CR_FAILURE;
    }

public:
    MGCP_QUERY_DEVICE( MGenAutoguider& root ):
        MGC(root, IOBuffer { root.getOpCode(commandByte()), 1, 1 }, IOBuffer (5) ) {};
};


class MGCMD_NOP1: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGCMD_NOP1; }
    virtual enum MGenAutoguider::OpMode opMode() const { return MGenAutoguider::OPM_APPLICATION; }

public:
    MGC::CommandResult ask(struct ftdi_context * const ftdi) throw (IOError)
    {
        write(ftdi);

        int const bytes_read = read(ftdi);
        if(answer[0] == query[0] && 1 == bytes_read)
        {
            _L("received heartbeat ack", "");
            return CR_SUCCESS;
        }

        _L("no heartbeat ack (%d bytes read)", bytes_read);
        return CR_FAILURE;
    }

public:
    MGCMD_NOP1( MGenAutoguider& root ):
        MGC(root, IOBuffer { root.getOpCode(commandByte()) }, IOBuffer (1) ) {};
};


class MGCMD_GET_FW_VERSION: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGCMD_GET_FW_VERSION; }
    virtual enum MGenAutoguider::OpMode opMode() const { return MGenAutoguider::OPM_APPLICATION; }

public:
    unsigned short fw_version() const { return (unsigned short) (answer[2]<<8) | answer[1]; }

public:
    MGC::CommandResult ask(struct ftdi_context * const ftdi) throw (IOError)
    {
        write(ftdi);

        int const bytes_read = read(ftdi);
        if(answer[0] == query[0] && ( 1 == bytes_read || 3 == bytes_read ))
        {
            _L("received firwmare version ack", "");
            return CR_SUCCESS;
        }

        _L("no firmware version ack (%d bytes read)", bytes_read);
        return CR_FAILURE;
    }

public:
    MGCMD_GET_FW_VERSION( MGenAutoguider& root ):
        MGC(root, IOBuffer { root.getOpCode(commandByte()) }, IOBuffer (1+1+2) ) {};
};


class MGCMD_READ_ADCS: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGCMD_READ_ADCS; }
    virtual enum MGenAutoguider::OpMode opMode() const { return MGenAutoguider::OPM_APPLICATION; }

public:
    float logic_voltage() const { return 1.6813e-4f * ((unsigned short) (answer[2]<<8) | answer[1]); }
    float input_voltage() const { return 3.1364e-4f * ((unsigned short) (answer[4]<<8) | answer[3]); }
    float refer_voltage() const { return 3.91e-5f * ((unsigned short) (answer[10]<<8) | answer[9]); }

public:
    MGC::CommandResult ask(struct ftdi_context * const ftdi) throw (IOError)
    {
        write(ftdi);

        int const bytes_read = read(ftdi);
        if(answer[0] == query[0] && ( 1+5*2 == bytes_read ))
        {
            _L("received ADCs ack", "");
            return CR_SUCCESS;
        }

        _L("no ADCs ack (%d bytes read)", bytes_read);
        return CR_FAILURE;
    }

public:
    MGCMD_READ_ADCS( MGenAutoguider& root ):
        MGC(root, IOBuffer { root.getOpCode(commandByte()) }, IOBuffer (1+5*2) ) {};
};

#endif /* 3RDPARTY_INDI_MGEN_MGENCOMMAND_HPP_ */
