#include "mixhousenowplayingmodel.h"
#include "mixhouse/mixhousesonglanes.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/resource.h>
#endif
#ifdef Q_OS_MAC
#include <libproc.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif
#ifdef Q_OS_WIN
#include <psapi.h>
#endif

using namespace au::mixhouse;

namespace {
// Bytes this process has read from disk so far; -1 where unknown.
double processDiskReadBytes()
{
#if defined(Q_OS_WIN)
    IO_COUNTERS io {};
    if (!GetProcessIoCounters(GetCurrentProcess(), &io)) {
        return -1.0;
    }
    return double(io.ReadTransferCount);
#elif defined(Q_OS_MAC)
    rusage_info_v2 info {};
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t*>(&info)) != 0) {
        return -1.0;
    }
    return double(info.ri_diskio_bytesread);
#else
    return -1.0;
#endif
}

// Memory this process uses (Activity Monitor's "Memory" / Task Manager's
// private bytes), and the computer's whole memory; -1 where unknown.
double processMemoryBytes()
{
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX pmc {};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return -1.0;
    }
    return double(pmc.PrivateUsage);
#elif defined(Q_OS_MAC)
    task_vm_info_data_t info {};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
        return -1.0;
    }
    return double(info.phys_footprint);
#else
    return -1.0;
#endif
}

double totalMemoryBytes()
{
#if defined(Q_OS_WIN)
    MEMORYSTATUSEX status {};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? double(status.ullTotalPhys) : -1.0;
#elif defined(Q_OS_MAC)
    uint64_t bytes = 0;
    size_t size = sizeof(bytes);
    return sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0 ? double(bytes) : -1.0;
#else
    return -1.0;
#endif
}

// Processor time this process has used so far, in seconds.
double processCpuSeconds()
{
#ifdef Q_OS_WIN
    FILETIME created, exited, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        return -1.0;
    }
    auto secs = [](const FILETIME& f) {
        ULARGE_INTEGER v;
        v.LowPart = f.dwLowDateTime;
        v.HighPart = f.dwHighDateTime;
        return double(v.QuadPart) / 1e7;   // 100 ns units
    };
    return secs(kernel) + secs(user);
#else
    rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1.0;
    }
    auto secs = [](const timeval& t) { return double(t.tv_sec) + double(t.tv_usec) / 1e6; };
    return secs(usage.ru_utime) + secs(usage.ru_stime);
#endif
}
}

MixHouseNowPlayingModel::MixHouseNowPlayingModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(50);   // the DJ clock's beat dots follow the beat
    QObject::connect(&m_updateTimer, &QTimer::timeout, this, [this]() { update(); });
    m_cpuTimer.setInterval(1000);
    QObject::connect(&m_cpuTimer, &QTimer::timeout, this, [this]() { sampleCpu(); });
}

void MixHouseNowPlayingModel::componentComplete()
{
    if (globalContext()) {
        if (const auto state = globalContext()->playbackState()) {
            m_playing = state->isPlaying();
            state->playbackStatusChanged().onReceive(this, [this](playback::PlaybackStatus) {
                updatePlaying();
            });
            state->playbackPositionChanged().onReceive(this, [this](muse::secs_t) {
                if (!m_updateTimer.isActive()) {
                    m_updateTimer.start();
                }
            });
        }
        globalContext()->currentTrackeditProjectChanged().onNotify(this, [this]() {
            m_songsValid = false;
            m_updateTimer.start();
        });
    }
    if (performance()) {
        performance()->performancePositionChanged().onReceive(this, [this](muse::secs_t) {
            if (!m_updateTimer.isActive()) {
                m_updateTimer.start();
            }
        });
        performance()->performanceStateChanged().onNotify(this, [this]() {
            updatePlaying();
            m_updateTimer.start();
        });
    }
    if (projectHistory()) {
        projectHistory()->historyChanged().onReceive(this, [this](trackedit::HistoryEvent) {
            m_songsValid = false;
            m_updateTimer.start();
        });
    }
    if (beatGrids()) {
        beatGrids()->beatGridsChanged().onNotify(this, [this]() {
            m_songsValid = false;
            m_updateTimer.start();
        });
    }
    // Keylock switched, or (Keylock off) a speed moved a song's key: the
    // harmonic flow reads the sounding keys again.
    if (keyMatch()) {
        keyMatch()->keysChanged().onNotify(this, [this]() {
            m_songsValid = false;
            m_updateTimer.start();
        });
    }
    if (selectionController()) {
        selectionController()->dataSelectedStartTimeChanged().onReceive(this, [this](trackedit::secs_t) {
            m_updateTimer.start();
        });
        selectionController()->dataSelectedEndTimeChanged().onReceive(this, [this](trackedit::secs_t) {
            m_updateTimer.start();
        });
    }
    m_cpuClock.start();
    m_lastCpuSecs = processCpuSeconds();
    m_cpuTimer.start();
    updatePlaying();
    update();
}

