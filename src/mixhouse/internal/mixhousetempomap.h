/*
 * GlideMix
 */
#pragma once

#include <memory>

#include "async/asyncable.h"
#include "modularity/ioc.h"
#include "context/iglobalcontext.h"
#include "trackedit/iprojecthistory.h"
#include <string>

#include "mixhouse/imixhouselibraryconfiguration.h"
#include "mixhouse/imixhousebeatgrids.h"

#include <QTimer>

#include "mixhouse/imixhousetempomap.h"

class BoundedEnvelope;

namespace au::mixhouse {
class MixHouseTempoMap : public IMixHouseTempoMap, public muse::Contextable, public muse::async::Asyncable
{
    muse::ContextInject<context::IGlobalContext> globalContext { this };
    muse::GlobalInject<IMixHouseLibraryConfiguration> configuration;
    muse::ContextInject<IMixHouseBeatGrids> beatGrids { this };
    muse::ContextInject<trackedit::IProjectHistory> projectHistory { this };

public:
    explicit MixHouseTempoMap(const muse::modularity::ContextPtr& iocCtx);
    ~MixHouseTempoMap() override;

    void init();

    Points points() const override;
    void setPoints(const Points& points, bool completed) override;
    void reset() override;

    double bpmAt(double time) const override;
    bool isConstant() const override;
    double secToBeat(double time) const override;
    double beatToSec(double beat) const override;

    muse::async::Notification tempoMapChanged() const override;
    muse::async::Notification doubleTimeMatched() const override { return m_doubleTimeMatched; }

    bool followsTracks() const override { return m_followTracks; }
    void setFollowsTracks(bool follow) override;
    std::vector<Handover> handovers() const override;
    std::vector<SongTempo> songTempos() const override;
    void trackPlaced(double stretchRatio, bool octaveMatched = false) override;
    void nudgeMasterBpm(double deltaBpm, double atTime) override;

private:
    struct TablePoint {
        double sec = 0.0;
        double beats = 0.0;   // beats elapsed at `sec`
        double bpm = 0.0;
        Point shape;   // the segment from here to the next entry
    };

    void onProjectChanged();
    //! Saved with the project (glidemix_tempo): the line, Follow tracks, real
    //! tempos, Master BPM. "" for a plain flat line at the project tempo.
    std::string savedLine() const;
    //! What changing it means to the user: the derived points left out while
    //! following, so the follow timer settling after an open is no edit.
    std::string userState() const;
    std::string fullLine() const;
    void restoreLine();
    void applyUndoLine(const std::string& line);
    void applySavedLine(const std::string& saved);
    void markIfChanged();
    void* au3Project() const;
    //! Publishes the line as the live playback warp: speed = BPM / project tempo.
    void rebuildPlaybackWarp();
    const BoundedEnvelope* playbackWarp();
    void followProjectTempo(double tempo);
    double projectTempo() const;
    static Points sanitized(Points points);
    std::vector<TablePoint> table() const;

    Points m_points;
    // Stands for the live curve in streams (PlaybackSpeedCurve::SetLive); its
    // own points are unused, only its speed range bounds the resamplers.
    std::unique_ptr<BoundedEnvelope> m_warpToken;
    std::shared_ptr<void> m_warpHook;
    muse::async::Notification m_changed;
    muse::async::Notification m_doubleTimeMatched;
    //! Rebuilds the line from the tracks when they changed since the last build.
    void followTracksNow(bool force);

    bool m_followTracks = false;
    bool m_realTempos = false;   ///< after Reset: no 8% rule, every track at its own BPM
    double m_masterFactor = 1.0;
    QString m_trackSignature;
    QTimer m_persistTimer;
    std::string m_persisted;        ///< userState() last seen: a change marks the project unsaved
    void* m_restoredFor = nullptr;  ///< the project whose saved line was applied
    bool m_inDrag = false;          ///< a point is being dragged: no history entry until it is let go
    bool m_retryRestore = false;    ///< the file may not have been read yet: look again on its first history state
    QTimer m_followTimer;
};
}
