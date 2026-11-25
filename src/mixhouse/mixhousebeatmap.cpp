/*
 * GlideMix
 */
#include "mixhousebeatmap.h"

#include <QFile>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

#include "mp3guessenc.h"

namespace au::mixhouse {
namespace {
double medianOf(std::vector<double> v)
{
    if (v.empty()) {
        return 0.0;
    }
    const size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    return v[mid];
}

double readBigDouble(const char* p)
{
    const quint64 bits = qFromBigEndian<quint64>(p);
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}
}

double MixHouseBeatMap::beatAt(double sec) const
{
    if (!valid()) {
        return 0.0;
    }
    const auto it = std::upper_bound(beatSecs.begin(), beatSecs.end(), sec);
    size_t i = it == beatSecs.begin() ? 0 : static_cast<size_t>(it - beatSecs.begin()) - 1;
    i = std::min(i, beatSecs.size() - 2);
    return double(i) + (sec - beatSecs[i]) / (beatSecs[i + 1] - beatSecs[i]);
}

double MixHouseBeatMap::secAt(double beat) const
{
    if (!valid()) {
        return 0.0;
    }
    const double last = double(beatSecs.size() - 2);
    const size_t i = static_cast<size_t>(std::clamp(std::floor(beat), 0.0, last));
    return beatSecs[i] + (beat - double(i)) * (beatSecs[i + 1] - beatSecs[i]);
}

double MixHouseBeatMap::bpmAt(double sec) const
{
    if (!valid()) {
        return 0.0;
    }
    const size_t i = static_cast<size_t>(std::clamp(std::floor(beatAt(sec)), 0.0, double(beatSecs.size() - 2)));
    return 60.0 / (beatSecs[i + 1] - beatSecs[i]);
}

MixHouseBeatMapFit fitConstant(const MixHouseBeatMap& map)
{
    MixHouseBeatMapFit fit;
    const size_t n = map.beatSecs.size();
    if (n < 2) {
        return fit;
    }
    // beat i at a + i * p, least squares.
    const double mi = (double(n) - 1.0) / 2.0;
    double mt = 0.0;
    for (double t : map.beatSecs) {
        mt += t;
    }
    mt /= double(n);
    double sxy = 0.0, sxx = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sxy += (double(i) - mi) * (map.beatSecs[i] - mt);
        sxx += (double(i) - mi) * (double(i) - mi);
    }
    const double p = sxy / sxx;
    const double a = mt - p * mi;
    std::vector<double> dev(n);
    for (size_t i = 0; i < n; ++i) {
        dev[i] = std::abs(map.beatSecs[i] - (a + double(i) * p)) * 1000.0;
    }
    fit.bpm = 60.0 / p;
    fit.firstBeatSec = a;
    fit.spreadMs = medianOf(dev);
    fit.worstMs = *std::max_element(dev.begin(), dev.end());
    return fit;
}

MixHouseBeatMap beatMapFromSegments(double firstBeatSec, const std::vector<MixHouseTempoSegment>& segments,
                                    double durationSec)
{
    MixHouseBeatMap map;
    double t = firstBeatSec;
    for (size_t k = 0; k < segments.size() && t < durationSec; ++k) {
        if (segments[k].bpm <= 0.0) {
            return MixHouseBeatMap {};
        }
        const double endBeat = k + 1 < segments.size() ? segments[k + 1].startBeat : 1e12;
        for (double b = segments[k].startBeat; b < endBeat - 1e-9 && t < durationSec; b += 1.0) {
            map.beatSecs.push_back(t);
            t += 60.0 / segments[k].bpm;
        }
    }
    return map;
}

MixHouseBeatMap beatMapFromMarkers(const std::vector<MixHouseTempoMarker>& markers, double durationSec)
{
    MixHouseBeatMap map;
    for (size_t k = 0; k < markers.size(); ++k) {
        const MixHouseTempoMarker& m = markers[k];
        if (m.bpm <= 0.0) {
            return MixHouseBeatMap {};
        }
        if (k + 1 < markers.size()) {
            const double span = markers[k + 1].sec - m.sec;
            if (span <= 0.0) {
                continue;
            }
            const long beats = std::max(1L, std::lround(span * m.bpm / 60.0));
            for (long q = 0; q < beats; ++q) {
                map.beatSecs.push_back(m.sec + span * double(q) / double(beats));
            }
        } else {
            for (double t = m.sec; t < durationSec; t += 60.0 / m.bpm) {
                map.beatSecs.push_back(t);
            }
        }
    }
    return map;
}

