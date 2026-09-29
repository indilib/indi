// SPDX-FileCopyrightText: 2026 Matteo Beretta
// SPDX-License-Identifier: LGPL-2.1-or-later

// Wheelly - INDI driver for the filter wheel.
//
// The driver only relays: the decisions - whether the wheel has arrived,
// whether to retry, whether to declare failure - live in the firmware. That
// way the wheel behaves the same even when driven by hand from a serial
// monitor, and there are no two copies of the same logic that can diverge.
//
// See firmware.md for the reasoning, and wheelly_protocol.h for the vocabulary.

#ifndef WHEELLY_H
#define WHEELLY_H

#include "wheelly_protocol.h"

#include <string>
#include <vector>

namespace wheelly
{
// A sample of the sweep: where the wheel stood, and how strongly the sensor
// saw the magnet at that moment.
struct Sample
{
    double angle;        // degrees, 0..360
    double magnitude;    // AS5600 counts
};
}  // namespace wheelly

// libindi's headers are named the way INDI's own drivers name them - no
// "libindi/" prefix - because that is the only spelling that resolves both
// against the installed headers (pkg-config puts include/libindi on the path)
// and inside INDI's source tree, where the headers sit in libs/indibase and
// there is no libindi/ directory at all.
#include "indifilterwheel.h"
#include "indipropertynumber.h"
#include "indipropertyswitch.h"
#include "indipropertytext.h"

#include <cstdio>
#include <string>
#include <map>
#include <vector>

// Line reader with its own buffer.
//
// tty_nread_section is not used because, when it times out halfway through a
// line, it returns what it has read and the rest of the line is lost. On our
// channel unsolicited lines arrive - the events - which can fall in the middle
// of an answer, so a reader that loses pieces here would cause trouble that is
// hard to understand. Whatever is not consumed stays in here until the next
// read.
class LineReader
{
    public:
        void attach(int descriptor);
        void flush();
        // Returns true and fills `out` if a whole line has arrived within the
        // given time. Returns false on timeout or on a channel error.
        bool line(std::string &out, int wait_ms);
        bool channel_down() const
        {
            return m_down;
        }
        // The errno of the failure that put the channel down; 0 when the
        // other end hung up (end of file) - which on a tty is the port gone.
        int down_errno() const
        {
            return m_errno;
        }

    private:
        int m_fd {-1};
        std::string m_rest;
        bool m_down {false};
        int m_errno {0};
};

class Wheelly : public INDI::FilterWheel
{
    public:
        Wheelly();
        virtual ~Wheelly() override = default;

        const char *getDefaultName() override;
        // Connecting in two passes: see wheelly.cpp. It is needed because the
        // wheel is recognised by its protocol and not by the name of the port.
        bool Connect() override;
        bool Disconnect() override;
        bool initProperties() override;
        bool updateProperties() override;
        bool ISNewNumber(const char *dev, const char *name, double values[],
                         char *names[], int n) override;
        bool ISNewSwitch(const char *dev, const char *name, ISState *states,
                         char *names[], int n) override;
        bool ISNewText(const char *dev, const char *name, char *texts[],
                       char *names[], int n) override;

    protected:
        bool Handshake() override;
        bool SelectFilter(int slot) override;
        int QueryFilter() override;
        bool SetFilterNames() override;
        bool GetFilterNames() override;
        void TimerHit() override;
        bool saveConfigItems(FILE *fp) override;

    private:
        // --- dialogue with the firmware ----------------------------------
        using Fields = std::map<std::string, std::string>;

        // Sends a command and waits for its outcome. The informative lines and
        // the events that arrive in the meantime do not end the wait: they are
        // set aside, because they are not what is being waited for.
        bool command(const std::string &text, Fields *fields = nullptr,
                     std::string *error_for_user = nullptr);
        void collect_unsolicited();          // events that arrived unasked
        void handle_unsolicited_line(const std::string &line);
        static Fields split_fields(const std::string &line);
        static double number(const Fields &fields, const char *key, double if_missing = 0.0);

