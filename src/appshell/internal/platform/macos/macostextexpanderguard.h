/*
 * GlideMix
 */
#pragma once

#include <string>
#include <vector>

#include <QtGlobal>

namespace au::appshell {
//! GlideMix 1.3.10 (Chris, 2026-11-18): a text expander (Typinator) erased an abbreviation it saw in
//! two quick V presses with Backspace -- in GlideMix that is Delete, and the selected song went.
//! The guard drops a Backspace / Delete that another app sends right after a typed key (and that
//! app's keys for a moment after, its replacement text), keeping real key presses, controllers and
//! macros as they are. macOS only; no-op elsewhere.
void installTextExpanderGuard();

//! Names of running apps known to type into other apps (text expanders, keyboard macros).
std::vector<std::string> runningTextExpanders();
}

#ifndef Q_OS_MAC
inline void au::appshell::installTextExpanderGuard() {}
inline std::vector<std::string> au::appshell::runningTextExpanders() { return {}; }
#endif
