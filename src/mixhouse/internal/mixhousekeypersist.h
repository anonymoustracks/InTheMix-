/*
 * GlideMix
 */
#pragma once

#include <functional>
#include <string>

namespace au::mixhouse {
//! Match keys (1.3.8, Chris 2026-11-16), saved with the project as the
//! `glidemix_keys` attribute: whether it is on, and which songs it transposed
//! (so turning it off puts only those back). No attribute = off.
//! `au3Project` is IAudacityProject::au3ProjectPtr(); `writer` is asked each
//! time the project is saved and returns the attribute's value.
void setKeyStateWriter(void* au3Project, std::function<std::string()> writer);
//! Whether the opened file carried the attribute, and its value.
bool loadedKeyState(void* au3Project, std::string* value);
}