MixHouseBeatMap seratoTempoMapToBeatMap(const QByteArray& body, double durationSec)
{
    // 02 00 | 00 01 | double first beat (s) | uint32 count | count x 33 bytes:
    // int64 position in 960ths of a beat, double BPM, double note unit (0.25),
    // 1 flag byte, int64 (0). All big-endian. Read from files Serato 5 wrote
    // (Fluid Beats test set, 2026-11-12); segments start on bar lines.
    constexpr int kHeader = 16, kSegment = 33;
    if (body.size() < kHeader || static_cast<unsigned char>(body[0]) != 0x02) {
        return {};
    }
    const char* p = body.constData();
    const double first = readBigDouble(p + 4);
    const quint32 count = qFromBigEndian<quint32>(p + 12);
    if (count == 0 || body.size() < kHeader + qsizetype(count) * kSegment) {
        return {};
    }
    std::vector<MixHouseTempoSegment> segments;
    for (quint32 k = 0; k < count; ++k) {
        const char* s = p + kHeader + qsizetype(k) * kSegment;
        MixHouseTempoSegment seg;
        seg.startBeat = double(qFromBigEndian<qint64>(s)) / 960.0;
        seg.bpm = readBigDouble(s + 8);
        if (!(seg.bpm > 0.0 && seg.bpm < 1000.0) || (!segments.empty() && seg.startBeat <= segments.back().startBeat)) {
            return {};
        }
        segments.push_back(seg);
    }
    MixHouseBeatMap map = beatMapFromSegments(first, segments, durationSec);
    map.source = QStringLiteral("Serato");
    return map;
}

QByteArray encodeBeatMap(const MixHouseBeatMap& map)
{
    QByteArray blob(qsizetype(map.beatSecs.size() * sizeof(double)), Qt::Uninitialized);
    char* out = blob.data();
    for (double t : map.beatSecs) {
        quint64 bits;
        std::memcpy(&bits, &t, sizeof bits);
        qToLittleEndian(bits, out);
        out += sizeof bits;
    }
    return blob;
}

MixHouseBeatMap decodeBeatMap(const QByteArray& blob, const QString& source)
{
    MixHouseBeatMap map;
    map.source = source;
    const char* p = blob.constData();
    for (qsizetype i = 0; i + qsizetype(sizeof(double)) <= blob.size(); i += sizeof(double)) {
        const quint64 bits = qFromLittleEndian<quint64>(p + i);
        double t;
        std::memcpy(&t, &bits, sizeof t);
        map.beatSecs.push_back(t);
    }
    return map;
}

int mp3TimingShiftCase(const QString& filePath)
{
    if (!filePath.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive)) {
        return 0;
    }
    static std::mutex guessMutex;
    std::lock_guard<std::mutex> lock(guessMutex);
    const int c = mp3guessenc_timing_shift_case(QFile::encodeName(filePath).constData());
    return c >= EXIT_CODE_CASE_A && c <= EXIT_CODE_CASE_D ? c : 0;
}

double seratoClockOffsetSec(const QString& filePath, int sampleRate)
{
    const int c = mp3TimingShiftCase(filePath);
    if (sampleRate <= 0 || c == 0 || c == EXIT_CODE_CASE_A) {
        return 0.0;
    }
    return 2257.0 / sampleRate;
}

MixHouseBeatMap seratoBeatMap(const QString& filePath, const QByteArray& tempoMapTag, double durationSec, int sampleRate)
{
    MixHouseBeatMap map = seratoTempoMapToBeatMap(tempoMapTag, durationSec + 1.0);
    if (!map.valid()) {
        return {};
    }
    const double offset = seratoClockOffsetSec(filePath, sampleRate);
    std::vector<double> beats;
    for (double t : map.beatSecs) {
        t -= offset;
        if (t >= 0.0 && t < durationSec) {
            beats.push_back(t);
        }
    }
    map.beatSecs = std::move(beats);
    return map.valid() ? map : MixHouseBeatMap {};
}
}

