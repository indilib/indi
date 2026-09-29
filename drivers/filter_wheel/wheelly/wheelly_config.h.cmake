// SPDX-FileCopyrightText: 2026 Matteo Beretta
// SPDX-License-Identifier: LGPL-2.1-or-later

// Called wheelly_config.h and not config.h: inside INDI's
// source tree the build directory already has a config.h of INDI's own, with
// the same CONFIG_H guard, and whichever came first on the include path would
// have silently hidden the other.

#ifndef WHEELLY_CONFIG_H
#define WHEELLY_CONFIG_H

#define WHEELLY_VERSION_MAJOR @WHEELLY_VERSION_MAJOR@
#define WHEELLY_VERSION_MINOR @WHEELLY_VERSION_MINOR@

// The device name, not written inline in the code: it is a trademark, and
// whoever forks the project must be able to change it in one place only (see
// TRADEMARK.md in the root).
#define WHEELLY_DEVICE_NAME "@WHEELLY_DEVICE_NAME@"

#endif  // WHEELLY_CONFIG_H
