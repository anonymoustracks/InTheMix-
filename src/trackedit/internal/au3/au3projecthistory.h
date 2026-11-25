/*
* Audacity: A Digital Audio Editor
*/

#pragma once

#include "framework/interactive/iinteractive.h"
#include "iprojecthistory.h"

#include "context/iglobalcontext.h"
#include "modularity/ioc.h"
#include "global/async/asyncable.h"

#include "au3wrap/au3types.h"

#include <QElapsedTimer>
#include <QTimer>

namespace au::trackedit {
class Au3ProjectHistory : public IProjectHistory, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<context::IGlobalContext> globalContext { this };
    muse::ContextInject<muse::IInteractive> interactive { this };

public:
    Au3ProjectHistory(const muse::modularity::ContextPtr& ctx)
        : muse::Contextable(ctx) {}

    void init() override;

    bool undoAvailable() const override;
    void undo() override;
    bool redoAvailable() const override;
    void redo() override;
    void pushHistoryState(
        const std::string& longDescription, const std::string& shortDescription) override;
    void pushHistoryState(
        const std::string& longDescription, const std::string& shortDescription, UndoPushType flags) override;
    void rollbackState() override;
    void modifyState(bool autoSave) override;
    void modifyState(const std::type_index& undoStateExtensionTypeIndex) override;
    void markUnsaved() override;

    void startUserInteraction() override;
    void endUserInteraction(bool modifyState) override;
    bool interactionOngoing() const override { return m_interactionOngoing; }

    void undoRedoToIndex(size_t index) override;

    const muse::TranslatableString topMostUndoActionName() const override;
    const muse::TranslatableString topMostRedoActionName() const override;
    size_t undoRedoActionCount() const override;
    size_t currentStateIndex() const override;
    const muse::TranslatableString lastActionNameAtIdx(size_t idx) const override;

    muse::async::Channel<HistoryEvent> historyChanged() const override;

private:
    au3::Au3Project& projectRef() const;

    void doUndo();
    void doRedo();

    muse::async::Channel<HistoryEvent> m_historyChanged;

    bool m_interactionOngoing = false;

private:
    //! GlideMix: warns (once per session) that the automatic backup failed.
    void warnIfAutoSaveFailed();
    bool m_autoSaveWarned = false;

    //! GlideMix (Marc, 2026-11-19; Chris, 2026-11-21): the project backup (autosave) is written
    //! once the edits pause (1.5 s), at the latest every 10 s while editing goes on -- not on
    //! every history entry, which held the UI ~1 s per edit on a big project.
    void scheduleAutoSave();
    void writePendingAutoSave();
    QTimer* m_autoSaveTimer = nullptr;
    QElapsedTimer m_autoSavePendingSince;
    void* m_autoSaveProject = nullptr;   ///< the project the pending backup is for
};
}
