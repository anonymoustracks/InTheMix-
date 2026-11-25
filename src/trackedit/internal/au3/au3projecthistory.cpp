/*
* Audacity: A Digital Audio Editor
*/

#include "au3projecthistory.h"

#include <chrono>

#include "au3-project-history/ProjectHistory.h"
#include "au3-project-history/UndoManager.h"
#include "au3-project/Project.h"

using namespace au::trackedit;
using namespace au::au3;

void au::trackedit::Au3ProjectHistory::init()
{
    auto& project = projectRef();
    ::ProjectHistory::Get(project).InitialState();

    m_historyChanged.send(HistoryEvent::RestoredState);
}

bool au::trackedit::Au3ProjectHistory::undoAvailable() const
{
    if (!globalContext()->currentProject()) {
        return false;
    }

    auto& project = projectRef();
    return ::ProjectHistory::Get(project).UndoAvailable();
}

void au::trackedit::Au3ProjectHistory::undo()
{
    doUndo();

    m_interactionOngoing = false;
    m_historyChanged.send(HistoryEvent::RestoredState);
}

bool au::trackedit::Au3ProjectHistory::redoAvailable() const
{
    if (!globalContext()->currentProject()) {
        return false;
    }

    auto& project = projectRef();
    return ::ProjectHistory::Get(project).RedoAvailable();
}

void au::trackedit::Au3ProjectHistory::redo()
{
    doRedo();

    m_interactionOngoing = false;
    m_historyChanged.send(HistoryEvent::RestoredState);
}

void au::trackedit::Au3ProjectHistory::pushHistoryState(const std::string& longDescription, const std::string& shortDescription)
{
    pushHistoryState(longDescription, shortDescription, UndoPushType::NONE);
}

void Au3ProjectHistory::pushHistoryState(const std::string& longDescription, const std::string& shortDescription, UndoPushType flags)
{
    LOGI() << "pushHistoryState(\"" << shortDescription << "\", " << flags << ")";
    auto& project = projectRef();
    UndoPush undoFlags = static_cast<UndoPush>(flags);
    // GlideMix (Marc, 2026-11-19: a hitch after every edit): every history entry also writes the
    // project backup (autosave) on this thread. Time it, and what reacts to the new state, so a
    // log shows which part takes the time on a big project.
    const auto t0 = std::chrono::steady_clock::now();
    // GlideMix: the backup is written when the edits pause (scheduleAutoSave), not here.
    const bool wantsAutoSave = (undoFlags & UndoPush::NOAUTOSAVE) == UndoPush::NONE;
    ::ProjectHistory::Get(project).PushState(::au3::untranslatable(longDescription), ::au3::untranslatable(shortDescription),
                                             undoFlags | UndoPush::NOAUTOSAVE);
    const auto t1 = std::chrono::steady_clock::now();
    if (wantsAutoSave) {
        scheduleAutoSave();
    }

    m_interactionOngoing = false;
    m_historyChanged.send(HistoryEvent::NewState);
    const auto t2 = std::chrono::steady_clock::now();
    const auto ms = [](auto a, auto b) { return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count(); };
    if (ms(t0, t2) >= 50) {
        LOGI() << "GlideMix history timing: \"" << shortDescription << "\" recorded in " << ms(t0, t1)
               << " ms, views updated in " << ms(t1, t2) << " ms";
    }
}

void au::trackedit::Au3ProjectHistory::rollbackState()
{
    auto& project = projectRef();
    ::ProjectHistory::Get(project).RollbackState();
}

void Au3ProjectHistory::startUserInteraction()
{
    LOGI() << "startUserInteraction()";
    IF_ASSERT_FAILED(!m_interactionOngoing) {
        return;
    }
    // Modify the state, so that if the interaction gets canceled,
    // rollbackState would revert to the state at the beginning of the action.
    modifyState(false);
    m_interactionOngoing = true;
}

void Au3ProjectHistory::endUserInteraction(bool modifyState)
{
    LOGI() << "endUserInteraction()";
    if (m_interactionOngoing) {
        m_interactionOngoing = false;
        if (modifyState) {
            // No new history entry was pushed -> update the state.
            this->modifyState(true);
        }
    }
}

void Au3ProjectHistory::modifyState(bool autoSave)
{
    LOGD() << "modifyState(" << (autoSave ? "true" : "false") << ")";
    if (m_interactionOngoing) {
        LOGW() << "Attempt to modify state during undoable action";
        return;
    }
    auto& project = projectRef();
    ::ProjectHistory::Get(project).ModifyState(false);
    if (autoSave) {
        scheduleAutoSave();   // GlideMix: when the edits pause
    }
}

void Au3ProjectHistory::scheduleAutoSave()
{
    if (!m_autoSaveTimer) {
        m_autoSaveTimer = new QTimer();
        m_autoSaveTimer->setSingleShot(true);
        m_autoSaveTimer->setInterval(1500);
        QObject::connect(m_autoSaveTimer, &QTimer::timeout, [this]() { writePendingAutoSave(); });
        // Another project (or none): the pending backup was for the one that closed.
        globalContext()->currentProjectChanged().onNotify(this, [this]() {
            m_autoSaveTimer->stop();
            m_autoSaveProject = nullptr;
        });
    }
    void* current = &projectRef();
    if (m_autoSaveProject != current) {
        m_autoSaveProject = current;
        m_autoSavePendingSince.start();
    }
    if (m_autoSavePendingSince.elapsed() >= 10000) {
        writePendingAutoSave();   // editing without a pause: at least every 10 s
        return;
    }
    m_autoSaveTimer->start();
}

