/*
 * GlideMix: the values in the capsule display at the top of the project
 * window -- key, energy and place in the mix of the song under the playhead,
 * whether it plays, and how busy the computer's processor is with GlideMix.
 */
#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QQmlParserStatus>
#include <QTimer>
#include <QtQmlIntegration>

#include "async/asyncable.h"
#include "modularity/ioc.h"

#include "context/iglobalcontext.h"
#include "mixhouse/imixhousebeatgrids.h"
#include "mixhouse/imixhousekeymatch.h"
#include "mixhouse/imixhouselibrary.h"
#include "mixhouse/imixhouselibraryconfiguration.h"
#include "mixhouse/imixhousetempomap.h"
#include "playback/iperformance.h"
#include "trackedit/iselectioncontroller.h"
#include "trackedit/iprojecthistory.h"
#include "trackedit/dom/clip.h"

#include <vector>

namespace au::mixhouse {
class MixHouseNowPlayingModel : public QObject, public QQmlParserStatus, public muse::Contextable,
    public muse::async::Asyncable
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(QString keyText READ keyText NOTIFY songChanged)
    Q_PROPERTY(QColor keyColor READ keyColor NOTIFY songChanged)
    Q_PROPERTY(int energy READ energy NOTIFY songChanged)
    Q_PROPERTY(int songIndex READ songIndex NOTIFY songChanged)
    Q_PROPERTY(int songCount READ songCount NOTIFY songChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int cpuPercent READ cpuPercent NOTIFY cpuChanged)
    //! MB per second GlideMix itself read from disk over the last second
    Q_PROPERTY(double diskMBps READ diskMBps NOTIFY cpuChanged)
    //! GlideMix's own memory use (what Activity Monitor / Task Manager show), GB,
    //! and as a share of the computer's memory.
    Q_PROPERTY(double memoryGB READ memoryGB NOTIFY cpuChanged)
    Q_PROPERTY(double memoryShare READ memoryShare NOTIFY cpuChanged)

    // GlideMix: the DJ clock (Chris 2026-11-13) -- the song under the playhead.
    Q_PROPERTY(bool hasSong READ hasSong NOTIFY clockChanged)
    Q_PROPERTY(double songElapsed READ songElapsed NOTIFY clockChanged)
    Q_PROPERTY(double songRemaining READ songRemaining NOTIFY clockChanged)
    Q_PROPERTY(double songProgress READ songProgress NOTIFY clockChanged)
    //! The LAST song of the mix is in its last 32 bars (Chris: only the end of
    //! the project flashes SONG LEFT).
    Q_PROPERTY(bool endWarning READ endWarning NOTIFY clockChanged)
    //! 32 bars or less to the next mix.
    Q_PROPERTY(bool mixWarning READ mixWarning NOTIFY clockChanged)
    Q_PROPERTY(int bar READ bar NOTIFY clockChanged)
    Q_PROPERTY(int beat READ beat NOTIFY clockChanged)
    Q_PROPERTY(int beatsPerBar READ beatsPerBar NOTIFY clockChanged)
    Q_PROPERTY(QString phraseText READ phraseText NOTIFY clockChanged)
    Q_PROPERTY(QColor phraseColor READ phraseColor NOTIFY clockChanged)
    Q_PROPERTY(int phraseBarsLeft READ phraseBarsLeft NOTIFY clockChanged)
    //! Seconds to the next transition (the playing song's OUT anchor); -1 = none, 0 = mixing now.
    Q_PROPERTY(double nextMixSec READ nextMixSec NOTIFY clockChanged)
    Q_PROPERTY(int nextMixBars READ nextMixBars NOTIFY clockChanged)
    Q_PROPERTY(double mixTotal READ mixTotal NOTIFY clockChanged)
    //! InTheMix: where the playhead is in the mix, seconds (Fusion's CURRENT).
    Q_PROPERTY(double mixPosition READ mixPosition NOTIFY clockChanged)
    //! The song's own tempo as it plays, and how far it is stretched (+ = faster).
    Q_PROPERTY(double songBpm READ songBpm NOTIFY clockChanged)
    Q_PROPERTY(double stretchPercent READ stretchPercent NOTIFY clockChanged)

