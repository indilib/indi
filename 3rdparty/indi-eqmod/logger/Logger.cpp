
#include "Logger.h"
#include <indicom.h>

const char *Logger::Tags[]= {
  "ERROR", "WARNING", "INFO", "DEBUG", "DRIVER", "SERIAL", "CALL", "STATUS"
};

#ifdef WITH_N0FMANY 
struct Logger::switchinit Logger::DebugLevelSInit[]={
  {"DBG_ERROR", "Errors", ISS_ON, DBG_ERROR},
  {"DBG_WARNING", "Warnings", ISS_ON, DBG_WARNING}, 
  {"DBG_SESSION", "Messages", ISS_ON, DBG_SESSION}, 
  {"DBG_DEBUG", "Driver Debug", ISS_OFF, DBG_DEBUG}, 
  {"DBG_MOUNT", "Mount Debug", ISS_OFF, DBG_MOUNT}, 
  {"DBG_COMM",  "Serial Port Debug", ISS_OFF, DBG_COMM},
  {"DBG_CALL", "Fun. Calls(unused)", ISS_OFF, DBG_CALL},
  {"DBG_SCOPE_STATUS", "Scope status(verbose)", ISS_OFF, DBG_SCOPE_STATUS}
};
#ifdef WITH_LOGGER
struct Logger::switchinit Logger::LogginLevelSInit[]={
  {"LOG_ERROR", "Errors", ISS_ON, DBG_ERROR},
  {"LOG_WARNING", "Warnings", ISS_ON, DBG_WARNING}, 
  {"LOG_SESSION", "Messages", ISS_ON, DBG_SESSION}, 
  {"LOG_DEBUG", "Driver Debug", ISS_OFF, DBG_DEBUG}, 
  {"LOG_MOUNT", "Mount Debug", ISS_OFF, DBG_MOUNT}, 
  {"LOG_COMM",  "Serial Port Debug", ISS_OFF, DBG_COMM},
  {"LOG_CALL", "Fun. Calls(unused)", ISS_OFF, DBG_CALL},
  {"LOG_SCOPE_STATUS", "Scope status(verbose)", ISS_OFF, DBG_SCOPE_STATUS}
};
#endif
#else
struct Logger::switchinit Logger::DebugLevelSInit[]={
  {"DBG_ERROR", "Errors", ISS_OFF, DBG_ERROR},
  {"DBG_WARNING", "Warnings", ISS_OFF, DBG_WARNING}, 
  {"DBG_SESSION", "Messages", ISS_ON, DBG_SESSION}, 
  {"DBG_DEBUG", "Driver Debug", ISS_OFF, DBG_DEBUG}, 
  {"DBG_MOUNT", "Mount Debug", ISS_OFF, DBG_MOUNT}, 
  {"DBG_COMM",  "Serial Port Debug", ISS_OFF, DBG_COMM},
  {"DBG_CALL", "Fun. Calls(unused)", ISS_OFF, DBG_CALL},
  {"DBG_SCOPE_STATUS", "Scope status(verbose)", ISS_OFF, DBG_SCOPE_STATUS}
};
#ifdef WITH_LOGGER
struct Logger::switchinit Logger::LoggingLevelSInit[]={
  {"LOG_ERROR", "Errors", ISS_OFF, DBG_ERROR},
  {"LOG_WARNING", "Warnings", ISS_OFF, DBG_WARNING}, 
  {"LOG_SESSION", "Messages", ISS_ON, DBG_SESSION}, 
  {"LOG_DEBUG", "Driver Debug", ISS_OFF, DBG_DEBUG}, 
  {"LOG_MOUNT", "Mount Debug", ISS_OFF, DBG_MOUNT}, 
  {"LOG_COMM",  "Serial Port Debug", ISS_OFF, DBG_COMM},
  {"LOG_CALL", "Fun. Calls(unused)", ISS_OFF, DBG_CALL},
  {"LOG_SCOPE_STATUS", "Scope status(verbose)", ISS_OFF, DBG_SCOPE_STATUS}
};
#endif
#endif

