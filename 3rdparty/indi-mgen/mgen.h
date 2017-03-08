/*
 * mgen.h
 *
 *  Created on: 8 mars 2017
 *      Author: TallFurryMan
 */

#ifndef _3RDPARTY_INDI_MGEN_MGEN_H_
#define _3RDPARTY_INDI_MGEN_MGEN_H_

#include <exception>
#include <string>
#include <typeinfo>


/** @brief A protocol mode in which the command is valid */
enum IOMode
{
    OPM_UNKNOWN,        /**< Unknown mode, no exchange done yet or connection error */
    OPM_COMPATIBLE,     /**< Compatible mode, just after boot */
    OPM_BOOT,           /**< Boot mode */
    OPM_APPLICATION,    /**< Normal applicative mode */
};


/** @internal Debug helper to stringify an IOMode value */
char const * const DBG_OpModeString(IOMode);


/** @brief The result of a command */
enum IOResult
{
    CR_SUCCESS,         /**< Command is successful, result is available through helpers or in field 'answer' */
    CR_FAILURE,         /**< Command is not successful, no acknowledge or unexpected data returned */
};


/** @brief Exception returned when there is I/O malfunction with the device */
class IOError: std::exception
{
protected:
    std::string const _what;
public:
    virtual const char * what() const noexcept { return _what.c_str(); }
    IOError(int code): std::exception(), _what(std::string("I/O error code ") + std::to_string(code)) {};
    virtual ~IOError() {};
};


/** @brief One word in the I/O protocol */
typedef unsigned char IOByte;

/** @brief A buffer of protocol words */
typedef std::vector<IOByte> IOBuffer;

#define _L(msg, ...) INDI::Logger::getInstance().print(MGenAutoguider::instance().getDeviceName(), INDI::Logger::DBG_SESSION, __FILE__, __LINE__, "%s::%s: " msg, __FUNCTION__, typeid(*this).name(), __VA_ARGS__)

class MGenAutoguider;
class MGenDeviceState;

#endif /* _3RDPARTY_INDI_MGEN_MGEN_H_ */
