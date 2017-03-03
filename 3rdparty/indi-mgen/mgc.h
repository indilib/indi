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

#define _L(msg, ...) INDI::Logger::getInstance().print(root.getDeviceName(), INDI::Logger::DBG_SESSION, std::string(__FILE__), __LINE__, "%s: " msg, __FUNCTION__, __VA_ARGS__)

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
    /** @brief A protocol mode in which the command is valid */
    typedef enum MGenAutoguider::OpMode IOMode;

    /** @brief One word in the I/O protocol */
    typedef unsigned char IOByte;

    /** @brief A buffer of protocol words */
    typedef std::vector<IOByte> IOBuffer;

public:
    virtual enum MGenAutoguider::CommandByte commandByte() const = 0; /* Deprecate this */
    virtual IOByte opCode() const = 0;
    virtual IOMode opMode() const = 0;
    virtual CommandResult ask(struct ftdi_context * const ftdi) throw (IOError) = 0;

protected:
    /** @internal The I/O query buffer to be written to the device */
    IOBuffer query;

    /** @internal The I/O answer buffer to be read from the device */
    IOBuffer answer;

public:
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
            _L("operating mode %s doesn't support command %s", root.getOpModeString(opMode()), name());
            return -1;
        }

        _L("writing %s - %d bytes to device: %02X %02X %02X %02X %02X ...", name(), query.size(), query.size()>0?query[0]:0, query.size()>1?query[1]:0, query.size()>2?query[2]:0, query.size()>3?query[3]:0, query.size()>4?query[4]:0);
        int const bytes_written = ftdi_write_data(ftdi, query.data(), query.size());

        /* FIXME: Adjust this to optimize how long device absorb the command for */
        usleep(20000);

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
            _L("operating mode %s doesn't support command %s", root.getOpModeString(opMode()), name());
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
    virtual IOByte opCode() const { return 0xAA; }
    virtual IOMode opMode() const { return MGenAutoguider::OPM_UNKNOWN; }

public:
    MGC::CommandResult ask(struct ftdi_context * const ftdi) throw (IOError)
    {
        write(ftdi);

        int const bytes_read = read(ftdi);
        if(answer[0] == (unsigned char) ~query[0] && 5 == bytes_read)
        {
            _L("device acknowledged identification, analyzing '%02X%02X%02X'", answer[2], answer[3], answer[4]);

            IOBuffer const app_mode_answer { (unsigned char) ~query[0], 3, 0x01, 0x80, 0x02 };
            IOBuffer const boot_mode_answer { (unsigned char) ~query[0], 3, 0x01, 0x80, 0x01 };

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
        MGC(root, IOBuffer { opCode(), 1, 1 }, IOBuffer (5) ) {};
};


class MGCMD_NOP1: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGCMD_NOP1; }
    virtual IOByte opCode() const { return 0xFF; }
    virtual IOMode opMode() const { return MGenAutoguider::OPM_APPLICATION; }

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
        MGC(root, IOBuffer { opCode() }, IOBuffer (1) ) {};
};


class MGCMD_GET_FW_VERSION: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGCMD_GET_FW_VERSION; }
    virtual IOByte opCode() const { return 0x03; }
    virtual IOMode opMode() const { return MGenAutoguider::OPM_APPLICATION; }

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
        MGC(root, IOBuffer { opCode() }, IOBuffer (1+1+2) ) {};
};


class MGCMD_READ_ADCS: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGCMD_READ_ADCS; }
    virtual IOByte opCode() const { return 0xA0; }
    virtual IOMode opMode() const { return MGenAutoguider::OPM_APPLICATION; }

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
        MGC(root, IOBuffer { opCode() }, IOBuffer (1+5*2) ) {};
};


class MGIO_READ_DISPLAY_FRAME: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGIO_READ_DISPLAY; }
    virtual IOByte opCode() const { return 0x5D; }
    virtual IOMode opMode() const { return MGenAutoguider::OPM_APPLICATION; }

protected:
    static std::size_t const frame_size = (128*64)/8;
    IOBuffer bitmap_frame;

