/*
 * GlideMix: Interface size -- the whole window scaled from 50 % to 200 %, like
 * Ableton's Display Zoom (Chris, 2026-11-13). Qt fixes the scale when the app
 * starts (QT_SCALE_FACTOR, set in main.cpp before the application exists), so
 * a new size applies after a restart.
 */
#pragma once

#include <array>

namespace au::appshell {
//! The setting, as stored by muse::settings() and read with QSettings in main.cpp.
inline constexpr const char* kInterfaceSizeKey = "ui/interfaceSize";
inline constexpr std::array<int, 7> kInterfaceSizes = { 50, 75, 100, 125, 150, 175, 200 };
inline constexpr const char* kInterfaceSizeActionPrefix = "interface-size-";
}