    // GlideMix: the DJ status bar (bottom right) -- the mix at a glance.
    //! The mix's tempo at the playhead (the master tempo line).
    Q_PROPERTY(double mixBpm READ mixBpm NOTIFY clockChanged)
    //! The selected range in bars of the mix's grid (-1 = no selection) and seconds.
    Q_PROPERTY(double selectionBars READ selectionBars NOTIFY clockChanged)
    Q_PROPERTY(double selectionSec READ selectionSec NOTIFY clockChanged)
    Q_PROPERTY(int beatsPerBarMix READ beatsPerBarMix NOTIFY clockChanged)
    //! The songs' own tempos, lowest and highest (0 = unknown).
    Q_PROPERTY(double mixMinBpm READ mixMinBpm NOTIFY clockChanged)
    Q_PROPERTY(double mixMaxBpm READ mixMaxBpm NOTIFY clockChanged)
    //! Transitions between songs whose keys mix (Camelot neighbours), of all with known keys.
    Q_PROPERTY(int harmonicGood READ harmonicGood NOTIFY clockChanged)
    Q_PROPERTY(int harmonicTotal READ harmonicTotal NOTIFY clockChanged)
    //! The first clashing transition, as "3 → 4" (song numbers); empty when none.
    Q_PROPERTY(QString harmonicClash READ harmonicClash NOTIFY clockChanged)

    muse::ContextInject<context::IGlobalContext> globalContext { this };
    muse::ContextInject<IMixHouseLibrary> library { this };
    muse::ContextInject<IMixHouseBeatGrids> beatGrids { this };
    muse::ContextInject<IMixHouseKeyMatch> keyMatch { this };
    muse::ContextInject<IMixHouseTempoMap> tempoMap { this };
    //! While PERFORM plays, the DJ clock follows the audience playhead, not the cue.
    muse::ContextInject<playback::IPerformance> performance { this };
    muse::ContextInject<trackedit::ISelectionController> selectionController { this };
    muse::ContextInject<trackedit::IProjectHistory> projectHistory { this };
    muse::GlobalInject<IMixHouseLibraryConfiguration> libraryConfiguration;

public:
    explicit MixHouseNowPlayingModel(QObject* parent = nullptr);

    QString keyText() const { return m_keyText; }
    QColor keyColor() const { return m_keyColor; }
    int energy() const { return m_energy; }
    int songIndex() const { return m_songIndex; }
    int songCount() const { return m_songCount; }
    bool playing() const { return m_playing; }
    int cpuPercent() const { return m_cpuPercent; }
    double diskMBps() const { return m_diskMBps; }
    double memoryGB() const { return m_memoryGB; }
    double memoryShare() const { return m_memoryShare; }

    bool hasSong() const { return m_clock.hasSong; }
    double songElapsed() const { return m_clock.elapsed; }
    double songRemaining() const { return m_clock.remaining; }
    double songProgress() const { return m_clock.progress; }
    bool endWarning() const { return m_clock.endWarning; }
    bool mixWarning() const { return m_clock.mixWarning; }
    int bar() const { return m_clock.bar; }
    int beat() const { return m_clock.beat; }
    int beatsPerBar() const { return m_clock.beatsPerBar; }
    QString phraseText() const { return m_clock.phraseText; }
    QColor phraseColor() const { return m_clock.phraseColor; }
    int phraseBarsLeft() const { return m_clock.phraseBarsLeft; }
    double nextMixSec() const { return m_clock.nextMixSec; }
    int nextMixBars() const { return m_clock.nextMixBars; }
    double mixTotal() const { return m_clock.mixTotal; }
    double mixPosition() const { return clockPosition(); }
    double songBpm() const { return m_clock.songBpm; }
    double stretchPercent() const { return m_clock.stretchPercent; }
    double mixBpm() const { return m_clock.mixBpm; }
    double selectionBars() const { return m_clock.selectionBars; }
    double selectionSec() const { return m_clock.selectionSec; }
    int beatsPerBarMix() const { return m_clock.beatsPerBarMix; }
    double mixMinBpm() const { return m_clock.mixMinBpm; }
    double mixMaxBpm() const { return m_clock.mixMaxBpm; }
    int harmonicGood() const { return m_clock.harmonicGood; }
    int harmonicTotal() const { return m_clock.harmonicTotal; }
    QString harmonicClash() const { return m_clock.harmonicClash; }

