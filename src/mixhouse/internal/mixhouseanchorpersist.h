/*
 * GlideMix
 */
#pragma once

#include <functional>
#include <string>

namespace au::mixhouse {
//! IN/OUT anchors changed in one project (1.3.6, Chris 2026-11-15), saved with
//! the project as the `glidemix_anchors` attribute and never written to the
//! library. A song not listed reads the library's anchors (its "song anchors"),
//! so a project saved before 1.3.6 plays as before.
//! `au3Project` is IAudacityProject::au3ProjectPtr(); `writer` is asked each
//! time the project is saved and returns the attribute's value.
void setAnchorStateWriter(void* au3Project, std::function<std::string()> writer);
//! Whether the opened file carried the attribute, and its value.
bool loadedAnchorState(void* au3Project, std::string* value);
}