ISwitch Logger::DebugLevelS[Logger::nlevels];
ISwitchVectorProperty Logger::DebugLevelSP;
#ifdef WITH_LOGGER
ISwitch Logger::LoggingLevelS[Logger::nlevels];
ISwitchVectorProperty Logger::LoggingLevelSP;
unsigned int Logger::fileVerbosityLevel_=Logger::defaultlevel;
unsigned int Logger::screenVerbosityLevel_=Logger::defaultlevel;
unsigned int Logger::rememberscreenlevel_=Logger::defaultlevel;
#endif

bool Logger::debugSerial(char cmd)
{
  switch (cmd) {
  case 'e':
  case 'a':
  case 'b':
  case 'g':
  case 's':
  case 'L':
  case 'K':
  case 'E':
  case 'G':
  case 'H':
  case 'M':
  case 'U':
  case 'I':
  case 'J': return true;
    default: return false;
  };
    /*Initialize ='F',
      InquireMotorBoardVersion='e',
      InquireGridPerRevolution='a',
      InquireTimerInterruptFreq='b',
      InquireHighSpeedRatio='g',
      InquirePECPeriod='s',
      InstantAxisStop='L',
      NotInstantAxisStop='K',
      SetAxisPosition='E',
      GetAxisPosition='j',
      GetAxisStatus='f',
      SetSwitch='O',
      SetMotionMode='G',
      SetGotoTargetIncrement='H',
      SetBreakPointIncrement='M',
      SetBreakSteps='U',
      SetStepPeriod='I',
      StartMotion='J',
      GetStepPeriod='D', // See Merlin protocol http://www.papywizard.org/wiki/DevelopGuide
      ActivateMotor='B', // See eq6direct implementation http://pierre.nerzic.free.fr/INDI/
      SetGuideRate='P',  // See EQASCOM driver
      Deactivate='d',
    */
  return false;
}