void Au3ProjectHistory::writePendingAutoSave()
{
    m_autoSaveTimer->stop();
    void* forProject = m_autoSaveProject;
    m_autoSaveProject = nullptr;
    const auto prj = globalContext()->currentProject();
    if (!forProject || !prj || reinterpret_cast<void*>(prj->au3ProjectPtr()) != forProject) {
        return;   // that project was closed
    }
    auto& project = projectRef();
    auto& history = ::ProjectHistory::Get(project);
    if (!history.GetDirty()) {
        return;   // saved since: the file is up to date, no backup needed
    }
    const auto t0 = std::chrono::steady_clock::now();
    try {
        ::ProjectHistory::AutoSave::Call(project);
    } catch (...) {
        LOGE() << "GlideMix: the automatic project backup failed";
        if (!m_autoSaveWarned && interactive()) {
            m_autoSaveWarned = true;
            interactive()->warning(muse::trc("trackedit", "InTheMix couldn't back up your project"),
                                   muse::trc("trackedit", "The automatic backup of your project could not be written (disk full, or the file is locked by another program). Your edits are still in InTheMix; save the project to keep them."));
        }
        return;
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    if (ms >= 50) {
        LOGI() << "GlideMix: project backup written in " << ms << " ms";
    }
}

void Au3ProjectHistory::warnIfAutoSaveFailed()
{
    if (!::ProjectHistory::Get(projectRef()).TakeAutoSaveFailed()) {
        return;
    }
    LOGE() << "GlideMix: the automatic project backup failed (disk full, or the file is locked by another program)";
    if (m_autoSaveWarned || !interactive()) {
        return;
    }
    m_autoSaveWarned = true;
    interactive()->warning(muse::trc("trackedit", "InTheMix couldn't back up your project"),
                           muse::trc("trackedit", "Your work is still open and you can carry on, but it is not being "
                                                  "backed up. The disk may be full, or another program (a sync or "
                                                  "backup tool, antivirus) may be using the project file. Use File > "
                                                  "Save project as to save it somewhere else."));
}

void Au3ProjectHistory::modifyState(const std::type_index& restorerType)
{
    if (m_interactionOngoing) {
        return;
    }
    auto& project = projectRef();
    ::ProjectHistory::Get(project).ModifyState(restorerType);
}

void Au3ProjectHistory::markUnsaved()
{
    LOGD() << "markUnsaved()";
    auto& project = projectRef();
    ::UndoManager::Get(project).MarkUnsaved();
}

void Au3ProjectHistory::undoRedoToIndex(size_t index)
{
    if (currentStateIndex() == index) {
        return;
    }

    while (currentStateIndex() > index && undoAvailable()) {
        doUndo();
    }
    while (currentStateIndex() < index && redoAvailable()) {
        doRedo();
    }

    m_historyChanged.send(HistoryEvent::RestoredState);
}

const muse::TranslatableString Au3ProjectHistory::topMostUndoActionName() const
{
    if (!undoAvailable()) {
        return {};
    }

    auto& project = projectRef();
    auto& undoManager = UndoManager::Get(project);

    int currentStateIndex = undoManager.GetCurrentState();

    TranslatableString actionName;
    undoManager.GetShortDescription(currentStateIndex, &actionName);

    return muse::TranslatableString::untranslatable(muse::String::fromQString(actionName.translated()));
}

const muse::TranslatableString Au3ProjectHistory::topMostRedoActionName() const
{
    if (!redoAvailable()) {
        return {};
    }

    auto& project = projectRef();
    auto& undoManager = UndoManager::Get(project);

    int currentStateIndex = undoManager.GetCurrentState();

    TranslatableString actionName;
    undoManager.GetShortDescription(currentStateIndex + 1, &actionName);

    return muse::TranslatableString::untranslatable(muse::String::fromQString(actionName.translated()));
}

size_t Au3ProjectHistory::undoRedoActionCount() const
{
    auto& project = projectRef();
    auto& undoManager = UndoManager::Get(project);
    return undoManager.GetNumStates();
}

size_t Au3ProjectHistory::currentStateIndex() const
{
    auto& project = projectRef();
    auto& undoManager = UndoManager::Get(project);
    return undoManager.GetCurrentState();
}

const muse::TranslatableString Au3ProjectHistory::lastActionNameAtIdx(size_t idx) const
{
    auto& project = projectRef();
    auto& undoManager = UndoManager::Get(project);

    TranslatableString actionName;
    undoManager.GetShortDescription(idx, &actionName);

    return muse::TranslatableString::untranslatable(muse::String::fromQString(actionName.translated()));
}

muse::async::Channel<HistoryEvent> Au3ProjectHistory::historyChanged() const
{
    return m_historyChanged;
}

Au3Project& au::trackedit::Au3ProjectHistory::projectRef() const
{
    return *reinterpret_cast<Au3Project*>(globalContext()->currentProject()->au3ProjectPtr());
}

void Au3ProjectHistory::doUndo()
{
    auto& project = projectRef();
    auto& undoManager = UndoManager::Get(project);
    undoManager.Undo(
        [&]( const UndoStackElem& elem ){
        ::ProjectHistory::Get(project).PopState(elem.state);
    });
}

void Au3ProjectHistory::doRedo()
{
    auto& project = projectRef();
    auto& undoManager = UndoManager::Get(project);
    undoManager.Redo(
        [&]( const UndoStackElem& elem ){
        ::ProjectHistory::Get(project).PopState(elem.state);
    });
}