        // --- reading and writing the state --------------------------------
        bool read_status(Fields &fields);
        bool read_calibration();             // angles, offsets, tolerances, motor
        void update_panels(const Fields &status);

        // --- the magnet sweep ---------------------------------------------
        // It is not a loop that just runs: blocking in here for twenty
        // seconds would freeze the driver, and with it the chance of stopping
        // it from Ekos. So it is a state machine that advances inside
        // TimerHit, one hop at a time, as a normal filter change does.
        void start_sweep();
        void sweep_step();
        void finish_sweep(bool succeeded);

        // --- the movement log ---------------------------------------------
        std::string log_path() const;
        std::string sweep_path() const;
        // The leading '~' is expanded by the shell, not by fopen: if the user
        // writes it in a text field, it has to be expanded here or the file
        // ends up in a folder really called "~".
        static std::string expand_home(const std::string &path);
        static std::string default_folder();
        // Creates the folder of the driver's files if it is not there yet. The
        // first run on a new machine really happens.
        static void ensure_folder(const std::string &file_path);
        void open_log();
        void close_log();
        void record_move(const Fields &status, const char *outcome);

        // --- properties: main panel ---------------------------------------
        INDI::PropertyNumber PositionNP {3};     // angle, error, retries
        INDI::PropertyNumber SensorNP {5};       // agc, mag, md, ml, mh

