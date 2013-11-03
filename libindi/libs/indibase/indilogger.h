/*******************************************************************************
  Copyright (C) 2012 Evidence Srl - www.evidence.eu.com

 Adapted to INDI Library by Jasem Mutlaq & Geehalel.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <fstream>
#include <ostream>
#include <string>
#include <sstream>
#include <sys/time.h>

#include <indiapi.h>
#include "libs/indibase/defaultdevice.h"

/**
 * \brief Macro to configure the logger.
 * Example of configuration of the Logger:
 * 	DEBUG_CONF("outputfile", Logger::file_on|Logger::screen_on, DBG_DEBUG, DBG_ERROR);
 */
#define DEBUG_CONF(outputFile, \
		configuration, \
		fileVerbosityLevel, \
		screenVerbosityLevel) { \
			Logger::getInstance().configure(outputFile, \
						configuration, \
						fileVerbosityLevel, \
						screenVerbosityLevel); \
		}

/**
 * \brief Macro to print log messages.
 * Example of usage of the Logger:
 *	    DEBUG(DBG_DEBUG, "hello " << "world");
 */
/*
#define DEBUG(priority, msg) { \
	std::ostringstream __debug_stream__; \
	__debug_stream__ << msg; \
	Logger::getInstance().print(priority, __FILE__, __LINE__, \
			__debug_stream__.str()); \
	}
*/
#define DEBUG(priority, msg) Logger::getInstance().print(getDeviceName(), priority, __FILE__, __LINE__,msg)
#define DEBUGF(priority, msg, ...) Logger::getInstance().print(getDeviceName(), priority, __FILE__, __LINE__, msg, __VA_ARGS__)
#define DEBUGDEVICE(device, priority, msg) Logger::getInstance().print(device, priority, __FILE__, __LINE__, msg)
#define DEBUGFDEVICE(device, priority, msg, ...) Logger::getInstance().print(device, priority, __FILE__, __LINE__,  msg, __VA_ARGS__)

namespace  INDI
{

/**
 * @brief The Logger class is a simple logger to log messages to file and INDI clients. This is the implementation of a simple
 *  logger in C++. It is implemented as a Singleton, so it can be easily called through two DEBUG macros.
 * It is Pthread-safe. It allows to log on both file and screen, and to specify a verbosity threshold for both of them.
 *
 * - By default, the class defines 4 levels of debugging/logging levels:
 *      -# Errors: Use macro DEBUG(Logger::DBG_ERROR, "My Error Message)
 *
 *      -# Warnings: Use macro DEBUG(Logger::DBG_WARNING, "My Warning Message)
 *
 *      -# Session: Use macro DEBUG(Logger::DBG_SESSION, "My Message) Session messages are the regular status messages from the driver.
 *
 *      -# Driver Debug: Use macro DEBUG(Logger::DBG_DEBUG, "My Driver Debug Message)
 *
 * \note Use DEBUGF macro if you have a variable list message. e.g. DEBUGF(Logger::DBG_SESSION, "Hello %s!", "There")
 *
 * The default \e active debug levels are Error, Warning, and Session. Driver Debug can be enabled by the client.
 *
 * To add a new debug level, call addDebugLevel(). You can add an additional 4 custom debug/logging levels.
 *
 * Check INDI Tutorial two for an example simple implementation.
 * \remarks Logger is part of the INDI namespace. To use it directly as Logger::DBG_XXXXX, you must declare \code using namespace INDI; \endcode in your driver files.
 * Otherwise, you need to refer to it as INDI::Logger::DBG_XXXXX.
 */
class Logger
{
    private:
        /**
         * \brief Type used for the configuration
         */
        enum loggerConf_	{L_nofile_	= 	1 << 0,
                    L_file_		=	1 << 1,
                    L_noscreen_	=	1 << 2,
                    L_screen_	=	1 << 3};

    #ifdef LOGGER_MULTITHREAD
        /**
         * \brief Lock for mutual exclusion between different threads
         */
        static pthread_mutex_t lock_;
    #endif

        bool configured_;