    void classBegin() override {}
    void componentComplete() override;

signals:
    void songChanged();
    void playingChanged();
    void cpuChanged();
    void clockChanged();

private:
    void update();
    void updatePlaying();
    double clockPosition() const;
    void sampleCpu();
    struct Clock;
    struct SongCache;
    void rebuildSongs();
    void updateClock(Clock& clock, const SongCache& song, bool hasNext, double pos);
    void updateMix(Clock& clock, double pos, const trackedit::ITrackeditProject& project);

    struct SongInfo {
        int keyId = 0;
        int energy = 0;
        double bpm = 0.0;
    };
    struct Clock {
        bool hasSong = false;
        double elapsed = 0.0;
        double remaining = 0.0;
        double progress = 0.0;
        bool endWarning = false;
        bool mixWarning = false;
        int bar = 0;
        int beat = 0;
        int beatsPerBar = 4;
        QString phraseText;
        QColor phraseColor;
        int phraseBarsLeft = 0;
        double nextMixSec = -1.0;
        int nextMixBars = 0;
        double mixTotal = 0.0;
        double songBpm = 0.0;
        double stretchPercent = 0.0;
        double mixBpm = 0.0;
        double selectionBars = -1.0;
        double selectionSec = 0.0;
        int beatsPerBarMix = 4;
        double mixMinBpm = 0.0;
        double mixMaxBpm = 0.0;
        int harmonicGood = 0;
        int harmonicTotal = 0;
        QString harmonicClash;
        bool operator==(const Clock&) const = default;
    };
    Clock m_clock;

    //! The mix, worked out once per project change (rebuildSongs), not per playhead move.
    struct SongCache {
        trackedit::Clip clip;
        double songStart = 0.0;   ///< where the song file's first second sits on the timeline
        double ratio = 1.0;
        SongInfo info;
        int keyId = 0;            ///< the key it sounds in (transposition included)
        double lineBpm = 0.0;     ///< its tempo in the tempo line's units (double for a double-time match)
        double ownBpm = 0.0;      ///< its own tempo (beat grid)
        std::vector<double> beatTimes;
        std::vector<int> barNumber;   ///< per beat: the bar it is in (0 before bar 1)
        std::vector<int> beatInBar;
        int beatsPerBar = 0;
        std::vector<IMixHouseBeatGrids::Phrase> phrases;
        double mixAt = -1.0;      ///< the next transition (OUT anchor), -1 = last song
        int number = 0;           ///< its song number in the mix (pieces of one song share it)
        bool continuation = false;   ///< the rest of a song after a loop or a split
    };
    struct MixSummary {
        double total = 0.0;
        double minBpm = 0.0;
        double maxBpm = 0.0;
        int harmonicGood = 0;
        int harmonicTotal = 0;
        QString harmonicClash;
    };
    std::vector<SongCache> m_mixSongs;
    MixSummary m_mix;
    bool m_songsValid = false;
    SongInfo songFor(const QString& path);

    QTimer m_updateTimer;
    QTimer m_cpuTimer;
    QHash<QString, SongInfo> m_songs;   // by file path, so the library is asked once per song

    QString m_keyText;
    QColor m_keyColor;
    int m_energy = 0;
    int m_songIndex = 0;
    int m_songCount = 0;
    bool m_playing = false;

    int m_cpuPercent = 0;
    double m_lastCpuSecs = -1.0;
    double m_diskMBps = 0.0;
    double m_memoryGB = 0.0;
    double m_memoryShare = 0.0;
    double m_lastDiskBytes = -1.0;
    QElapsedTimer m_cpuClock;
};
}
