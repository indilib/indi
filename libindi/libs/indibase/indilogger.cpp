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

#include "indilogger.h"
#include <indicom.h>
#include <cstdio>

#include <iostream>

namespace INDI
{


char Logger::Tags[Logger::nlevels][MAXINDINAME]=
{
  "ERROR", "WARNING", "INFO", "DEBUG", "DBG_EXTRA_1", "DBG_EXTRA_2", "DBG_EXTRA_3", "DBG_EXTRA_4"
};


struct Logger::switchinit Logger::DebugLevelSInit[]=
{
  {"DBG_ERROR", "Errors", ISS_ON, DBG_ERROR},
  {"DBG_WARNING", "Warnings", ISS_ON, DBG_WARNING}, 
  {"DBG_SESSION", "Messages", ISS_ON, DBG_SESSION}, 
  {"DBG_DEBUG", "Driver Debug", ISS_OFF, DBG_DEBUG}, 
  {"DBG_EXTRA_1", "Debug Extra 1", ISS_OFF, DBG_EXTRA_1},
  {"DBG_EXTRA_2",  "Debug Extra 2", ISS_OFF, DBG_EXTRA_2},
  {"DBG_EXTRA_3", "Debug Extra 3", ISS_OFF, DBG_EXTRA_3},
  {"DBG_EXTRA_4", "Debug Extra 4", ISS_OFF, DBG_EXTRA_4}
};

struct Logger::switchinit Logger::LoggingLevelSInit[]=
{
  {"LOG_ERROR", "Errors", ISS_ON, DBG_ERROR},
  {"LOG_WARNING", "Warnings", ISS_ON, DBG_WARNING}, 
  {"LOG_SESSION", "Messages", ISS_ON, DBG_SESSION}, 
  {"LOG_DEBUG", "Driver Debug", ISS_OFF, DBG_DEBUG}, 
  {"LOG_EXTRA_1", "Log Extra 1", ISS_OFF, DBG_EXTRA_1},
  {"LOG_EXTRA_2",  "Log Extra 2", ISS_OFF, DBG_EXTRA_2},
  {"LOG_EXTRA_3", "Log Extra 3", ISS_OFF, DBG_EXTRA_3},
  {"LOG_EXTRA_4", "Log Extra 4", ISS_OFF, DBG_EXTRA_4}
};

ISwitch Logger::DebugLevelS[Logger::nlevels];
ISwitchVectorProperty Logger::DebugLevelSP;
ISwitch Logger::LoggingLevelS[Logger::nlevels];
ISwitchVectorProperty Logger::LoggingLevelSP;
ISwitch Logger::ConfigurationS[2];
ISwitchVectorProperty Logger::ConfigurationSP;

unsigned int Logger::fileVerbosityLevel_=Logger::defaultlevel;
unsigned int Logger::screenVerbosityLevel_=Logger::defaultlevel;
unsigned int Logger::rememberscreenlevel_=Logger::defaultlevel;
Logger::loggerConf Logger::configuration_= Logger::screen_on | Logger::file_off;
std::string Logger::logFile_;

unsigned int Logger::customLevel=4;

int Logger::addDebugLevel(const char *debugLevelName, const char * loggingLevelName)
{
    // Cannot create any more levels
    if (customLevel == nlevels)
        return -1;

    strncpy(Tags[customLevel], loggingLevelName, MAXINDINAME);
    strncpy(DebugLevelSInit[customLevel].label, debugLevelName, MAXINDINAME);
    strncpy(LoggingLevelSInit[customLevel].label, debugLevelName, MAXINDINAME);

    return DebugLevelSInit[customLevel++].levelmask;
}

bool Logger::updateProperties(bool debugenable, INDI::DefaultDevice *device) 
{
  if (debugenable) 
    {
      unsigned int i;
      for (i=0; i<customLevel; i++)
      {
          IUFillSwitch(&DebugLevelS[i], DebugLevelSInit[i].name,DebugLevelSInit[i].label,DebugLevelSInit[i].state);
          DebugLevelS[i].aux = (void *)&DebugLevelSInit[i].levelmask;
          IUFillSwitch(&LoggingLevelS[i], LoggingLevelSInit[i].name,LoggingLevelSInit[i].label,LoggingLevelSInit[i].state);
          LoggingLevelS[i].aux = (void *)&LoggingLevelSInit[i].levelmask;
     }

    IUFillSwitchVector(&DebugLevelSP, DebugLevelS, customLevel, device->getDeviceName(), "DEBUG_LEVEL" , "Debug Levels", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);
    IUFillSwitchVector(&LoggingLevelSP, LoggingLevelS, customLevel, device->getDeviceName(), "LOGGING_LEVEL" , "Logging Levels", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);
    device->defineSwitch(&DebugLevelSP);
    device->defineSwitch(&LoggingLevelSP);
    screenVerbosityLevel_=rememberscreenlevel_;

    IUFillSwitch(&ConfigurationS[0], "CLIENT_DEBUG", "To Client", ISS_ON);
    IUFillSwitch(&ConfigurationS[1], "FILE_DEBUG", "To Log File", ISS_OFF);
    IUFillSwitchVector(&ConfigurationSP, ConfigurationS, 2, device->getDeviceName(), "LOG_OUTPUT", "Log Output", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);
    device->defineSwitch(&ConfigurationSP);

    } 
  else 
  {
      device->deleteProperty(DebugLevelSP.name);
      device->deleteProperty(LoggingLevelSP.name);
      device->deleteProperty(ConfigurationSP.name);
      rememberscreenlevel_=screenVerbosityLevel_;
      screenVerbosityLevel_=defaultlevel;
  }

  return true;
}

bool Logger::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
  int debug_level=0, log_level=0, bitmask=0, verbose_level=0;
  if(strcmp(name,"DEBUG_LEVEL")==0)
    {  
      ISwitch *sw;
      IUUpdateSwitch(&DebugLevelSP,states,names,n);
      sw=IUFindOnSwitch(&DebugLevelSP);
      if (sw == NULL)
      {
          DebugLevelSP.s=IPS_IDLE;
          IDSetSwitch(&DebugLevelSP,NULL);
          screenVerbosityLevel_ = 0;
          return true;
      }

      for (int i=0; i < DebugLevelSP.nsp; i++)
      {
          sw = &DebugLevelSP.sp[i];
          bitmask = *((unsigned int *)sw->aux);
          if (sw->s == ISS_ON)
          {
            debug_level = i;
            verbose_level |= bitmask;
          }
          else
            verbose_level &= ~bitmask;

      }

      screenVerbosityLevel_ = verbose_level;

      DEBUGFDEVICE(dev, Logger::DBG_DEBUG, "Toggle Debug Level -- %s", DebugLevelSInit[debug_level].label);
      DebugLevelSP.s=IPS_OK;
      IDSetSwitch(&DebugLevelSP,NULL);
      return true;
    }

