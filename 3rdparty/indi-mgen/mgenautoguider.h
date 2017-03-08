#ifndef MGENAUTOGUIDER_H
#define MGENAUTOGUIDER_H

/*
   INDI Developers Manual
   Tutorial #1

   "Hello INDI"

   We construct a most basic (and useless) device driver to illustate INDI.

   Refer to README, which contains instruction on how to build this driver, and use it
   with an INDI-compatible client.

*/

/** \file mgenautoguider.h
    \brief Construct a basic INDI device for the Lacerta MGen Autoguider.
    \author TallFurryMan

    \example mgenautoguider.h
    A very minimal autoguider! It connects/disconnects to the autoguider, and manages operational modes.
*/


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


class MGenAutoguider : public INDI::CCD
{
public:
    MGenAutoguider();
    virtual ~MGenAutoguider();

public:

protected:
    //ITextVectorProperty *DevPathSP;
    char dev_path[MAXINDINAME];
    int fd;

    class DeviceState
    {
    protected:
        pthread_mutex_t _lock;
    public:
        bool lock() { return !pthread_mutex_lock(&_lock); }
        void unlock() { pthread_mutex_unlock(&_lock); }

    protected:
        bool is_active;
    public:
        bool isActive() const { return is_active; }
        void enable() { if(lock()) { is_active = true; unlock(); } }
        void disable() { if(lock()) { is_active = false; unlock(); } }

    protected:
        IOMode mode;
    public:
        IOMode getOpMode() const { return mode; }
        void setOpMode(IOMode _mode) { if(lock()) { mode = _mode; unlock(); } }

    protected:
        std::queue<unsigned int> button_queue;
    public:
        bool hasButtons() const { return !button_queue.empty(); }
        void pushButton(unsigned int _button) { if(lock()) { button_queue.push(_button); unlock(); } }
        unsigned int popButton() { unsigned int b = -1; if(lock()) { b = button_queue.front(); button_queue.pop(); unlock(); } return b; }

    public:
        bool tried_turn_on;
        void * usb_channel;

    public:
        unsigned int no_ack_count;

    public:
        struct version
        {
            unsigned short uploaded_firmware;
            unsigned short camera_firmware;
        } version;

    public:
        struct voltage
        {
            time_t timestamp;
            float logic;
            float input;
            float reference;
        } voltage;

    public:
        struct ui_frame
        {
            time_t timestamp;
        } ui_frame;

    public:
        struct heartbeat
        {
            time_t timestamp;
        } heartbeat;

    public:
        DeviceState():
            is_active(false),
            tried_turn_on(false),
            usb_channel(NULL),
            no_ack_count(0),
            mode(OPM_UNKNOWN),
            button_queue(),
            version({0}),
            voltage({0}),
            ui_frame({0}),
            heartbeat({0})
        {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&_lock,&attr);
            pthread_mutexattr_destroy(&attr);
        }
        virtual ~DeviceState()
        {
            pthread_mutex_destroy(&_lock);
        }
    } connectionStatus;

protected:
    IText VersionFirmwareT;
    ITextVectorProperty VersionTP;
    INumber VoltageN[3];
    INumberVectorProperty VoltageNP;
    INumber UIFramerateN;
    INumberVectorProperty UIFramerateNP;
    ISwitch UIButtonS[6];
    ISwitchVectorProperty UIButtonSP[2];
    IBLOB UIFrameB;
    IBLOBVectorProperty UIFrameBP;
    bool initProperties();

public:
    virtual bool ISNewNumber(char const * dev, char const * name, double values[], char * names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n);

protected:
    bool Connect();
    bool Disconnect();

protected:
    const char *getDefaultName();

public:
    /* @brief Write the query field of a command to the device.
     * @return the number of bytes written, or -1 if the command is invalid or device is not accessible.
     * @throw MGC::IOError when device communication is malfunctioning.
     */
    int write(IOBuffer const &) throw (IOError);

    /* @brief Read the answer part of a command from the device.
     * @return the number of bytes read, or -1 if the command is invalid or device is not accessible.
     * @throw MGC::IOError when device communication is malfunctioning.
     */
    int read(IOBuffer &) throw (IOError);

protected:
    static void* connectionThreadWrapper( void* );
    void connectionThread();

    friend class MGC;

protected:
    int heartbeat(struct ftdi_context * const ftdi);
    int setOpModeBaudRate(struct ftdi_context * const ftdi, IOMode const mode);
};

#endif // MGENAUTOGUIDER_H
