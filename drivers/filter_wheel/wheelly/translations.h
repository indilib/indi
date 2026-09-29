// SPDX-FileCopyrightText: 2026 Matteo Beretta
// SPDX-License-Identifier: LGPL-2.1-or-later

// Wheelly - the texts for the user, by key.
//
// Project rule (firmware.md, section 13): no user-facing string is written
// inline in the code. Each one has a key, and the texts live here, one per
// language.
//
// The criterion for whether a string is translated or not is simple: if a
// machine reads it, or whoever is watching the raw serial, it is English and
// never changes; if the user reads it, it has a key and a translation.
//
// The keys are strings and not an enum on purpose: adding one does not force
// touching two files, and a wrong key shows at once because the text that
// appears is the key itself and the driver writes it in the log.
//
// The TAB NAMES stay in English, and it is not an oversight: they sit in the
// same tab bar INDI creates by itself - Connection, Options - which we cannot
// translate. A bar half in Italian and half in English would look broken.
// Translated is everything inside the panels, which is ours.

#ifndef WHEELLY_TRANSLATIONS_H
#define WHEELLY_TRANSLATIONS_H

#include <cstddef>
#include <string>
#include <vector>

namespace wheelly
{

enum class Language { AUTO, ENGLISH, ITALIAN };

// Chooses the language. With AUTO the system environment is looked at.
// It must be called BEFORE creating the properties, because the labels are
// composed only once: INDI clients keep them for the whole connection.
// Changing language takes effect on reconnection.
void set_language(Language language);
Language active_language();
const char *active_language_code();

// The text for a key. If it is missing in the chosen language, English is
// the fallback; if it is missing altogether the key itself comes back, which
// is ugly to see but is the programmer's defect, not the user's, and it is
// noticed at once.
const char *tr(const char *key);

// Like tr(), but with the parameters slotted in. The placeholders are numbered
// (%1$s, %2$s) on purpose: that way a language can put them in another order
// without the code knowing anything about it.
std::string trf(const char *key, const std::vector<std::string> &params);

// Composes the sentence for an error code that came from the firmware. The
// code is the key, the named fields of the error line are the parameters.
std::string error_message(int code, const std::string &expected,
                          const std::string &got, const std::string &reason);

// The reason a filter name was rejected, in words. It wants the name too, and
// not only the outcome, because on a character that is not allowed it NAMES
// the culprit - "there is a space" - instead of listing the allowed ones and
// leaving the user the job of spotting the difference.
std::string filter_name_reason(int outcome, const char *text);

// --- the catalogues' plumbing, for translations.cpp and translations_it.cpp

struct CatalogueEntry
{
    const char *key;
    const char *text;
};

// The Italian catalogue (translations_it.cpp), compiled only with
// WHEELLY_ITALIAN: see translations.cpp for why it is a file of its own.
extern const CatalogueEntry ITALIAN_CATALOGUE[];
extern const size_t ITALIAN_CATALOGUE_SIZE;

}  // namespace wheelly

#endif  // WHEELLY_TRANSLATIONS_H
