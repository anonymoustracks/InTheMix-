/*
 * GlideMix -- playback engine
 */
#include "MixtapeArrangement.h"

#include <mutex>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "au3-mixer/Envelope.h"
#include "au3-track/Track.h"
#include "au3-wave-track/Sequence.h"
#include "au3-wave-track/SampleBlock.h"
#include "au3-wave-track/WaveClip.h"
#include "au3-wave-track/WaveTrack.h"

namespace mixtape {
void ReadSource(const SourceChannel& channel, int64_t position, float* out, size_t count, SourceCursor* cursor)
{
    size_t done = 0;
    if (position < 0) {
        const size_t silence = static_cast<size_t>(std::min<int64_t>(count, -position));
        std::fill_n(out, silence, 0.0f);
        done = silence;
        position += static_cast<int64_t>(silence);
    }
    const auto& blocks = channel.blocks;
    // The block holding `position`: the last one starting at or before it.
    auto it = std::upper_bound(blocks.begin(), blocks.end(), position,
                               [](int64_t pos, const SourceBlock& b) { return pos < b.start; });
    size_t index = static_cast<size_t>(it - blocks.begin());
    if (index == 0) {
        std::fill(out + done, out + count, 0.0f);
        return;
    }
    --index;
    while (done < count && position < channel.numSamples && index < blocks.size()) {
        const SourceBlock& block = blocks[index];
        std::shared_ptr<std::vector<float> > view;
        if (cursor && cursor->index == index && cursor->view) {
            view = cursor->view;
        } else if (cursor && cursor->nextIndex == index && cursor->nextView) {
            view = cursor->nextView;
        } else {
            view = block.block ? block.block->GetFloatSampleView(false) : nullptr;
        }
        if (cursor && view) {
            // The block being played and the one just before it (a read
            // straddling the boundary needs both).
            if (cursor->index != index) {
                cursor->nextIndex = cursor->index;
                cursor->nextView = std::move(cursor->view);
                cursor->index = index;
                cursor->view = view;
            }
        }
        const int64_t offset = position - block.start;
        const int64_t blockSize = view ? static_cast<int64_t>(view->size()) : 0;
        if (offset < 0 || offset >= blockSize) {
            ++index;
            continue;
        }
        const size_t n = static_cast<size_t>(std::min<int64_t>(count - done, blockSize - offset));
        std::memcpy(out + done, view->data() + offset, n * sizeof(float));
        done += n;
        position += static_cast<int64_t>(n);
        ++index;
    }
    std::fill(out + done, out + count, 0.0f);
}

bool ClipSnapshot::SameAudio(const ClipSnapshot& other) const
{
    if (clipId != other.clipId || sampleRate != other.sampleRate || sequenceStart != other.sequenceStart
        || stretchRatio != other.stretchRatio || centShift != other.centShift || warp != other.warp
        || keylock != other.keylock
        || channels.size() != other.channels.size()) {
        return false;
    }
    for (size_t c = 0; c < channels.size(); ++c) {
        const auto& a = channels[c];
        const auto& b = other.channels[c];
        if (a.numSamples != b.numSamples || a.blocks.size() != b.blocks.size()) {
            return false;
        }
        for (size_t i = 0; i < a.blocks.size(); ++i) {
            if (a.blocks[i].block != b.blocks[i].block || a.blocks[i].start != b.blocks[i].start) {
                return false;
            }
        }
    }
    return true;
}

float TrackSnapshot::ChannelGain(size_t channel) const
{
    // WaveTrack::GetChannelVolume
    float left = 1.0f;
    float right = 1.0f;
    if (pan < 0) {
        right = pan + 1.0f;
    } else if (pan > 0) {
        left = 1.0f - pan;
    }
    return volume * ((channel % 2) == 0 ? left : right);
}

namespace {
std::mutex& warpProviderMutex()
{
    static std::mutex m;
    return m;
}
ClipWarpProvider& warpProvider()
{
    static ClipWarpProvider p;
    return p;
}
}

void SetClipWarpProvider(ClipWarpProvider provider)
{
    std::lock_guard<std::mutex> lock(warpProviderMutex());
    warpProvider() = std::move(provider);
}

namespace {
ClipKeylockProvider& keylockProvider()
{
    static ClipKeylockProvider p;
    return p;
}
}

void SetClipKeylockProvider(ClipKeylockProvider provider)
{
    std::lock_guard<std::mutex> lock(warpProviderMutex());
    keylockProvider() = std::move(provider);
}

double ClipWarp::Warp(double sec) const
{
    const size_t n = mapSecs.size();
    if (n < 2 || gridBeatSec <= 0.0) {
        return sec;
    }
    // The grid's beat number at `sec`, then the map's beat with that number.
    const double k = (sec - gridFirstBeatSec) / gridBeatSec - shiftBeats;
    const auto gridSec = [&](double mapBeat) { return gridFirstBeatSec + (mapBeat + shiftBeats) * gridBeatSec; };
    if (k <= 0.0) {
        return sec + (mapSecs.front() - gridSec(0.0));   // before the first beat: keep its offset
    }
    if (k >= double(n - 1)) {
        return sec + (mapSecs.back() - gridSec(double(n - 1)));   // after the last one: likewise
    }
    const size_t i = static_cast<size_t>(k);
    return mapSecs[i] + (k - double(i)) * (mapSecs[i + 1] - mapSecs[i]);
}

double ClipWarp::Unwarp(double sourceSec) const
{
    const size_t n = mapSecs.size();
    if (n < 2 || gridBeatSec <= 0.0) {
        return sourceSec;
    }
    const auto gridSec = [&](double mapBeat) { return gridFirstBeatSec + (mapBeat + shiftBeats) * gridBeatSec; };
    if (sourceSec <= mapSecs.front()) {
        return sourceSec - (mapSecs.front() - gridSec(0.0));
    }
    if (sourceSec >= mapSecs.back()) {
        return sourceSec - (mapSecs.back() - gridSec(double(n - 1)));
    }
    const size_t i = size_t(std::upper_bound(mapSecs.begin(), mapSecs.end(), sourceSec) - mapSecs.begin()) - 1;
    const double k = double(i) + (sourceSec - mapSecs[i]) / (mapSecs[i + 1] - mapSecs[i]);
    return gridSec(k);
}

void ClipWarp::RateRange(const std::vector<double>& beats, double beatSec, double& slowest, double& fastest)
{
    slowest = fastest = 1.0;
    if (beatSec <= 0.0) {
        return;
    }
    for (size_t i = 1; i < beats.size(); ++i) {
        const double rate = (beats[i] - beats[i - 1]) / beatSec;
        slowest = std::max(slowest, rate);   // a long gap: the song's beat is slower than the grid's
        fastest = std::min(fastest, rate);
    }
}

int ClipWarp::RepairMap(std::vector<double>& beats, double beatSec, std::vector<RepairedStretch>* stretches)
{
    if (beats.size() < 3 || beatSec <= 0.0) {
        return 0;
    }
    // A gap is broken when it is a fifth shorter or a quarter longer than the
    // grid's beat: a drifting or accelerating song moves a few percent per
    // beat at most. The broken stretch is then widened while its neighbours
    // still run more than 10 % off -- the analysis smooths over about two
    // bars, so a doubled section comes with a ramp into and out of it.
    const auto rate = [&](size_t gap) { return (beats[gap + 1] - beats[gap]) / beatSec; };
    const auto broken = [&](size_t gap) { const double r = rate(gap); return r < 0.8 || r > 1.25; };
    const auto off = [&](size_t gap) { const double r = rate(gap); return r < 0.9 || r > 1.11; };
    const size_t gaps = beats.size() - 1;
    std::vector<std::pair<size_t, size_t> > runs;   // beat indices [first, last] of each stretch
    for (size_t g = 0; g < gaps; ++g) {
        if (!broken(g)) {
            continue;
        }
        size_t lo = g, hi = g;
        for (int ramp = 0; hi + 1 < gaps;) {
            if (broken(hi + 1)) {
                ++hi;
                ramp = 0;
            } else if (off(hi + 1) && ramp < 8) {
                ++hi;
                ++ramp;
            } else {
                break;
            }
        }
        for (int k = 0; k < 8 && lo > 0 && off(lo - 1); ++k) {
            --lo;
        }
        if (!runs.empty() && lo <= runs.back().second + 1) {
            runs.back().second = hi + 1;   // touching the one before: one stretch
        } else {
            runs.push_back({ lo, hi + 1 });
        }
        g = hi;
    }
    if (runs.empty()) {
        return 0;
    }
    std::vector<double> out;
    out.reserve(beats.size());
    size_t next = 0;
    for (const auto& [first, last] : runs) {
        out.insert(out.end(), beats.begin() + long(next), beats.begin() + long(first));
        const double from = beats[first], to = beats[last];
        const long n = std::max(1L, std::lround((to - from) / beatSec));
        if (stretches) {
            stretches->push_back({ from, to, int(last - first), int(n) });
        }
        for (long q = 0; q < n; ++q) {
            out.push_back(from + (to - from) * double(q) / double(n));
        }
        next = last;   // the stretch's last beat starts what follows
    }
    out.insert(out.end(), beats.begin() + long(next), beats.end());
    beats = std::move(out);
    return int(runs.size());
}

ClipSnapshot SnapshotClip(const WaveClip& clip)
{
    ClipSnapshot snap;
    snap.clipId = clip.GetId();
    snap.sampleRate = clip.GetRate();
    snap.sequenceStart = clip.GetSequenceStartTime();
    snap.playStart = clip.GetPlayStartTime();
    snap.playEnd = clip.GetPlayEndTime();
    snap.stretchRatio = clip.GetStretchRatio();
    snap.centShift = clip.GetCentShift();
    snap.envelope = std::make_shared<const Envelope>(clip.GetEnvelope());
    {
        std::lock_guard<std::mutex> lock(warpProviderMutex());
        if (warpProvider()) {
            snap.warp = warpProvider()(clip);
        }
        if (keylockProvider()) {
            snap.keylock = keylockProvider()(clip);
        }
    }
    for (const auto& curve : clip.GetParameterAutomation().Curves()) {
        if (!curve.enabled || curve.effectInstanceId != clipautomation::kMixtapeBuiltinInstanceId
            || curve.envelope.GetNumberOfPoints() == 0) {
            continue;
        }
        if (curve.parameterId == clipautomation::kMixtapeLowBandParameter) {
            snap.lowBand = std::make_shared<const Envelope>(curve.envelope);
        } else if (curve.parameterId == clipautomation::kMixtapeMidBandParameter) {
            snap.midBand = std::make_shared<const Envelope>(curve.envelope);
        } else if (curve.parameterId == clipautomation::kMixtapeHighBandParameter) {
            snap.highBand = std::make_shared<const Envelope>(curve.envelope);
        } else if (curve.parameterId == clipautomation::kMixtapeLowBoostParameter) {
            snap.lowBoost = std::make_shared<const Envelope>(curve.envelope);
        } else if (curve.parameterId == clipautomation::kMixtapeMidBoostParameter) {
            snap.midBoost = std::make_shared<const Envelope>(curve.envelope);
        } else if (curve.parameterId == clipautomation::kMixtapeHighBoostParameter) {
            snap.highBoost = std::make_shared<const Envelope>(curve.envelope);
        }
    }
    for (size_t ii = 0; ii < clip.NChannels(); ++ii) {
        SourceChannel channel;
        if (const Sequence* sequence = clip.GetSequence(ii)) {
            channel.numSamples = sequence->GetNumSamples().as_long_long();
            for (const SeqBlock& block : sequence->GetBlockArray()) {
                channel.blocks.push_back({ block.start.as_long_long(), block.sb });
            }
        }
        snap.channels.push_back(std::move(channel));
    }
    return snap;
}

TrackSnapshot SnapshotTrack(const WaveTrack& track)
{
    TrackSnapshot snap;
    snap.trackId = track.GetId().raw();
    snap.volume = track.GetVolume();
    snap.pan = track.GetPan();
    snap.mute = track.GetMute();
    snap.solo = track.GetSolo();
    snap.track = track.SharedPointer<const WaveTrack>();
    uint64_t signature = 1469598103934665603ull;
    const auto mix = [&signature](uint64_t value) {
        signature = (signature ^ value) * 1099511628211ull;
    };
    for (const auto& clip : track.Intervals()) {
        // GlideMix: a disabled clip is not played (nor exported, which renders through here).
        if (clip->GetPlayEndTime() > clip->GetPlayStartTime() && !clip->GetParameterAutomation().IsClipDisabled()) {
            snap.clips.push_back(SnapshotClip(*clip));
        }
        mix(static_cast<uint64_t>(clip->GetId()));
        mix(static_cast<uint64_t>(std::llround(clip->GetPlayStartTime() * 1e6)));
        mix(static_cast<uint64_t>(std::llround(clip->GetPlayEndTime() * 1e6)));
        mix(static_cast<uint64_t>(std::llround(clip->GetStretchRatio() * 1e9)));
        if (!snap.clips.empty() && snap.clips.back().clipId == clip->GetId()) {
            mix(reinterpret_cast<uintptr_t>(snap.clips.back().warp.get()));
        }
        for (const auto& curve : clip->GetParameterAutomation().Curves()) {
            mix(curve.envelope.GetNumberOfPoints());
            mix(curve.bezier.segments.size());
            mix(curve.enabled ? 1 : 2);
        }
    }
    snap.automationSignature = signature;
    return snap;
}

std::shared_ptr<Arrangement> SnapshotArrangement(const TrackList& tracks, uint64_t generation)
{
    auto arrangement = std::make_shared<Arrangement>();
    arrangement->generation = generation;
    for (const WaveTrack* track : tracks.Any<const WaveTrack>()) {
        arrangement->tracks.push_back(SnapshotTrack(*track));
        arrangement->anySolo |= arrangement->tracks.back().solo;
    }
    return arrangement;
}
}
