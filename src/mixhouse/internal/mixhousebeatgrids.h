/*
 * GlideMix
 */
#pragma once

#include <atomic>
#include <memory>

#include <mutex>

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include "au3-mixtape-engine/MixtapeArrangement.h"

#include "async/asyncable.h"
#include "modularity/ioc.h"
#include "context/iglobalcontext.h"
#include "trackedit/itrackeditinteraction.h"
#include "trackedit/itracksinteraction.h"
#include "trackedit/iselectioncontroller.h"
#include "trackedit/iprojecthistory.h"

#include "mixhouse/imixhousebeatgrids.h"
#include "mixhouse/imixhouseactivity.h"
#include "mixhouse/imixhouselibrary.h"
#include "mixhouse/imixhouselexicon.h"
#include "mixhouse/imixhouserekordboxxml.h"
#include "mixhouse/imixhouserekordboxphrases.h"
#include "mixhouse/imixhouselibraryconfiguration.h"
#include "mixhouse/imixhousetransitions.h"
#include "mixhouse/mixhousewaveform.h"

namespace au::mixhouse {
class MixHouseBeatGrids : public IMixHouseBeatGrids, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<context::IGlobalContext> globalContext { this };
    muse::ContextInject<trackedit::ITrackeditInteraction> trackeditInteraction { this };
    muse::ContextInject<trackedit::ITracksInteraction> tracksInteraction { this };
    muse::ContextInject<trackedit::ISelectionController> selectionController { this };
    muse::ContextInject<trackedit::IProjectHistory> projectHistory { this };
    muse::ContextInject<IMixHouseLibrary> library { this };
    muse::ContextInject<IMixHouseLexicon> lexicon { this };
    muse::ContextInject<IMixHouseRekordboxXml> rekordboxXml { this };
    muse::ContextInject<IMixHouseRekordboxPhrases> rekordboxPhrases { this };
    muse::ContextInject<IMixHouseActivity> activity { this };
    muse::GlobalInject<IMixHouseLibraryConfiguration> libraryConfiguration;
    muse::ContextInject<IMixHouseTransitions> transitions { this };

public:
    explicit MixHouseBeatGrids(const muse::modularity::ContextPtr& iocCtx);
    ~MixHouseBeatGrids() override;

    void init();

    std::vector<Tick> clipTicks(const trackedit::ClipKey& clip, double fromTime, double toTime) const override;

    std::vector<Cue> clipCues(const trackedit::ClipKey& clip, double fromTime, double toTime) const override;
    void seatBarOneAtProjectStart(const trackedit::ClipKey& clip) override;
    void beginClipMove(const trackedit::ClipKey& clip) override;
    void insertAtTransition(const trackedit::ClipKey& clip, bool reordered) override;
    void cancelClipMove() override;
    void tidyRowsAfterMove(const trackedit::ClipKey& clip) override;
    bool clipMoveFromSampleLine() const override;
    trackedit::TrackId clearRow(trackedit::TrackId preferred, double start, double end, int channels,
                                const trackedit::ClipKeyList& ignore = {},
                                const trackedit::TrackIdList& taken = {}, bool sample = false) override;
    std::vector<Phrase> clipPhrases(const trackedit::ClipKey& clip, double fromTime, double toTime) const override;
    std::vector<double> energyCurve(double t0, double step, size_t count) const override;
    muse::async::Notification beatGridsChanged() const override;
    int clipLibraryTrackId(const trackedit::ClipKey& clip) const override;
    std::shared_ptr<const mixtape::ClipWarp> clipWarp(const trackedit::ClipKey& clip) const override;
    QString beatMapNote(int trackId, double bpm, double firstBeatSec, double downbeatSec) const override;
    std::vector<WaveColumn> clipWaveColumns(const trackedit::ClipKey& clip, double fromTime, double toTime,
                                            int columns) const override;
    double clipWaveColumnRate(const trackedit::ClipKey& clip) const override;
    double clipBpm(const trackedit::ClipKey& clip) const override;
    double clipOwnBpm(const trackedit::ClipKey& clip) const override;
    bool clipFile(const trackedit::ClipKey& clip, ClipFile& out) const override;
    bool clipVolume(const trackedit::ClipKey& clip, double t0, double step, size_t count,
                    std::vector<double>& out) const override;
    bool clipMonoSamples(const trackedit::ClipKey& clip, double fromSec, double toSec, std::vector<float>& out,
                         double& sampleRate, double& startSec, int channel = -1) const override;
    int selectedLibraryTrackId() const override;
    double lockedClipStart(const trackedit::ClipKey& clip, double proposedStart, double pxPerSec = 0.0) const override;
    Anchors clipAnchors(const trackedit::ClipKey& clip) const override;
    double syncGridSec() const override;
    double magneticClipStart(const trackedit::ClipKey& clip, double proposedStart, double thresholdSec) const override;
    void moveClipAnchor(const trackedit::ClipKey& clip, int which, double projectTime, bool completed) override;
    std::optional<std::pair<double, double> > takeClipAnchorMove(const trackedit::ClipKey& clip) override;
    void putClipAnchorMove(const trackedit::ClipKey& clip, const std::pair<double, double>& anchors) override;
    void resetClipAnchors(const trackedit::ClipKey& clip) override;
    void useClipAnchorsForSong(const trackedit::ClipKey& clip) override;
    void adjustSelectedClips(Adjust adjust) override;
    int splitSelectedAtGridJumps() override;
    bool addLoopAt(const trackedit::ClipKey& clip, double projectTime, int bars) override;
    std::pair<double, double> clipEffectRange(const trackedit::ClipKey& clip, double projectTime,
                                              int defaultBars) const override;
    bool setSelectedClipAnchorAt(int which, double projectTime) override;

private:
    struct Grid {
        int trackId = -1;
        QString title;
        double bpm = 0.0;
        double firstBeatSec = 0.0;
        double downbeatSec = -1.0;
        double downbeatConfidence = 0.0;
        QString filePath;
        std::vector<MixHouseCue> cues;   ///< external library tracks: their cue points (source seconds)
        bool valid() const { return trackId != -1 && bpm > 0.0; }   // Lexicon tracks have ids < -1
    };