public:
    typedef std::array<unsigned char, frame_size*8> ByteFrame;
    ByteFrame& get_frame(ByteFrame &frame) const
    {
        /* A display byte is 8 display bits shaping a column, LSB at the top
         *
         *      C0      C1      C2      --    C127
         * L0  D0[0]   D1[0]   D2[0]    --   D127[0]
         * L1  D0[1]   D1[1]   D2[1]    --   D127[1]
         * |
         * L7  D0[7]   D1[7]   D2[7]    --   D127[7]
         * L8  D128[0] D129[0] D130[0]  --   D255[0]
         * L9  D128[1] D129[1] D130[1]  --   D255[1]
         * |
         * L15 D128[7] D129[7] D130[7]  --   D255[7]
         * ...
         */
        for(unsigned int i = 0; i < frame.size(); i++)
        {
            unsigned int const c = i % 128;
            unsigned int const l = i / 128;
            unsigned int const B = c + (l / 8) * 128;
            unsigned int const b = l % 8;

            frame[i] = ((bitmap_frame[B] >> b) & 0x01) ? '0' : ' ';
        }
        /*for(unsigned int i = 0; i < frame_size; i++)
        {
            frame[(i%128)+128*(i/128)+7*128] = 255*( ( bitmap_frame[i] >> 7 ) & 0x01 );
            frame[(i%128)+128*(i/128)+6*128] = 255*( ( bitmap_frame[i] >> 6 ) & 0x01 );
            frame[(i%128)+128*(i/128)+5*128] = 255*( ( bitmap_frame[i] >> 5 ) & 0x01 );
            frame[(i%128)+128*(i/128)+4*128] = 255*( ( bitmap_frame[i] >> 4 ) & 0x01 );
            frame[(i%128)+128*(i/128)+3*128] = 255*( ( bitmap_frame[i] >> 3 ) & 0x01 );
            frame[(i%128)+128*(i/128)+2*128] = 255*( ( bitmap_frame[i] >> 2 ) & 0x01 );
            frame[(i%128)+128*(i/128)+1*128] = 255*( ( bitmap_frame[i] >> 1 ) & 0x01 );
            frame[(i%128)+128*(i/128)+0*128] = 255*( ( bitmap_frame[i] >> 0 ) & 0x01 );
        }*/
        _L("    0123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|1234567","");
        for(unsigned int i = 0; i < frame.size()/128; i++)
        {
            char line[128];
            for(unsigned int j = 0; j < 128; j++)
                line[j] = frame[i*128+j];
            _L("%03d %128.128s", i, line);
        }
        return frame;
    };

public:
    MGC::CommandResult ask(struct ftdi_context * const ftdi) throw (IOError)
    {
        bitmap_frame.clear();

        /* Sorted out from spec and experiment:
         * Query:  IO_FUNC SUBFUNC ADDR_L ADDR_H COUNT for each block
         * Answer: IO_FUNC D0 D1 D2... (COUNT bytes)
         *
         * To finish communication (not exactly perfect, but keeps I/O synced)
         * Query:  IO_FUNC 0xFF
         * Answer: IO_FUNC
         */

        /* Read the ack first, then 8 blocks of 128 bytes */
        answer.resize(1+128);
        for(unsigned int block = 0; block < 8*128; block += 128)
        {
            IOByte const length = 128;
            query[2] = (unsigned char)((block&0x03FF)>>0);
            query[3] = (unsigned char)((block&0x03FF)>>8);
            query[4] = length;

            write(ftdi);
            if(read(ftdi) < length + 1)
                _L("failed reading frame block, pushing back nonetheless","");
            if(opCode() != answer[0])
                _L("failed acking frame block, command is desynced, pushing back nonetheless","");
            bitmap_frame.insert(bitmap_frame.end(), answer.begin() + 1, answer.end());
        }

//            /* Read the last block of 4 bytes */
//            answer.resize(4);
//            query[0] = (unsigned char)(((4*255)&0xFF00)>>8);
//            query[1] = (unsigned char)(((4*255)&0x00FF)>>0);
//            query[2] = 4;
//            write(ftdi);
//            if(read(ftdi) < 4 )
//                _L("failed reading last frame block, pushing back nonetheless","");
//            bitmap_frame.insert(bitmap_frame.end(), answer.begin(), answer.end());

        /* FIXME: don't ever try to reuse this command after ask() was called... */
        /* FIXME: somehow we're forced to always send IO_FUNC first, but sending NOP1 won't get acked, as if we shouldn't have to send IO_FUNC before SUBFUNC... well, no problem, close the subfunc with NOP1 */
        //::MGCMD_NOP1(root).ask(ftdi);
        query.resize(2);
        query[1] = 0xFF;
        write(ftdi);
        answer.resize(1);
        read(ftdi);

        return CR_SUCCESS;
    }

public:
    MGIO_READ_DISPLAY_FRAME( MGenAutoguider& root ):
        MGC(root, IOBuffer { opCode(), 0x0D, 0, 0, 0 }, IOBuffer (1) ), bitmap_frame(frame_size) {};
};

class MGIO_INSERT_BUTTON: MGC
{
public:
    virtual enum MGenAutoguider::CommandByte commandByte() const { return MGenAutoguider::MGCMD_IO_FUNCTIONS; }
    virtual IOByte opCode() const { return 0x5D; }
    virtual IOMode opMode() const { return MGenAutoguider::OPM_APPLICATION; }

public:
    enum Button
    {
        IOB_ESC = 0,
        IOB_SET = 1,
        IOB_LEFT = 2,
        IOB_RIGHT = 3,
        IOB_UP = 4,
        IOB_DOWN = 5,
        IOB_LONG_ESC = 6,
    };

public:
    MGC::CommandResult ask(struct ftdi_context * const ftdi) throw (IOError)
    {
        write(ftdi);
        ::MGCMD_NOP1(root).ask(ftdi);
        return CR_SUCCESS;
    }

public:
    MGIO_INSERT_BUTTON( MGenAutoguider& root, Button button ):
        MGC(root, IOBuffer {
                opCode(),
                0x01, (unsigned char) (0x00 | (unsigned char) button),
                0x01, (unsigned char) (0x80 | (unsigned char) button) },
            IOBuffer (1) ) {};
};

#endif /* 3RDPARTY_INDI_MGEN_MGENCOMMAND_HPP_ */