bool Logger::updateProperties(bool debugenable, INDI::DefaultDevice *device) 
{
  if (debugenable) 
    {
      unsigned int i;
      for (i=0; i<nlevels; i++) {
	IUFillSwitch(&DebugLevelS[i], DebugLevelSInit[i].name,DebugLevelSInit[i].label,DebugLevelSInit[i].state); 
	DebugLevelS[i].aux = (void *)&DebugLevelSInit[i].levelmask;
#ifdef WITH_LOGGER
	IUFillSwitch(&LoggingLevelS[i], LoggingLevelSInit[i].name,LoggingLevelSInit[i].label,LoggingLevelSInit[i].state); 
        LoggingLevelS[i].aux = (void *)&LoggingLevelSInit[i].levelmask;
#endif
      }
#ifdef WITH_N0FMANY 
      IUFillSwitchVector(&DebugLevelSP, DebugLevelS, nlevels, device->getDeviceName(), "DEBUG_LEVEL" , "Debug Levels", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);
#ifdef WITH_LOGGER
      IUFillSwitchVector(&LoggingLevelSP, LoggingLevelS, nlevels, device->getDeviceName(), "LOGGING_LEVEL" , "Logging Levels", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);
#endif
#else
      IUFillSwitchVector(&DebugLevelSP, DebugLevelS, nlevels, device->getDeviceName(), "DEBUG_LEVEL" , "Debug Levels", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
#ifdef WITH_LOGGER
      IUFillSwitchVector(&LoggingLevelSP, LoggingLevelS, nlevels, device->getDeviceName(), "LOGGING_LEVEL" , "Log Levels", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
#endif
#endif
      device->defineSwitch(&DebugLevelSP);
#ifndef WITH_LOGGER
      level=rememberlevel;
#else
      device->defineSwitch(&LoggingLevelSP);
      screenVerbosityLevel_=rememberscreenlevel_;
#endif
    } 
  else 
    {
      device->deleteProperty(DebugLevelSP.name);
#ifndef WITH_LOGGER
      rememberlevel=level;
      level=defaultlevel;
#else
      device->deleteProperty(LoggingLevelSP.name);
      rememberscreenlevel_=screenVerbosityLevel_;
      screenVerbosityLevel_=defaultlevel;
#endif

    }

  return true;
}

bool Logger::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
  if(strcmp(name,"DEBUG_LEVEL")==0)
    {  
      ISwitch *sw;
      IUUpdateSwitch(&DebugLevelSP,states,names,n);
      sw=IUFindOnSwitch(&DebugLevelSP);
#ifndef WITH_LOGGER
#ifdef WITH_N0FMANY 
      level ^= *((unsigned int *)sw->aux);
#else
      level = *((unsigned int *)sw->aux);
#endif
#else
#ifdef WITH_N0FMANY 
      screenVerbosityLevel_ ^= *((unsigned int *)sw->aux);
#else
      screenVerbosityLevel_ = *((unsigned int *)sw->aux);
#endif
#endif
      DEBUGFDEVICE(dev, Logger::DBG_DEBUG, "Toggle Debug Level -- %s", sw->label);
      DebugLevelSP.s=IPS_IDLE;
      IDSetSwitch(&DebugLevelSP,NULL);
      return true;
    }
#ifdef WITH_LOGGER
  if(strcmp(name,"LOGGING_LEVEL")==0)
    {  
      ISwitch *sw;
      IUUpdateSwitch(&LoggingLevelSP,states,names,n);
      sw=IUFindOnSwitch(&LoggingLevelSP);
#ifdef WITH_N0FMANY 
      fileVerbosityLevel_ ^= *((unsigned int *)sw->aux);
#else
      fileVerbosityLevel_ = *((unsigned int *)sw->aux);
#endif
      DEBUGFDEVICE(dev, Logger::DBG_DEBUG, "Toggle Logging Level -- %s", sw->label);
      LoggingLevelSP.s=IPS_IDLE;
      IDSetSwitch(&LoggingLevelSP,NULL);
      return true;
    }
#endif
  return true;
}

#ifndef WITH_LOGGER
unsigned int Logger::level = Logger::defaultlevel;
unsigned int Logger::rememberlevel = Logger::defaultlevel;
#endif

#ifdef WITH_LOGGER
#include <iostream>
#include <new>
#include <cstdlib>
#include <stdarg.h>

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
 * \brief Method to configure the logger. Called by the DEBUG_CONF() macro.
 * To make implementation easier, the old stream is always closed.
 * Then, in case, it is open again in append mode.
 * @param Name of the file used for logging
 * @param Configuration (i.e., log on file and on screen on or off)
 * @param Verbosity threshold for file
 * @param Verbosity threshold for screen
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
		if (outputFile != logFile_){
			std::ostringstream oss;
			time_t currTime;
			time(&currTime);
			struct tm *currTm = localtime(&currTime);
			oss << outputFile << "_" << timestamp() << ".log";
			  //		currTm->tm_mday << "_" <<
			  //		currTm->tm_mon << "_" <<
			  //		(1900 + currTm->tm_year) << "_" <<
			  //		currTm->tm_hour << "-" <<
			  //		currTm->tm_min << "-" <<
			  //		currTm->tm_sec << ".log";
			logFile_ = oss.str().c_str();
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
 * \brief Method used to print messages.
 * Called by the DEBUG() macro.
 * @param Priority of the message
 * @param Source file where the method has been called (set equal to __FILE__
 * 	      by the DEBUG macro)
 * @param Source line where the method has been called (set equal to __LINE__
          by the macro)
 * @param Message
 */
unsigned int Logger::rank(unsigned int l) {
  switch(l) {
  case DBG_ERROR: return 0;
  case DBG_WARNING: return 1;
  case DBG_SESSION: return 2;
  case DBG_DEBUG: return 3;
  case DBG_MOUNT: return 4;
  case DBG_COMM: return 5;
  case DBG_CALL: return 6;
  case DBG_SCOPE_STATUS: return 7;
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
#ifdef WITH_NOFMANY
  bool filelog = (verbosityLevel & fileVerbosityLevel_) != 0;
  bool screenlog = (verbosityLevel & screenVerbosityLevel_) != 0;
#else
  bool filelog = (verbosityLevel <= fileVerbosityLevel_);
  bool screenlog = (verbosityLevel <= screenVerbosityLevel_);
#endif

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

#endif /* WITHLOGGER */