//! Playing = the cue plays, or PERFORM plays to the audience.
void MixHouseNowPlayingModel::updatePlaying()
{
    const auto st = globalContext() ? globalContext()->playbackState() : nullptr;
    const bool playing = (st && st->isPlaying()) || (performance() && performance()->performing());
    if (playing != m_playing) {
        m_playing = playing;
        emit playingChanged();
    }
}

//! Where the DJ clock reads the mix: the audience playhead while PERFORM plays
//! (that is what the room hears), else the playhead.
double MixHouseNowPlayingModel::clockPosition() const
{
    if (performance() && performance()->performing()) {
        return performance()->performancePosition().to_double();
    }
    const auto state = globalContext() ? globalContext()->playbackState() : nullptr;
    return state ? state->playbackPosition().to_double() : 0.0;
}

MixHouseNowPlayingModel::SongInfo MixHouseNowPlayingModel::songFor(const QString& path)
{
    const auto it = m_songs.constFind(path);
    if (it != m_songs.constEnd()) {
        return it.value();
    }
    SongInfo info;
    if (library()) {
        IMixHouseLibrary::TrackQuery q;
        q.location = path;
        const MixHouseTrackList found = library()->tracks(q);
        if (!found.empty()) {
            info.keyId = found.front().keyId;
            info.energy = int(std::lround(found.front().energy));
            info.bpm = found.front().bpm;
        }
    }
    m_songs.insert(path, info);
    return info;
}