  if(strcmp(name,"LOGGING_LEVEL")==0)
    {  
      ISwitch *sw;
      IUUpdateSwitch(&LoggingLevelSP,states,names,n);
      sw=IUFindOnSwitch(&LoggingLevelSP);
      if (sw == NULL)
      {
          fileVerbosityLevel_ =0;
          LoggingLevelSP.s=IPS_IDLE;
          IDSetSwitch(&LoggingLevelSP,NULL);
          return true;
      }

      for (int i=0; i < LoggingLevelSP.nsp; i++)
      {
          sw = &LoggingLevelSP.sp[i];
          bitmask = *((unsigned int *)sw->aux);
          if (sw->s == ISS_ON)
          {
            log_level = i;
            fileVerbosityLevel_ |= bitmask;
          }
          else
              fileVerbosityLevel_ &= ~bitmask;
      }

      DEBUGFDEVICE(dev, Logger::DBG_DEBUG, "Toggle Logging Level -- %s", LoggingLevelSInit[log_level].label);
      LoggingLevelSP.s=IPS_OK;
      IDSetSwitch(&LoggingLevelSP,NULL);
      return true;
    }

  if (!strcmp(name, "LOG_OUTPUT"))
  {
      ISwitch *sw;
      IUUpdateSwitch(&ConfigurationSP,states,names,n);
      sw=IUFindOnSwitch(&ConfigurationSP);

      if (sw == NULL)
      {
          configuration_ = screen_off | file_off;
          ConfigurationSP.s = IPS_IDLE;
          IDSetSwitch(&ConfigurationSP, NULL);
          return true;
      }

      bool wasFileOff = configuration_ & file_off;

      configuration_ = (loggerConf) 0;

      if (ConfigurationS[1].s == ISS_ON)
          configuration_ = configuration_ | file_on;
      else
          configuration_ = configuration_ | file_off;

      if (ConfigurationS[0].s == ISS_ON)
          configuration_ = configuration_ | screen_on;
      else
          configuration_ = configuration_ | screen_off;

      // If file was off, then on again
      if (wasFileOff && (configuration_&file_on))
          Logger::getInstance().configure(logFile_, configuration_, fileVerbosityLevel_, screenVerbosityLevel_);

      ConfigurationSP.s = IPS_OK;
      IDSetSwitch(&ConfigurationSP, NULL);

      return true;
  }

  return true;
}

// Definition (and initialization) of static attributes
Logger* Logger::m_ = 0;

#ifdef LOGGER_MULTITHREAD
pthread_mutex_t Logger::lock_ = PTHREAD_MUTEX_INITIALIZER;
inline void Logger::lock()
{
	pthread_mutex_lock(&lock_);
}

