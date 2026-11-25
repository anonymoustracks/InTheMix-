/*
 * GlideMix: the tempo line in Undo / Redo.
 */
#pragma once

#include <functional>
#include <optional>
#include <string>

namespace au::mixhouse {
//! Every undo state keeps a copy of the tempo line (MixHouseTempoMap's saved form); undo and redo
//! put it back (Marc and Chris, 2026-11-21: Cmd+Z must work on the tempo points too).
//! `capture` returns nullopt while the line of the open project is not known yet (just opened):
//! such a state leaves the line alone when it is restored.
void setTempoLineUndoHandlers(std::function<std::optional<std::string>()> capture,
                              std::function<void(const std::string&)> restore);
void registerTempoLineRestorer();
}