    //! The library grid of the track a clip was imported from, found by the
    //! clip's name (Audacity names an imported clip after its file).
    //! The warp for a song with this grid (null: play straight); `note`
    //! gets a plain sentence when its beat map was repaired or skipped.
    //! `name` empty: nothing logged (asked for the note only).
    std::shared_ptr<const mixtape::ClipWarp> warpForGrid(const Grid& grid, const QString& name, QString* note,
                                                         bool repair = true) const;
    Grid gridForClipName(const QString& clipName, bool fresh) const;
    //! A song's energy rating and its loudness over time (from the library
    //! analysis' waveform overview), for the ENERGY lane.
    struct SongEnergy {
        double energy = 0.0;        ///< 1-10, 0 = not rated
        double durationSec = 0.0;
        double refDb = 0.0;         ///< the song's loud parts (90th percentile)
        std::vector<float> power;   ///< overview columns, linear power
        //! Mean level in dB of file seconds [a, b); refDb when unknown.
        double levelDb(double a, double b) const;
    };
    const SongEnergy& songEnergy(const QString& clipName) const;
    mutable QHash<QString, SongEnergy> m_songEnergy;
    //! Songs on the timeline without an energy rating or a loudness curve
    //! (rekordbox imports, Lexicon songs, songs GlideMix never analysed) get
    //! them in the background.
    void requestEnergy(int libraryId, const QString& filePath, double bpm, double firstBeatSec, double downbeatSec,
                       bool storeEnergy) const;
    void startEnergyJob() const;
    struct EnergyJob {
        int libraryId = -1;
        QString filePath;
        double bpm = 0.0;
        double firstBeatSec = 0.0;
        double downbeatSec = -1.0;  ///< the song's own grid: its phrases sit on its bars
        bool storeEnergy = false;   ///< the song had no rating: save the new one
    };
    mutable QList<EnergyJob> m_energyQueue;
    mutable QSet<QString> m_energyRequested;       ///< file paths queued, running or done
    struct ComputedEnergy {
        double energy = 0.0;
        double durationSec = 0.0;
        std::vector<float> power;
        //! The same analysis' phrases, for a song neither rekordbox nor
        //! GlideMix's collection has phrases for (a Lexicon song).
        std::vector<MixHousePhrase> phrases;
    };
    mutable QHash<QString, ComputedEnergy> m_computedEnergy;   ///< by file path
    mutable bool m_energyJobRunning = false;
    mutable int m_energyJobsDone = 0;
    QString clipName(const trackedit::ClipKey& clip) const;
    void reseatClipsOf(const Grid& grid);
    double barLockedClipStart(const trackedit::ClipKey& clip, double proposedStart) const;
    //! The phrases shown for a grid's file: edited ones, else rekordbox's,
    //! else GlideMix's own (a Lexicon song's worked out in the background).
    std::vector<MixHousePhrase> phrasesFor(const Grid& grid, bool* audioTime = nullptr) const;
    //! A clip's anchors in seconds into its file, and the clip's mapping.
    struct SourceAnchors {
        bool valid = false;
        double intro = 0.0;
        double outro = 0.0;
        bool moved = false;
        double sequenceStart = 0.0;   // project seconds of file second 0
        double ratio = 1.0;           // project seconds per file second
    };
    //! The file's loudness, one average per bar from `bar1` (the waveform
    //! overview's peaks); empty without a waveform.
    std::vector<double> barLevels(const Grid& grid, double bar1, double bar, double fileSeconds) const;
    SourceAnchors sourceAnchors(const trackedit::ClipKey& clip) const;
    //! Anchor source seconds being dragged, by file (not yet saved).
    mutable QHash<QString, std::pair<double, double> > m_anchorDrag;

