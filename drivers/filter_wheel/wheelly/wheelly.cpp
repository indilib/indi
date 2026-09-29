// SPDX-FileCopyrightText: 2026 Matteo Beretta
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "wheelly.h"
#include "translations.h"
#include "wheelly_config.h"

#include "connectionplugins/connectionserial.h"
#include "indicom.h"
#include "indilogger.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <vector>
#include <cstring>
#include <ctime>
#include <memory>
#include <climits>
#include <cstdlib>
#include <dirent.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace wheelly;

// The names of the tabs stay in English: they sit in the same tab bar that
// INDI creates by itself - Connection, Options - which we cannot translate.
// Everything inside them is translated. See translations.h.
static const char *TAB_MAIN = "Main Control";
// The name an empty filter-name field becomes, followed by "_<slot>": the
// reasons are where it is used, in ISNewText.
static const char *EMPTY_SLOT_NAME = "Empty";
// "Calibration and Diagnostics" and not "Calibration":
// the tab holds the teaching and the checks - angles, jogs, tolerances,
// hardware check, magnet sweep, movement log. The settings of the machine -
// motor, holding current, LED - moved to Options. "and", not "&": a Qt tab
// label takes "&" as a keyboard shortcut marker and would drop it.
static const char *TAB_CALIBRATION = "Calibration and Diagnostics";

// The jog rows: the steps, the element names and the
// translation keys of the labels, in the same order. A step of 0 is the SLOT
// PITCH, 360/slots, signed by its row: its value and its label follow the
// number of slots (see label_jogs), so the element name says "pitch", not a
// number. No 0.05 step (rejected): below the AS5600's count (0.088
// degrees) a jog that did not move cannot be told from one that did.
static const double JOG_STEPS[] = {0, -10, -1, -0.1, 0.1, 1, 10, 0};
static const char *JOG_NAMES[] = {"JOG_M_PITCH", "JOG_M10", "JOG_M1", "JOG_M0_1",
                                  "JOG_P0_1", "JOG_P1", "JOG_P10", "JOG_P_PITCH"
                                 };
static const char *JOG_LABELS[] = {"prop.jog.mpitch", "prop.jog.m10", "prop.jog.m1", "prop.jog.m01",
                                   "prop.jog.p01", "prop.jog.p1", "prop.jog.p10", "prop.jog.ppitch"
                                  };
static const char *TAB_OPTIONS     = "Options";

static const int ANSWER_WAIT_MS = 3000;
// The pause between two attempts to find a lost wheel again: from the first,
// doubling up to the last. A second is about the time a XIAO takes to boot
// and enumerate; ten keeps a wheel left unplugged from costing anything.
static const double RETRY_FIRST_S = 1.0;
static const double RETRY_MAX_S = 10.0;

// The driver's ceiling on a move. Better that we say it, with a message that
// explains: if Ekos says it, all that is left in the log is "timeout". It
// follows the WHEEL's cap, which is derived from speed and
// direction (one way round the longest path is nearly two turns, not half of
// one): the wheel's number plus this margin, for the serial line and the
// polling period. 28 s stays only for a firmware that does not tell its cap.
static const double MOVE_CEILING_S = 28.0;
static const double MOVE_CEILING_MARGIN_S = 3.0;

// Every query during the sweep is a sample of the magnet.
static const int SAMPLE_INTERVAL_MS = 100;

static std::unique_ptr<Wheelly> device(new Wheelly());

static double now_s()
{
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
}

// ------------------------------------------------------------------ LineReader

void LineReader::attach(int descriptor)
{
    m_fd = descriptor;
    m_rest.clear();
    m_down = false;
    m_errno = 0;
}

void LineReader::flush()
{
    m_rest.clear();
    if (m_fd < 0) return;
    // Whatever was left on the channel from before is thrown away: at opening
    // there can be anything, including the leftovers of a previous session.
    std::string rubbish;
    while (line(rubbish, 50)) { }
}

bool LineReader::line(std::string &out, int wait_ms)
{
    if (m_fd < 0) return false;

    const double deadline = now_s() + wait_ms / 1000.0;
    for (;;)
    {
        const size_t end = m_rest.find('\n');
        if (end != std::string::npos)
        {
            out = m_rest.substr(0, end);
            m_rest.erase(0, end + 1);
            while (!out.empty() && (out.back() == '\r' || out.back() == ' '))
                out.pop_back();
            return true;
        }

        const double left = deadline - now_s();
        if (left <= 0) return false;

        fd_set set;
        FD_ZERO(&set);
        FD_SET(m_fd, &set);
        struct timeval how_long;
        how_long.tv_sec = (time_t)left;
        how_long.tv_usec = (suseconds_t)((left - (double)how_long.tv_sec) * 1e6);

        const int ready = select(m_fd + 1, &set, nullptr, nullptr, &how_long);
        if (ready < 0)
        {
            if (errno == EINTR) continue;
            m_down = true;
            m_errno = errno;
            return false;
        }
        if (ready == 0) return false;

        char chunk[512];
        const ssize_t got = read(m_fd, chunk, sizeof(chunk));
        if (got <= 0)
        {
            if (got < 0 && (errno == EAGAIN || errno == EINTR)) continue;
            m_down = true;
            m_errno = got < 0 ? errno : 0;
            return false;
        }
        m_rest.append(chunk, (size_t)got);
        // An absurdly long line means that on the other side there is not
        // what we think: better to truncate than to grow without end.
        if (m_rest.size() > 8 * WHEELLY_LINE_MAX) m_rest.clear();
    }
}

// --------------------------------------------------------------------- Wheelly

Wheelly::Wheelly()
{
    setVersion(WHEELLY_VERSION_MAJOR, WHEELLY_VERSION_MINOR);
    setFilterConnection(CONNECTION_SERIAL);
}

const char *Wheelly::getDefaultName()
{
    // The name comes from wheelly_config.h, that is from a CMake variable: it is a
    // trademark, and the policy in TRADEMARK.md asks that a modified
    // derivative is not called that. For that rule to be enforceable and not
    // only written, the name must live in one place only.
    return WHEELLY_DEVICE_NAME;
}