//! GlideMix: everything about the songs that does not depend on the playhead --
//! keys (a library query each), tempos, beat ticks, phrases, OUT anchors --
//! worked out once per change of the project, not on every playhead move.
//! (Before, two readouts asked the library for every song's key 20 times a
//! second each: the whole app lagged on a long mix -- Chris, 2026-11-13.)
void MixHouseNowPlayingModel::rebuildSongs()
{
    m_mixSongs.clear();
    m_mix = MixSummary {};
    m_songsValid = true;
    const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    if (!project) {
        return;
    }
    std::vector<trackedit::Clip> clips;
    for (const trackedit::TrackId trackId : countedSongTrackIds(*project)) {
        for (const trackedit::Clip& clip : project->clipList(trackId)) {
            clips.push_back(clip);
        }
    }
    std::sort(clips.begin(), clips.end(), [](const trackedit::Clip& a, const trackedit::Clip& b) {
        return a.startTime < b.startTime;
    });
    int previousKey = 0;
    int number = 0;
    for (size_t i = 0; i < clips.size(); ++i) {
        SongCache song;
        song.clip = clips[i];
        // The rest of a song after a Cmd+D loop or a split is the same song:
        // it keeps the song number and is no key change (Chris, 2026-11-16).
        song.continuation = isContinuationPiece(clips[i], clips);
        song.number = song.continuation ? number : ++number;
        song.songStart = clips[i].startTime;
        IMixHouseBeatGrids::ClipFile file;
        if (beatGrids() && beatGrids()->clipFile(clips[i].key, file)) {
            song.info = songFor(file.path);
            song.ratio = file.ratio;
            song.songStart = file.sequenceStart;
        }
        song.keyId = song.info.keyId;
        if (keyMatch()) {
            const int sounding = keyMatch()->keyInfo(clips[i].key).effectiveKeyId;   // a transposed clip
            if (sounding > 0) {
                song.keyId = sounding;
            }
        }
        if (beatGrids()) {
            song.lineBpm = beatGrids()->clipBpm(clips[i].key);
            song.ownBpm = beatGrids()->clipOwnBpm(clips[i].key);
            int bars = 0;
            int beatInBar = 0;
            for (const auto& t : beatGrids()->clipTicks(clips[i].key, std::min(song.songStart, clips[i].startTime),
                                                        clips[i].endTime + 0.002)) {
                if (t.isBar) {
                    if (bars > 0 && beatInBar > 1 && song.beatsPerBar == 0) {
                        song.beatsPerBar = beatInBar;   // the first whole bar
                    }
                    ++bars;
                    beatInBar = 1;
                } else if (bars > 0) {
                    ++beatInBar;
                }
                song.beatTimes.push_back(t.time);
                song.barNumber.push_back(bars);
                song.beatInBar.push_back(beatInBar);
            }
            song.phrases = beatGrids()->clipPhrases(clips[i].key, clips[i].startTime, clips[i].endTime);
            if (i + 1 < clips.size()) {
                const IMixHouseBeatGrids::Anchors anchors = beatGrids()->clipAnchors(clips[i].key);
                song.mixAt = anchors.valid ? anchors.outro : clips[i + 1].startTime;
            }
        } else if (i + 1 < clips.size()) {
            song.mixAt = clips[i + 1].startTime;
        }
        if (song.beatsPerBar <= 1) {
            song.beatsPerBar = 4;
        }

        m_mix.total = std::max(m_mix.total, clips[i].endTime);
        if (song.info.bpm > 0.0) {
            m_mix.minBpm = m_mix.minBpm > 0.0 ? std::min(m_mix.minBpm, song.info.bpm) : song.info.bpm;
            m_mix.maxBpm = std::max(m_mix.maxBpm, song.info.bpm);
        }
        if (i > 0 && !song.continuation && song.keyId > 0 && previousKey > 0) {
            ++m_mix.harmonicTotal;
            if (IMixHouseKeyMatch::shortestStepsToCompatibleKey(previousKey, song.keyId) == 0) {
                ++m_mix.harmonicGood;
            } else if (m_mix.harmonicClash.isEmpty()) {
                m_mix.harmonicClash = QStringLiteral("%1 → %2").arg(song.number - 1).arg(song.number);
            }
        }
        previousKey = song.keyId;
        m_mixSongs.push_back(std::move(song));
    }
}

