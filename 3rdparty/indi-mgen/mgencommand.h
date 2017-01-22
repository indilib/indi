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

#include "indicom.h"

#include "mgenautoguider.h"

template < enum MGenAutoguider::OpMode OPM, enum MGenAutoguider::CommandByte CB >
class MGenCommand
{
protected:
    MGenAutoguider& root;

public:
    enum MGenAutoguider::CommandByte commandByte() const { return CB; }
    enum MGenAutoguider::CommandByte opMode() const { return OPM; }

public:
    unsigned char opCode() const { root.getOpCode(CB); }
    char const * name() const { root.getOpCodeString(CB); }

public:
    virtual int write()
    {
        if( OPM != root.connectionStatus.mode )
            return 0;

        if( 0 <= root.fd )
        {
            int bytes_written = 0;
            switch( tty_write(root.fd, &opCode(), 1, &bytes_written) )
            {
                case TTY_OK:
                    return bytes_written;

                default: break;
            }
        }

        return 0;
    }

public:
    virtual std::vector<char> read()
    {
        if( OPM != root.connectionStatus.mode )
            return 0;

        if( 0 <= root.fd )
        {
            char buffer[1];
            int bytes_read = 0;
            switch( tty_read(root.fd, buffer, 1, 1, &bytes_read) )
            {
                case TTY_OK:
                    return std::vector<char> { buffer[0] };

                default: break;
            }
        }

        return 0;
    }

protected:
    MGenCommand( MGenAutoguider& root ): root(root) {};
    virtual ~MGenCommand() {};
};



#endif /* 3RDPARTY_INDI_MGEN_MGENCOMMAND_HPP_ */
