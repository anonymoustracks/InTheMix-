/*
 * GlideMix
 */
#pragma once

#include <functional>
#include <string>

namespace au::mixhouse {
//! The tempo line and Follow tracks, saved with the project (the
//! `glidemix_tempo` attribute). `au3Project` is IAudacityProject::au3ProjectPtr().
//! `writer` returns what to save ("" = nothing); it is asked each time the project is saved.
void setTempoLineWriter(void* au3Project, std::function<std::string()> writer);
//! What the project file held when it was opened ("" for a new or older project).
std::string loadedTempoLine(void* au3Project);
}