void MixHouseNowPlayingModel::update()
{
    QString keyText;
    QColor keyColor;
    int energy = 0;
    int songIndex = 0;
    int songCount = 0;
    Clock clock;

    const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    const auto state = globalContext() ? globalContext()->playbackState() : nullptr;
    if (project && state) {
        if (!m_songsValid) {
            rebuildSongs();
        }
        const double pos = clockPosition();
        const int pieces = int(m_mixSongs.size());
        songCount = m_mixSongs.empty() ? 0 : m_mixSongs.back().number;
        // The song under the playhead is the latest to start (the incoming one in a transition).
        int best = -1;
        for (int i = 0; i < pieces; ++i) {
            if (m_mixSongs[i].clip.startTime <= pos && pos < m_mixSongs[i].clip.endTime) {
                best = i;
            }
        }
        clock.mixTotal = m_mix.total;
        clock.mixMinBpm = m_mix.minBpm;
        clock.mixMaxBpm = m_mix.maxBpm;
        clock.harmonicGood = m_mix.harmonicGood;
        clock.harmonicTotal = m_mix.harmonicTotal;
        clock.harmonicClash = m_mix.harmonicClash;
        updateMix(clock, pos, *project);
        if (best >= 0) {
            const SongCache& song = m_mixSongs[best];
            songIndex = song.number;
            energy = song.info.energy;
            // The tempo heard: the line at the playhead in this song's own
            // units (a song matched double-time sits on the line at twice its
            // BPM), so a song at its own tempo reads its BPM and 0 %.
            if (song.lineBpm > 0.0 && song.ownBpm > 0.0 && clock.mixBpm > 0.0) {
                clock.songBpm = clock.mixBpm * song.ownBpm / song.lineBpm;
                clock.stretchPercent = (clock.songBpm / song.ownBpm - 1.0) * 100.0;
                clock.mixBpm = clock.songBpm;   // the DJ bar's TEMPO: what is heard, too
            } else if (song.info.bpm > 0.0 && song.ratio > 0.0) {
                clock.songBpm = song.info.bpm / song.ratio;
                clock.stretchPercent = (1.0 / song.ratio - 1.0) * 100.0;
            }
            updateClock(clock, song, best + 1 < pieces, pos);
            if (song.keyId > 0) {
                const MixHouseKeyNotation notation = libraryConfiguration() ? libraryConfiguration()->keyNotation()
                                                     : MixHouseKeyNotation {};
                keyText = IMixHouseKeyMatch::keyText(song.keyId, notation);
                keyColor = IMixHouseKeyMatch::keyColor(song.keyId);
            }
        }
    }

    if (!(clock == m_clock)) {
        m_clock = clock;
        emit clockChanged();
    }

    if (keyText != m_keyText || keyColor != m_keyColor || energy != m_energy
        || songIndex != m_songIndex || songCount != m_songCount) {
        m_keyText = keyText;
        m_keyColor = keyColor;
        m_energy = energy;
        m_songIndex = songIndex;
        m_songCount = songCount;
        emit songChanged();
    }
}

//! The DJ status bar: the mix's tempo at the playhead and the selection in bars
//! (the songs' tempo range and harmonic flow come from rebuildSongs).
void MixHouseNowPlayingModel::updateMix(Clock& clock, double pos, const trackedit::ITrackeditProject& project)
{
    const trackedit::TimeSignature ts = project.timeSignature();
    clock.beatsPerBarMix = ts.upper > 0 ? ts.upper : 4;
    clock.mixBpm = tempoMap() ? tempoMap()->bpmAt(pos) : ts.tempo;

    if (selectionController()) {
        const double from = selectionController()->dataSelectedStartTime();
        const double to = selectionController()->dataSelectedEndTime();
        if (to > from) {
            clock.selectionSec = to - from;
            const double beats = tempoMap() ? tempoMap()->secToBeat(to) - tempoMap()->secToBeat(from)
                                 : (ts.tempo > 0.0 ? (to - from) * ts.tempo / 60.0 : 0.0);
            clock.selectionBars = beats / clock.beatsPerBarMix;
        }
    }
}

