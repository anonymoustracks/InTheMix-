/*
 * GlideMix
 */
#pragma once

#include <functional>
#include <string>

namespace au::mixhouse {
//! Which songs of a project keep the 1.3.4 warp (their beat map followed as
//! stored, never repaired), saved with the project as the `glidemix_warp`
//! attribute. Chris (2026-11-15): "Keep old mixes as-is" -- a project saved
//! before 1.3.5 has no such attribute; its songs at the time it is opened keep
//! the old warp, songs added since get the repaired one.
//! `au3Project` is IAudacityProject::au3ProjectPtr(); `writer` is asked each
//! time the project is saved and returns the attribute's value.
void setWarpStateWriter(void* au3Project, std::function<std::string()> writer);
//! Whether the opened file carried the attribute, and its value.
bool loadedWarpState(void* au3Project, std::string* value);
}