        // --- properties: calibration panel --------------------------------
        // How many slots the wheel has, set from the panel (simpler for the
        // user than a serial monitor). It lives
        // in the wheel; changing it restarts the calibration from evenly
        // spaced angles, in the wheel's working memory only until "Save".
        INDI::PropertyNumber SlotsNP {1};
        void size_slots();                   // the per-slot elements, exactly m_slots
        bool change_slots(int count);
        // The calibration angles, EDITABLE, ONE PROPERTY PER SLOT,
        // WHEELLY_ANGLE_1 .. _n, one element each: every row needs a Set of
        // its own, and KStars gives one Set per property, with a caption that
        // cannot be changed. A Set on a row teaches that slot the angle in
        // its field (set_angle). Rejected: a single vector (WHEELLY_ANGLES)
        // plus a "Save position" button (WHEELLY_TEACH) - the Set on the
        // current row does what that button did. No rotation trim under the
        // angles (WHEELLY_OFFSET, removed): firmware.md 2.2, "One number per
        // slot". Built in initProperties, all MAX_SLOTS of them
        // (reserve: no reallocation after), defined only the first m_slots.
        std::vector<INDI::PropertyNumber> AngleNP;
        int m_angles_defined {0};            // how many rows the client has
        // THE CURRENT SLOT IS MARKED in its row's label, "▶ 2": INDI cannot
        // colour a property. A label reaches a client
        // only with a DEFINITION, so the rows are taken down and defined
        // again - and with them every property after them in the tab, or
        // KStars, which appends a new definition at the end of its group,
        // would move them to the bottom. Only when something changes, never
        // at a plain poll. The marked slot is the driver's current filter: it
        // stays on while the wheel is jogged off it to centre the filter.
        int m_marked_slot {0};
        // THE CURRENT ROW FOLLOWS THE JOGS: otherwise, with the filter jogged
        // centred, the row still shows the old angle and the number to
        // confirm is nowhere on the panel. After every jog the current slot's
        // element shows the angle read NOW, labelled "▶ 2 *" - not taught
        // yet. KStars fills the editable field only from a definition, hence
        // the redefinition, and only at the end of a jog, at a change of
        // slot or at a teach, never at a poll. "Set" on that row confirms
        // it; any other move (a filter change, the sweep, a jog that
        // lands on another slot) gives the row back its taught angle.
        // m_taught holds the wheel's angles, which the elements show except
        // for that one row: every comparison and every "before" in the log
        // is made against it, never against the shown value.
        double m_taught[wheelly::MAX_SLOTS] {};
        int m_unsaved_slot {0};              // 0 = no row shows a live angle
        double m_live_angle {0.0};
        bool m_angles_stale {false};         // a redefinition is owed
        void label_slots();                  // the angles' labels, marker included
        void mark_current_slot();            // re-label and redefine if it changed
        void forget_unsaved();               // the live row back to its taught angle
        // Redefines the angles if something is owed, once the wheel is still:
        // during a move it waits, so a change of slot costs one definition
        // and not two, and during a sweep it never runs (every hop changes slot).
        void refresh_angles();
        void redefine_calibration();         // the rows and all after them in the tab
        void define_angles();                // the first m_slots rows
        void delete_angles();                // the rows the client has
        void set_angle(int slot, double value);
        // Every angle change, in the log and in the movement register.
        void note_angle_change(int slot, double before, double after, const char *what);
        INDI::PropertySwitch ActionsSP {1};      // save (no "clear trims": there are no trims)
        // "Save to the wheel" again, in Options right under INDI's
        // "Configuration": a property of its own, since
        // CONFIG_PROCESS belongs to INDI - Ekos counts on its four elements -
        // and with five buttons KStars would make it a drop-down menu.
        INDI::PropertySwitch WheelConfigSP {1};
        bool save_to_wheel();
        // THE JOG BUTTONS: -pitch -10 -1 -0.1 and +0.1 +1 +10 +pitch
        // degrees, the pitch being 360/slots (72 on a five-slot wheel; the
        // 0.05 step was rejected, below one AS5600 count). ONE row would be
        // simpler, but KStars draws a switch vector of more than four
        // exclusive elements as a drop-down menu, so it is two rows, back and
        // forward. A jog is `jog <deg>`: a move TO the angle read plus that
        // much, closed loop in the firmware; the driver only follows it. No
        // third row "Save position" (WHEELLY_TEACH, rejected): the Set on the
        // current slot's row, which follows the jogs, does the same.
        static const int JOG_PER_ROW = 4;
        INDI::PropertySwitch JogDownSP {JOG_PER_ROW};
        INDI::PropertySwitch JogUpSP {JOG_PER_ROW};
        bool m_jogging {false};
        INDI::PropertySwitch *m_jog_row {nullptr};   // the row whose jog runs
        bool jog_pressed(INDI::PropertySwitch &row, int first,
                         ISState *states, char *names[], int n);
        void label_jogs();                   // the pitch buttons follow m_slots
        void finish_jog(const Fields &status, const std::string &outcome);
        INDI::PropertyNumber ToleranceNP {3};
        INDI::PropertyNumber MotorNP {3};
        // [0] the holding current at rest, [1] the settling hold after every
        // leg (`settle`): one property, because both say how the
        // motor holds the disc, and the panel gets no new row to explain it
        INDI::PropertyNumber HoldNP {2};
        // No WHEELLY_DETENT switch: the wheel's detent is
        // always removed (the motor cannot climb out of its notches), so the
        // "has a detent / no detent" setting had one right value. Why, at
        // length: NO DETENT OPTION in wheelly_protocol.h.
        // Which way the wheel may turn: the shortest way, or one way only for
        // a mechanism stiffer one way (an asymmetric detent, for instance,
        // stalls the motor on its steep flank).
        // Read at connection with 'direction', set with
        // 'direction shortest|up|down'.
        INDI::PropertySwitch DirectionSP {3};    // shortest, up, down
        // Reads the direction and the wheel's move time cap; with warn, says
        // in the log when the worst case would go past Ekos's 30 s.
        void read_direction(bool warn);
        // Takes the wheel's cap from a reply that carries it, and warns if
        // it is past Ekos's timeout.
        void take_ceiling(const Fields &reply, bool warn);
        // The driver's own safety net on a move, in seconds: the wheel's cap
        // plus a margin, or 28 s with a firmware that does not tell it.
        double move_ceiling_s() const;
        // Reads the settling hold after every leg into HoldNP[1].
        void read_settle();
        void read_led_mode();
        INDI::PropertySwitch LedSP {4};          // steady, pulse, off, test
        INDI::PropertySwitch DiagSP {1};         // "is it talking to the hardware?"
        INDI::PropertySwitch LogSP {2};          // on, off
        // The sweep: a full turn measuring the magnet at every step. The
        // samples go to a CSV file in the sweeps folder; the driver draws
        // nothing (see finish_sweep).
        INDI::PropertySwitch SweepSP {1};
        // The two files the driver leaves on disk: the movement log and the
        // last sweep. Written in the panel, because a path said once in the
        // log is a lost path.
        enum FileRow { FILE_LOG, FILE_SWEEP, FILE_COUNT };
        INDI::PropertyText FileTP {FILE_COUNT};
        // Where the sweeps end up. Writable, and saved in the configuration:
        // "~/Documents" is a reasonable default, not a truth - whoever keeps
        // their data on an external disk points it there.
        INDI::PropertyText SweepDirTP {1};