//! The DJ clock for `song` at project time `pos`, from its stored beats: bars
//! count from the song's own bar 1 even when the clip is trimmed.
void MixHouseNowPlayingModel::updateClock(Clock& clock, const SongCache& song, bool hasNext, double pos)
{
    const trackedit::Clip& c = song.clip;
    const double length = c.endTime - c.startTime;
    clock.hasSong = true;
    clock.elapsed = std::max(0.0, pos - c.startTime);
    clock.remaining = std::max(0.0, c.endTime - pos);
    clock.progress = length > 0.0 ? std::clamp(clock.elapsed / length, 0.0, 1.0) : 0.0;

    const double mixAt = hasNext ? song.mixAt : -1.0;
    if (hasNext) {
        clock.nextMixSec = std::max(0.0, mixAt - pos);
    }
    // Without a grid: about 32 bars at a club tempo.
    constexpr int kWarnBars = 32;
    constexpr double kWarnSecNoGrid = 60.0;
    clock.endWarning = !hasNext && c.endTime > pos && c.endTime - pos < kWarnSecNoGrid;
    clock.mixWarning = hasNext && mixAt > pos && mixAt - pos < kWarnSecNoGrid;

    // Beats up to and including the playhead (2 ms early counts as on it).
    const auto beatsUpTo = [&](double t) {
        return int(std::upper_bound(song.beatTimes.begin(), song.beatTimes.end(), t + 0.002) - song.beatTimes.begin());
    };
    const auto barAt = [&](int beats) { return beats > 0 ? song.barNumber[beats - 1] : 0; };
    const int k = beatsUpTo(pos);
    const int bars = barAt(k);
    clock.bar = bars;
    clock.beat = k > 0 ? song.beatInBar[k - 1] : 0;
    clock.beatsPerBar = song.beatsPerBar;
    if (song.beatTimes.empty()) {
        return;
    }
    // Bar lines after `from` up to `to`.
    const auto barsBetween = [&](double from, double to) {
        return to <= from ? 0 : std::max(0, barAt(beatsUpTo(to)) - barAt(beatsUpTo(from)));
    };
    if (hasNext && mixAt > pos) {
        clock.nextMixBars = barsBetween(pos, mixAt);
        if (bars > 0) {   // the song has a grid: count bars, not seconds
            clock.mixWarning = clock.nextMixBars <= kWarnBars;
        }
    }
    if (!hasNext && c.endTime > pos && bars > 0) {
        clock.endWarning = barsBetween(pos, c.endTime) <= kWarnBars;
    }
    for (const auto& phrase : song.phrases) {
        if (phrase.time <= pos + 0.001 && pos < phrase.endTime) {
            clock.phraseText = phrase.label;
            clock.phraseColor = phrase.color;
            clock.phraseBarsLeft = barsBetween(pos, phrase.endTime - 0.01);
            break;
        }
    }
}

void MixHouseNowPlayingModel::sampleCpu()
{
    const double now = processCpuSeconds();
    const double wall = m_cpuClock.restart() / 1000.0;

    const double memory = processMemoryBytes();
    if (memory >= 0.0) {
        static const double total = totalMemoryBytes();
        const double gb = memory / (1024.0 * 1024.0 * 1024.0);
        if (std::abs(gb - m_memoryGB) > 0.005) {
            m_memoryGB = gb;
            m_memoryShare = total > 0.0 ? std::clamp(memory / total, 0.0, 1.0) : 0.0;
            emit cpuChanged();
        }
    }

    const double diskBytes = processDiskReadBytes();
    if (diskBytes >= 0.0 && m_lastDiskBytes >= 0.0 && wall > 0.0) {
        const double mbps = std::max(0.0, (diskBytes - m_lastDiskBytes) / wall / (1024.0 * 1024.0));
        if (std::abs(mbps - m_diskMBps) > 0.05) {
            m_diskMBps = mbps;
            emit cpuChanged();
        }
    }
    m_lastDiskBytes = diskBytes;
    if (now < 0.0 || m_lastCpuSecs < 0.0 || wall <= 0.0) {
        m_lastCpuSecs = now;
        return;
    }
    // Share of the whole processor (all cores), like a DJ app's CPU meter.
    const int cores = std::max(1, QThread::idealThreadCount());
    const int percent = std::clamp(int(std::lround((now - m_lastCpuSecs) / wall / cores * 100.0)), 0, 100);
    m_lastCpuSecs = now;
    if (percent != m_cpuPercent) {
        m_cpuPercent = percent;
        emit cpuChanged();
    }
}