    //! MixMeister's linked anchors: while one side of a transition (an OUT
    //! anchor meeting the next track's IN) is dragged, the next track and
    //! every track after it move with it, so the mix stays joined.
    struct RippleDrag {
        bool active = false;
        trackedit::ClipKey clip;
        int which = -1;
        double originalAnchor = 0.0;   ///< the dragged anchor, project seconds
        double minShift = 0.0;         ///< so no follower runs into a clip before it
        double applied = 0.0;
        std::vector<std::pair<trackedit::ClipKey, double> > followers;   ///< and their start times
        QString filePath;                        ///< the dragged clip's song
        trackedit::ClipKey outgoing, incoming;   ///< the transition the anchor belongs to
    };
    RippleDrag m_ripple;
    //! Where a dragged song's anchors were when the drag began (project seconds).
    std::optional<Anchors> m_moveBefore;
    bool m_moveCaptured = false;
    trackedit::TrackId m_moveFromTrack = -1;   // the row the dragged song started on
    QString m_moveFromTitle;
    void startRipple(const trackedit::ClipKey& clip, int which);
    //! True when it pushed a history step (the songs after it moved).
    bool applyRipple(double anchorNow, bool completed);

    //! Anchors are kept per song in the library, outside the project's undo
    //! history; a transition move records the song's anchors before and after
    //! it against its history step, so undo and redo put them back too.
    //! Since 1.3.6 the anchors a project changes are its own (none = the song's).
    struct AnchorRecord {
        size_t stateIndex = 0;
        QString filePath;
        std::optional<std::pair<double, double> > before;
        std::optional<std::pair<double, double> > after;
    };
    std::vector<AnchorRecord> m_anchorHistory;
    bool m_subscribedToHistory = false;
    void subscribeToHistory();
    //! Sets (or, with none, drops) this project's anchors for a file.
    void setProjectAnchors(const QString& filePath, const std::optional<std::pair<double, double> >& anchors);
    //! This project's anchors, by file (saved with it, mixhouseanchorpersist).
    //! A song read from the library is copied in on first use, so "Use these
    //! anchors in new mixes" from another mix never moves it here.
    mutable QHash<QString, std::pair<double, double> > m_projectAnchors;
    void* m_projectAnchorsFor = nullptr;   ///< the au3 project they were loaded for
    void loadProjectAnchors(void* au3Project);
    std::string savedAnchorState() const;
    //! One undo step for a change of a file's project anchors (no step was pushed for it).
    void pushAnchorStep(const QString& filePath, const std::optional<std::pair<double, double> >& before,
                        const std::optional<std::pair<double, double> >& after, bool pushed);
    //! Saved anchors by file, read once (cleared on library changes).
    mutable QHash<QString, std::optional<std::pair<double, double> > > m_anchorCache;
    //! Library tracks rekordbox analysed take its grid (background read, one write).
    void applyRekordboxGrids();

    //! The colour waveform of each file, decoded once: rekordbox's own where
    //! there is one, else this app's analysis. An invalid entry means "looked,
    //! found nothing" and stops the lookup repeating every repaint.
    mutable QHash<QString, MixHouseWaveformOverview> m_waveCache;

    mutable QHash<QString, Grid> m_gridByClipName;
    //! GlideMix: the warp of each library song onto its grid (Logic's "apply
    //! project tempo to region"), built once (null = played straight);
    //! cleared when the library or the grids change.
    std::shared_ptr<const mixtape::ClipWarp> warpForClipName(const QString& name) const;
    mutable QHash<QString, std::shared_ptr<const mixtape::ClipWarp> > m_warpByClipName;
    mutable QSet<QString> m_beatMapNoteShown;   ///< songs whose beat-map note was in the status line
    //! "Keep old mixes as-is" (Chris, 2026-11-15): the songs of a project saved
    //! before 1.3.5, as they were when it was opened, keep the 1.3.4 warp (their
    //! map followed as stored). Saved with the project (mixhousewarppersist).
    void decideWarpEra();
    std::string savedWarpState() const;
    bool keepsOldWarp(const QString& clipName) const;
    mutable std::mutex m_oldWarpMutex;
    QSet<QString> m_oldWarpNames;
    void* m_warpEraFor = nullptr;   ///< the au3 project the decision is for (null: not decided yet)
    std::vector<Grid> m_undo;
    muse::async::Notification m_beatGridsChanged;
    bool m_phraseRibbonVisible = true;
    bool m_rekordboxPassRunning = false;
    std::shared_ptr<std::atomic<bool> > m_alive;
};
}
