// SPDX-FileCopyrightText: 2026 Matteo Beretta
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "translations.h"

#include "wheelly_config.h"
#include "wheelly_protocol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wheelly
{

namespace
{

// The English catalogue. The Italian one lives in translations_it.cpp, by the
// same keys: English is the language of INDI's drivers and
// the only one a build inside INDI's tree needs, so the Italian file is
// compiled only with WHEELLY_ITALIAN (a CMake option, ON by default in this
// repository) and a copy of this folder can leave it out without editing a
// line. The reasoning comments stay here, next to the English source text;
// firmware/test/test_driver_standalone.py checks that both catalogues have
// the same keys.
//
// One key, one whole sentence. Never compose a sentence by gluing together
// pieces translated separately: word order changes from language to language
// and the result is wrong grammar that nobody proofreads.
const CatalogueEntry CATALOGUE[] =
{
    // --- property labels ----------------------------------------------------
    {"prop.slot",            "Filter slot"},
    // The label says the name of the field and nothing more. The naming rule
    // lives in the refusal MESSAGE, which is the only place in the INDI panel
    // where a long text really fits: the log box, at the bottom, which wraps
    // by itself. See firmware.md, section 5.1, for the three routes tried
    // before and why none of them works.
    {"prop.names",           "Filter names"},
    {"prop.position",        "Where the wheel is"},
    {"prop.position.angle",  "Angle"},
    {"prop.position.err",    "Residual error"},
    {"prop.position.retries", "Retries"},
    {"prop.sensor",          "Magnetic sensor"},
    {"prop.sensor.agc",      "Gain"},
    {"prop.sensor.mag",      "Magnitude"},
    {"prop.sensor.md",       "Magnet detected"},
    {"prop.sensor.ml",       "Field too weak"},
    {"prop.sensor.mh",       "Field too strong"},

    // The calibration angles, one row - one property - per slot, each with
    // its Set; the row's label is the slot number, "▶ n" on the slot the
    // wheel stands on. There is no rotation trim under them any more
    // (firmware.md 2.2, "One number per slot"). The element's label says what the number is.
    {"prop.angle.value",     "Calibration angle (°)"},
    {"prop.angles.here",     "▶ %1$s"},
    // the current row after a jog: the angle read now, not taught yet -
    // its Set confirms it (m_unsaved_slot)
    {"prop.angles.unsaved",  "▶ %1$s *"},
    {"prop.actions",         "Calibration actions"},
    {"prop.slots",           "Number of slots"},
    {"prop.slots.count",     "Slots"},
    {"prop.actions.save",    "Save to the wheel"},
    // The same "Save to the wheel", in Options right under INDI's
    // "Configuration": the settings of Options - motor,
    // holding, direction, LED - are saved from there, without going to the
    // calibration tab. A property of its own: CONFIG_PROCESS is INDI's.
    {"prop.wheelconfig",     "Wheel configuration"},
    // how a decimal number is written in the jog labels that are computed
    // (the slot pitch, 360/slots: 51.43 on a seven-slot wheel)
    {"num.decimal",          "."},
    // The jog rows. The Italian steps have the decimal comma.
    // Two rows, not one: KStars draws more than four exclusive switches as a
    // drop-down menu. There is no third row ("Save position", rejected): the
    // Set on the current slot's row does the same.
    {"prop.jog.down",        "Step back"},
    {"prop.jog.up",          "Step forward"},
    // The steps: one slot pitch, 360/slots, then 10, 1, 0.1 each way. No
    // 0.05 step (rejected): below the AS5600's count (0.088) it cannot be
    // told from not moving. The pitch follows the number of
    // slots: %1$s is it, already written with the language's decimal mark.
    {"prop.jog.mpitch",      "-%1$s°"},
    {"prop.jog.m10",         "-10°"},
    {"prop.jog.m1",          "-1°"},
    {"prop.jog.m01",         "-0.1°"},
    {"prop.jog.p01",         "+0.1°"},
    {"prop.jog.p1",          "+1°"},
    {"prop.jog.p10",         "+10°"},
    {"prop.jog.ppitch",      "+%1$s°"},
    {"prop.tolerance",       "Tolerances"},
    {"prop.tolerance.good",  "Good (deg)"},
    {"prop.tolerance.warn",  "Alert (deg)"},
    {"prop.tolerance.retries", "Max retries"},
    {"prop.motor",           "Motor"},
    {"prop.motor.ma",        "Run current (mA)"},
    {"prop.motor.speed",     "Speed"},
    {"prop.motor.accel",     "Acceleration"},
    {"prop.hold",            "Holding current at rest"},
    {"prop.hold.ma",         "Current (mA, 0 = released)"},
    {"prop.hold.settle",     "Hold after arrival (ms)"},
    // Which way the wheel may turn. "Angles" and not "clockwise":
    // which way is clockwise depends on the side the wheel is looked from,
    // the angle the sensor reads does not.
    {"prop.direction",       "Direction of travel"},
    {"prop.direction.shortest", "Shortest way"},
    {"prop.direction.up",    "Increasing angles only"},
    {"prop.direction.down",  "Decreasing angles only"},
    {"prop.led",             "LED"},
    {"prop.led.on",          "Steady on"},
    {"prop.led.pulse",       "Pulses while moving"},
    {"prop.led.off",         "Off"},
    {"prop.led.test",        "Test (blinks WHEELLY)"},
    {"prop.diag",            "Hardware check"},
    {"prop.diag.run",        "Ask the wheel"},
    {"prop.sweep",           "Magnet sweep"},
    {"prop.sweep.run",       "Turn all the way round"},
    {"prop.sweep.image",     "Last sweep"},
    {"prop.sweep.plot",      "Plot"},
    {"prop.log",             "Movement log"},
    {"prop.log.on",          "On"},
    {"prop.log.off",         "Off"},
    {"prop.files",           "Files on disk"},
    {"prop.files.log",       "Movement log"},
    {"prop.files.sweep",     "Last sweep"},
    {"prop.files.none",      "not written yet"},
    {"prop.sweepdir",        "Sweeps folder"},
    {"prop.sweepdir.path",   "Folder"},
    {"prop.firmware",        "Firmware"},
    {"prop.firmware.version", "Version"},
    {"prop.firmware.proto",  "Protocol"},
    {"prop.firmware.serial", "Serial number"},
    {"prop.language",        "Language"},
    {"prop.language.auto",   "From the system"},
    {"prop.language.en",     "English"},
    {"prop.language.it",     "Italian"},

    // --- messages ----------------------------------------------------------
    {
        "msg.connected",
        "Connected to Wheelly, firmware %1$s, protocol %2$s."
    },
    {
        "msg.wrong.device",
        "The device on this port did not answer as a Wheelly wheel. Check the port."
    },
    {
        "msg.serial.other",
        "This is a different Wheelly (serial %1$s) from the one this profile uses "
        "(%2$s): looking for yours on the other ports."
    },
    {
        "msg.serial.notfound",
        "The wheel of this profile (serial %1$s) is not on any port. Connecting to "
        "whichever Wheelly is here instead."
    },
    {
        "msg.serial.learned",
        "From now on this profile looks for the wheel with serial %1$s."
    },
    // The single advice about holding: the detent is always
    // removed, so there is no "if yours has one" left to ask - a wheel that
    // drifts with the motor released needs the holding current, full stop.
    {
        "msg.drift.suggest",
        "The wheel moved %1$s degrees on its own while at rest, past the alarm "
        "tolerance, and the motor is released at rest, so nothing holds it: turn "
        "on the holding current at rest - %2$s mA is the value this project "
        "suggests."
    },
    {
        "msg.drift.holding",
        "The wheel moved %1$s degrees on its own while at rest, past the alarm "
        "tolerance, and the holding current is already on at %2$s mA. Raising it "
        "may help, but look at the mechanics too: a clutch that lets go at rest is "
        "a matter of spring preload."
    },
    {
        "msg.wrong.protocol",
        "This firmware speaks protocol %1$s, this driver speaks %2$s. Update one of the two."
    },
    {
        "msg.slots.unsupported",
        "The wheel reports %1$s slots, which this driver cannot handle "
        "(from %2$s to %3$s)."
    },
    {
        "msg.no.answer",
        "The wheel is not answering."
    },
    {
        "msg.moving",
        "Moving to slot %1$s."
    },
    {
        "msg.arrived",
        "Slot %1$s reached, residual error %2$s deg."
    },
    {
        "msg.warning",
        "Slot %1$s reached but off by %2$s deg, beyond the good tolerance. "
        "Imaging continues."
    },
    {
        "msg.failed",
        "Slot %1$s NOT reached: off by %2$s deg after %3$s retries. "
        "Imaging is stopped so that no frame is taken with the wheel out of place."
    },
    {
        "msg.timeout",
        "The wheel did not finish within %1$s seconds. Imaging is stopped."
    },
    // THE USB LINK. Said once when it goes and once when it comes back:
    // before, a wheel unplugged left the driver writing to a dead port and
    // logging the failure at every poll, 25 times in a few seconds.
    // %1$s is the system's reason, e.g. "Input/output error".
    {
        "msg.link.lost",
        "The USB link to the wheel was lost (%1$s). The driver stays connected and "
        "reconnects by itself as soon as the wheel is back; until then commands "
        "cannot reach it."
    },
    {
        "msg.link.hangup",
        "the port was closed"
    },
    {
        "msg.link.back",
        "The wheel is back on %1$s: reconnected."
    },
    {
        "msg.link.down",
        "The wheel is not reachable right now: waiting for its USB link to come back."
    },
    // THE MOTOR DRIVER SET UP AGAIN by the firmware (the `! driver` event):
    // its settings live on the 12 V, not on the USB.
    {
        "msg.driver.power",
        "The motor driver got its 12 V after the wheel had started, and has been "
        "set up now."
    },
    {
        "msg.driver.reset",
        "The motor driver had lost its settings - the 12 V dropped and came back - "
        "and has been set up again before moving."
    },
    {
        "msg.magnet.lost",
        "The sensor no longer detects the magnet. This is serious: check the "
        "sensor wiring before imaging further."
    },
    {
        "msg.hold.on",
        "Holding current is on: the motor stays energised at rest. It warms up "
        "next to the sensor and the driver sings during exposures. Leave it off "
        "unless the wheel drifts at rest."
    },
    // THE HINT ON SLIP OR STALL. Said on its own line
    // right after every "not reached": the slot NOT reached, the time cap,
    // the failed jog. The first suspect when the motor stalls or the clutch
    // slips is a detent left in place, because the motor cannot climb out of
    // its notches - it is removed when the magnet is fitted, and a wheel
    // assembled from an older guide may still have it. A line of its own and
    // not appended: an INDI message is cut silently at 255 characters (see
    // "nome.rifiutato" below), and "msg.failed" in Italian is already 150.
    // The chapter is named by its title too: its number is prose here, not
    // the guide's {cap_the_wheel_body}, and a chapter inserted before it
    // would leave the title still right.
    {
        "msg.hint.detent",
        "If the motor stalls or the clutch slips, check first that the wheel's "
        "detent (its spring click stop) has been removed: the motor cannot climb "
        "out of its notches. Assembly guide, chapter 10, The wheel body."
    },
    {
        "msg.diag",
        "Asking the wheel whether it can really talk to its sensor and its motor "
        "driver:"
    },
    {
        "msg.slots.changed",
        "The wheel now has %1$s slots. Its calibration starts again from evenly "
        "spaced angles and generic names: teach each slot (steps, then Set on its "
        "row) and name the filters, then press 'Save to the wheel'. Until then nothing "
        "is written: switching the wheel off brings back the previous setup."
    },
    {
        "msg.slots.refused",
        "The wheel refused the new number of slots: %1$s"
    },
    {
        "msg.saved",
        "Calibration saved in the wheel."
    },
    {
        "msg.notconnected",
        "Connect the wheel first."
    },
    // Every change of a calibration angle, from the Set or from 'Save
    // position': the history the rotation trim was meant to
    // keep, with its date, in the log and in the movement register.
    {
        "msg.angle.changed",
        "Slot %1$s: %2$s° → %3$s°."
    },
    {
        "msg.angle.volatile",
        "The new angles live in the wheel's working memory: press 'Save to the "
        "wheel' to keep them after the wheel is switched off."
    },
    {
        "msg.angle.refused",
        "Slot %1$s: the angle was not changed. %2$s"
    },
    {
        "msg.angle.notnow",
        "Cannot change the angles while the wheel is moving: wait for it to stop."
    },
    {
        "msg.taught",
        "Slot %1$s now means the angle the wheel is at right now. Press 'Save to "
        "the wheel' to keep it after the wheel is switched off."
    },
    // the jog: %1$s the angle now, %2$s how far from the target
    {
        "msg.jog.done",
        "The wheel is at %1$s°, %2$s° from where the step asked."
    },
    {
        "msg.jog.failed",
        "The wheel did not get where the step asked: it is at %1$s°, %2$s° away. "
        "Try again, or turn it by hand and write the angle it reaches in its row."
    },
    {
        "msg.jog.notnow",
        "Cannot move the wheel by a step while it is moving: wait for it to stop."
    },
    {
        "msg.led.pulse",
        "The LED stays off, and breathes gently while the wheel goes to another "
        "filter. Press \"Save to the wheel\" to keep the choice after power-off."
    },
    {
        "msg.led.mode",
        "LED mode changed. Press \"Save to the wheel\" to keep the choice after power-off."
    },
    {
        "msg.direction.set",
        "Direction of travel changed. Press \"Save to the wheel\" to keep the choice after power-off."
    },
    {
        "msg.ceiling.ekos",
        "With these motor settings the longest filter change can take up to %1$s s, more than "
        "the %2$s s after which Ekos gives up on it. Raise the motor speed, shorten the hold "
        "after arrival, or choose the shortest way if your wheel allows it."
    },
    {
        "msg.led.test",
        "The LED is blinking WHEELLY in Morse for %1$s seconds. "
        "Do this with the cover open or in daylight: it is inside the optical path."
    },
    {
        "msg.sweep.start",
        "Turning all the way round and measuring the magnet at every step. The "
        "wheel moves: do this with the cover open, not during a sequence."
    },
    {
        "msg.sweep.done",
        "Sweep done: %1$s samples, magnitude from %2$s to %3$s, excursion %4$s "
        "counts (%5$s%%). The smaller it is, the better the magnet is centred."
    },
    {
        "msg.sweep.file",
        "The plot is here: %1$s"
    },
    {
        "msg.sweepdir.bad",
        "The folder %1$s cannot be used right now: %2$s. The sweeps will not be "
        "saved until it can."
    },
    {
        "msg.sweep.nofile",
        "Cannot write the plot to %1$s: %2$s"
    },
    {
        "msg.sweep.notnow",
        "Cannot sweep while the wheel is moving: wait for it to stop."
    },
    {
        "msg.sweep.failed",
        "The sweep stopped before finishing the turn: the plot would lie about "
        "the magnet, so none was made."
    },
    {
        "msg.log.on",
        "Movement log on: %1$s"
    },
    {
        "msg.log.off",
        "Movement log off."
    },
    {
        "msg.log.failed",
        "Cannot write the log file %1$s: %2$s"
    },
    {
        "msg.language.later",
        "The language changes when the driver starts again: its panels are built "
        "once, at start. In Ekos, stop INDI and start it again."
    },

    // --- the labels inside the sweep plot ------------------------------------
    // All capitals and without accents: the hand-drawn font in plot.cpp has
    // only those, and a letter it does not know becomes a space.
    {"graf.titolo",     "MAGNET SWEEP"},
    {"graf.asse.x",     "ANGLE (DEG)"},
    {"graf.asse.y",     "MAGNITUDE"},
    {"graf.campioni",   "SAMPLES"},
    {"graf.escursione", "EXCURSION"},
    {"graf.min",        "MIN"},
    {"graf.max",        "MAX"},

    // --- errors that come from the firmware ----------------------------------
    // The error code is the key: it is written in wheelly_protocol.h, and it is
    // the same thing for the machine and for the translation.
    {"err.1", "The wheel did not understand the command."},
    {"err.2", "Wrong arguments for the command."},
    {"err.3", "Value out of range: expected %1$s, got %2$s."},
    {
        "err.4", "The position sensor is not answering. Check its wiring, and "
        "the VDD5V-VDD3V3 jumper on the module."
    },
    {"err.5", "The sensor works but does not see the magnet."},
    // Nearly always the 12 V: the driver's logic lives on the motor supply,
    // and a wheel powered from the USB alone has a driver that says nothing.
    {"err.6", "The motor driver is not answering: is the 12 V supply connected? The wheel does not move without it."},
    {"err.7", "Not allowed right now: the wheel is busy."},
    {"err.8", "The wheel could not save to its memory."},
    {"err.9", "Invalid filter name (%1$s)."},
    {"err.unknown", "The wheel reported error %1$s."},

    // --- why a filter name was refused -----------------------------------------
    // The text does not only say "no": it says what to write. The name ends up
    // in the FITS header and in the file name, and a refusal that does not
    // explain turns into a user who retries at random.
    {"nome.empty",     "the name is empty"},
    {"nome.too-long",  "too long: at most 32 characters"},
    {"nome.bad-edge",  "it must start and end with a letter or a digit"},
    // The reason says what is wrong and nothing more: the list of allowed
    // characters is given by the refusal sentence, always, and repeating it
    // here would say it twice on the same line.
    //
    // The culprit character is named: "only letters, digits, _ - and ." forces
    // rereading the name character by character to find which one is the
    // crooked one; "there is a space" is understood in an instant.
    {"nome.bad-char",  "there is a character that cannot be used"},
    {
        "nome.bad-char.space",
        "there is a space"
    },
    {
        "nome.bad-char.accent",
        "there is an accented or non-English letter"
    },
    {
        "nome.bad-char.invisible",
        "there is an invisible character in it"
    },
    {
        "nome.bad-char.quale",
        "\"%1$s\" cannot be used"
    },
    {
        "nome.reserved",  "this is a reserved device name on Windows, and the "
        "filter name is also used as a folder name"
    },
    {
        "nome.duplicate", "another slot already has this name, ignoring upper "
        "and lower case"
    },
    {"nome.rifiutato", "Filter name refused: %1$s"},
    // The refusal ends up in the log, and that is the only place where the
    // explanation fits: so it says everything. The rule ("nome.ammessi") is
    // joined to it only when the two fit together: an INDI message is a
    // char[MAXINDIMESSAGE] with MAXINDIMESSAGE = 255 (indiapi.h), and whatever
    // is left over is cut SILENTLY - with the rule always appended, the refusal
    // of "-Lum-" reached Ekos truncated at "...una cif". When they do not fit
    // they go on two lines, the refusal LAST (see refuse() in wheelly.cpp).
    {
        "nome.rifiutato.slot",
        "Slot %1$s: \"%2$s\" refused - %3$s."
    },
    {
        "nome.rifiutato.slot.prova",
        "Slot %1$s: \"%2$s\" refused - %3$s. A name that would be accepted here: "
        "\"%4$s\"."
    },
    // A slot left without a name is a slot without a filter: the driver names
    // it and says so, so that the name appearing in the field is not a mystery.
    {
        "nome.vuoto.dato",
        "Slot %1$s left empty, so it has no filter: it is now called \"%2$s\"."
    },
    // The rule, which ALWAYS goes with the refusal - even when the
    // reason has nothing to do with the characters: whoever reads the refusal
    // is the person who at that moment wants to know what they can write.
    {
        "nome.ammessi",
        "Allowed: letters without accents, digits, _ - and . ; 1 to 32 characters; "
        "the first and the last must be a letter or a digit."
    },

};

Language g_language = Language::ENGLISH;

Language from_environment()
{
    const char *variables[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    for (const char *v : variables)
    {
        const char *value = std::getenv(v);
        if (value && value[0] != '\0')
        {
            if (std::strncmp(value, "it", 2) == 0) return Language::ITALIAN;
            return Language::ENGLISH;
        }
    }
    return Language::ENGLISH;
}

const CatalogueEntry *find(const CatalogueEntry *catalogue, size_t size,
                           const char *key)
{
    for (size_t i = 0; i < size; i++)
    {
        if (std::strcmp(catalogue[i].key, key) == 0) return &catalogue[i];
    }
    return nullptr;
}

const CatalogueEntry *find_english(const char *key)
{
    return find(CATALOGUE, sizeof CATALOGUE / sizeof CATALOGUE[0], key);
}

}  // namespace

void set_language(Language language)
{
    g_language = (language == Language::AUTO) ? from_environment() : language;
#if !WHEELLY_ITALIAN
    // Built without the Italian catalogue: whatever was asked for, or found
    // in the environment, the texts are English, and saying so here keeps
    // active_language() honest for whoever reads it.
    g_language = Language::ENGLISH;
#endif
}

Language active_language()
{
    return g_language;
}

const char *active_language_code()
{
    return g_language == Language::ITALIAN ? "it" : "en";
}

const char *tr(const char *key)
{
    const CatalogueEntry *en = find_english(key);
    if (en == nullptr) return key;     // the programmer's defect, and it shows
#if WHEELLY_ITALIAN
    if (g_language == Language::ITALIAN)
    {
        const CatalogueEntry *it = find(ITALIAN_CATALOGUE, ITALIAN_CATALOGUE_SIZE, key);
        if (it != nullptr && it->text != nullptr && it->text[0] != '\0')
            return it->text;
    }
#endif
    return en->text;                   // English is always the safety net
}

std::string trf(const char *key, const std::vector<std::string> &params)
{
    const std::string format = tr(key);
    std::string out;
    out.reserve(format.size() + 64);

    // The numbered placeholders %1$s, %2$s, ... are interpreted by hand instead
    // of being passed to snprintf: with snprintf the number and the type of the
    // parameters would come from a catalogue string, that is from data, and a
    // badly written catalogue would become a random memory read. Here the worst
    // that can happen is a placeholder left written as it is.
    for (size_t i = 0; i < format.size(); i++)
    {
        if (format[i] != '%')
        {
            out += format[i];
            continue;
        }
        if (i + 1 < format.size() && format[i + 1] == '%')
        {
            out += '%';
            i++;
            continue;
        }
        size_t j = i + 1;
        size_t number = 0;
        while (j < format.size() && format[j] >= '0' && format[j] <= '9')
        {
            number = number * 10 + (size_t)(format[j] - '0');
            j++;
        }
        if (number >= 1 && j + 1 < format.size() &&
                format[j] == '$' && format[j + 1] == 's')
        {
            if (number <= params.size()) out += params[number - 1];
            i = j + 1;
        }
        else
        {
            out += format[i];
        }
    }
    return out;
}

std::string error_message(int code, const std::string &expected,
                          const std::string &got, const std::string &reason)
{
    char key[16];
    std::snprintf(key, sizeof(key), "err.%d", code);
    if (find_english(key) == nullptr)
        return trf("err.unknown", {std::to_string(code)});

    switch (code)
    {
        case ERR_OUT_OF_RANGE:
            return trf(key, {expected, got});
        case ERR_BAD_FILTER_NAME:
        {
            std::string explanation = reason.empty()
                                      ? std::string() : std::string(tr(("nome." + reason).c_str()));
            if (explanation == "nome." + reason) explanation = reason;
            return trf(key, {explanation});
        }
        default:
            return tr(key);
    }
}

std::string filter_name_reason(int outcome, const char *text)
{
    // The generic reason - "only letters, digits, _ - and ." - forces rereading
    // one's own name character by character to find which one is the crooked
    // one. Here the culprit is named: "there is a space" is understood in an
    // instant.
    //
    // The accented letter has a sentence all of its own because it really
    // cannot be printed: in UTF-8 it is two bytes, and sending back only one of
    // them would not merely give a wrong character. Tried on purpose by removing
    // this line: the unpaired byte is not valid UTF-8, hence not valid XML, and
    // the INDI client dies with "not well-formed (invalid token)" - not on this
    // message, on the whole stream. A name with the wrong character would
    // switch the panel off.
    if (outcome == NAME_BAD_CHAR && text != nullptr)
    {
        for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++)
        {
            if (name_is_allowed((char) * p)) continue;
            if (*p == ' ') return tr("nome.bad-char.space");
            if (*p >= 0x80) return tr("nome.bad-char.accent");
            if (*p < 0x20 || *p == 0x7f) return tr("nome.bad-char.invisible");
            const char one[2] = {(char)*p, '\0'};
            return trf("nome.bad-char.quale", {one});
        }
    }

    switch (outcome)
    {
        case NAME_EMPTY:
            return tr("nome.empty");
        case NAME_TOO_LONG:
            return tr("nome.too-long");
        case NAME_BAD_EDGE:
            return tr("nome.bad-edge");
        case NAME_BAD_CHAR:
            return tr("nome.bad-char");
        case NAME_RESERVED:
            return tr("nome.reserved");
        default:
            return std::string();
    }
}


}  // namespace wheelly
