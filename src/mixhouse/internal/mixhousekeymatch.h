/*
 * GlideMix
 */
#pragma once

#include <QMap>
#include <QSet>
#include <QTimer>

#include "async/asyncable.h"
#include "modularity/ioc.h"
#include "context/iglobalcontext.h"
#include "trackedit/iclipsinteraction.h"
#include "trackedit/iprojecthistory.h"
#include "trackedit/itrackeditinteraction.h"

#include "mixhouse/imixhousebeatgrids.h"
#include "mixhouse/imixhousekeymatch.h"
#include "mixhouse/imixhouselibraryconfiguration.h"
#include "mixhouse/imixhouselibrary.h"
#include "mixhouse/imixhouselexicon.h"
#include "mixhouse/imixhouserekordboxxml.h"
#include "mixhouse/imixhousetempomap.h"
#include "mixhouse/imixhousetransitions.h"

namespace au::mixhouse {
class MixHouseKeyMatch : public IMixHouseKeyMatch, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<context::IGlobalContext> globalContext { this };
    muse::ContextInject<trackedit::ITrackeditInteraction> trackeditInteraction { this };
    muse::ContextInject<IMixHouseLibrary> library { this };
    muse::ContextInject<IMixHouseLexicon> lexicon { this };
    muse::ContextInject<IMixHouseRekordboxXml> rekordboxXml { this };
    muse::ContextInject<IMixHouseTransitions> transitions { this };
    muse::ContextInject<IMixHouseBeatGrids> beatGrids { this };
    muse::ContextInject<IMixHouseTempoMap> tempoMap { this };
    muse::ContextInject<trackedit::IProjectHistory> projectHistory { this };
    muse::ContextInject<trackedit::IClipsInteraction> clipsInteraction { this };
    muse::GlobalInject<IMixHouseLibraryConfiguration> libraryConfiguration;

public:
    explicit MixHouseKeyMatch(const muse::modularity::ContextPtr& iocCtx);
    ~MixHouseKeyMatch() override;

    void init();

    ClipKeyInfo keyInfo(const trackedit::ClipKey& clip) const override;
    Match match(const trackedit::ClipKey& clip) const override;
    bool transpose(const trackedit::ClipKey& clip, int semitones) override;
    muse::async::Notification keysChanged() const override;
    bool matchKeysOn() const override;
    void setMatchKeysOn(bool on) override;
    bool clashHint() const override;
    muse::async::Notification matchKeysChanged() const override;

private:
    int libraryKeyId(const QString& clipTitle) const;
    ClipKeyInfo keyInfoAt(const trackedit::Clip& clip, double atTime) const;
    bool keylockOff() const;
    //! Auto-Keylock: a song whose tempo line over [start, end] is ever more than
    //! the set percent from `unstretchedBpm` (where it plays at its own speed).
    bool autoKeylocked(double unstretchedBpm, double start, double end) const;
    void keysMayHaveChanged();

    //! Match keys: one pass over the transitions, in time order. On: matches
    //! clashing incoming songs (and records them in m_auto). Off: looks for new
    //! clashes it would fix, for the hint. True if a clip was transposed.
    bool runMatchPass();
    void ensureProjectState();
    std::string savedKeyState() const;
    void setHint(bool hint);

    muse::async::Notification m_keysChanged;
    muse::async::Notification m_matchKeysChanged;
    bool m_matchOn = false;
    QMap<QString, int> m_auto;        //!< song title -> cents set by Match keys
    void* m_stateFor = nullptr;       //!< the au3 project m_matchOn / m_auto belong to
    QSet<QString> m_seenClashes;      //!< "out|in" pairs already hinted
    bool m_seedClashes = false;       //!< next pass only notes the clashes (an opened mix)
    QTimer m_passTimer;
    QTimer m_hintTimer;
    bool m_hint = false;
    QTimer m_keysTimer;
    bool m_keylock = true;
    int m_autoKeylock = 0;
};
}
