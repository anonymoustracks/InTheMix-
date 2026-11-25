/*
 * GlideMix: developer tool -- runs the app's own track analysis (beats, bar 1,
 * key, waveform, phrases) over a list of files and writes the results as JSON
 * Lines (one object per track, written as each finishes), for measuring the
 * detectors and for the overnight trainer. Not shipped.
 *
 *   glidemix-analyze <list.txt> <out.jsonl> [threads] [--no-key] [--drift]
 *   glidemix-analyze --measure <file> <bpm> <barOneSec>
 *
 * --drift adds how the hits sit on the analysed grid, bar by bar
 * (mixhousegridshifts.h: measureBarOffsets / summarizeBarOffsets).
 * --measure prints that for one file and a grid you give (calibration).
 *
 * Resumable: tracks already in <out.jsonl> are skipped, so a stopped run
 * continues where it left off. --no-key skips key detection.
 */
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>

#include "mixhouseanalyzer.h"
#include "mixhouseaudiosource.h"
#include "mixhousegridshifts.h"
#include "mixhousebeatmap.h"
#include "mixhousefiletags.h"
#include "mixhousephrasedetector.h"
#include "mixhousewaveform.h"

using namespace au::mixhouse;

namespace {
//! The file as the timeline hears it, mixed to mono.
std::vector<float> decodeMono(const QString& path, int& sampleRate)
{
    const AudioInfo probe = probeAudioFile(path);
    sampleRate = probe.sampleRate;
    const int channels = path.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive) ? 2 : std::max(1, probe.channels);
    std::vector<float> mono;
    decodeAudioFile(path, [&](const float* samples, long long frames) {
        for (long long i = 0; i < frames; ++i) {
            float m = 0.0f;
            for (int c = 0; c < channels; ++c) {
                m += samples[i * channels + c];
            }
            mono.push_back(m / channels);
        }
        return true;
    });
    return mono;
}

