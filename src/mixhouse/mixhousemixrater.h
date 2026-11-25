/*
 * GlideMix
 */
#pragma once

#include <vector>

#include <QString>
#include <QStringList>

namespace au::mixhouse {
/// GlideMix 1.3.8 "Rate mix" (Chris, 2026-11-16): how well each song of a
/// playlist flows into the next one, from the DJ practice gathered in
/// docs/research/auto-anchors.md section 2 -- compatible keys, a small tempo
/// change, energy that flows, an outro and an intro to blend over, no vocals at
/// both ends. Only what the analysis already stores: no audio is read.
struct MixRateSong {
    double bpm = 0.0;
    int keyId = 0;          ///< Mixxx ChromaticKey id, 0 = unknown
    double energy = 0.0;    ///< 1-10, 0 = unknown
    double durationSec = 0.0;
    /// Phrase labels from the start of the song and from its end (first two and
    /// last two), e.g. "Intro", "Verse 1"; empty = not analysed.
    QStringList firstPhrases;
    QStringList lastPhrases;
    double introSec = 0.0;  ///< length of the leading Intro phrases (0 = none)
    double outroSec = 0.0;  ///< length of the trailing Outro / Down phrases (0 = none)
};

struct MixRating {
    double stars = -1.0;    ///< 1-5 in half stars; -1 = not rated (last song, nothing known)
    QStringList why;        ///< short reasons, worst first
};

/// The transition from `a` into `b`. `maxKeyShift`: what Match keys may move
/// (a clash it can fix costs less).
MixRating rateTransition(const MixRateSong& a, const MixRateSong& b, int maxKeyShift);

/// Rates every song into the next, in order; the last one gets stars -1.
std::vector<MixRating> rateMix(const std::vector<MixRateSong>& songs, int maxKeyShift);
}
