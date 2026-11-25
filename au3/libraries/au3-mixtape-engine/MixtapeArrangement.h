/*
 * GlideMix -- playback engine
 *
 * An immutable copy of what the engine plays. The main thread builds a new
 * Arrangement after every edit and hands it over; the render thread picks it
 * up at the next block. Nothing here points at mutable project data: sample
 * blocks are immutable and shared, envelopes are copied. That is what lets
 * edits happen while the music keeps playing.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class Envelope;
class SampleBlock;
class TrackList;
class WaveClip;
class WaveTrack;

namespace mixtape {
//! One stored block of a clip channel, `start` samples into the sequence.
struct SourceBlock {
    int64_t start = 0;
    std::shared_ptr<SampleBlock> block;
};

struct SourceChannel {
    std::vector<SourceBlock> blocks;
    int64_t numSamples = 0;
};

//! Keeps the decoded samples of the blocks last read alive. A sample block
//! caches its decoded floats only while someone holds them (a weak_ptr), so
//! reading without this decoded the whole block from the project database
//! again for every 256 frames.
struct SourceCursor {
    size_t index = static_cast<size_t>(-1);
    std::shared_ptr<std::vector<float> > view;
    size_t nextIndex = static_cast<size_t>(-1);
    std::shared_ptr<std::vector<float> > nextView;
};

//! Zero-fills outside the channel. Render-thread safe: reads immutable blocks.
//! `cursor` (one per voice and channel) makes consecutive reads cheap.
void ReadSource(const SourceChannel& channel, int64_t position, float* out, size_t count,
                SourceCursor* cursor = nullptr);

//! GlideMix: a song whose tempo moves, bent onto the project grid (Logic's
//! "apply project tempo to region"). The clip keeps its constant stretch and
//! place; on top, beat n of the song's constant grid plays the song's REAL beat
//! n (its beat map), so every beat lands where the grid -- and with it the
//! project grid -- has it. All times are source (file) seconds.
struct ClipWarp {
    double gridFirstBeatSec = 0.0;   //!< the constant grid: beat 0
    double gridBeatSec = 0.5;        //!< and its beat length
    double shiftBeats = 0.0;         //!< grid beat n is map beat n - shiftBeats
    std::vector<double> mapSecs;     //!< every real beat, increasing

    //! Where the music really is when the constant grid says `sec`.
    double Warp(double sec) const;
    //! The other way: the grid time at which the music's moment `sourceSec`
    //! plays (Warp(Unwarp(s)) == s). For cue points and other marks taken from
    //! the audio itself.
    double Unwarp(double sourceSec) const;

    //! A beat map made safe to follow, before it becomes `mapSecs`
    //! (`beats` counted at the grid's level, `beatSec` = the grid's beat).
    //! A stretch where the tracker went double (or half) time -- beats far
    //! closer (or farther) than the grid's beat, as in a breakdown -- would
    //! make the warp play that part at half (or double) speed. Each such
    //! stretch, widened over its ramp, is replaced by as many evenly spaced
    //! beats as fit its length at the grid's tempo. Returns the stretches
    //! repaired (0: `beats` untouched); each is described in `stretches`
    //! when given (for telling the user).
    struct RepairedStretch {
        double fromSec = 0.0;   //!< the stretch's first and last beat (unchanged by the repair)
        double toSec = 0.0;
        int beatsBefore = 0;    //!< beat gaps in it as stored ...
        int beatsAfter = 0;     //!< ... and after evening it out
    };
    static int RepairMap(std::vector<double>& beats, double beatSec, std::vector<RepairedStretch>* stretches = nullptr);
    //! The slowest and fastest beat of a map against the grid's beat
    //! (gap / beatSec; 1 = the grid's tempo). The warp plays the song at
    //! 1 / that rate around each beat.
    static void RateRange(const std::vector<double>& beats, double beatSec, double& slowest, double& fastest);
    //! How far one beat may run from the grid's beat and still be followed:
    //! beyond this the map is not the music (no song changes tempo by a
    //! third within one beat) and the song plays straight instead.
    static constexpr double kMaxRateOff = 1.33;
};

//! Asked for every clip when the arrangement is built (main thread): the
//! clip's warp, or null to play it straight. GlideMix sets one; tests don't.
using ClipWarpProvider = std::function<std::shared_ptr<const ClipWarp>(const WaveClip&)>;
void SetClipWarpProvider(ClipWarpProvider provider);

//! GlideMix Auto-Keylock (1.3.8): asked for each clip when a snapshot is taken
//! (main thread); true = this clip keeps its pitch even while Keylock is off.
using ClipKeylockProvider = std::function<bool (const WaveClip&)>;
void SetClipKeylockProvider(ClipKeylockProvider provider);

struct ClipSnapshot {
    int64_t clipId = 0;
    double sampleRate = 44100.0;
    std::vector<SourceChannel> channels;
    //! Timeline second where source sample 0 plays.
    double sequenceStart = 0.0;
    //! The audible part on the timeline.
    double playStart = 0.0;
    double playEnd = 0.0;
    //! Timeline seconds per source second.
    double stretchRatio = 1.0;
    int centShift = 0;
    //! The clip's volume line (absolute timeline times); may be null.
    std::shared_ptr<const Envelope> envelope;
    //! The clip's bass line (absolute timeline times, 0..1: 1 leaves the bass
    //! alone, 0 removes it). Null for clips without one, which then skip the
    //! bass stage entirely and play bit for bit as before.
    std::shared_ptr<const Envelope> lowBand;
    std::shared_ptr<const Envelope> midBand;    // GlideMix: the transition EQ's mids and highs
    std::shared_ptr<const Envelope> highBand;
    //! GlideMix 1.3.10: each band's boost (0..1 = 0..+10 dB), on top of the band above.
    std::shared_ptr<const Envelope> lowBoost;
    std::shared_ptr<const Envelope> midBoost;
    std::shared_ptr<const Envelope> highBoost;
    //! GlideMix: the song's warp onto the grid (a song whose tempo moves); null
    //! = played at the constant stretch, bit for bit as without warps.
    std::shared_ptr<const ClipWarp> warp;
    //! GlideMix Auto-Keylock: keeps its pitch while Keylock is off (decided
    //! once per song). False = follows Keylock, as before.
    bool keylock = false;

    //! Timeline time of a source sample (the constant stretch; a warp moves
    //! the music by at most a fraction of a beat from this).
    double TimeOfSource(double sample) const { return sequenceStart + sample / sampleRate * stretchRatio; }
    //! The source sample playing at timeline second `time`.
    double SourceAt(double time) const
    {
        const double sec = (time - sequenceStart) / stretchRatio;
        return (warp ? warp->Warp(sec) : sec) * sampleRate;
    }

    //! Same audio at the same place: a voice playing the old one can go on.
    bool SameAudio(const ClipSnapshot& other) const;
};

struct TrackSnapshot {
    int64_t trackId = 0;
    float volume = 1.0f;
    float pan = 0.0f;
    bool mute = false;
    bool solo = false;
    std::vector<ClipSnapshot> clips;
    //! Keeps the track (its effect list) alive while the engine may still
    //! process it, even if it is deleted meanwhile. Null in tests.
    std::shared_ptr<const WaveTrack> track;
    //! Changes when clips move or automation curves change: automation needs rebuilding.
    uint64_t automationSignature = 0;
    //! Frames the track's effects delay it by (their reported latency).
    size_t latencyFrames = 0;

    float ChannelGain(size_t channel) const;
};

struct Arrangement {
    std::vector<TrackSnapshot> tracks;
    uint64_t generation = 0;
    bool anySolo = false;
    //! GlideMix: the master effects' latency (a look-ahead limiter ...), in
    //! output frames; the whole mix comes out that much later.
    size_t masterLatencyFrames = 0;
};

ClipSnapshot SnapshotClip(const WaveClip& clip);
TrackSnapshot SnapshotTrack(const WaveTrack& track);
std::shared_ptr<Arrangement> SnapshotArrangement(const TrackList& tracks, uint64_t generation);
}
