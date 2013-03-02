/* This is based on  http://retis.sssup.it/~scordino/code/logger.html */
#ifndef LOGGER_H
#define LOGGER_H

#include <config.h>

#ifdef WITH_LOGGER
#include <fstream>
#include <ostream>
#include <string>
#include <sstream>
#include <sys/time.h>

/// Comment this line if you don't need multithread support
//#define LOGGER_MULTITHREAD

#ifdef LOGGER_MULTITHREAD
#include <pthread.h>
#endif

#endif /* WITH_LOGGER */

#include <indiapi.h>
#include <defaultdevice.h>

#ifndef WITH_LOGGER
#define DEBUGCONF

#ifdef WITH_NOFMANY
#define DEBUG(priority, msg) if (priority & Logger::level) IDMessage(getDeviceName(), msg)
#define DEBUGF(priority, msg, ...) if (priority & Logger::level) IDMessage(getDeviceName(), msg, __VA_ARGS__)
#define DEBUGDEVICE(device, priority, msg) if (priority & Logger::level) IDMessage(device, msg)
#define DEBUGFDEVICE(device, priority, msg, ...) if (priority & Logger::level) IDMessage(device, msg, __VA_ARGS__)
#else
#define DEBUG(priority, msg) if (priority <= Logger::level) IDMessage(getDeviceName(), msg)
#define DEBUGF(priority, msg, ...) if (priority <= Logger::level) IDMessage(getDeviceName(), msg, __VA_ARGS__)
#define DEBUGDEVICE(device, priority, msg) if (priority <= Logger::level) IDMessage(device, msg)
#define DEBUGFDEVICE(device, priority, msg, ...) if (priority <= Logger::level) IDMessage(device, msg, __VA_ARGS__)
#endif
#else /* WITH_LOGGER */
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
#endif /* WITH_LOGGER */

class Logger {
 public:
  enum VerbosityLevel 
  {DBG_ERROR=0x1, DBG_WARNING=0x2, DBG_SESSION=0x4, DBG_DEBUG=0x8, DBG_MOUNT=0x10, 
   DBG_COMM=0X20, DBG_CALL=0x40, DBG_SCOPE_STATUS=0x80};

  struct switchinit {const char *name; const char *label; ISState state; unsigned int levelmask;};

  static const unsigned int defaultlevel=DBG_ERROR | DBG_WARNING | DBG_SESSION;

  static const unsigned int nlevels=8;

  static struct switchinit DebugLevelSInit[nlevels];
  static ISwitch DebugLevelS[nlevels];
  static ISwitchVectorProperty DebugLevelSP;
  static bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
  static bool updateProperties(bool debugenable, INDI::DefaultDevice *device);
  static bool debugSerial(char cmd);
  static const char *Tags[nlevels];
  static unsigned int rank(unsigned int l);

#ifndef WITH_LOGGER

  static unsigned int level, rememberlevel;
}; /* Class Logger */

#else /* WITH_LOGGER */

/**
 * \brief Simple logger to log messages on file and console.
 * This is the implementation of a simple logger in C++. It is implemented 
 * as a Singleton, so it can be easily called through two DEBUG macros.
 * It is Pthread-safe.
 * It allows to log on both file and screen, and to specify a verbosity
 * threshold for both of them.
 */

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
	std::string logFile_;

	/**
	 * \brief Current configuration of the logger.
	 * Variable to know if logging on file and on screen are enabled.
	 * Note that if the log on file is enabled, it means that the
	 * logger has been already configured, therefore the stream is
	 * already open.
	 */
	loggerConf_ configuration_;

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
        static struct switchinit LoggingLevelSInit[nlevels];
        static ISwitch LoggingLevelS[nlevels];
        static ISwitchVectorProperty LoggingLevelSP;
	typedef loggerConf_ loggerConf;
	static const loggerConf file_on= 	L_nofile_;
	static const loggerConf file_off= 	L_file_;
	static const loggerConf screen_on= 	L_noscreen_;
	static const loggerConf screen_off= L_screen_;

	static Logger& getInstance();

	void print(const char *devicename,
		   const unsigned int		verbosityLevel,
		   const std::string&	sourceFile,
		   const int 		codeLine,
		   //const std::string& 	message,
		   const char *message,
		   ...);

	void configure (const std::string&	outputFile,
			const loggerConf	configuration,
			const int		fileVerbosityLevel,
			const int		screenVerbosityLevel);
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

#endif /* WITHLOGGER */

#endif /* LOGGER_H */