inline void Logger::unlock()
{
	pthread_mutex_unlock(&lock_);
}
#else
void Logger::lock(){}
void Logger::unlock(){}
#endif


/**
 * \brief Constructor.
 * It is a private constructor, called only by getInstance() and only the
 * first time. It is called inside a lock, so lock inside this method
 * is not required.
 * It only initializes the initial time. All configuration is done inside the
 * configure() method.
 */
Logger::Logger(): configured_(false)
{
  gettimeofday(&initialTime_, NULL);
}

/**
 * \brief Method to configure the logger. Called by the DEBUG_CONF() macro. To make implementation easier, the old stream is always closed.
 * Then, in case, it is open again in append mode.
 * @param outputFile of the file used for logging
 * @param configuration (i.e., log on file and on screen on or off)
 * @param fileVerbosityLevel threshold for file
 * @param screenVerbosityLevel threshold for screen
 */
void Logger::configure (const std::string&	outputFile,
			const loggerConf	configuration,
			const int		fileVerbosityLevel,
			const int		screenVerbosityLevel)
{
		Logger::lock();

		fileVerbosityLevel_ = fileVerbosityLevel;
		screenVerbosityLevel_ = screenVerbosityLevel;
		rememberscreenlevel_= screenVerbosityLevel_;
		// Close the old stream, if needed
		if (configuration_&file_on)
			out_.close();

		// Compute a new file name, if needed
        if (outputFile != logFile_)
        {
            char logFileBuf[512];
            snprintf(logFileBuf, 512, "%s_%s.log", outputFile.c_str(), timestamp());
            logFile_ = logFileBuf;
		}

		// Open a new stream, if needed
        if (configuration&file_on)
			out_.open(logFile_.c_str(), std::ios::app);

		configuration_ = configuration;
		configured_ = true;

		Logger::unlock();
}

/**
 * \brief Destructor.
 * It only closes the file, if open, and cleans memory.
 */

Logger::~Logger()
{
	Logger::lock();
	if (configuration_&file_on)
		out_.close();
	delete m_;
	Logger::unlock();

}

/**
 * \brief Method to get a reference to the object (i.e., Singleton)
 * It is a static method.
 * @return Reference to the object.
 */
Logger& Logger::getInstance()
{
	Logger::lock();
    if (m_ == 0)
        m_ = new Logger;
	Logger::unlock();
	return *m_;
}


/**
 * \brief Method used to print message called by the DEBUG() macro.
   @ param i which debugging to query its rank. The lower the rank, the more priority it is.
   @ return rank of debugging level requested.
 */
unsigned int Logger::rank(unsigned int l) {
  switch(l) {
  case DBG_ERROR: return 0;
  case DBG_WARNING: return 1;
  case DBG_SESSION: return 2;
  case DBG_DEBUG: return 3;
  case DBG_EXTRA_1: return 4;
  case DBG_EXTRA_2: return 5;
  case DBG_EXTRA_3: return 6;
  case DBG_EXTRA_4: return 7;
  }
}

void Logger::print(const char *devicename,
		   const unsigned int verbosityLevel,
		   const std::string& file,
		   const int line,
		   //const std::string& message,
		   const char *message,
		   ...)
{
  bool filelog = (verbosityLevel & fileVerbosityLevel_) != 0;
  bool screenlog = (verbosityLevel & screenVerbosityLevel_) != 0;

  va_list ap;
  char msg[257];
  char usec[7];

  msg[256]='\0';
  va_start(ap, message);
  vsnprintf(msg, 257, message, ap);
  va_end(ap);

	if (!configured_) {
			std::cerr << "ERROR: Logger not configured!" << 
				std::endl;
			return;
	}
	struct timeval currentTime, resTime;
	usec[6]='\0';
	gettimeofday(&currentTime, NULL);
	timersub(&currentTime, &initialTime_, &resTime);
	snprintf(usec, 7, "%06ld", resTime.tv_usec);
	Logger::lock();

	if ((configuration_&file_on) && filelog)
	  //out_ << Tags[rank(verbosityLevel)] << "\t["<< file << ":" << line << "]\t" <<
	  out_ << Tags[rank(verbosityLevel)] << "\t" <<
	    (resTime.tv_sec) <<"."<<(usec) << " sec"<<
				"\t: " << msg << std::endl;

	if ((configuration_&screen_on) && screenlog)
	  IDMessage(devicename, "%s", msg);
	    /*		std::cerr << "DEBUG [" << file << ":" << line << "] @ " <<
				(currentTime.tv_sec - initialTime_.tv_sec) <<
	    			":" << message << std::endl;
	    */
	Logger::unlock();
}

}
