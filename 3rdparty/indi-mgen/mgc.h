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

#include "mgen_device.h"

class MGC
{
public:
    /** @internal Stringifying the name of the command */
    char const * name() const { return typeid(*this).name(); }

public:
    /** @brief Returning the character operation code of the command */
    virtual IOByte opCode() const = 0;

    /** @brief Returning the operation mode for this command */
    virtual IOMode opMode() const = 0;

protected:
    /** @internal The I/O query buffer to be written to the device */
    IOBuffer query;

    /** @internal The I/O answer buffer to be read from the device */
    IOBuffer answer;

    /** @brief Basic verifications to call before running the actual command implementation */
    virtual IOResult ask(MGenDevice& root) throw (IOError)
    {
        if(opMode() != OPM_UNKNOWN && opMode() != root.getOpMode())
        {
            _L("operating mode %s does not support command", MGenDevice::DBG_OpModeString(opMode()));
            return CR_FAILURE;
        }

        return CR_SUCCESS;
    }

protected:
    MGC(IOBuffer query, IOBuffer answer):
        query(query),
        answer(answer) {};

    virtual ~MGC() {};
};


class MGCP_QUERY_DEVICE: MGC
{
public:
    virtual IOByte opCode() const { return 0xAA; }
    virtual IOMode opMode() const { return OPM_UNKNOWN; }

public:
    virtual IOResult ask(MGenDevice& root) throw (IOError)
    {
        if(CR_SUCCESS != MGC::ask(root))
            return CR_FAILURE;

        root.write(query);

        int const bytes_read = root.read(answer);
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
    MGCP_QUERY_DEVICE():
        MGC(IOBuffer { opCode(), 1, 1 }, IOBuffer (5)) {};
};


class MGCMD_NOP1: MGC
{
public:
    virtual IOByte opCode() const { return 0xFF; }
    virtual IOMode opMode() const { return OPM_APPLICATION; }

public:
    virtual IOResult ask(MGenDevice& root) throw (IOError)
    {
        if(CR_SUCCESS != MGC::ask(root))
            return CR_FAILURE;

        root.write(query);

        int const bytes_read = root.read(answer);
        if(answer[0] == query[0] && 1 == bytes_read)
            return CR_SUCCESS;

        _L("no ack (%d bytes read)", bytes_read);
        return CR_FAILURE;
    }

public:
    MGCMD_NOP1():
        MGC(IOBuffer { opCode() }, IOBuffer (1)) {};
};


class MGCP_ENTER_NORMAL_MODE: MGC
{
public:
    virtual IOByte opCode() const { return 0x42; }
    virtual IOMode opMode() const { return OPM_COMPATIBLE; }

public:
    virtual IOResult ask(MGenDevice& root) throw (IOError)
    {
        if(CR_SUCCESS != MGC::ask(root))
            return CR_FAILURE;

        root.write(query);
        sleep(1);
        return CR_SUCCESS;
    }

public:
    MGCP_ENTER_NORMAL_MODE():
        MGC(IOBuffer { opCode() }, IOBuffer ()) {};
};


class MGCMD_GET_FW_VERSION: MGC
{
public:
    virtual IOByte opCode() const { return 0x03; }
    virtual IOMode opMode() const { return OPM_APPLICATION; }

public:
    unsigned short fw_version() const { return (unsigned short) (answer[2]<<8) | answer[1]; }

public:
    virtual IOResult ask(MGenDevice& root) throw (IOError)
    {
        if(CR_SUCCESS != MGC::ask(root))
            return CR_FAILURE;

        root.write(query);

        int const bytes_read = root.read(answer);
        if(answer[0] == query[0] && ( 1 == bytes_read || 3 == bytes_read ))
            return CR_SUCCESS;

        _L("no ack (%d bytes read)", bytes_read);
        return CR_FAILURE;
    }

public:
    MGCMD_GET_FW_VERSION():
        MGC(IOBuffer { opCode() }, IOBuffer (1+1+2)) {};
};


class MGCMD_READ_ADCS: MGC
{
public:
    virtual IOByte opCode() const { return 0xA0; }
    virtual IOMode opMode() const { return OPM_APPLICATION; }

public:
    float logic_voltage() const { return 1.6813e-4f * ((unsigned short) (answer[2]<<8) | answer[1]); }
    float input_voltage() const { return 3.1364e-4f * ((unsigned short) (answer[4]<<8) | answer[3]); }
    float refer_voltage() const { return 3.91e-5f * ((unsigned short) (answer[10]<<8) | answer[9]); }

public:
    virtual IOResult ask(MGenDevice& root) throw (IOError)
    {
        if(CR_SUCCESS != MGC::ask(root))
            return CR_FAILURE;

        root.write(query);

        int const bytes_read = root.read(answer);
        if(answer[0] == query[0] && ( 1+5*2 == bytes_read ))
            return CR_SUCCESS;

        _L("no ack (%d bytes read)", bytes_read);
        return CR_FAILURE;
    }

public:
    MGCMD_READ_ADCS():
        MGC(IOBuffer { opCode() }, IOBuffer (1+5*2)) {};
};


class MGIO_READ_DISPLAY_FRAME: MGC
{
public:
    virtual IOByte opCode() const { return 0x5D; }
    virtual IOMode opMode() const { return OPM_APPLICATION; }

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
#if 0
        _L("    0123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|123456789|1234567","");
        for(unsigned int i = 0; i < frame.size()/128; i++)
        {
            char line[128];
            for(unsigned int j = 0; j < 128; j++)
                line[j] = frame[i*128+j];
            _L("%03d %128.128s", i, line);
        }
#endif
        return frame;
    };

public:
    virtual IOResult ask(MGenDevice& root) throw (IOError)
    {
        if(CR_SUCCESS != MGC::ask(root))
            return CR_FAILURE;

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

            root.write(query);
            if(root.read(answer) < length + 1)
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
//            root.write(query);
//            if(root.read(answer) < 4 )
//                _L("failed reading last frame block, pushing back nonetheless","");
//            bitmap_frame.insert(bitmap_frame.end(), answer.begin(), answer.end());

        /* FIXME: don't ever try to reuse this command after ask() was called... */
        /* FIXME: somehow we're forced to always send IO_FUNC first, but sending NOP1 won't get acked, as if we shouldn't have to send IO_FUNC before SUBFUNC... well, no problem, close the subfunc with NOP1 */
        //::MGCMD_NOP1(root).ask(ftdi);
        query.resize(2);
        query[1] = 0xFF;
        root.write(query);
        answer.resize(1);
        root.read(answer);

        return CR_SUCCESS;
    }

public:
    MGIO_READ_DISPLAY_FRAME():
        MGC(IOBuffer { opCode(), 0x0D, 0, 0, 0 }, IOBuffer (1)), bitmap_frame(frame_size) {};
};

class MGIO_INSERT_BUTTON: MGC
{
public:
    virtual IOByte opCode() const { return 0x5D; }
    virtual IOMode opMode() const { return OPM_APPLICATION; }

public:
    enum Button
    {
        IOB_NONE = -1,
        IOB_ESC = 0,
        IOB_SET = 1,
        IOB_LEFT = 2,
        IOB_RIGHT = 3,
        IOB_UP = 4,
        IOB_DOWN = 5,
        IOB_LONG_ESC = 6,
    };

public:
    virtual IOResult ask(MGenDevice& root) throw (IOError)
    {
        if(CR_SUCCESS != MGC::ask(root))
            return CR_FAILURE;

        _L("sending button %d",query[2]);
        query[2] &= 0x7F;
        root.write(query);
        root.read(answer);
        query[2] |= 0x80;
        root.write(query);
        root.read(answer);
        return CR_SUCCESS;
    }

public:
    MGIO_INSERT_BUTTON(Button button):
        MGC(IOBuffer {opCode(), 0x01, (unsigned char) button}, IOBuffer (2)) {};
};

#endif /* 3RDPARTY_INDI_MGEN_MGENCOMMAND_HPP_ */