        // --- properties: options ------------------------------------------
        INDI::PropertyText FirmwareTP {3};

        // --- the USB link, lost and found again -----------------------------
        // A wheel unplugged and plugged back left the driver "Connected" on a
        // descriptor that could only fail: every write "Input/output error",
        // one line in the log per poll, the angle frozen in the panel, and
        // only Disconnect/Connect by hand brought it back. Now an I/O error
        // that means the port is gone (link_is_gone) closes the descriptor,
        // says so ONCE, and TimerHit tries to open the wheel again, with a
        // growing pause, until it answers with this profile's serial number.
        // CONNECTION stays On and Ok (Ekos keeps its device, see link_lost);
        // the position and sensor turn Alert while the link is down.
        static bool link_is_gone(int error_number);
        void link_lost(const std::string &why);
        bool try_reconnect();
        void link_found(const std::string &path);
        void remember_alias();
        bool m_link_lost {false};
        bool m_reconnecting {false};         // Handshake() on a reconnection
        double m_next_attempt {0.0};
        double m_retry_pause_s {0.0};
        // A descriptor opened by the reconnection, not by libindi's serial
        // plugin, which was told to let the dead one go: closed by Disconnect().
        int m_own_fd {-1};
        // The /dev/serial/by-id/ link of the port in use, found at connection:
        // it carries the USB serial number, so it points at the wheel under
        // whatever ttyACMn the kernel gives it when it comes back.
        std::string m_alias;
        std::string m_port_in_use;

        // --- internal state -------------------------------------------------
        LineReader m_reader;
        bool m_moving {false};
        int m_last_outcome_reported {-1};
        double m_move_start {0.0};
        double m_wheel_ceiling_s {0.0};          // 0 = the wheel did not say
        bool m_magnet_lost {false};
        // The '#' lines are developer diagnostics and normally end up only in
        // the protocol's debug level. During 'diag', though, they are the
        // content of the answer itself, and have to be shown.
        bool m_show_comments {false};
        std::FILE *m_log {nullptr};

        bool m_sweeping {false};
        int m_sweep_remaining {0};
        std::vector<wheelly::Sample> m_samples;
        // The serial number of the wheel of WHOEVER USES THIS PROFILE, learned
        // at the first connection and saved in the configuration. Empty until
        // nothing has ever been connected.
        std::string m_expected_serial;
        bool m_only_mine {false};     // true during the first pass
        bool m_serial_to_save {false};

        // A valid name close to the refused one, to suggest to the user.
        // Empty if none is found that does not tread on a name already in use.
        std::string suggest_name(const char *text, int slot);

        // How many slots the connected wheel has. It is NOT a constant: the
        // wheel says it at the handshake, and the driver adapts. It is the
        // reason why a Wheelly5 driver and a Wheelly7 one are not needed.
        int m_slots {wheelly::FACTORY_SLOTS};

        // names as the wheel knows them, so as not to rewrite what does not change
        std::string m_names[wheelly::MAX_SLOTS];
};

#endif  // WHEELLY_H
