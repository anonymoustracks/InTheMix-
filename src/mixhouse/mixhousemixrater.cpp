/*
 * GlideMix
 */
#include "mixhousemixrater.h"

#include <algorithm>
#include <cmath>

#include "imixhousekeymatch.h"

using namespace au::mixhouse;

namespace {
bool hasVocals(const QString& label)
{
    // GlideMix's phrase names: Verse and Chorus usually carry the vocals.
    return label.startsWith(QLatin1String("Verse")) || label.startsWith(QLatin1String("Chorus"));
}

struct Penalty {
    double stars = 0.0;
    QString why;
};
}

MixRating au::mixhouse::rateTransition(const MixRateSong& a, const MixRateSong& b, int maxKeyShift)
{
    MixRating r;
    if (a.bpm <= 0.0 || b.bpm <= 0.0) {
        r.why << QStringLiteral("not analysed");
        return r;   // nothing to judge by: no stars rather than wrong ones
    }
    std::vector<Penalty> penalties;
    std::vector<QString> good;

    // Keys (DJ practice 7: long blends need matching keys).
    if (a.keyId > 0 && b.keyId > 0) {
        const int steps = IMixHouseKeyMatch::shortestStepsToCompatibleKey(b.keyId, a.keyId);
        if (steps == 0) {
            good.push_back(QStringLiteral("keys match"));
        } else if (std::abs(steps) <= maxKeyShift) {
            penalties.push_back({ 0.5, QStringLiteral("keys clash (Match keys: %1%2 st)").arg(steps > 0 ? "+" : "").arg(steps) });
        } else {
            penalties.push_back({ 2.0, QStringLiteral("keys clash") });
        }
    }

    // Tempo: the change the next song needs (half / double time counts as the same).
    double ratio = b.bpm / a.bpm;
    while (ratio > 1.5) {
        ratio /= 2.0;
    }
    while (ratio < 0.75) {
        ratio *= 2.0;
    }
    const double pct = (ratio - 1.0) * 100.0;
    const QString tempo = QStringLiteral("%1%2 % tempo").arg(pct > 0 ? "+" : "").arg(std::lround(pct));
    // Real mixes: 86 % of songs change tempo by less than 5 %, 95 % by less than 10 % (Kim et al.).
    if (std::abs(pct) > 10.0) {
        penalties.push_back({ 2.0, tempo });
    } else if (std::abs(pct) > 6.0) {
        penalties.push_back({ 1.0, tempo });
    } else if (std::abs(pct) > 3.0) {
        penalties.push_back({ 0.5, tempo });
    }

    // Energy flows: no big drop, no accidental jump (a lift of 1-2 is fine).
    if (a.energy > 0.0 && b.energy > 0.0) {
        const double d = b.energy - a.energy;
        if (d <= -3.0) {
            penalties.push_back({ 1.0, QStringLiteral("energy drops %1").arg(std::lround(-d)) });
        } else if (d >= 4.0) {
            penalties.push_back({ 1.0, QStringLiteral("energy jumps +%1").arg(std::lround(d)) });
        } else if (d <= -2.0) {
            penalties.push_back({ 0.5, QStringLiteral("energy drops %1").arg(std::lround(-d)) });
        }
    }

    // Something to blend over: an outro on A, an intro on B (16 bars or more).
    const double barA = 240.0 / a.bpm;
    const double barB = 240.0 / b.bpm;
    const bool analysed = !a.lastPhrases.isEmpty() && !b.firstPhrases.isEmpty();
    if (analysed) {
        const bool outro = a.outroSec >= 15.5 * barA;
        const bool intro = b.introSec >= 15.5 * barB;
        if (!outro && !intro) {
            penalties.push_back({ 1.0, QStringLiteral("no outro or intro: short blend") });
        } else if (!outro) {
            penalties.push_back({ 0.5, QStringLiteral("no outro") });
        } else if (!intro) {
            penalties.push_back({ 0.5, QStringLiteral("no intro") });
        } else {
            good.push_back(QStringLiteral("outro into intro"));
        }
        // Never two vocals at once: vocals at the end of A and at the start of B.
        if (!a.lastPhrases.isEmpty() && !b.firstPhrases.isEmpty() && hasVocals(a.lastPhrases.last())
            && hasVocals(b.firstPhrases.first())) {
            penalties.push_back({ 0.5, QStringLiteral("vocals at both ends") });
        }
    }

    double stars = 5.0;
    for (const Penalty& p : penalties) {
        stars -= p.stars;
    }
    r.stars = std::clamp(std::round(stars * 2.0) / 2.0, 1.0, 5.0);
    std::stable_sort(penalties.begin(), penalties.end(), [](const Penalty& x, const Penalty& y) { return x.stars > y.stars; });
    for (const Penalty& p : penalties) {
        r.why << p.why;
    }
    if (penalties.empty()) {
        for (const QString& g : good) {
            r.why << g;
        }
    }
    return r;
}

std::vector<MixRating> au::mixhouse::rateMix(const std::vector<MixRateSong>& songs, int maxKeyShift)
{
    std::vector<MixRating> out(songs.size());
    for (size_t i = 0; i + 1 < songs.size(); ++i) {
        out[i] = rateTransition(songs[i], songs[i + 1], maxKeyShift);
    }
    return out;
}