        /**
         * \brief Pointer to the unique Logger (i.e., Singleton)
         */
        static Logger* m_;

        /**
         * \brief Initial part of the name of the file used for Logging.
         * Date and time are automatically appended.
         */
        static std::string logFile_;

        /**
         * \brief Current configuration of the logger.
         * Variable to know if logging on file and on screen are enabled.
         * Note that if the log on file is enabled, it means that the
         * logger has been already configured, therefore the stream is
         * already open.
         */
        static loggerConf_ configuration_;

        /**
         * \brief Stream used when logging on a file
         */
        std::ofstream out_;

        /**
         * \brief Initial time (used to print relative times)
         */
        struct timeval initialTime_;

        /**
         * \brief Verbosity threshold for files
         */
        static unsigned int fileVerbosityLevel_;

        /**
         * \brief Verbosity threshold for screen
         */
        static unsigned int screenVerbosityLevel_;
            static unsigned int rememberscreenlevel_;

        Logger();
        ~Logger();

        /**
         * \brief Method to lock in case of multithreading
         */
        inline static void lock();

        /**
         * \brief Method to unlock in case of multithreading
         */
        inline static void unlock();

 public:
  enum VerbosityLevel 
  {DBG_ERROR=0x1, DBG_WARNING=0x2, DBG_SESSION=0x4, DBG_DEBUG=0x8, DBG_EXTRA_1=0x10,
   DBG_EXTRA_2=0X20, DBG_EXTRA_3=0x40, DBG_EXTRA_4=0x80};

    struct switchinit {char name[MAXINDINAME]; char label[MAXINDILABEL]; ISState state; unsigned int levelmask;};
    static const unsigned int defaultlevel=DBG_ERROR | DBG_WARNING | DBG_SESSION;
    static const unsigned int nlevels=8;
    static struct switchinit LoggingLevelSInit[nlevels];
    static ISwitch LoggingLevelS[nlevels];
    static ISwitchVectorProperty LoggingLevelSP;
    static ISwitch ConfigurationS[2];
    static ISwitchVectorProperty ConfigurationSP;
    typedef loggerConf_ loggerConf;
    static const loggerConf file_on= 	L_nofile_;
    static const loggerConf file_off= 	L_file_;
    static const loggerConf screen_on= 	L_noscreen_;
    static const loggerConf screen_off= L_screen_;
    static unsigned int customLevel;

    static loggerConf_ getConfiguration() {return configuration_;}
    static Logger& getInstance();

    /**
     * @brief Adds a new debugging level to the driver.
     *
     * @param debugLevelName The descriptive debug level defined to the client. e.g. Scope Status
     * @param LoggingLevelName the short logging level recorded in the logfile. e.g. SCOPE
     * @return bitmask of the new debugging level to be used for any subsequent calls to DEBUG and DEBUGF to record events to this debug level.
     */
    int addDebugLevel(const char *debugLevelName, const char *LoggingLevelName);

    void print(const char *devicename,
     const unsigned int		verbosityLevel,
     const std::string&	sourceFile,
     const int 		codeLine,
     //const std::string& 	message,
     const char *message,
     ...);

    void configure (const std::string&	outputFile,  const loggerConf configuration,  const int fileVerbosityLevel, const int	screenVerbosityLevel);

    static struct switchinit DebugLevelSInit[nlevels];
    static ISwitch DebugLevelS[nlevels];
    static ISwitchVectorProperty DebugLevelSP;
    static bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    static bool updateProperties(bool debugenable, INDI::DefaultDevice *device);
    static char Tags[nlevels][MAXINDINAME];
    static unsigned int rank(unsigned int l);


}; /* Class logger */

inline Logger::loggerConf operator|
	(Logger::loggerConf __a, Logger::loggerConf __b)
{
	return Logger::loggerConf(static_cast<int>(__a) |
		static_cast<int>(__b));
}

inline Logger::loggerConf operator&
	(Logger::loggerConf __a, Logger::loggerConf __b)
{
	return Logger::loggerConf(static_cast<int>(__a) &
		static_cast<int>(__b)); }


}

#endif /* LOGGER_H */