QJsonObject driftJson(const QString& path, double bpm, double barOneSec)
{
    int rate = 0;
    const std::vector<float> mono = decodeMono(path, rate);
    QJsonObject o;
    if (mono.empty() || rate <= 0 || bpm <= 0.0) {
        return o;
    }
    const std::vector<MixHouseBarOffset> bars = qEnvironmentVariableIsSet("GLIDEMIX_DRIFT_ONSETS")
        ? measureBarOffsets(detectBeatOnsets(mono.data(), mono.size(), rate), double(mono.size()) / rate, bpm, barOneSec)
        : measureBarPhase(detectOnsetCurve(mono.data(), mono.size(), rate), double(mono.size()) / rate, bpm, barOneSec);
    const MixHouseGridFit fit = summarizeBarOffsets(bars);
    const auto r1 = [](double v) { return std::round(v * 10.0) / 10.0; };
    o.insert("bars", fit.bars);
    o.insert("median_ms", r1(fit.medianMs));
    o.insert("spread_ms", r1(fit.spreadMs));
    o.insert("drift_ms_per_min", r1(fit.driftMsPerMin));
    o.insert("start_ms", r1(fit.startMs));
    o.insert("end_ms", r1(fit.endMs));
    o.insert("worst16_ms", r1(fit.worstWindowMs));
    QJsonArray perBar;
    for (const MixHouseBarOffset& b : bars) {
        perBar.append(QJsonArray { int(b.bar), r1(b.offsetSec * 1000.0) });
    }
    o.insert("per_bar", perBar);
    return o;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc == 5 && QByteArray(argv[1]) == "--measure") {
        const QJsonObject o = driftJson(QString::fromLocal8Bit(argv[2]), atof(argv[3]), atof(argv[4]));
        std::fprintf(stdout, "%s\n", QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
        return 0;
    }
    if (argc == 3 && QByteArray(argv[1]) == "--serato") {
        // The Serato 5 beat map in a file's tags, on our clock: count, first, last.
        const QString path = QString::fromLocal8Bit(argv[2]);
        const AudioInfo probe = probeAudioFile(path);
        const MixHouseBeatMap map
            = seratoBeatMap(path, readFileTags(path).seratoTempoMap, probe.durationSec, probe.sampleRate);
        const MixHouseBeatMapFit fit = fitConstant(map);
        std::fprintf(stdout, "%zu %.6f %.6f %.3f %.2f\n", map.beatSecs.size(), map.valid() ? map.beatSecs.front() : 0.0,
                     map.valid() ? map.beatSecs.back() : 0.0, fit.bpm, fit.spreadMs);
        return 0;
    }
    if (argc < 3) {
        std::fprintf(stderr, "usage: glidemix-analyze <list.txt> <out.jsonl> [threads] [--no-key]\n");
        return 2;
    }
    QFile list(QString::fromLocal8Bit(argv[1]));
    if (!list.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    int threads = 4;
    bool drift = false;
    for (int a = 3; a < argc; ++a) {
        if (QByteArray(argv[a]) == "--drift") {
            drift = true;
        } else if (QByteArray(argv[a]) == "--no-key") {
            setMixHouseKeyDetectionEnabled(false);
        } else {
            threads = std::max(1, atoi(argv[a]));
        }
    }

    // Tracks already written by an earlier run.
    QSet<QString> done;
    QFile out(QString::fromLocal8Bit(argv[2]));
    if (out.open(QIODevice::ReadOnly)) {
        while (!out.atEnd()) {
            const QJsonObject o = QJsonDocument::fromJson(out.readLine()).object();
            if (o.contains("path")) {
                done.insert(o.value("path").toString());
            }
        }
        out.close();
    }
    QStringList paths;
    for (QString p : QString::fromUtf8(list.readAll()).split(u'\n', Qt::SkipEmptyParts)) {
        p = p.trimmed();   // Windows lists end their lines in \r\n
        if (!p.isEmpty() && !done.contains(p)) {
            paths.append(p);
        }
    }
    if (!out.open(QIODevice::Append)) {
        std::fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }
    std::fprintf(stderr, "%d done before, %d to go\n", int(done.size()), int(paths.size()));

    std::atomic<int> next { 0 };
    std::atomic<int> finished { 0 };
    std::mutex mutex;
    std::vector<std::thread> pool;
    for (int w = 0; w < threads; ++w) {
        pool.emplace_back([&]() {
            for (int i; (i = next++) < paths.size();) {
                const MixHouseAnalysisResult r = analyzeTrackFile(paths[i]);
                const double downbeat = r.downbeatSec >= 0.0 ? r.downbeatSec : r.firstBeatSec;
                QJsonArray phrases;
                if (r.valid && r.bpm > 0.0) {
                    const MixHouseWaveformOverview overview = decodeWaveformOverview(qUncompress(
                        encodeWaveformOverview(r.waveformOverview, r.waveformLow, r.waveformMid, r.waveformHigh)));
                    for (const MixHousePhrase& p : detectPhrases(overview, r.durationSec, r.bpm, downbeat)) {
                        phrases.append(QJsonArray { p.startSec, p.endSec, p.label });
                    }
                }
                QJsonObject o;
                o.insert("path", paths[i]);
                o.insert("valid", r.valid);
                o.insert("bpm", r.bpm);
                o.insert("firstBeat", r.firstBeatSec);
                o.insert("downbeat", downbeat);
                o.insert("duration", r.durationSec);
                o.insert("key", r.keyId);
                o.insert("phrases", phrases);
                QJsonArray energy;
                for (float e : r.beatBandEnergy) {
                    energy.append(std::round(e * 100.0) / 100.0);
                }
                o.insert("energy", energy);
                QJsonArray features;
                for (float f : r.energyFeatures) {
                    features.append(std::round(f * 1000.0) / 1000.0);
                }
                o.insert("energy_features", features);
                o.insert("energy_rating", r.energy);
                QJsonArray soundsLike;
                for (float f : r.soundsLike) {
                    soundsLike.append(std::round(f * 1000.0) / 1000.0);
                }
                o.insert("sounds_like", soundsLike);
                QJsonArray model;
                for (const MixHousePhrase& p : r.modelPhrases) {
                    model.append(QJsonArray { p.startSec, p.endSec, p.label });
                }
                o.insert("model_phrases", model);
                if (drift && r.valid && r.bpm > 0.0) {
                    o.insert("drift", driftJson(paths[i], r.bpm, downbeat));
                    QJsonArray beats;
                    for (float b : r.beatThisBeats) {
                        beats.append(std::round(b * 1000.0) / 1000.0);
                    }
                    o.insert("beatthis_beats", beats);
                    QJsonArray map;
                    for (double b : r.beatMapSecs) {
                        map.append(std::round(b * 10000.0) / 10000.0);
                    }
                    o.insert("beat_map", map);
                }
                const QByteArray line = QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
                std::lock_guard<std::mutex> lock(mutex);
                out.write(line);
                out.flush();
                std::fprintf(stdout, "%d/%d\n", ++finished, int(paths.size()));
                std::fflush(stdout);
            }
        });
    }
    for (std::thread& t : pool) {
        t.join();
    }
    std::fprintf(stderr, "%d tracks\n", int(finished));
    return 0;
}
