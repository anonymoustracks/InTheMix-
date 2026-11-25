/*
 * GlideMix: the tempo line in Undo / Redo.
 */
#include "mixhousetempoundo.h"

#include "au3-project-history/UndoManager.h"

namespace {
std::function<std::optional<std::string>()>& captureHandler()
{
    static std::function<std::optional<std::string>()> f;
    return f;
}

std::function<void(const std::string&)>& restoreHandler()
{
    static std::function<void(const std::string&)> f;
    return f;
}

struct TempoLineRestorer final : UndoStateExtension
{
    explicit TempoLineRestorer(AudacityProject&)
    {
        if (captureHandler()) {
            line = captureHandler()();
        }
    }

    void RestoreUndoRedoState(AudacityProject&) override
    {
        if (line && restoreHandler()) {
            restoreHandler()(*line);
        }
    }

    std::optional<std::string> line;
};
}

void au::mixhouse::setTempoLineUndoHandlers(std::function<std::optional<std::string>()> capture,
                                            std::function<void(const std::string&)> restore)
{
    captureHandler() = std::move(capture);
    restoreHandler() = std::move(restore);
}

void au::mixhouse::registerTempoLineRestorer()
{
    static UndoRedoExtensionRegistry::Entry<TempoLineRestorer> sEntry { [](AudacityProject& project) -> std::shared_ptr<UndoStateExtension>
        { return std::make_shared<TempoLineRestorer>(project); }
    };
}
