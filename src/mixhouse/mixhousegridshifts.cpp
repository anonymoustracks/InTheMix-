/*
 * GlideMix
 */
#include "mixhousegridshifts.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#ifdef GRIDSHIFT_DEBUG
#include <cstdio>
#endif

namespace au::mixhouse {
namespace {
// rawOnsetTimes() reports a hit this much after "the beat" (negative: before).
// Measured 2026-11-12 on synthetic kicks at known times: a hard kick's start
// reads -2.5 ms, a soft one's (2 ms attack) -1.0 ms. Lexicon (the reference
// Chris trusts) puts its line ~2.6 ms before that start: on 46 library songs
// our onset-refined grids sat a median +2.64 ms after Lexicon's with -2 ms
// here. So the beat line sits ~3 ms ahead of the kick's start, like Lexicon.
constexpr double kOnsetBiasSec = 0.0006;
constexpr double kMinJumpSec = 0.020;      // smaller is not heard as a flam
constexpr double kSteadySec = 0.008;       // a section's bars sit this close to its median (MAD)
constexpr int kMinShiftBars = 4;           // the shortest piece the fit considers
constexpr size_t kMinSteadyBars = 8;       // a section that counts (reference or jump) holds this long
constexpr double kMaxDriftDiffSec = 0.002;  // per bar: steady sections share the grid's tempo error
constexpr double kMinSteadyShare = 0.5;    // less of the song steady: its beats are not clear enough to move
constexpr double kSplitPenalty = 0.002;    // squared seconds of residual a new piece has to explain

// In-place radix-2 FFT (n a power of two).
void fft(std::vector<std::complex<double> >& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> wl = std::polar(1.0, -2.0 * M_PI / len);
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

double median(std::vector<double> v)
{
    if (v.empty()) {
        return 0.0;
    }
    const size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    double m = v[mid];
    if (v.size() % 2 == 0) {
        m = (m + *std::max_element(v.begin(), v.begin() + mid)) / 2.0;
    }
    return m;
}

/// Onset times (seconds, with a constant small bias, which cancels out: only
/// differences between sections matter): spectral flux of the whole band at
/// about 11 kHz. The kick's click times a hit the same way whatever the kick
/// sounds like; the low band alone did not (a new kick sound in a new section
/// moved its onsets by up to 40 ms in a steady DAW track).
//! Spectral flux, one value per hop; frame j is centred at
//! (j * hopSec + centreSec). Empty when the audio is too short.
struct Flux {
    std::vector<double> values;
    double hopSec = 0.0;
    double centreSec = 0.0;
};
Flux spectralFlux(const float* mono, size_t frames, int sampleRate)
{
    const size_t decim = std::max<size_t>(1, static_cast<size_t>(std::lround(sampleRate / 11025.0)));
    const double rate = static_cast<double>(sampleRate) / decim;
    std::vector<double> x(frames / decim);
    for (size_t i = 0; i < x.size(); ++i) {
        double sum = 0.0;
        for (size_t k = 0; k < decim; ++k) {
            sum += mono[i * decim + k];
        }
        x[i] = sum / decim;
    }
    constexpr size_t kWindow = 128, kHop = 32;
    if (x.size() < kWindow * 4) {
        return Flux {};
    }
    const size_t n = (x.size() - kWindow) / kHop;
    std::vector<double> hann(kWindow);
    for (size_t i = 0; i < kWindow; ++i) {
        hann[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / kWindow);
    }
    std::vector<double> flux(n, 0.0), previous(kWindow / 2 + 1, 0.0);
    std::vector<std::complex<double> > buf(kWindow);
    for (size_t j = 0; j < n; ++j) {
        for (size_t i = 0; i < kWindow; ++i) {
            buf[i] = x[j * kHop + i] * hann[i];
        }
        fft(buf);
        double f = 0.0;
        for (size_t k = 0; k <= kWindow / 2; ++k) {
            const double level = std::log1p(100.0 * std::abs(buf[k]));
            if (j > 0) {
                f += std::max(0.0, level - previous[k]);
            }
            previous[k] = level;
        }
        flux[j] = f;
    }
    Flux out;
    out.values = std::move(flux);
    out.hopSec = kHop / rate;
    out.centreSec = kWindow / 2.0 / rate;
    return out;
}

std::vector<double> rawOnsetTimes(const float* mono, size_t frames, int sampleRate)
{
    const Flux fx = spectralFlux(mono, frames, sampleRate);
    const std::vector<double>& flux = fx.values;
    const size_t n = flux.size();
    if (n < 16) {
        return {};
    }
    double mean = 0.0;
    for (double f : flux) {
        mean += f;
    }
    mean /= n;
    double var = 0.0;
    for (double f : flux) {
        var += (f - mean) * (f - mean);
    }
    const double stddev = std::sqrt(var / n);

    // Local peaks (60 ms either side) above the local average (+-0.5 s) plus
    // the overall spread, at least 100 ms apart.
    const double hopSec = fx.hopSec;
    const long peakHalf = std::max(1L, std::lround(0.060 / hopSec));
    const long avgHalf = peakHalf * 8;
    std::vector<double> prefix(n + 1, 0.0);
    for (size_t j = 0; j < n; ++j) {
        prefix[j + 1] = prefix[j] + flux[j];
    }
    std::vector<double> onsets;
    double last = -1.0;
    for (long j = peakHalf; j + peakHalf < static_cast<long>(n); ++j) {
        const long lo = std::max(0L, j - avgHalf), hi = std::min(static_cast<long>(n), j + avgHalf);
        if (flux[j] <= (prefix[hi] - prefix[lo]) / (hi - lo) + stddev) {
            continue;
        }
        if (flux[j] < *std::max_element(flux.begin() + (j - peakHalf), flux.begin() + (j + peakHalf))) {
            continue;
        }
        // Between hops (2.9 ms): the top of a parabola through the peak and
        // its neighbours, so a bar's median is not stuck on the hop grid.
        const double l = flux[j - 1], c = flux[j], r = flux[j + 1];
        const double den = l - 2.0 * c + r;
        const double frac = den < 0.0 ? std::clamp(0.5 * (l - r) / den, -0.5, 0.5) : 0.0;
        const double t = (j + frac) * fx.hopSec + fx.centreSec;
        if (t - last >= 0.100) {
            onsets.push_back(t);
            last = t;
        }
    }
    return onsets;
}
}

std::vector<MixHouseGridShift> detectGridShifts(const float* mono, size_t frames, int sampleRate,
                                                double bpm, double barOneSec, int beatsPerBar)
{
    std::vector<MixHouseGridShift> shifts;
    if (!mono || sampleRate <= 0 || bpm <= 0.0 || beatsPerBar <= 0 || frames < static_cast<size_t>(sampleRate) * 10) {
        return shifts;
    }
    const double beat = 60.0 / bpm;
    const double bar = beat * beatsPerBar;
    const double duration = static_cast<double>(frames) / sampleRate;
    std::vector<double> m;
    std::vector<long> barIndex;
    for (const MixHouseBarOffset& o : measureBarOffsets(detectBeatOnsets(mono, frames, sampleRate), duration, bpm,
                                                        barOneSec, beatsPerBar)) {
        m.push_back(o.offsetSec);
        barIndex.push_back(o.bar);
    }
    const size_t n = std::min<size_t>(m.size(), 800);
    if (n < 2 * kMinShiftBars) {
        return shifts;
    }

    // Fewest straight pieces (offset plus drift: a grid a hair off the
    // song's tempo drifts steadily, which is not a jump) that explain the
    // per-bar offsets, by least squares over bar times: dynamic programming
    // over the piece boundaries, prefix sums for each piece's line.
    std::vector<double> t(n);
    for (size_t i = 0; i < n; ++i) {
        t[i] = barIndex[i] * bar;
    }
    std::vector<double> st(n + 1, 0.0), sm(n + 1, 0.0), stt(n + 1, 0.0), stm(n + 1, 0.0), smm(n + 1, 0.0);
    for (size_t i = 0; i < n; ++i) {
        st[i + 1] = st[i] + t[i];
        sm[i + 1] = sm[i] + m[i];
        stt[i + 1] = stt[i] + t[i] * t[i];
        stm[i + 1] = stm[i] + t[i] * m[i];
        smm[i + 1] = smm[i] + m[i] * m[i];
    }
    struct Line {
        double a = 0.0, b = 0.0;   // offset = a + b * t
        double at(double x) const { return a + b * x; }
    };
    auto line = [&](size_t i, size_t j) {
        const double k = static_cast<double>(j - i);
        const double mt = (st[j] - st[i]) / k, mm = (sm[j] - sm[i]) / k;
        const double vtt = (stt[j] - stt[i]) / k - mt * mt;
        const double vtm = (stm[j] - stm[i]) / k - mt * mm;
        Line l;
        l.b = vtt > 1e-12 ? vtm / vtt : 0.0;
        l.a = mm - l.b * mt;
        return l;
    };
    auto cost = [&](size_t i, size_t j) {
        const Line l = line(i, j);
        const double k = static_cast<double>(j - i);
        // Sum of (m - a - b t)^2 from the prefix sums.
        const double c = (smm[j] - smm[i]) - 2.0 * l.a * (sm[j] - sm[i]) - 2.0 * l.b * (stm[j] - stm[i])
                         + k * l.a * l.a + 2.0 * l.a * l.b * (st[j] - st[i]) + l.b * l.b * (stt[j] - stt[i]);
        return std::max(0.0, c);
    };
    std::vector<double> best(n + 1, std::numeric_limits<double>::infinity());
    std::vector<size_t> from(n + 1, 0);
    best[0] = 0.0;
    for (size_t j = kMinShiftBars; j <= n; ++j) {
        for (size_t i = 0; i + kMinShiftBars <= j; ++i) {
            if (!std::isfinite(best[i])) {
                continue;
            }
            const double c = best[i] + cost(i, j) + kSplitPenalty;
            if (c < best[j]) {
                best[j] = c;
                from[j] = i;
            }
        }
    }
    std::vector<std::pair<size_t, size_t> > pieces;
    for (size_t j = n; j > 0; j = from[j]) {
        pieces.emplace_back(from[j], j);
    }
    std::reverse(pieces.begin(), pieces.end());

    // Jumps between steady pieces: the step from the earlier piece's line to
    // the later one's where the later one starts. The cut goes right after
    // the earlier piece's last bar: what lies between (a fill, the loop
    // change) has no steady beat to keep, but a late bar left unmoved flams. Unsteady or short pieces
    // (an intro, a breakdown without kicks, the loop change itself) move
    // nothing. A song that is mostly unsteady is left alone altogether.
    struct Piece {
        size_t first, end;
        Line line;
    };
    std::vector<Piece> steady;
    size_t steadyBars = 0;
    for (const auto& [i, j] : pieces) {
        const Line l = line(i, j);
        std::vector<double> dev;
        for (size_t q = i; q < j; ++q) {
            dev.push_back(std::abs(m[q] - l.at(t[q])));
        }
#ifdef GRIDSHIFT_DEBUG
        std::fprintf(stderr, "piece bars %ld-%ld (%zu) offset %+.1f ms drift %+.2f ms/bar MAD %.1f ms\n", barIndex[i],
                     barIndex[j - 1], j - i, l.at(t[i]) * 1000, l.b * bar * 1000, median(dev) * 1000);
#endif
        // A steep line through a breakdown can look steady; a real section
        // drifts like the rest of the song (the same small tempo error).
        const bool drifts = !steady.empty() && std::abs(l.b - steady.front().line.b) * bar > kMaxDriftDiffSec;
        if (j - i >= kMinSteadyBars && median(dev) <= kSteadySec && !drifts) {
            steady.push_back({ i, j, l });
            steadyBars += j - i;
        }
    }
    if (steady.size() < 2 || steadyBars < kMinSteadyShare * n) {
        return shifts;
    }
    double total = 0.0;
    for (size_t p = 1; p < steady.size(); ++p) {
        const double at = t[steady[p].first];
        const double step = steady[p].line.at(at) - steady[p - 1].line.at(at);
        if (std::abs(step) >= kMinJumpSec && std::abs(total + step) < beat / 6.0) {
            total += step;
            shifts.push_back({ barOneSec + (barIndex[steady[p - 1].end - 1] + 1) * bar, total });
        }
    }
    return shifts;
}

std::vector<double> detectBeatOnsets(const float* mono, size_t frames, int sampleRate)
{
    std::vector<double> onsets = rawOnsetTimes(mono, frames, sampleRate);
    for (double& t : onsets) {
        t -= kOnsetBiasSec;
    }
    return onsets;
}

std::vector<MixHouseBarOffset> measureBarOffsets(const std::vector<double>& onsets, double durationSec,
                                                 double bpm, double barOneSec, int beatsPerBar)
{
    std::vector<MixHouseBarOffset> out;
    if (bpm <= 0.0 || beatsPerBar <= 0 || durationSec <= 0.0) {
        return out;
    }
    const double beat = 60.0 / bpm;
    const double bar = beat * beatsPerBar;
    // One median offset (onset - nearest grid beat) per bar, from the
    // onsets within a sixth of a beat of a beat (a quarter let sections
    // whose loudest hits are 16ths count as jumps).
    const long firstBar = static_cast<long>(std::floor((0.0 - barOneSec) / bar));
    const long lastBar = static_cast<long>(std::ceil((durationSec - barOneSec) / bar));
    std::vector<std::vector<double> > perBar(static_cast<size_t>(lastBar - firstBar + 1));
    for (double t : onsets) {
        const double k = std::round((t - barOneSec) / beat);
        const double r = t - (barOneSec + k * beat);
        if (std::abs(r) >= beat / 6.0) {
            continue;
        }
        const long b = static_cast<long>(std::floor(k * beat / bar + 1e-9));
        if (b >= firstBar && b <= lastBar) {
            perBar[static_cast<size_t>(b - firstBar)].push_back(r);
        }
    }
    for (size_t i = 0; i < perBar.size(); ++i) {
        if (perBar[i].size() >= 2) {
            MixHouseBarOffset o;
            o.bar = firstBar + static_cast<long>(i);
            o.barSec = barOneSec + o.bar * bar;
            o.offsetSec = median(perBar[i]);
            o.hits = static_cast<int>(perBar[i].size());
            out.push_back(o);
        }
    }
    return out;
}

MixHouseGridFit summarizeBarOffsets(const std::vector<MixHouseBarOffset>& offsets)
{
    MixHouseGridFit fit;
    fit.bars = static_cast<int>(offsets.size());
    if (offsets.size() < 4) {
        return fit;
    }
    std::vector<double> ms;
    for (const MixHouseBarOffset& o : offsets) {
        ms.push_back(o.offsetSec * 1000.0);
    }
    fit.medianMs = median(ms);
    std::vector<double> dev;
    for (double v : ms) {
        dev.push_back(std::abs(v - fit.medianMs));
    }
    fit.spreadMs = median(dev);
    // Straight line through the bars that sit near the grid (outliers -- a
    // fill, a breakdown's stray hits -- would tilt a plain fit).
    double n = 0, st = 0, sm = 0, stt = 0, stm = 0;
    for (size_t i = 0; i < ms.size(); ++i) {
        if (std::abs(ms[i] - fit.medianMs) > std::max(6.0, 3.0 * fit.spreadMs)) {
            continue;
        }
        const double t = offsets[i].barSec / 60.0;
        n += 1;
        st += t;
        sm += ms[i];
        stt += t * t;
        stm += t * ms[i];
    }
    if (n >= 4) {
        const double vtt = stt / n - (st / n) * (st / n);
        const double slope = vtt > 1e-12 ? (stm / n - (st / n) * (sm / n)) / vtt : 0.0;
        const double a = sm / n - slope * st / n;
        fit.driftMsPerMin = slope;
        fit.startMs = a + slope * offsets.front().barSec / 60.0;
        fit.endMs = a + slope * offsets.back().barSec / 60.0;
    }
    constexpr size_t kWindow = 16;
    for (size_t i = 0; i + kWindow <= ms.size(); ++i) {
        const double w = median(std::vector<double>(ms.begin() + i, ms.begin() + i + kWindow)) - fit.medianMs;
        if (std::abs(w) > std::abs(fit.worstWindowMs)) {
            fit.worstWindowMs = w;
        }
    }
    return fit;
}

MixHouseOnsetCurve detectOnsetCurve(const float* mono, size_t frames, int sampleRate)
{
    const Flux fx = spectralFlux(mono, frames, sampleRate);
    MixHouseOnsetCurve curve;
    curve.hopSec = fx.hopSec;
    curve.startSec = fx.centreSec - kOnsetBiasSec;
    // Minus its local average (+-0.25 s), clipped at 0: only rises count, a
    // loud section does not pull the comb.
    const size_t n = fx.values.size();
    const long half = fx.hopSec > 0.0 ? std::max(1L, std::lround(0.25 / fx.hopSec)) : 1;
    std::vector<double> prefix(n + 1, 0.0);
    for (size_t j = 0; j < n; ++j) {
        prefix[j + 1] = prefix[j] + fx.values[j];
    }
    curve.strength.resize(n);
    for (size_t j = 0; j < n; ++j) {
        const long lo = std::max(0L, long(j) - half), hi = std::min(long(n), long(j) + half + 1);
        curve.strength[j] = static_cast<float>(std::max(0.0, fx.values[j] - (prefix[hi] - prefix[lo]) / (hi - lo)));
    }
    return curve;
}

std::vector<MixHouseBarOffset> measureBarPhase(const MixHouseOnsetCurve& curve, double durationSec, double bpm,
                                               double barOneSec, int beatsPerBar)
{
    std::vector<MixHouseBarOffset> out;
    if (bpm <= 0.0 || beatsPerBar <= 0 || curve.hopSec <= 0.0 || curve.strength.size() < 16) {
        return out;
    }
    const double beat = 60.0 / bpm;
    const double bar = beat * beatsPerBar;
    const auto at = [&](double t) {   // strength at file time t, linear between hops
        const double x = (t - curve.startSec) / curve.hopSec;
        if (x < 0.0 || x >= double(curve.strength.size() - 1)) {
            return 0.0;
        }
        const size_t i = static_cast<size_t>(x);
        const double f = x - double(i);
        return curve.strength[i] * (1.0 - f) + curve.strength[i + 1] * f;
    };
    // Shifts tried: up to a fifth of a beat either way (a 16th is a quarter),
    // in steps of half a hop.
    const double step = curve.hopSec / 2.0;
    const int steps = static_cast<int>(std::floor(beat / 5.0 / step));
    const long firstBar = static_cast<long>(std::ceil((0.0 - barOneSec) / bar));
    const long lastBar = static_cast<long>(std::floor((durationSec - barOneSec) / bar)) - 1;
    std::vector<double> score(2 * steps + 1);
    for (long b = firstBar; b <= lastBar; ++b) {
        // The comb: every beat of this bar and the bars either side (three
        // bars, so one bar's syncopation or a missing kick does not decide).
        const double from = barOneSec + (b - 1) * bar;
        const int beats = 3 * beatsPerBar;
        double total = 0.0;
        for (int s = -steps; s <= steps; ++s) {
            double sum = 0.0;
            for (int k = 0; k < beats; ++k) {
                sum += at(from + k * beat + s * step);
            }
            score[s + steps] = sum;
            total += sum;
        }
        const auto best = std::max_element(score.begin(), score.end());
        const double mean = total / score.size();
        if (*best <= 0.0 || *best < 2.0 * mean) {
            continue;   // no beat stands out: a breakdown, silence
        }
        const long i = best - score.begin();
        double frac = 0.0;
        if (i > 0 && i + 1 < long(score.size())) {
            const double l = score[i - 1], c = score[i], r = score[i + 1];
            const double den = l - 2.0 * c + r;
            frac = den < 0.0 ? std::clamp(0.5 * (l - r) / den, -0.5, 0.5) : 0.0;
        }
        if (i == 0 || i + 1 == long(score.size())) {
            continue;   // the best shift is at the edge: not this grid's beat
        }
        MixHouseBarOffset o;
        o.bar = b;
        o.barSec = barOneSec + b * bar;
        o.offsetSec = (double(i - steps) + frac) * step;
        o.hits = static_cast<int>(std::lround(*best / mean * 10.0));   // contrast x10
        out.push_back(o);
    }
    return out;
}

MixHouseGridRefinement refineConstantGrid(const MixHouseOnsetCurve& curve, double durationSec, double bpm,
                                          double barOneSec, int beatsPerBar)
{
    // A song whose bars sit further than this from one straight line is not
    // steady enough to move (it needs a beat map, not a better constant).
    constexpr double kMaxSpreadMs = 8.0;
    constexpr size_t kMinBars = 16;
    constexpr double kMaxBpmChange = 0.1;   // the start grid's tempo is right to this
    MixHouseGridRefinement out;
    out.bpm = bpm;
    out.barOneSec = barOneSec;
    auto measure = [&](double b, double one) {
        return measureBarPhase(curve, durationSec, b, one, beatsPerBar);
    };
    std::vector<MixHouseBarOffset> bars = measure(bpm, barOneSec);
    out.before = summarizeBarOffsets(bars);
    out.after = out.before;
    if (bars.size() < kMinBars || out.before.spreadMs > kMaxSpreadMs) {
        return out;
    }
    double newBpm = bpm, newOne = barOneSec;
    for (int pass = 0; pass < 2; ++pass) {
        // Straight line offset(t) = a + b * t (seconds) through the bars
        // near the median (fills, stray hits left out).
        std::vector<double> ms;
        for (const MixHouseBarOffset& o : bars) {
            ms.push_back(o.offsetSec * 1000.0);
        }
        const double mid = median(ms);
        std::vector<double> dev;
        for (double v : ms) {
            dev.push_back(std::abs(v - mid));
        }
        const double cut = std::max(4.0, 3.0 * median(dev));
        double n = 0, st = 0, so = 0, stt = 0, sto = 0;
        for (const MixHouseBarOffset& o : bars) {
            if (std::abs(o.offsetSec * 1000.0 - mid) > cut) {
                continue;
            }
            n += 1;
            st += o.barSec;
            so += o.offsetSec;
            stt += o.barSec * o.barSec;
            sto += o.barSec * o.offsetSec;
        }
        if (n < kMinBars) {
            return out;
        }
        const double vtt = stt / n - (st / n) * (st / n);
        const double b = vtt > 1e-9 ? (sto / n - (st / n) * (so / n)) / vtt : 0.0;
        const double a = so / n - b * st / n;
        // Every grid line t moves to t + a + b t: the bar-1 line moves by
        // a + b * one, and the beat stretches by (1 + b).
        newOne = newOne + a + b * newOne;
        newBpm = newBpm / (1.0 + b);
        bars = measure(newBpm, newOne);
        if (bars.size() < kMinBars) {
            return out;
        }
    }
    const double beat = 60.0 / bpm;
    const double moved = std::remainder(newOne - barOneSec, beat);
    if (std::abs(newBpm - bpm) > kMaxBpmChange || std::abs(moved) > beat / 5.0) {
        return out;
    }
    const MixHouseGridFit after = summarizeBarOffsets(bars);
    if (after.spreadMs > out.before.spreadMs + 0.5) {
        return out;   // no better: keep what came in
    }
    out.changed = true;
    out.bpm = newBpm;
    out.barOneSec = newOne;
    out.after = after;
    return out;
}

std::vector<double> beatMapFromTrackerBeats(const std::vector<float>& beats, const MixHouseOnsetCurve& curve,
                                            double durationSec, int stages)
{
    std::vector<double> t(beats.begin(), beats.end());
    std::sort(t.begin(), t.end());
    if (t.size() < 8) {
        return t;
    }
    // The typical beat gap around beat i (median of up to 16 gaps nearby).
    auto localGap = [](const std::vector<double>& b, size_t i) {
        std::vector<double> g;
        const size_t lo = i > 8 ? i - 8 : 0, hi = std::min(b.size() - 1, i + 8);
        for (size_t k = lo; k < hi; ++k) {
            g.push_back(b[k + 1] - b[k]);
        }
        return g.empty() ? 0.5 : median(g);
    };
    if (stages & 1) {
        // Doubled beats: a gap under half the local one -> drop the later beat.
        std::vector<double> kept { t.front() };
        for (size_t i = 1; i < t.size(); ++i) {
            if (t[i] - kept.back() >= 0.5 * localGap(t, i)) {
                kept.push_back(t[i]);
            }
        }
        // Missed beats: a gap of about k local gaps -> k-1 beats in between
        // (a breakdown without beats too, up to 16 bars).
        std::vector<double> filled { kept.front() };
        for (size_t i = 1; i < kept.size(); ++i) {
            const double gap = kept[i] - kept[i - 1];
            const double local = localGap(kept, i);
            const double k = std::round(gap / local);
            if (k >= 2.0 && k <= 64.0 && std::abs(gap / local - k) < 0.3) {
                for (int q = 1; q < int(k); ++q) {
                    filled.push_back(kept[i - 1] + gap * q / k);
                }
            }
            filled.push_back(kept[i]);
        }
        t = std::move(filled);
    }
    if ((stages & 2) && curve.hopSec > 0.0 && !curve.strength.empty()) {
        // Each beat onto the strongest onset within +-35 ms, weighted towards
        // where the tracker put it (a Gaussian, 15 ms); nothing there: stays.
        const long w = std::lround(0.035 / curve.hopSec);
        std::vector<double> snapped(t.size());
        for (size_t i = 0; i < t.size(); ++i) {
            const double x = (t[i] - curve.startSec) / curve.hopSec;
            const long c = std::lround(x);
            double best = 0.0;
            long at = -1;
            for (long j = std::max(1L, c - w); j <= std::min(long(curve.strength.size()) - 2, c + w); ++j) {
                const double d = (double(j) - x) * curve.hopSec / 0.015;
                const double v = curve.strength[size_t(j)] * std::exp(-0.5 * d * d);
                if (v > best && curve.strength[size_t(j)] >= curve.strength[size_t(j - 1)]
                    && curve.strength[size_t(j)] >= curve.strength[size_t(j + 1)]) {
                    best = v;
                    at = j;
                }
            }
            snapped[i] = t[i];
            if (at > 0) {
                const double l = curve.strength[size_t(at - 1)], m = curve.strength[size_t(at)],
                             r = curve.strength[size_t(at + 1)];
                const double den = l - 2.0 * m + r;
                const double frac = den < 0.0 ? std::clamp(0.5 * (l - r) / den, -0.5, 0.5) : 0.0;
                snapped[i] = curve.startSec + (double(at) + frac) * curve.hopSec;
            }
        }
        t = std::move(snapped);
    }
    if (stages & 4) {
        // A straight line through the 9 beats around each beat (least
        // squares over beat number), robust: the 2 farthest left out.
        std::vector<double> smooth(t.size());
        for (size_t i = 0; i < t.size(); ++i) {
            const size_t lo = i >= 4 ? i - 4 : 0, hi = std::min(t.size() - 1, i + 4);
            auto fit = [&](const std::vector<size_t>& use) {
                double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
                for (size_t k : use) {
                    const double x = double(k) - double(i);
                    n += 1; sx += x; sy += t[k]; sxx += x * x; sxy += x * t[k];
                }
                const double d = n * sxx - sx * sx;
                const double b = std::abs(d) > 1e-12 ? (n * sxy - sx * sy) / d : 0.0;
                const double a = (sy - b * sx) / n;
                return std::pair<double, double>(a, b);
            };
            std::vector<size_t> use;
            for (size_t k = lo; k <= hi; ++k) {
                use.push_back(k);
            }
            auto [a, b] = fit(use);
            if (use.size() >= 7) {
                std::sort(use.begin(), use.end(), [&](size_t p, size_t q) {
                    return std::abs(t[p] - (a + b * (double(p) - double(i)))) < std::abs(t[q] - (a + b * (double(q) - double(i))));
                });
                use.resize(use.size() - 2);
                std::tie(a, b) = fit(use);
            }
            smooth[i] = a;
        }
        t = std::move(smooth);
    }
    std::vector<double> out;
    for (double v : t) {
        if (v >= 0.0 && v < durationSec && (out.empty() || v > out.back() + 1e-3)) {
            out.push_back(v);
        }
    }
    return out;
}
}