bool Wheelly::initProperties()
{
    // The language is chosen BEFORE everything else, because the labels are
    // composed here and the clients keep them for the whole connection.
    // Built English only (WHEELLY_ITALIAN off, as inside INDI's tree) there is
    // nothing to choose, and the switch is neither made nor shown.
#if WHEELLY_ITALIAN
    LanguageSP[0].fill("AUTO", "", ISS_ON);
    LanguageSP[1].fill("EN", "", ISS_OFF);
    LanguageSP[2].fill("IT", "", ISS_OFF);
    LanguageSP.fill(getDeviceName(), "WHEELLY_LANGUAGE", "", TAB_OPTIONS,
                    IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    LanguageSP.load();
    switch (LanguageSP.findOnSwitchIndex())
    {
        case 1:
            set_language(Language::ENGLISH);
            break;
        case 2:
            set_language(Language::ITALIAN);
            break;
        default:
            set_language(Language::AUTO);
            break;
    }
    // now that the language is there, the labels
    LanguageSP[0].setLabel(tr("prop.language.auto"));
    LanguageSP[1].setLabel(tr("prop.language.en"));
    LanguageSP[2].setLabel(tr("prop.language.it"));
    LanguageSP.setLabel(tr("prop.language"));
#else
    set_language(Language::ENGLISH);
#endif

    INDI::FilterWheel::initProperties();

    if (serialConnection != nullptr)
    {
        serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
        // The ESP32-S3 presents itself with Espressif's code: this way the
        // port is found by INDI and not by the user, in the dark, trying them all.
        // "espressif": on Linux the ports under /dev/serial/by-id/ carry the
        // vendor name, not its id 303a (as seen on AstroArch)
        serialConnection->setPortMatchPattern("303a|espressif|wheelly|usbmodem");
    }

    // The two properties of the base class speak the user's language and
    // sit in the main panel, together with the rest of the everyday
    // commands.
    FilterSlotNP.setLabel(tr("prop.slot"));
    FilterSlotNP.setGroupName(TAB_MAIN);

    // --- main panel: where the wheel is, and how the sensor is doing ------
    PositionNP[0].fill("ANGLE", tr("prop.position.angle"), "%.2f", 0, 360, 0, 0);
    PositionNP[1].fill("ERROR", tr("prop.position.err"), "%.2f", -180, 180, 0, 0);
    PositionNP[2].fill("RETRIES", tr("prop.position.retries"), "%.0f", 0, 99, 0, 0);
    PositionNP.fill(getDeviceName(), "WHEELLY_POSITION", tr("prop.position"),
                    TAB_MAIN, IP_RO, 60, IPS_IDLE);

    SensorNP[0].fill("AGC", tr("prop.sensor.agc"), "%.0f", 0, 255, 0, 0);
    SensorNP[1].fill("MAGNITUDE", tr("prop.sensor.mag"), "%.0f", 0, 4095, 0, 0);
    SensorNP[2].fill("MD", tr("prop.sensor.md"), "%.0f", 0, 1, 0, 0);
    SensorNP[3].fill("ML", tr("prop.sensor.ml"), "%.0f", 0, 1, 0, 0);
    SensorNP[4].fill("MH", tr("prop.sensor.mh"), "%.0f", 0, 1, 0, 0);
    SensorNP.fill(getDeviceName(), "WHEELLY_SENSOR", tr("prop.sensor"),
                  TAB_MAIN, IP_RO, 60, IPS_IDLE);

    // --- calibration panel ------------------------------------------------
    // Every possible element is prepared: how many are really needed the
    // wheel will say at connection, and the others go away.
    // One property per slot, one element each (see AngleNP in wheelly.h).
    AngleNP.reserve(MAX_SLOTS);
    for (int i = 0; i < MAX_SLOTS; i++)
    {
        // 32 and not 16: with 16 gcc warns that the name MIGHT overflow.
        // It cannot - i goes from 0 to MAX_SLOTS - but the compiler cannot
        // narrow the integer type, and a warning one already knows to ignore
        // is a warning that one day hides a real one.
        char name[32];
        snprintf(name, sizeof(name), "WHEELLY_ANGLE_%d", i + 1);
        AngleNP.emplace_back(1);
        AngleNP[i][0].fill("ANGLE", tr("prop.angle.value"), "%.2f", 0, ANGLE_MAX_DEG, 0.01, 0);
        // read-write: KStars shows an edit field and a Set on every row
        AngleNP[i].fill(getDeviceName(), name, std::to_string(i + 1).c_str(),
                        TAB_CALIBRATION, IP_RW, 60, IPS_IDLE);
    }
    SlotsNP[0].fill("COUNT", tr("prop.slots.count"), "%.0f", wheelly::MIN_SLOTS, wheelly::MAX_SLOTS, 1,
                    wheelly::FACTORY_SLOTS);
    SlotsNP.fill(getDeviceName(), "WHEELLY_SLOTS", tr("prop.slots"),
                 TAB_CALIBRATION, IP_RW, 60, IPS_IDLE);
    ActionsSP[0].fill("SAVE", tr("prop.actions.save"), ISS_OFF);
    ActionsSP.fill(getDeviceName(), "WHEELLY_SAVE", tr("prop.actions"),
                   TAB_CALIBRATION, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    WheelConfigSP[0].fill("SAVE", tr("prop.actions.save"), ISS_OFF);
    WheelConfigSP.fill(getDeviceName(), "WHEELLY_CONFIG", tr("prop.wheelconfig"),
                       TAB_OPTIONS, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // The jog buttons: the calibration is done here - jog until the filter
    // is centred, then Set on its row, which has followed the jogs. The
    // element names carry the steps, so a script can read what each does.
    // Four to a row: KStars turns five or more into a drop-down menu.
    for (int i = 0; i < JOG_PER_ROW; i++)
    {
        JogDownSP[i].fill(JOG_NAMES[i], tr(JOG_LABELS[i]), ISS_OFF);
        JogUpSP[i].fill(JOG_NAMES[JOG_PER_ROW + i], tr(JOG_LABELS[JOG_PER_ROW + i]), ISS_OFF);
    }
    label_jogs();
    JogDownSP.fill(getDeviceName(), "WHEELLY_JOG_DOWN", tr("prop.jog.down"),
                   TAB_CALIBRATION, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    JogUpSP.fill(getDeviceName(), "WHEELLY_JOG_UP", tr("prop.jog.up"),
                 TAB_CALIBRATION, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // limits and factory values from the shared header, the same the wheel
    // uses (factory values there: 0.30 / 0.80)
    ToleranceNP[0].fill("GOOD", tr("prop.tolerance.good"), "%.2f", 0.01, TOLERANCE_MAX_DEG, 0.01, FACTORY_GOOD_DEG);
    ToleranceNP[1].fill("WARN", tr("prop.tolerance.warn"), "%.2f", 0.01, TOLERANCE_MAX_DEG, 0.01, FACTORY_WARN_DEG);
    ToleranceNP[2].fill("RETRIES", tr("prop.tolerance.retries"), "%.0f", 0, RETRIES_MAX, 1, 3);
    ToleranceNP.fill(getDeviceName(), "WHEELLY_TOLERANCE", tr("prop.tolerance"),
                     TAB_CALIBRATION, IP_RW, 60, IPS_IDLE);

    // the limits are the wheel's own, from the shared header: numbers
    // written here would drift from the ones the wheel checks
    MotorNP[0].fill("RUN_MA", tr("prop.motor.ma"), "%.0f", 1, MOTOR_MA_MAX, 5, FACTORY_RUN_MA);
    MotorNP[1].fill("SPEED", tr("prop.motor.speed"), "%.0f", 1, MOTOR_SPEED_MAX, 10, FACTORY_SPEED);
    MotorNP[2].fill("ACCEL", tr("prop.motor.accel"), "%.0f", 1, MOTOR_ACCEL_MAX, 50, FACTORY_ACCEL);
    MotorNP.fill(getDeviceName(), "WHEELLY_MOTOR", tr("prop.motor"),
                 TAB_OPTIONS, IP_RW, 60, IPS_IDLE);

    // The field shows the wheel's REAL value, 0 included, read back at
    // connection. Rejected: starting from the recommended 150 mA and not
    // writing a 0 read from the wheel, leaving the "off" to the grey light
    // alone - a user who saved 0 then sees 150 and believes the wheel has
    // gone back to 150. A field that shows a number the wheel does not have
    // is a lie, however good the intention. The recommendation lives where it
    // is needed - in the drift message (msg.drift.suggest), said when the
    // wheel moves on its own at rest - and in the panel guide.
    HoldNP[0].fill("HOLD_MA", tr("prop.hold.ma"), "%.0f", 0, 800, 5, 0);
    // How long the motor stays energised, at the run current, after every
    // leg - even with the current at rest at 0 - before the wheel reads its
    // verdict and lets go: the stopped motor brakes the disc
    // through the tyre. Starts at the factory value, which is also what a
    // firmware without the setting does; the wheel's own is read at connection.
    HoldNP[1].fill("SETTLE_MS", tr("prop.hold.settle"), "%.0f", 0, SETTLE_MS_MAX, 50,
                   FACTORY_SETTLE_MS);
    HoldNP.fill(getDeviceName(), "WHEELLY_HOLD", tr("prop.hold"),
                TAB_OPTIONS, IP_RW, 60, IPS_IDLE);

    // Starts on the factory direction (FACTORY_DIRECTION in the shared
    // header: the shortest way); Idle until
    // the wheel's own value is read at connection.
    DirectionSP[0].fill("DIR_SHORTEST", tr("prop.direction.shortest"),
                        strcmp(FACTORY_DIRECTION, ARG_SHORTEST) == 0 ? ISS_ON : ISS_OFF);
    DirectionSP[1].fill("DIR_UP", tr("prop.direction.up"),
                        strcmp(FACTORY_DIRECTION, ARG_UP) == 0 ? ISS_ON : ISS_OFF);
    DirectionSP[2].fill("DIR_DOWN", tr("prop.direction.down"),
                        strcmp(FACTORY_DIRECTION, ARG_DOWN) == 0 ? ISS_ON : ISS_OFF);
    DirectionSP.fill(getDeviceName(), "WHEELLY_DIRECTION", tr("prop.direction"),
                     TAB_OPTIONS, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Four choices in the property that was already there, not a new one:
    // the panel stays lean. "Pulse" is the default: off
    // at rest, breathing while the wheel goes to another slot; "Off" is off
    // for good, pulse included, for whoever wants no light near the optics.
    // The element names are identifiers: LED_ON and LED_OFF keep the names
    // they had, so a saved Ekos configuration still means the same thing.
    LedSP[0].fill("LED_ON", tr("prop.led.on"), ISS_OFF);
    LedSP[1].fill("LED_PULSE", tr("prop.led.pulse"), ISS_ON);
    LedSP[2].fill("LED_OFF", tr("prop.led.off"), ISS_OFF);
    LedSP[3].fill("LED_TEST", tr("prop.led.test"), ISS_OFF);
    LedSP.fill(getDeviceName(), "WHEELLY_LED", tr("prop.led"),
               TAB_OPTIONS, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    DiagSP[0].fill("RUN", tr("prop.diag.run"), ISS_OFF);
    DiagSP.fill(getDeviceName(), "WHEELLY_DIAG", tr("prop.diag"),
                TAB_CALIBRATION, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    LogSP[0].fill("LOG_ON", tr("prop.log.on"), ISS_OFF);
    LogSP[1].fill("LOG_OFF", tr("prop.log.off"), ISS_ON);
    LogSP.fill(getDeviceName(), "WHEELLY_LOG", tr("prop.log"),
               TAB_CALIBRATION, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    SweepSP[0].fill("RUN", tr("prop.sweep.run"), ISS_OFF);
    SweepSP.fill(getDeviceName(), "WHEELLY_SWEEP", tr("prop.sweep"),
                 TAB_CALIBRATION, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    // The format says ".wheelly.png" and not ".png", and the difference is not
    // a whim. KStars, when it receives a BLOB whose extension is an image
    // format Qt can read, OPENS A WINDOW to show it - and that window is a
    // child of the main one just as the INDI panel is, so closing it takes the
    // panel away with it. With an extension Qt does not recognise, KStars only
    // saves the file. The name still ends in .png, so any viewer opens it
    // anyway.
    SweepBP[0].fill("PLOT", tr("prop.sweep.plot"), ".wheelly.png");
    SweepBP.fill(getDeviceName(), "WHEELLY_SWEEP_PLOT", tr("prop.sweep.image"),
                 TAB_CALIBRATION, IP_RO, 60, IPS_IDLE);

    FileTP[FILE_LOG].fill("PATH", tr("prop.files.log"), log_path().c_str());
    FileTP[FILE_SWEEP].fill("SWEEP", tr("prop.files.sweep"), tr("prop.files.none"));
    FileTP.fill(getDeviceName(), "WHEELLY_FILES", tr("prop.files"),
                TAB_CALIBRATION, IP_RO, 60, IPS_IDLE);

    SweepDirTP[0].fill("DIR", tr("prop.sweepdir.path"), default_folder().c_str());
    SweepDirTP.fill(getDeviceName(), "WHEELLY_SWEEP_DIR", tr("prop.sweepdir"),
                    TAB_CALIBRATION, IP_RW, 60, IPS_IDLE);
    SweepDirTP.load();

    // --- options -----------------------------------------------------------
    FirmwareTP[0].fill("FW", tr("prop.firmware.version"), "");
    FirmwareTP[1].fill("PROTO", tr("prop.firmware.proto"), "");
    FirmwareTP[2].fill("SERIAL", tr("prop.firmware.serial"), "");
    FirmwareTP.fill(getDeviceName(), "WHEELLY_FIRMWARE", tr("prop.firmware"),
                    TAB_OPTIONS, IP_RO, 60, IPS_IDLE);

    ToleranceNP.load();
    LogSP.load();
    // The serial number of this profile's wheel, if one is already saved.
    FirmwareTP.load();
    if (FirmwareTP[2].getText() != nullptr)
        m_expected_serial = FirmwareTP[2].getText();

    // The dialogue with the XIAO can be seen by turning on *Driver Debug* in
    // the Options tab: every line exchanged, in both directions, without
    // recompiling anything. It is the first thing to turn on when the wheel
    // does something unexpected.
    //
    // Rejected: a level of its own, with
    // addDebugLevel("Protocol Verbose", "PROTO"), a road that looks
    // right and does not work: in libindi 2.2 that call writes into the slot
    // DBG_EXTRA_2, but the DEBUG_LEVEL property publishes only five of them and
    // stops at DBG_EXTRA_1, which stays labelled "Alignment Subsystem". The
    // level existed, the switch to turn it on did not - that is, the feature
    // meant to explain trouble was unreachable exactly when it was needed. Verified with a probe that prints the table of levels.
    // addAuxControls() by hand, to put "Wheel configuration" right under
    // INDI's "Configuration": a client lays out a
    // group in the order the properties are defined, and the device defines
    // them in the order they were registered. It is always there, like
    // "Configuration": pressed with the wheel disconnected, it says so.
    addDebugControl();
    addSimulationControl();
    addConfigurationControl();
    defineProperty(WheelConfigSP);
    addPollPeriodControl();
    setDefaultPollingPeriod(250);
    return true;
}

bool Wheelly::updateProperties()
{
    if (isConnected())
    {
        // The surplus elements are removed BEFORE the properties are defined,
        // otherwise the client finds itself with twelve angles on a five-slot wheel.
        size_slots();
        label_jogs();
        SlotsNP[0].setValue(m_slots);
        FilterSlotNP[0].setMin(1);
        FilterSlotNP[0].setMax(m_slots);
        FilterSlotNP[0].setStep(1);
        FilterSlotNP.updateMinMax();
    }

    // THE NAMES ARE THE WHEEL'S, ALWAYS. libindi loads
    // FILTER_NAME from the configuration when the driver starts, and asks
    // GetFilterNames() only when it has none: left to itself, from the second
    // start the panel shows the names saved on the PC - stale, if the wheel
    // was renamed elsewhere or reflashed - and m_names stays empty, so the
    // first Set writes every name again. Asked here, before the base class defines the property.
    if (isConnected())
        GetFilterNames();

    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineProperty(FirmwareTP);
        // THE SAVING GOES HERE AND NOT IN THE HANDSHAKE. Inside Handshake() the
        // property is not DEFINED yet - this line defines it, and it runs only
        // on a successful connection - and saveConfig() on a property the
        // device has not registered yet writes nothing and does not complain.
        // Saved in the handshake, the message "from now on I look for this
        // serial number" comes out in the log, and the configuration keeps the
        // previous serial number.
        if (m_serial_to_save)
        {
            m_serial_to_save = false;
            LOGF_INFO("%s", trf("msg.serial.learned", {m_expected_serial}).c_str());
            saveConfig(true, FirmwareTP.getName());
        }
        defineProperty(PositionNP);
        defineProperty(SensorNP);
        defineProperty(SlotsNP);
        define_angles();
        defineProperty(JogDownSP);
        defineProperty(JogUpSP);
        defineProperty(ActionsSP);
        defineProperty(ToleranceNP);
        defineProperty(MotorNP);
        defineProperty(HoldNP);
        defineProperty(DirectionSP);
        defineProperty(LedSP);
        defineProperty(DiagSP);
        defineProperty(SweepSP);
        defineProperty(SweepBP);
        defineProperty(SweepDirTP);
        defineProperty(LogSP);
        defineProperty(FileTP);
#if WHEELLY_ITALIAN
        defineProperty(LanguageSP);
#endif

        read_calibration();
        if (LogSP[0].getState() == ISS_ON) open_log();
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(FirmwareTP);
        deleteProperty(PositionNP);
        deleteProperty(SensorNP);
        deleteProperty(SlotsNP);
        delete_angles();
        deleteProperty(JogDownSP);
        deleteProperty(JogUpSP);
        deleteProperty(ActionsSP);
        deleteProperty(ToleranceNP);
        deleteProperty(MotorNP);
        deleteProperty(HoldNP);
        deleteProperty(DirectionSP);
        deleteProperty(LedSP);
        deleteProperty(DiagSP);
        deleteProperty(SweepSP);
        deleteProperty(SweepBP);
        deleteProperty(SweepDirTP);
        deleteProperty(LogSP);
        deleteProperty(FileTP);
#if WHEELLY_ITALIAN
        deleteProperty(LanguageSP);
#endif
        close_log();
    }
    return true;
}

// ------------------------------------------------------------------ dialogue

Wheelly::Fields Wheelly::split_fields(const std::string &line)
{
    Fields fields;
    size_t i = 0;
    // the first word is skipped: it is the outcome or the name of the event
    while (i < line.size() && line[i] != ' ') i++;
    while (i < line.size())
    {
        while (i < line.size() && line[i] == ' ') i++;
        const size_t start = i;
        while (i < line.size() && line[i] != ' ') i++;
        const std::string piece = line.substr(start, i - start);
        const size_t equals = piece.find('=');
        if (equals != std::string::npos && equals > 0)
            fields[piece.substr(0, equals)] = piece.substr(equals + 1);
    }
    return fields;
}

double Wheelly::number(const Fields &fields, const char *key, double if_missing)
{
    const auto found = fields.find(key);
    if (found == fields.end()) return if_missing;
    try
    {
        return std::stod(found->second);
    }
    catch (...)
    {
        return if_missing;
    }
}

void Wheelly::handle_unsolicited_line(const std::string &line)
{
    if (line.empty()) return;

    if (line[0] == PREFIX_COMMENT)
    {
        LOGF_DEBUG("<- %s", line.c_str());
        if (m_show_comments) LOGF_INFO("%s", line.c_str() + 1);
        return;
    }
    if (line[0] != PREFIX_EVENT) return;

    LOGF_DEBUG("<- %s", line.c_str());

    // The events are there to make the driver react quickly, not to inform:
    // everything they say is found again in 'status', which is what the driver
    // decides on. The only one handled apart is the lost magnet, because it is
    // serious trouble and has to be said when it happens, not at the next question.
    size_t i = 1;
    while (i < line.size() && line[i] == ' ') i++;
    const size_t end = line.find(' ', i);
    const std::string name = line.substr(i, end == std::string::npos ? std::string::npos : end - i);

    // The motor driver had to be set up again (EV_DRIVER in the protocol):
    // after a power-up in the guide's order - USB, then 12 V - it is the
    // normal case and only information; a reset with the wheel running means
    // the 12 V dropped, and is a warning.
    if (name == EV_DRIVER)
    {
        const Fields c = split_fields(line);
        const auto reason = c.find(F_REASON);
        if (reason != c.end() && reason->second == V_RESET) LOG_WARN(tr("msg.driver.reset"));
        else LOG_INFO(tr("msg.driver.power"));
    }

    if (name == EV_SENSOR && !m_magnet_lost)
    {
        m_magnet_lost = true;
        LOG_ERROR(tr("msg.magnet.lost"));
    }

    // The wheel moved on its own, at rest. The firmware says so once per
    // episode, so there is no need to limit the frequency here: what happened
    // is said and THE REMEDY IS SUGGESTED, which is the reason the event
    // exists: holding at rest is exactly the thing that is missing. This is
    // the ONLY advice about holding: the detent is always removed, and a "no
    // detent and no holding current" warning would sound at every connection.
    //
    // The suggestion changes if holding is ALREADY on: telling "turn it on" to
    // someone who has already turned it on is advice that makes one lose trust
    // in all the others.
    if (name == EV_DRIFT)
    {
        const Fields c = split_fields(line);
        char deviation[16];
        snprintf(deviation, sizeof(deviation), "%.2f", std::fabs(number(c, F_ERR, 0)));
        const bool holding_on = HoldNP.getState() == IPS_OK
                                && HoldNP[0].getValue() > 0;
        if (holding_on)
        {
            char how_much[16];
            snprintf(how_much, sizeof(how_much), "%.0f", HoldNP[0].getValue());
            LOGF_WARN("%s", trf("msg.drift.holding", {deviation, how_much}).c_str());
        }
        else
        {
            LOGF_WARN("%s", trf("msg.drift.suggest",
            {deviation, std::to_string(RECOMMENDED_HOLD_MA)}).c_str());
        }
    }
}

void Wheelly::collect_unsolicited()
{
    std::string line;
    while (m_reader.line(line, 0)) handle_unsolicited_line(line);
}

bool Wheelly::command(const std::string &text, Fields *fields,
                      std::string *error_for_user)
{
    // The link is down and TimerHit is looking for the wheel: said once per
    // command the user gives, never per poll (TimerHit does not poll then).
    if (m_link_lost && !m_reconnecting)
    {
        LOG_WARN(tr("msg.link.down"));
        return false;
    }
    if (PortFD < 0) return false;

    LOGF_DEBUG("-> %s", text.c_str());

    const std::string to_send = text + "\n";
    int written = 0;
    const int result = tty_write_string(PortFD, to_send.c_str(), &written);
    if (result != TTY_OK)
    {
        // errno first: anything called after the failed write may change it
        const int error_number = errno;
        if (!m_reconnecting && link_is_gone(error_number))
        {
            link_lost(strerror(error_number));
            return false;
        }
        char explanation[MAXRBUF];
        tty_error_msg(result, explanation, MAXRBUF);
        if (!m_reconnecting) LOGF_ERROR("%s (%s)", tr("msg.no.answer"), explanation);
        return false;
    }

    // Lines are read until one arrives that does not start with '#' or '!':
    // those are informative and do not close anything.
    std::string line;
    while (m_reader.line(line, ANSWER_WAIT_MS))
    {
        if (line.empty()) continue;
        if (line[0] == PREFIX_COMMENT || line[0] == PREFIX_EVENT)
        {
            handle_unsolicited_line(line);
            continue;
        }
        LOGF_DEBUG("<- %s", line.c_str());

        if (line.compare(0, strlen(PREFIX_OK), PREFIX_OK) == 0)
        {
            if (fields != nullptr) *fields = split_fields(line);
            return true;
        }
        if (line.compare(0, strlen(PREFIX_ERROR), PREFIX_ERROR) == 0)
        {
            const Fields c = split_fields(line);
            int code = 0;
            sscanf(line.c_str(), "%*s %d", &code);
            const auto take = [&c](const char *k)
            {
                const auto t = c.find(k);
                return t == c.end() ? std::string() : t->second;
            };
            const std::string sentence = error_message(
                                             code, take(F_EXPECTED), take(F_GOT), take(F_REASON));
            if (error_for_user != nullptr) *error_for_user = sentence;
            else LOGF_ERROR("%s", sentence.c_str());
            if (fields != nullptr) *fields = c;
            return false;
        }
        // a line that is neither an outcome nor informative: noise on the channel
        LOGF_DEBUG("Unexpected line from the wheel: %s", line.c_str());
    }

    if (m_reconnecting) return false;       // try_reconnect() says what counts
    if (m_reader.channel_down() && link_is_gone(m_reader.down_errno()))
    {
        link_lost(m_reader.down_errno() ? strerror(m_reader.down_errno())
                  : tr("msg.link.hangup"));
        return false;
    }
    LOG_ERROR(tr("msg.no.answer"));
    return false;
}

// ------------------------------------------------------------------ connection

bool Wheelly::Connect()
{
    // WHEELLY IS RECOGNISED BY ITS PROTOCOL, NOT BY THE NAME OF THE PORT. The
    // name is not an identity: on macOS /dev/cu.usbmodem20114301 carries the
    // POSITION OF THE SOCKET, not the device, and moving the plug to another
    // port of the hub is enough to change it. Whoever opens KStars on the Mac
    // would find a name that yesterday belonged to something else.
    //
    // So: the candidate ports are opened, `version` is written, and the wheel
    // is recognised by what it answers. And since two wheels both answer
    // "wheelly", the SERIAL NUMBER is looked at, which the firmware derives
    // from the MAC and is different for every unit.
    //
    // Two passes, and the second is the one that avoids locking the user out:
    //
    //   1. ONLY this profile's wheel is accepted. With two wheels attached
    //      each one ends up on its own, whatever names the ports have taken;
    //   2. if that wheel is on no port - it stayed at home, or the board was
    //      replaced - it is tried again accepting any Wheelly, saying so in
    //      the log. Without this pass, changing the XIAO would mean a driver
    //      that no longer connects and no explanation of why.
    // a Connect by hand takes over from the automatic search
    m_link_lost = false;
    m_only_mine = !m_expected_serial.empty();
    if (INDI::FilterWheel::Connect()) return true;
    if (!m_only_mine) return false;

    m_only_mine = false;
    LOGF_WARN("%s", trf("msg.serial.notfound", {m_expected_serial}).c_str());
    return INDI::FilterWheel::Connect();
}

bool Wheelly::Handshake()
{
    m_reader.attach(PortFD);
    m_reader.flush();
    m_magnet_lost = false;

    // On a reconnection (try_reconnect) the refusals are not said: the
    // attempt is repeated every few seconds while the wheel boots, and the
    // loss has already been said once.
    Fields c;
    if (!command(CMD_VERSION, &c))
    {
        if (!m_reconnecting) LOG_ERROR(tr("msg.wrong.device"));
        return false;
    }
    if (c[F_NAME] != "wheelly")
    {
        if (!m_reconnecting) LOG_ERROR(tr("msg.wrong.device"));
        return false;
    }
    if (c[F_PROTO] != std::to_string(PROTOCOL_VERSION))
    {
        if (!m_reconnecting)
            LOGF_ERROR("%s", trf("msg.wrong.protocol",
        {c[F_PROTO], std::to_string(PROTOCOL_VERSION)}).c_str());
        return false;
    }

    // The right wheel, not any wheel: see Connect(). In the first pass a
    // Wheelly with the wrong serial number is discarded as if it were not a
    // Wheelly, and INDI moves on to the next port.
    const std::string serial = c.count(F_SERIAL) ? c[F_SERIAL] : std::string();
    if (m_only_mine && serial != m_expected_serial)
    {
        if (!m_reconnecting)
            LOGF_INFO("%s", trf("msg.serial.other", {serial, m_expected_serial}).c_str());
        return false;
    }
    // The very first connection, or a board replaced and found again in the
    // second pass: from now on this profile's wheel is this one. If it were not
    // adopted, every connection would pay two scanning rounds forever. It is
    // SAVED further down, after the property has been filled: saving it here
    // would write a still empty element, and it happened.
    if (!serial.empty() && serial != m_expected_serial)
    {
        m_expected_serial = serial;
        m_serial_to_save = true;
    }

    // How many slots the wheel has, the wheel says. A firmware older than the
    // field does not send it: in that case the factory value is kept, which is
    // better than the theoretical maximum.
    if (c.count(F_SLOTS))
    {
        const int count = (int)number(c, F_SLOTS, FACTORY_SLOTS);
        if (count >= MIN_SLOTS && count <= MAX_SLOTS)
        {
            m_slots = count;
        }
        else
        {
            // through the catalogue like every user-facing text, and with the
            // whole range: "max 12" alone read wrong for a wheel reporting 1
            LOGF_WARN("%s", trf("msg.slots.unsupported",
            {
                std::to_string(count), std::to_string(MIN_SLOTS),
                std::to_string(MAX_SLOTS)
            }).c_str());
        }
    }

    FirmwareTP[0].setText(c[F_FW].c_str());
    FirmwareTP[1].setText(c[F_PROTO].c_str());
    FirmwareTP[2].setText(c[F_SERIAL].c_str());

    if (!m_reconnecting)
    {
        LOGF_INFO("%s", trf("msg.connected", {c[F_FW], c[F_PROTO]}).c_str());
        m_port_in_use = serialConnection != nullptr ? serialConnection->port() : "";
        remember_alias();
    }
    return true;
}

// ------------------------------------------------- the USB link, lost and found

bool Wheelly::link_is_gone(int error_number)
{
    // The errors of a port that is no longer there: EIO is what a USB CDC
    // port unplugged under an open descriptor gives on Linux (seen on the
    // reference wheel: "Write Error: Input/output error"), ENXIO and ENODEV
    // a device node without its device, EBADF a descriptor closed, EPIPE
    // and 0 - end of file on a read - the other end hung up. A timeout is
    // not among them: a busy or rebooting wheel is still there.
    switch (error_number)
    {
        case 0:
        case EIO:
        case ENXIO:
        case ENODEV:
        case EBADF:
        case EPIPE:
            return true;
        default:
            return false;
    }
}

void Wheelly::link_lost(const std::string &why)
{
    if (m_link_lost) return;
    m_link_lost = true;
    LOGF_ERROR("%s", trf("msg.link.lost", {why}).c_str());

    // The dead descriptor is closed AT ONCE. Kept open, it also keeps the
    // device node busy, and the kernel gives the wheel coming back another
    // name (ttyACM1 became ttyACM2).
    if (m_own_fd >= 0)
    {
        tty_disconnect(m_own_fd);
        m_own_fd = -1;
    }
    else if (serialConnection != nullptr)
    {
        serialConnection->Disconnect();
    }
    PortFD = -1;
    m_reader.attach(-1);

    // What was under way cannot finish: said as failed, so that Ekos stops a
    // sequence instead of waiting for a filter that is not coming.
    if (m_moving)
    {
        m_moving = false;
        FilterSlotNP.setState(IPS_ALERT);
        FilterSlotNP.apply();
    }
    if (m_jogging)
    {
        m_jogging = false;
        if (m_jog_row != nullptr)
        {
            m_jog_row->setState(IPS_ALERT);
            m_jog_row->apply();
        }
    }
    if (m_sweeping) finish_sweep(false);
    // The angle shown is the last one read, no longer a live one: Alert
    // until the first status after the return.
    //
    // CONNECTION IS LEFT AS IT IS, On and Ok. Tried first: Alert on it, to
    // show the link down. libindi's isConnected() is true only for On AND
    // Ok, so with Alert the driver itself took the device for disconnected -
    // TimerHit stopped, and nothing looked for the wheel any more. For Ekos
    // the device simply stays: its filter manager is not torn down and built
    // again for a plug that comes back in two seconds.
    PositionNP.setState(IPS_ALERT);
    PositionNP.apply();
    SensorNP.setState(IPS_ALERT);
    SensorNP.apply();

    m_retry_pause_s = RETRY_FIRST_S;
    m_next_attempt = now_s() + m_retry_pause_s;
}

void Wheelly::remember_alias()
{
    // The /dev/serial/by-id/ link that leads to the port in use, if there is
    // one (Linux; on macOS the cu.usbmodem name already comes back the same).
    // Only links are read here - no port is opened - so it looks at nothing
    // but the one device the driver is already talking to.
    m_alias.clear();
    if (m_port_in_use.empty()) return;
    char resolved[PATH_MAX];
    if (realpath(m_port_in_use.c_str(), resolved) == nullptr) return;
    const char *folder = getenv("WHEELLY_SERIAL_BY_ID");   // the bench's own
    const std::string by_id = folder != nullptr ? folder : "/dev/serial/by-id";
    DIR *d = opendir(by_id.c_str());
    if (d == nullptr) return;
    while (struct dirent *e = readdir(d))
    {
        if (e->d_name[0] == '.') continue;
        const std::string link = by_id + "/" + e->d_name;
        char target[PATH_MAX];
        if (realpath(link.c_str(), target) != nullptr && strcmp(target, resolved) == 0)
        {
            m_alias = link;
            break;
        }
    }
    closedir(d);
    if (!m_alias.empty()) LOGF_DEBUG("Port %s is also %s", m_port_in_use.c_str(), m_alias.c_str());
}

bool Wheelly::try_reconnect()
{
    // The port the user chose, then its by-id link: the wheel coming back
    // under another ttyACMn is found by the link, which follows the USB
    // serial number. No other port is opened - that is Auto Search's
    // business, and the user's choice - and the wheel must answer with this
    // profile's serial number (m_only_mine): a reconnection never adopts a
    // different wheel.
    std::vector<std::string> paths;
    if (serialConnection != nullptr && serialConnection->port() != nullptr)
        paths.push_back(serialConnection->port());
    if (!m_alias.empty() && std::find(paths.begin(), paths.end(), m_alias) == paths.end())
        paths.push_back(m_alias);

    const int slots_before = m_slots;
    for (const std::string &path : paths)
    {
        struct stat st;
        if (stat(path.c_str(), &st) != 0) continue;       // not back yet
        int fd = -1;
        if (tty_connect(path.c_str(), 115200, 8, 0, 1, &fd) != TTY_OK) continue;
        PortFD = fd;
        m_reconnecting = true;
        m_only_mine = !m_expected_serial.empty();
        const bool good = Handshake();
        m_reconnecting = false;
        if (good)
        {
            m_own_fd = fd;
            m_link_lost = false;
            m_port_in_use = path;
            remember_alias();
            link_found(path);
            if (m_slots != slots_before)
            {
                // A wheel restarted with another slot count (one changed and
                // never saved): the panel is rebuilt as at a connection.
                setConnected(false, IPS_IDLE);
                updateProperties();
                setConnected(true, IPS_OK);
                updateProperties();
            }
            return true;
        }
        tty_disconnect(fd);
        PortFD = -1;
        m_reader.attach(-1);
    }
    return false;
}

void Wheelly::link_found(const std::string &path)
{
    LOGF_INFO("%s", trf("msg.link.back", {path}).c_str());
    // The XIAO restarted with the USB: what it held only in working memory
    // is gone, so the panel is read again from the wheel.
    forget_unsaved();
    read_calibration();
    if (GetFilterNames()) FilterNameTP.apply();
}

bool Wheelly::Disconnect()
{
    // A descriptor opened by the reconnection is ours to close: libindi's
    // serial plugin does not know it.
    if (m_own_fd >= 0)
    {
        tty_disconnect(m_own_fd);
        m_own_fd = -1;
        PortFD = -1;
    }
    m_link_lost = false;
    m_reader.attach(-1);
    return INDI::FilterWheel::Disconnect();
}

// The rows of the angles follow m_slots: all MAX_SLOTS exist, the first
// m_slots are defined (define_angles). Rejected: the elements of one
// property, cut with resize() - a wheel with MORE slots than the one before
// grows them back nameless. Separate properties, each filled once, cannot.
void Wheelly::size_slots()
{
    m_marked_slot = (CurrentFilter >= 1 && CurrentFilter <= m_slots) ? CurrentFilter : 0;
    // a new set of elements carries no live row: they are defined right after
    m_unsaved_slot = 0;
    m_angles_stale = false;
    label_slots();
}

void Wheelly::label_slots()
{
    for (int i = 0; i < m_slots; i++)
    {
        const std::string n = std::to_string(i + 1);
        const char *key = i + 1 == m_unsaved_slot ? "prop.angles.unsaved"
                          : i + 1 == m_marked_slot ? "prop.angles.here" : nullptr;
        AngleNP[i].setLabel((key != nullptr ? trf(key, {n}) : n).c_str());
    }
}

void Wheelly::define_angles()
{
    for (int i = 0; i < m_slots; i++) defineProperty(AngleNP[i]);
    m_angles_defined = m_slots;
}

void Wheelly::delete_angles()
{
    for (int i = 0; i < m_angles_defined; i++) deleteProperty(AngleNP[i]);
    m_angles_defined = 0;
}

void Wheelly::mark_current_slot()
{
    const int now = (CurrentFilter >= 1 && CurrentFilter <= m_slots) ? CurrentFilter : 0;
    if (now != m_marked_slot)
    {
        m_marked_slot = now;
        // the live angle belonged to the slot the wheel has left: a jog that
        // landed on the next slot, a turn by hand
        if (m_unsaved_slot != 0 && m_unsaved_slot != now) forget_unsaved();
        m_angles_stale = true;
    }
    refresh_angles();
}

void Wheelly::forget_unsaved()
{
    if (m_unsaved_slot == 0) return;
    if (m_unsaved_slot <= MAX_SLOTS)
        AngleNP[m_unsaved_slot - 1][0].setValue(m_taught[m_unsaved_slot - 1]);
    m_unsaved_slot = 0;
    m_angles_stale = true;
}

void Wheelly::refresh_angles()
{
    if (!m_angles_stale || m_moving || m_jogging || m_sweeping) return;
    m_angles_stale = false;
    label_slots();
    redefine_calibration();
}

// Takes down and defines again the angles' rows and every property after
// them in the calibration tab, in the order updateProperties() defines them:
// KStars puts a property defined again at the END of its group, so
// redefining the angles alone would move them under the log. The
// Options properties defined in between are not touched: they live in
// another tab. The rows taken down are those the client HAS, the rows
// defined those the wheel has now: the number of slots may have changed.
void Wheelly::redefine_calibration()
{
    INDI::Property *tail[] =
    {
        std::addressof(JogDownSP), std::addressof(JogUpSP),
        std::addressof(ActionsSP), std::addressof(ToleranceNP),
        std::addressof(DiagSP), std::addressof(SweepSP), std::addressof(SweepBP),
        std::addressof(SweepDirTP), std::addressof(LogSP), std::addressof(FileTP)
    };
    delete_angles();
    for (INDI::Property *one : tail) deleteProperty(one->getName());
    define_angles();
    for (INDI::Property *one : tail) defineProperty(*one);
}

// The pitch buttons: 360/slots, written with the language's decimal mark and
// without trailing zeros - "72" on five slots, "51,43" in Italian on seven.
void Wheelly::label_jogs()
{
    char pitch[16];
    snprintf(pitch, sizeof(pitch), "%.2f", 360.0 / m_slots);
    std::string text = pitch;
    while (!text.empty() && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
    const size_t dot = text.find('.');
    if (dot != std::string::npos) text.replace(dot, 1, tr("num.decimal"));
    JogDownSP[0].setLabel(trf(JOG_LABELS[0], {text}).c_str());
    JogUpSP[JOG_PER_ROW - 1].setLabel(trf(JOG_LABELS[2 * JOG_PER_ROW - 1], {text}).c_str());
}

// A new number of slots, from the panel: the wheel is told, then every
// property whose size follows it is taken down and defined again - a client
// does not notice elements added to a property it already has. The jog rows
// go with the angles: their pitch buttons follow the number of slots.
bool Wheelly::change_slots(int count)
{
    if (count == m_slots) return true;
    Fields c;
    std::string error;
    if (!command(std::string(CMD_SLOTS) + " " + std::to_string(count), &c, &error))
    {
        LOGF_ERROR("%s", trf("msg.slots.refused", {error}).c_str());
        return false;
    }
    m_slots = (int)number(c, F_SLOTS, count);
    deleteProperty(FilterNameTP);
    size_slots();
    label_jogs();
    FilterSlotNP[0].setMax(m_slots);
    FilterSlotNP.updateMinMax();
    GetFilterNames();
    defineProperty(FilterNameTP);
    redefine_calibration();
    read_calibration();
    LOGF_WARN("%s", trf("msg.slots.changed", {std::to_string(m_slots)}).c_str());
    return true;
}

bool Wheelly::read_calibration()
{
    Fields c;
    if (command(CMD_ANGLES, &c))
    {
        for (int i = 0; i < m_slots; i++)
        {
            m_taught[i] = number(c, ("a" + std::to_string(i + 1)).c_str());
            AngleNP[i][0].setValue(i + 1 == m_unsaved_slot ? m_live_angle : m_taught[i]);
            AngleNP[i].setState(IPS_OK);
            AngleNP[i].apply();
        }
    }
    if (command(CMD_TOLERANCE, &c))
    {
        ToleranceNP[0].setValue(number(c, F_GOOD, FACTORY_GOOD_DEG));
        ToleranceNP[1].setValue(number(c, F_WARN, FACTORY_WARN_DEG));
        ToleranceNP[2].setValue(number(c, F_RETRIES, 3));
        ToleranceNP.setState(IPS_OK);
        ToleranceNP.apply();
    }
    if (command(CMD_MOTOR, &c))
    {
        MotorNP[0].setValue(number(c, F_MA, FACTORY_RUN_MA));
        MotorNP[1].setValue(number(c, F_SPEED, FACTORY_SPEED));
        MotorNP[2].setValue(number(c, F_ACCEL, FACTORY_ACCEL));
        MotorNP.setState(IPS_OK);
        MotorNP.apply();
    }
    if (command(CMD_HOLD, &c))
    {
        // Zero from the firmware means "at rest the motor is released", the
        // default, and zero is what the field shows (not the suggested 150,
        // see initProperties). The light says
        // it too: grey when released, green when it holds.
        const double ma = number(c, F_MA, 0);
        HoldNP[0].setValue(ma);
        HoldNP.setState(ma > 0 ? IPS_OK : IPS_IDLE);
        HoldNP.apply();
        // An older firmware also says `detent=yes|no` here:
        // ignored, the option is gone (NO DETENT OPTION, wheelly_protocol.h).
    }
    read_settle();
    read_direction(true);
    read_led_mode();
    return true;
}

void Wheelly::take_ceiling(const Fields &reply, bool warn)
{
    const double ms = number(reply, F_CEILING, 0);
    if (ms <= 0) return;                 // a firmware that does not say it
    m_wheel_ceiling_s = ms / 1000.0;
    // Ekos gives up by itself after 30 s. Past that, a long move ends in
    // Ekos's own bare "timeout" before the wheel has had its say: the user is
    // told now, while choosing the setting, not in the middle of the night.
    if (warn && ms > (double)EKOS_FILTER_TIMEOUT_MS)
    {
        char worst[16], ekos[16];
        snprintf(worst, sizeof(worst), "%.0f", std::ceil(ms / 1000.0));
        snprintf(ekos, sizeof(ekos), "%lu", EKOS_FILTER_TIMEOUT_MS / 1000);
        LOGF_WARN("%s", trf("msg.ceiling.ekos", {worst, ekos}).c_str());
    }
}

void Wheelly::read_direction(bool warn)
{
    Fields c;
    // A firmware older than the setting answers "unknown command": the
    // switch stays Idle - unknown, not "shortest" - and the cap stays 28 s.
    if (!command(CMD_DIRECTION, &c)) return;
    const auto t = c.find(F_DIRECTION);
    if (t != c.end())
    {
        DirectionSP.reset();
        DirectionSP[t->second == ARG_UP ? 1 : t->second == ARG_DOWN ? 2 : 0].setState(ISS_ON);
        DirectionSP.setState(IPS_OK);
        DirectionSP.apply();
    }
    take_ceiling(c, warn);
}

void Wheelly::read_settle()
{
    // A firmware older than the setting answers "unknown
    // command": said at debug level only, not as an error at every
    // connection, and the field keeps the factory 300 ms - which is exactly
    // the fixed wait that firmware makes. The cap is taken without a warning:
    // read_direction(), right after, warns once for all.
    Fields c;
    std::string ignored;
    if (!command(CMD_SETTLE, &c, &ignored))
    {
        LOGF_DEBUG("settle: %s", ignored.c_str());
        return;
    }
    HoldNP[1].setValue(number(c, F_MS, FACTORY_SETTLE_MS));
    HoldNP.apply();
    take_ceiling(c, false);
}

double Wheelly::move_ceiling_s() const
{
    return m_wheel_ceiling_s > 0 ? m_wheel_ceiling_s + MOVE_CEILING_MARGIN_S : MOVE_CEILING_S;
}

// The LED switch shows the mode the WHEEL has, read back, not the one the
// panel last asked for: the mode is saved on the wheel, and after a power
// cycle or with another client in between the two could differ.
void Wheelly::read_led_mode()
{
    Fields c;
    if (!command(CMD_LED, &c)) return;
    const std::string mode = c.count(F_MODE) ? c[F_MODE] : std::string(ARG_PULSE);
    LedSP.reset();
    LedSP[mode == ARG_ON ? 0 : mode == ARG_OFF ? 2 : 1].setState(ISS_ON);
    LedSP.setState(IPS_OK);
    LedSP.apply();
}

// ------------------------------------------------------------------- the filters

bool Wheelly::GetFilterNames()
{
    // The names are known by the wheel: it describes itself, and connected to
    // a new computer it already shows up with the right names.
    Fields c;
    if (PortFD < 0 || !command(CMD_NAMES, &c))
        return INDI::FilterWheel::GetFilterNames();

    FilterNameTP.resize(0);
    FilterNameTP.reserve(m_slots);
    for (int i = 0; i < m_slots; i++)
    {
        const std::string key = "n" + std::to_string(i + 1);
        m_names[i] = c.count(key) ? c[key] : ("Filter" + std::to_string(i + 1));
        INDI::WidgetText one;
        one.fill(("FILTER_SLOT_NAME_" + std::to_string(i + 1)).c_str(),
                 std::to_string(i + 1).c_str(), m_names[i].c_str());
        FilterNameTP.push(std::move(one));
    }
    FilterNameTP.fill(getDeviceName(), "FILTER_NAME", tr("prop.names"),
                      TAB_MAIN, IP_RW, 60, IPS_IDLE);
    return true;
}

bool Wheelly::SetFilterNames()
{
    // The real validation has already happened in ISNewText: here it is only written.
    for (int i = 0; i < m_slots && i < (int)FilterNameTP.count(); i++)
    {
        const std::string fresh = FilterNameTP[i].getText();
        if (fresh == m_names[i]) continue;     // what does not change is not rewritten
        std::string error;
        if (!command(std::string(CMD_NAME) + " " + std::to_string(i + 1) + " " + fresh,
                     nullptr, &error))
        {
            LOGF_ERROR("%s", trf("nome.rifiutato", {error}).c_str());
            return false;
        }
        m_names[i] = fresh;
    }
    return true;
}

bool Wheelly::SelectFilter(int slot)
{
    Fields c;
    if (!command(std::string(CMD_GO) + " " + std::to_string(slot), &c))
        return false;                          // the base class puts FILTER_SLOT in Alert

    TargetFilter = slot;
    // a move that is not a jog: the row showing the live angle goes back to
    // the taught one (redefined when the wheel stops, refresh_angles)
    forget_unsaved();
    // a filter change takes over from a jog under way: its verdict will be
    // the change's, and the jog row goes back to idle
    if (m_jogging)
    {
        m_jogging = false;
        if (m_jog_row != nullptr)
        {
            m_jog_row->setState(IPS_IDLE);
            m_jog_row->apply();
        }
    }
    // Busy here, whoever asked: libindi sets it before calling us from the
    // panel, but its joystick handler calls SelectFilter directly, and a move
    // from the joystick left FILTER_SLOT idle while the wheel turned
    FilterSlotNP.setState(IPS_BUSY);
    FilterSlotNP.apply();
    m_moving = true;
    m_move_start = now_s();
    LOGF_DEBUG("%s", trf("msg.moving", {std::to_string(slot)}).c_str());
    return true;
}

int Wheelly::QueryFilter()
{
    return CurrentFilter;
}

bool Wheelly::read_status(Fields &fields)
{
    return command(CMD_STATUS, &fields);
}

void Wheelly::update_panels(const Fields &status)
{
    PositionNP[0].setValue(number(status, F_ANGLE));
    PositionNP[1].setValue(number(status, F_ERR));
    PositionNP[2].setValue(number(status, F_RETRIES));
    PositionNP.setState(IPS_OK);
    PositionNP.apply();

    const bool magnet = number(status, F_MD, 1) > 0.5;
    // the magnet is back: a later loss is a new episode, and is said again.
    // Cleared only at connection, a second loss in the same night would go
    // unsaid
    if (magnet) m_magnet_lost = false;
    SensorNP[0].setValue(number(status, F_AGC));
    SensorNP[1].setValue(number(status, F_MAG));
    SensorNP[2].setValue(magnet ? 1 : 0);
    SensorNP[3].setValue(number(status, F_ML));
    SensorNP[4].setValue(number(status, F_MH));
    SensorNP.setState(magnet ? IPS_OK : IPS_ALERT);
    SensorNP.apply();
}

void Wheelly::TimerHit()
{
    if (!isConnected()) return;

    // The link is down: no status polls - each would only fail - but an
    // attempt to find the wheel again, with a pause that doubles up to
    // RETRY_MAX_S. It does not give up: a wheel plugged back after an hour
    // is found after an hour.
    if (m_link_lost)
    {
        if (now_s() >= m_next_attempt)
        {
            if (try_reconnect())
            {
                SetTimer(getCurrentPollingPeriod());
                return;
            }
            m_retry_pause_s = std::min(2.0 * m_retry_pause_s, RETRY_MAX_S);
            m_next_attempt = now_s() + m_retry_pause_s;
        }
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    collect_unsolicited();

    Fields status;
    if (!read_status(status))
    {
        // command() has already turned a port that is gone into link_lost()
        SetTimer(getCurrentPollingPeriod());
        return;
    }

    update_panels(status);

    // One sample at every query: it is the same 'status' answer that updates
    // the panels, so the sweep does not cost a single extra command on the
    // serial line. The ceiling is a safety net against a wheel that never
    // arrived: memory must not grow without end.
    if (m_sweeping && m_samples.size() < 4000)
    {
        m_samples.push_back({number(status, F_ANGLE), number(status, F_MAG)});
    }

    const std::string motion = status.count(F_MOTION) ? status.at(F_MOTION) : "";
    const std::string outcome = status.count(F_OUTCOME) ? status.at(F_OUTCOME) : OUTCOME_NONE;
    const int slot = (int)number(status, F_POS);
    const double error = number(status, F_ERR);
    const int retries = (int)number(status, F_RETRIES);

    if (m_jogging)
    {
        const bool still = (motion == MOTION_IDLE || motion == MOTION_FAILED);
        if (!still && now_s() - m_move_start > move_ceiling_s())
        {
            command(CMD_STOP);
            finish_jog(status, EV_FAILED);
        }
        else if (still && outcome != OUTCOME_NONE)
        {
            finish_jog(status, outcome);
        }
    }

    if (m_moving)
    {
        const bool still = (motion == MOTION_IDLE || motion == MOTION_FAILED);

        if (!still && now_s() - m_move_start > move_ceiling_s())
        {
            // The ceiling is ours on purpose: if Ekos's one expires, all that
            // is left in the log is "timeout" and nobody knows why.
            m_moving = false;
            command(CMD_STOP);
            LOGF_ERROR("%s", trf("msg.timeout",
            {std::to_string((int)std::ceil(move_ceiling_s()))}).c_str());
            LOG_WARN(tr("msg.hint.detent"));
            FilterSlotNP.setState(IPS_ALERT);
            FilterSlotNP.apply();
            record_move(status, "timeout");
            if (m_sweeping) finish_sweep(false);
        }
        else if (still && outcome != OUTCOME_NONE)
        {
            m_moving = false;
            const std::string which = std::to_string(TargetFilter);
            char deviation[16];
            snprintf(deviation, sizeof(deviation), "%.2f", std::fabs(error));

            if (outcome == EV_FAILED)
            {
                // Alert on FILTER_SLOT: Ekos stops the sequence at once, and
                // that is exactly what is wanted - no frame with the wheel out
                // of place.
                LOGF_ERROR("%s", trf("msg.failed",
                {which, deviation, std::to_string(retries)}).c_str());
                LOG_WARN(tr("msg.hint.detent"));
                FilterSlotNP.setState(IPS_ALERT);
                FilterSlotNP.apply();
            }
            else if (outcome == EV_WARNING)
            {
                // Outside the good tolerance but inside the warning one: warn
                // and let it go on. Never Alert here, otherwise a recoverable
                // warning would stop the night.
                LOGF_WARN("%s", trf("msg.warning", {which, deviation}).c_str());
                CurrentFilter = slot > 0 ? slot : TargetFilter;
                SelectFilterDone(CurrentFilter);
            }
            else
            {
                if (outcome != EV_ARRIVED)
                {
                    // A verdict we do not know means that the firmware is
                    // newer than the driver. It should not happen - the
                    // protocol version is checked at connection - but if it
                    // happens it is not silently taken for a success: it is
                    // said, and the driver lets it go on, because the wheel is
                    // still at rest anyway and blocking the night would be worse.
                    LOGF_WARN("Unknown outcome from the wheel: %s", outcome.c_str());
                }
                LOGF_INFO("%s", trf("msg.arrived", {which, deviation}).c_str());
                CurrentFilter = slot > 0 ? slot : TargetFilter;
                SelectFilterDone(CurrentFilter);
            }
            record_move(status, outcome.c_str());

            // The sweep goes on from here: one hop is over, on to the next.
            // If the hop failed the turn stops, because a plot with a hole in
            // it would say something false about the magnet.
            if (m_sweeping)
            {
                if (outcome == EV_FAILED) finish_sweep(false);
                else sweep_step();
            }
        }
    }
    else if (slot > 0 && slot != CurrentFilter)
    {
        // Someone moved the wheel without going through here - by hand, or
        // from a serial monitor. It is taken note of: the encoder is absolute,
        // so the true position is always known.
        CurrentFilter = slot;
        FilterSlotNP[0].setValue(CurrentFilter);
        FilterSlotNP.setState(IPS_OK);
        FilterSlotNP.apply();
    }

    // During the sweep the wheel is queried more often, because there every
    // answer is a sample: at the normal pace a whole turn gave twenty, and
    // twenty points scattered over three hundred and sixty degrees do not draw
    // a curve, they draw a constellation. It costs little - a 'status' line is
    // two milliseconds of wire at 115200 - and lasts as long as the turn.
    // the "▶" on the current slot's angle, only when the slot changed. Not
    // during a sweep: every hop changes slot, and redefining the plot's BLOB
    // under it loses the plot (found on the bench); the sweep ends on the
    // slot it started from, so there is nothing to catch up afterwards.
    if (!m_sweeping) mark_current_slot();

    SetTimer(m_sweeping ? SAMPLE_INTERVAL_MS : getCurrentPollingPeriod());
}

// ------------------------------------------------------------ commands from Ekos

std::string Wheelly::suggest_name(const char *text, int slot)
{
    // The suggestion is made by sanitize_filter_name(), which is the same
    // function the driver uses as a safety net: by construction - and it is
    // verified by the tests - what comes out of it passes check_filter_name().
    // So the rule is not checked again here; what is checked is the only thing
    // the rule does not know, that is whether the suggested name already
    // belongs to another slot. Suggesting a name that then gets refused would
    // be worse than suggesting nothing.
    char clean[wheelly::FILTER_NAME_MAX + 1];
    sanitize_filter_name(text, clean, sizeof(clean));
    if (std::strcmp(clean, text) == 0) return std::string();

    for (int i = 0; i < (int)FilterNameTP.count(); i++)
    {
        if (i + 1 == slot) continue;
        const char *other = FilterNameTP[i].getText();
        if (other != nullptr && name_equal_nocase(clean, other)) return std::string();
    }
    return std::string(clean);
}

bool Wheelly::ISNewText(const char *dev, const char *name, char *texts[],
                        char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0 &&
            SweepDirTP.isNameMatch(name))
    {
        SweepDirTP.update(texts, names, n);
        // The folder is accepted even if it cannot be used right now - it can
        // be a disk not mounted yet - but it is SAID at once, instead of
        // leaving it to be discovered at the first lost sweep.
        const std::string where = expand_home(SweepDirTP[0].getText());
        ::mkdir(where.c_str(), 0755);
        const bool usable = (::access(where.c_str(), W_OK) == 0);
        if (!usable)
            LOGF_WARN("%s", trf("msg.sweepdir.bad",
        {where, std::strerror(errno)}).c_str());
        SweepDirTP.setState(usable ? IPS_OK : IPS_ALERT);
        SweepDirTP.apply();
        saveConfig(SweepDirTP);
        return true;
    }

    // The names as they will be applied - an empty field replaced, see below.
    // Out here and not in the block that fills them: the base class reads
    // them after that block has closed.
    std::vector<std::string> own;
    std::vector<char *> fixed;
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0 &&
            FilterNameTP.isNameMatch(name))
    {
        // Validation happens BEFORE the property is allowed to update: the rule
        // lives in the shared header, so the very same one the firmware will
        // apply is applied here. The name is refused and not corrected behind
        // the user's back, because it ends up in the FITS header and in the
        // file name, and a name silently corrected breaks the pairing between
        // lights and flats.
        auto which_slot = [this](const char *element)
        {
            for (size_t i = 0; i < FilterNameTP.count(); i++)
                if (FilterNameTP[i].isNameMatch(element)) return (int)i + 1;
            return 0;
        };

        // The refusal is said in one place only, the log, and for this the
        // sentence must be complete: which slot, which name, what is wrong,
        // and a good name to write in its place. The rule instead is read
        // before making the mistake, in the property's label - which in
        // KStars is also its tooltip.
        auto refuse = [&](int slot, const char *text,
                          const std::string & why,
                          const std::string & suggestion)
        {
            const std::string sentence = suggestion.empty()
                                         ? trf("nome.rifiutato.slot",
            {std::to_string(slot), text, why})
                : trf("nome.rifiutato.slot.prova",
            {std::to_string(slot), text, why, suggestion});

            // Two signals together, because one alone is not enough: the red
            // light on the field, and the text going back to the good one - so
            // one SEES that it was not accepted, instead of believing it saved.
            //
            // The red light goes on the NAMES and not on FILTER_SLOT: that one
            // in Alert, in Ekos, means "filter change failed" and stops the
            // capture sequence. A badly written name is a mistake of whoever
            // writes it, not of the wheel, and must not throw away a night.
            FilterNameTP.setState(IPS_ALERT);

            // The allowed characters go with every refusal, and the refusal
            // itself must be the LAST thing said. KStars' device log puts the
            // newest line on top (and its status bar shows only the newest):
            // with the rule sent after the refusal, the line on top was the
            // generic rule and the one saying which slot and why sat under
            // it, unseen - a user left a slot empty, read only "Allowed: ...",
            // and could not tell what was wrong. So: one line when the two fit
            // together, and when they do not (MAXINDIMESSAGE = 255 bytes, the
            // cut is silent - a long name, a long reason and a long suggestion
            // do not fit), the rule first and the refusal after it.
            const std::string rule = tr("nome.ammessi");
            const std::string both = sentence + " " + rule;
            if (both.size() < MAXINDIMESSAGE)
            {
                FilterNameTP.apply("%s", both.c_str());
                return;
            }
            LOGF_INFO("%s", rule.c_str());
            FilterNameTP.apply("%s", sentence.c_str());
        };

        // AN EMPTY FIELD IS A SLOT WITHOUT A FILTER, and that is normal: a
        // five-slot wheel with four filters. Refusing it refused the whole set,
        // and the firmware cannot store an empty name (check_filter_name says
        // NAME_EMPTY, the same rule on both sides), so the driver gives the
        // slot a name: "Empty_<slot>". Why this form:
        //  - the slot number makes it unique by construction, so several empty
        //    slots never clash with each other under the duplicate rule, and no
        //    exemption from that rule is needed - an exemption would let two
        //    slots share a name in the FITS header and in the folder names,
        //    which is exactly what the rule exists to prevent;
        //  - it passes the name rule as it is, so the firmware (protocol 2,
        //    unchanged) stores it like any other name;
        //  - it is NOT translated: it is stored in the wheel and written in
        //    the FITS FILTER keyword, and a wheel moved to a computer in
        //    another language must keep the same names - like the factory
        //    ones, Lum, Red, ...
        // The field then shows the name given, so what Ekos lists is what the
        // wheel holds, and one log line says it was done - once the whole set
        // is accepted, not before a refusal that would undo it.
        // A field of blanks only counts as empty: it is what is left after
        // clearing a field by typing spaces over it.
        own.assign(texts, texts + n);
        fixed.resize(n);
        std::vector<int> given;
        for (int i = 0; i < n; i++)
        {
            if (own[i].find_first_not_of(" \t") == std::string::npos)
            {
                const int slot = which_slot(names[i]);
                if (slot > 0)
                {
                    own[i] = std::string(EMPTY_SLOT_NAME) + "_" + std::to_string(slot);
                    given.push_back(i);
                }
            }
            fixed[i] = &own[i][0];
        }
        texts = fixed.data();

        for (int i = 0; i < n; i++)
        {
            const NameCheck check = check_filter_name(texts[i]);
            if (check == NAME_OK) continue;

            const int slot = which_slot(names[i]);
            refuse(slot, texts[i], filter_name_reason(check, texts[i]),
                   suggest_name(texts[i], slot));
            return true;
        }

        // duplicates, without telling upper and lower case apart
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < (int)FilterNameTP.count(); j++)
            {
                const bool same_field = FilterNameTP[j].isNameMatch(names[i]);
                const char *other = same_field ? nullptr : FilterNameTP[j].getText();
                bool clash = (other != nullptr && name_equal_nocase(texts[i], other));
                for (int k = 0; k < n && !clash; k++)
                    if (k != i && name_equal_nocase(texts[i], texts[k])) clash = true;
                if (!clash) continue;

                // Nothing is suggested here: the name is already valid on its
                // own, and cleaning it would leave it as it is. A free name is
                // chosen by whoever knows what the slot is for, not by the driver.
                refuse(which_slot(names[i]), texts[i],
                       tr("nome.duplicate"), std::string());
                return true;
            }
        }

        for (int i : given)
            LOGF_INFO("%s", trf("nome.vuoto.dato",
        {std::to_string(which_slot(names[i])), own[i]}).c_str());
    }
    return INDI::FilterWheel::ISNewText(dev, name, texts, names, n);
}

bool Wheelly::ISNewNumber(const char *dev, const char *name, double values[],
                          char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        for (int i = 0; i < m_slots; i++)
        {
            if (!AngleNP[i].isNameMatch(name)) continue;
            if (n >= 1) set_angle(i + 1, values[0]);
            return true;
        }

        if (ToleranceNP.isNameMatch(name))
        {
            ToleranceNP.update(values, names, n);
            char line[96];
            snprintf(line, sizeof(line), "%s %.2f %.2f %.0f", CMD_TOLERANCE,
                     ToleranceNP[0].getValue(), ToleranceNP[1].getValue(),
                     ToleranceNP[2].getValue());
            Fields c;
            const bool ok = command(line, &c);
            if (ok)
            {
                ToleranceNP[0].setValue(number(c, F_GOOD, ToleranceNP[0].getValue()));
                ToleranceNP[1].setValue(number(c, F_WARN, ToleranceNP[1].getValue()));
                ToleranceNP[2].setValue(number(c, F_RETRIES, ToleranceNP[2].getValue()));
            }
            ToleranceNP.setState(ok ? IPS_OK : IPS_ALERT);
            ToleranceNP.apply();
            // more retries, a longer worst case: the wheel's cap follows
            if (ok) read_direction(true);
            return true;
        }

        if (SlotsNP.isNameMatch(name))
        {
            SlotsNP.update(values, names, n);
            const bool ok = change_slots((int)SlotsNP[0].getValue());
            SlotsNP[0].setValue(m_slots);
            SlotsNP.setState(ok ? IPS_OK : IPS_ALERT);
            SlotsNP.apply();
            return true;
        }

        if (MotorNP.isNameMatch(name))
        {
            MotorNP.update(values, names, n);
            char line[64];
            // the three together: sending the current alone, speed and
            // acceleration would go nowhere - the fields would show what was
            // typed until the next connection read the wheel's again
            snprintf(line, sizeof(line), "%s %.0f %.0f %.0f", CMD_MOTOR, MotorNP[0].getValue(),
                     MotorNP[1].getValue(), MotorNP[2].getValue());
            Fields c;
            const bool ok = command(line, &c);
            if (ok)
            {
                MotorNP[0].setValue(number(c, F_MA, MotorNP[0].getValue()));
                MotorNP[1].setValue(number(c, F_SPEED, MotorNP[1].getValue()));
                MotorNP[2].setValue(number(c, F_ACCEL, MotorNP[2].getValue()));
                // the cap follows the speed: warned here, where it is chosen
                take_ceiling(c, true);
            }
            MotorNP.setState(ok ? IPS_OK : IPS_ALERT);
            MotorNP.apply();
            return true;
        }

        if (HoldNP.isNameMatch(name))
        {
            const double settle_before = HoldNP[1].getValue();
            HoldNP.update(values, names, n);
            char line[64];
            snprintf(line, sizeof(line), "%s %.0f", CMD_HOLD, HoldNP[0].getValue());
            Fields c;
            bool ok = command(line, &c);
            if (ok)
            {
                HoldNP[0].setValue(number(c, F_MA, HoldNP[0].getValue()));
                // Whoever turns holding on must know what they are buying: heat
                // next to the cooled sensor and a chopper singing during the exposure.
                if (HoldNP[0].getValue() > 0) LOG_WARN(tr("msg.hold.on"));
            }
            // The settling hold is sent only when it changed: a firmware
            // without `settle` then keeps working as before when only the
            // current is set, and goes to Alert only if the hold is asked of
            // it. Its reply carries the move time cap, which grows with it.
            if (ok && HoldNP[1].getValue() != settle_before)
            {
                snprintf(line, sizeof(line), "%s %.0f", CMD_SETTLE, HoldNP[1].getValue());
                Fields s;
                ok = command(line, &s);
                if (ok)
                {
                    HoldNP[1].setValue(number(s, F_MS, HoldNP[1].getValue()));
                    take_ceiling(s, true);
                }
                else
                {
                    HoldNP[1].setValue(settle_before);
                }
            }
            // released is grey, as at connection: green here would make the
            // same state read two ways
            HoldNP.setState(!ok ? IPS_ALERT : (HoldNP[0].getValue() > 0 ? IPS_OK : IPS_IDLE));
            HoldNP.apply();
            return true;
        }
    }
    return INDI::FilterWheel::ISNewNumber(dev, name, values, names, n);
}

bool Wheelly::ISNewSwitch(const char *dev, const char *name, ISState *states,
                          char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        // "Save to the wheel", in the calibration tab and in Options: the
        // same command, and each button shows its own outcome
        if (ActionsSP.isNameMatch(name) || WheelConfigSP.isNameMatch(name))
        {
            INDI::PropertySwitch &which = ActionsSP.isNameMatch(name) ? ActionsSP : WheelConfigSP;
            which.update(states, names, n);
            const bool pressed = which.findOnSwitchIndex() == 0;
            which.reset();
            const bool ok = pressed ? save_to_wheel() : true;
            which.setState(!pressed ? IPS_IDLE : ok ? IPS_OK : IPS_ALERT);
            which.apply();
            return true;
        }

        if (JogDownSP.isNameMatch(name))
            return jog_pressed(JogDownSP, 0, states, names, n);
        if (JogUpSP.isNameMatch(name))
            return jog_pressed(JogUpSP, JOG_PER_ROW, states, names, n);

        if (DirectionSP.isNameMatch(name))
        {
            DirectionSP.update(states, names, n);
            const int choice = DirectionSP.findOnSwitchIndex();
            const char *argument = choice == 1 ? ARG_UP : choice == 2 ? ARG_DOWN : ARG_SHORTEST;
            Fields c;
            const bool ok = command(std::string(CMD_DIRECTION) + " " + argument, &c);
            if (ok)
            {
                // the switch shows what the wheel answered, not what was asked
                const auto t = c.find(F_DIRECTION);
                const std::string read = t != c.end() ? t->second : std::string(argument);
                DirectionSP.reset();
                DirectionSP[read == ARG_UP ? 1 : read == ARG_DOWN ? 2 : 0].setState(ISS_ON);
                LOG_INFO(tr("msg.direction.set"));
                take_ceiling(c, true);
            }
            DirectionSP.setState(ok ? IPS_OK : IPS_ALERT);
            DirectionSP.apply();
            return true;
        }

        if (LedSP.isNameMatch(name))
        {
            LedSP.update(states, names, n);
            const int choice = LedSP.findOnSwitchIndex();
            const char *argument = choice == 0 ? ARG_ON
                                   : choice == 1 ? ARG_PULSE
                                   : choice == 2 ? ARG_OFF : ARG_TEST;
            Fields c;
            const bool ok = command(std::string(CMD_LED) + " " + argument, &c);
            if (ok && choice == 3)
            {
                LOGF_INFO("%s", trf("msg.led.test",
                {c.count(F_DURATION) ? c[F_DURATION] : "7"}).c_str());
            }
            else if (ok)
            {
                // the explanation goes in the log, not in the panel
                LOG_INFO(tr(choice == 1 ? "msg.led.pulse" : "msg.led.mode"));
            }
            // the test ends by itself: the switch goes back to the mode the
            // wheel has, read from it, and so does a refused command
            if (ok)
            {
                read_led_mode();
            }
            else
            {
                LedSP.setState(IPS_ALERT);
                LedSP.apply();
            }
            return true;
        }

        if (DiagSP.isNameMatch(name))
        {
            // "Is it really talking to the hardware?" is a single question, and
            // it is worth being able to ask it from Ekos: a wire come loose on
            // the motor driver shows up as "the motor does not turn" and sends
            // one looking for the problem in ten wrong places.
            DiagSP.update(states, names, n);
            m_show_comments = true;
            LOG_INFO(tr("msg.diag"));
            const bool ok = command(CMD_DIAG);
            m_show_comments = false;
            DiagSP.reset();
            DiagSP.setState(ok ? IPS_OK : IPS_ALERT);
            DiagSP.apply();
            return true;
        }

        if (SweepSP.isNameMatch(name))
        {
            SweepSP.update(states, names, n);
            SweepSP.reset();
            if (m_moving || m_sweeping)
            {
                LOG_ERROR(tr("msg.sweep.notnow"));
                SweepSP.setState(IPS_ALERT);
                SweepSP.apply();
            }
            else
            {
                start_sweep();
            }
            return true;
        }

        if (LogSP.isNameMatch(name))
        {
            LogSP.update(states, names, n);
            if (LogSP[0].getState() == ISS_ON) open_log();
            else
            {
                close_log();
                LOG_INFO(tr("msg.log.off"));
            }
            LogSP.setState(IPS_OK);
            LogSP.apply();
            saveConfig(LogSP);
            return true;
        }

#if WHEELLY_ITALIAN
        if (LanguageSP.isNameMatch(name))
        {
            LanguageSP.update(states, names, n);
            LanguageSP.setState(IPS_OK);
            LanguageSP.apply();
            saveConfig(LanguageSP);
            LOG_INFO(tr("msg.language.later"));
            return true;
        }
#endif
    }
    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool Wheelly::saveConfigItems(FILE *fp)
{
    INDI::FilterWheel::saveConfigItems(fp);
    // The wheel's serial number: it is the only thing in this property worth
    // reading back, but the whole of it is saved because it costs nothing and
    // the panel is not cluttered with a property of its own.
    FirmwareTP.save(fp);
    ToleranceNP.save(fp);
    SweepDirTP.save(fp);
    LogSP.save(fp);
#if WHEELLY_ITALIAN
    LanguageSP.save(fp);
#endif
    return true;
}

// ------------------------------------------------------- the calibration angles

// A Set on one row of the calibration angles (a Set per row: see AngleNP in
// wheelly.h). The slot is taught the angle
// in the field, and the wheel goes there - as the position control does,
// a live centring - with ONE exception: the
// row that follows the jogs ("▶ 2 *", m_unsaved_slot) sent back as the driver
// put it is a CONFIRMATION, the wheel is already there and nothing moves;
// that is what a "Save position" button would do. Edited, that row is an
// ordinary change and the wheel goes. A row whose value did not change sends
// no `angle` and only takes the wheel to its slot: pressing Set on a row
// always means "that slot, at that angle". The range is the wheel's own:
// its refusal, translated, is what the log says, and the row keeps its old
// value, in Alert.
void Wheelly::set_angle(int slot, double value)
{
    INDI::PropertyNumber &row = AngleNP[slot - 1];
    if (m_moving || m_sweeping || m_jogging)
    {
        LOG_ERROR(tr("msg.angle.notnow"));
        row.setState(IPS_ALERT);
        row.apply();
        return;
    }
    const bool confirm = slot == m_unsaved_slot && std::fabs(value - m_live_angle) < 0.005;
    const double before = m_taught[slot - 1];
    if (confirm || std::fabs(value - before) >= 0.005)
    {
        char line[64];
        snprintf(line, sizeof(line), "%s %d %.2f", CMD_ANGLE, slot, confirm ? m_live_angle : value);
        Fields c;
        std::string error;
        if (!command(line, &c, &error))
        {
            LOGF_ERROR("%s", trf("msg.angle.refused", {std::to_string(slot), error}).c_str());
            row[0].setValue(slot == m_unsaved_slot ? m_live_angle : before);
            row.setState(IPS_ALERT);
            row.apply();
            return;
        }
        const double after = number(c, ("a" + std::to_string(slot)).c_str(), value);
        m_taught[slot - 1] = after;
        row[0].setValue(after);
        if (slot == m_unsaved_slot)
        {
            m_unsaved_slot = 0;
            m_angles_stale = true;     // the "*" goes: a label, so a definition
        }
        note_angle_change(slot, before, after, confirm ? "angle-taught" : "angle-set");
        LOGF_INFO("%s", trf(confirm ? "msg.taught" : "msg.angle.volatile",
        {std::to_string(slot)}).c_str());
    }
    row.setState(IPS_OK);
    row.apply();
    if (confirm)
    {
        refresh_angles();
        return;
    }
    // to that slot now, through the same path as FILTER_SLOT, so Ekos sees
    // the change of filter and its verdict; a "*" still owed goes when the
    // wheel stops (refresh_angles waits for it)
    FilterSlotNP[0].setValue(slot);
    if (!SelectFilter(slot))
    {
        FilterSlotNP.setState(IPS_ALERT);
        FilterSlotNP.apply();
        refresh_angles();
    }
}

// The line in the log, and a row in the movement register with its own
// outcome ("angle-set", "angle-taught"): the slot, the new angle in the
// target column, the old one in the angle column, and the change as the
// error. The history of the corrections the rotation trim was meant to show.
void Wheelly::note_angle_change(int slot, double before, double after, const char *what)
{
    char was[16], is[16], delta[16];
    snprintf(was, sizeof(was), "%.2f", before);
    snprintf(is, sizeof(is), "%.2f", after);
    snprintf(delta, sizeof(delta), "%.2f", std::remainder(after - before, 360.0));
    LOGF_INFO("%s", trf("msg.angle.changed", {std::to_string(slot), was, is}).c_str());
    if (m_log == nullptr) return;
    char when[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%S", std::gmtime(&now));
    std::fprintf(m_log, "%s,%d,%s,%s,%s,,%s,,\n", when, slot, is, was, delta, what);
    std::fflush(m_log);
}

bool Wheelly::save_to_wheel()
{
    if (!isConnected())
    {
        LOG_ERROR(tr("msg.notconnected"));
        return false;
    }
    const bool ok = command(CMD_SAVE);
    if (ok) LOG_INFO(tr("msg.saved"));
    return ok;
}

// ------------------------------------------------------------------ the jog

bool Wheelly::jog_pressed(INDI::PropertySwitch &row, int first,
                          ISState *states, char *names[], int n)
{
    row.update(states, names, n);
    const int choice = row.findOnSwitchIndex();
    row.reset();
    if (choice < 0)
    {
        row.apply();
        return true;
    }
    if (m_moving || m_sweeping || m_jogging)
    {
        LOG_ERROR(tr("msg.jog.notnow"));
        row.setState(IPS_ALERT);
        row.apply();
        return true;
    }
    std::string error;
    // a step of 0 is the slot pitch, signed by its row (JOG_STEPS)
    double degrees = JOG_STEPS[first + choice];
    if (degrees == 0.0) degrees = (first == 0 ? -360.0 : 360.0) / m_slots;
    char line[32];
    snprintf(line, sizeof(line), "%s %.4f", CMD_JOG, degrees);
    if (!command(line, nullptr, &error))
    {
        // an old firmware answers "unknown command": said, translated
        LOGF_ERROR("%s", error.empty() ? tr("msg.no.answer") : error.c_str());
        row.setState(IPS_ALERT);
        row.apply();
        return true;
    }
    m_jogging = true;
    m_jog_row = std::addressof(row);   // PropertySwitch overloads &
    m_move_start = now_s();
    row.setState(IPS_BUSY);
    row.apply();
    return true;
}

void Wheelly::finish_jog(const Fields &status, const std::string &outcome)
{
    // Not a filter change: FILTER_SLOT is left alone, and the verdict is
    // said in the log in plain words. A jog close to the sensor's count
    // (0.088 degrees) may be reported by the wheel as a "warning" a hair
    // past its target: that is still a done jog, said as information; only
    // a failure is an error.
    m_jogging = false;
    char angle[16], off[16];
    snprintf(angle, sizeof(angle), "%.2f", number(status, F_ANGLE));
    snprintf(off, sizeof(off), "%.2f", std::fabs(number(status, F_ERR)));
    const bool failed = outcome == EV_FAILED;
    if (failed)
    {
        LOGF_ERROR("%s", trf("msg.jog.failed", {angle, off}).c_str());
        LOG_WARN(tr("msg.hint.detent"));
    }
    else        LOGF_INFO("%s", trf("msg.jog.done", {angle, off}).c_str());
    if (m_jog_row != nullptr)
    {
        m_jog_row->setState(failed ? IPS_ALERT : IPS_OK);
        m_jog_row->apply();
    }
    // The current row shows where the wheel is now, "▶ 2 *", to confirm with
    // the Set on that row (see m_unsaved_slot). A failed jog too: the wheel
    // stands somewhere, and that is what the row says. If the jog carried the
    // wheel onto another slot, mark_current_slot, right after in the same
    // TimerHit, gives it back and moves the marker.
    const int slot = (CurrentFilter >= 1 && CurrentFilter <= m_slots) ? CurrentFilter : 0;
    if (slot != 0)
    {
        if (m_unsaved_slot != 0 && m_unsaved_slot != slot) forget_unsaved();
        m_unsaved_slot = slot;
        // rounded as the field shows it (%.2f): KStars sends back the text
        // it shows, and an unrounded live angle 0.005 away would read as an
        // edit - and move the wheel - instead of a confirmation
        m_live_angle = std::round(number(status, F_ANGLE) * 100.0) / 100.0;
        AngleNP[slot - 1][0].setValue(m_live_angle);
        m_angles_stale = true;
    }
}

// ------------------------------------------------------------- the magnet sweep

void Wheelly::start_sweep()
{
    m_samples.clear();
    m_samples.reserve(512);
    // One hop per slot: starting from where the wheel stands and always moving
    // on by one, after m_slots hops it is back at the starting point having
    // covered the whole turn. This way the wheel ends where it was, and
    // whoever was capturing does not find a filter different from the one
    // they had.
    m_sweep_remaining = m_slots;
    // The live row goes back to its taught angle NOW, before the sweep: the
    // redefinition it needs would wipe the plot if it came during or after
    // the turn (the previous plot is still gone either way: it is on disk).
    forget_unsaved();
    refresh_angles();
    m_sweeping = true;
    SweepSP.setState(IPS_BUSY);
    SweepSP.apply();
    LOG_INFO(tr("msg.sweep.start"));
    sweep_step();
}

void Wheelly::sweep_step()
{
    if (m_sweep_remaining <= 0)
    {
        finish_sweep(true);
        return;
    }
    m_sweep_remaining--;
    // Always to the next slot, never to a far one: this way the wheel turns
    // all in one direction and the samples cover the turn without going back.
    const int next = (CurrentFilter % m_slots) + 1;
    if (!SelectFilter(next))
    {
        finish_sweep(false);
        return;
    }
    // FILTER_SLOT goes Busy inside SelectFilter(), whoever calls it: it is
    // not set again here.
}

void Wheelly::finish_sweep(bool succeeded)
{
    m_sweeping = false;

    // Fewer than four samples is not a plot, it is a pretext: better to say
    // that it did not work than to draw a straight line and make it look like
    // a measurement.
    if (!succeeded || m_samples.size() < 4)
    {
        LOG_ERROR(tr("msg.sweep.failed"));
        SweepSP.setState(IPS_ALERT);
        SweepSP.apply();
        return;
    }

    double minimum = m_samples[0].magnitude, maximum = minimum;
    for (const Sample &c : m_samples)
    {
        minimum = std::min(minimum, c.magnitude);
        maximum = std::max(maximum, c.magnitude);
    }

    std::vector<double> angles;
    for (int i = 0; i < m_slots; i++) angles.push_back(m_taught[i]);

    PlotLabels labels;
    labels.title     = tr("graf.titolo");
    labels.x_axis    = tr("graf.asse.x");
    labels.y_axis    = tr("graf.asse.y");
    labels.samples   = tr("graf.campioni");
    labels.span      = tr("graf.escursione");
    labels.minimum   = tr("graf.min");
    labels.maximum   = tr("graf.max");
    m_png = sweep_png(m_samples, angles, labels);

    // The format is not repeated here: fill() declares it, and it is what the
    // client reads: a second declaration here would be dead code.
    SweepBP[0].setBlob(const_cast<char *>(m_png.data()));
    SweepBP[0].setBlobLen((int)m_png.size());
    SweepBP[0].setSize((int)m_png.size());
    SweepBP.setState(IPS_OK);
    SweepBP.apply();

    // The plot is also written to disk, with a path the driver KNOWS and can
    // therefore say. The BLOB's one is chosen by the client - in KStars the
    // FITS folder, with a name full of date and time - and the driver has no
    // way of knowing it or of writing it anywhere.
    const std::string where = sweep_path();
    ensure_folder(where);
    bool written = false;
    // The reason is taken RIGHT after the call that fails. Taken at the end,
    // errno can be that of something else entirely - here it was the EEXIST of
    // the mkdir above, and the message said "File exists" to someone who could
    // not write a file.
    int why = 0;
    if (std::FILE *f = std::fopen(where.c_str(), "wb"))
    {
        written = std::fwrite(m_png.data(), 1, m_png.size(), f) == m_png.size();
        if (!written) why = errno;
        if (std::fclose(f) != 0 && written)
        {
            written = false;
            why = errno;
        }
    }
    else
    {
        why = errno;
    }
    if (written)
    {
        FileTP[FILE_SWEEP].setText(where.c_str());
    }
    else
    {
        FileTP[FILE_SWEEP].setText(tr("prop.files.none"));
        LOGF_WARN("%s", trf("msg.sweep.nofile",
        {where, std::strerror(why)}).c_str());
    }
    FileTP.setState(written ? IPS_OK : IPS_ALERT);
    FileTP.apply();

    char deviation[16];
    snprintf(deviation, sizeof(deviation), "%.1f",
             maximum > 0 ? (maximum - minimum) / maximum * 100.0 : 0.0);
    LOGF_INFO("%s", trf("msg.sweep.done",
    {
        std::to_string(m_samples.size()),
        std::to_string((long)minimum), std::to_string((long)maximum),
        std::to_string((long)(maximum - minimum)), deviation
    }).c_str());
    if (written) LOGF_INFO("%s", trf("msg.sweep.file", {where}).c_str());
    SweepSP.setState(IPS_OK);
    SweepSP.apply();
}

// ------------------------------------------------------------- the movement log

std::string Wheelly::sweep_path() const
{
    // Every sweep its own file, with date and time in the name. The point of
    // measuring the magnet is comparing a before and an after - a touch to the
    // air gap, a screw tightened again - and a fixed name that gets overwritten
    // throws away exactly the term of comparison.
    //
    // The time is LOCAL and not UTC, unlike the one in the movement log: that
    // one is read by a spreadsheet, this one by whoever remembers touching the
    // wheel "last night around nine".
    //
    // And in Documents, not in the driver's hidden folder: a plot is something
    // to look at, and goes where things are looked at.
    char when[32];
    const std::time_t now = std::time(nullptr);
    std::tm parts {};
    localtime_r(&now, &parts);
    std::strftime(when, sizeof(when), "%Y-%m-%d_%H-%M-%S", &parts);

    std::string folder = expand_home(SweepDirTP[0].getText());
    while (folder.size() > 1 && folder.back() == '/') folder.pop_back();
    if (folder.empty()) folder = default_folder();
    return folder + "/" + when + "_wheelly_sweep.png";
}

std::string Wheelly::default_folder()
{
    const char *home = getenv("HOME");
    return std::string(home ? home : "/tmp") + "/Documents";
}

std::string Wheelly::expand_home(const std::string &path)
{
    if (path.compare(0, 2, "~/") != 0) return path;
    const char *home = getenv("HOME");
    return std::string(home ? home : "/tmp") + path.substr(1);
}

void Wheelly::ensure_folder(const std::string &file_path)
{
    const size_t cut = file_path.rfind('/');
    if (cut == std::string::npos || cut == 0) return;
    ::mkdir(file_path.substr(0, cut).c_str(), 0755);   // if it is already there, never mind
}

std::string Wheelly::log_path() const
{
    const char *home = getenv("HOME");
    // English, as every file the project writes (the project is published
    // in English); an older name was wheelly_movimenti.csv - see open_log()
    return std::string(home ? home : "/tmp") + "/.indi/wheelly_movements.csv";
}

void Wheelly::open_log()
{
    if (m_log != nullptr) return;
    const std::string path = log_path();

    // The log's old, Italian name: a log started before the rename carries on
    // under the new one instead of being left behind, so the history of the
    // wheel stays in one file. Only when the new one does not exist yet - never
    // over it.
    const std::string old = path.substr(0, path.rfind('/')) + "/wheelly_movimenti.csv";
    if (access(path.c_str(), F_OK) != 0 && access(old.c_str(), F_OK) == 0)
        std::rename(old.c_str(), path.c_str());

    const bool fresh = (access(path.c_str(), F_OK) != 0);
    m_log = std::fopen(path.c_str(), "a");
    if (m_log == nullptr)
    {
        LOGF_ERROR("%s", trf("msg.log.failed",
        {path, std::strerror(errno)}).c_str());
        LogSP.reset();
        LogSP[1].setState(ISS_ON);
        return;
    }
    // The header is in English like the rest of what is meant to be read by a
    // machine: this file is opened in a spreadsheet, not in Ekos. A change
    // of a calibration angle is a row too, told apart by
    // its outcome, angle-set or angle-taught: the new angle under "target",
    // the old one under "angle", the change under "error"
    // (note_angle_change).
    if (fresh)
        std::fprintf(m_log,
                     "timestamp,slot,target,angle,error,retries,outcome,agc,magnitude\n");
    std::fflush(m_log);
    LOGF_INFO("%s", trf("msg.log.on", {path}).c_str());
}

void Wheelly::close_log()
{
    if (m_log == nullptr) return;
    std::fclose(m_log);
    m_log = nullptr;
}

void Wheelly::record_move(const Fields &status, const char *outcome)
{
    if (m_log == nullptr) return;

    char when[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%S", std::gmtime(&now));

    const auto take = [&status](const char *k)
    {
        const auto t = status.find(k);
        return t == status.end() ? std::string("") : t->second;
    };
    std::fprintf(m_log, "%s,%d,%s,%s,%s,%s,%s,%s,%s\n",
                 when, TargetFilter, take(F_TARGET).c_str(),
                 take(F_ANGLE).c_str(), take(F_ERR).c_str(),
                 take(F_RETRIES).c_str(), outcome,
                 take(F_AGC).c_str(), take(F_MAG).c_str());
    std::fflush(m_log);
}
