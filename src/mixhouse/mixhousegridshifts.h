/*
 * GlideMix
 *
 * Grid jumps: places where a song's beats move off its (constant) beat grid
 * and stay off -- a demo stitched from loops, an edit, a live recording that
 * was cut. One grid fits only one side of the jump, so from there on the
 * metronome and the other songs flam against the kicks.
 */
#pragma once

#include <cstddef>
#include <vector>

namespace au::mixhouse {
struct MixHouseGridShift {
    double startSec = 0.0;   ///< file time of the grid bar line where the new section starts
    double shiftSec = 0.0;   ///< from there on the beats are this much late (negative: early), against the grid
};

/// Kick onsets (below 150 Hz) of `mono` compared with the grid `bpm` /
/// `barOneSec` (any bar 1, file seconds), one median per bar, then fitted
/// with as few constant pieces as explain them. A piece of at least
/// kMinShiftBars bars that sits steadily (within a few ms) at least 20 ms
/// away from the section before it is a jump. The offsets are relative to
/// the first steady section, which is where the grid itself fits.
std::vector<MixHouseGridShift> detectGridShifts(const float* mono, size_t frames, int sampleRate,
                                                double bpm, double barOneSec, int beatsPerBar = 4);

/// Beat onsets of `mono` in file seconds: spectral-flux peaks of the whole
/// band at about 11 kHz, at least 100 ms apart. Shifted by a constant
/// (kOnsetBiasSec, measured on clicks) so a sharp hit counts at its start.
std::vector<double> detectBeatOnsets(const float* mono, size_t frames, int sampleRate);

/// How one bar's hits sit against the grid: the median of (onset - nearest
/// grid beat) over the onsets within a sixth of a beat of a beat.
struct MixHouseBarOffset {
    long bar = 0;              ///< bar number counted from the grid's bar 1 (0 = bar 1, may be negative)
    double barSec = 0.0;       ///< file time of the bar line
    double offsetSec = 0.0;    ///< late (+) or early (-) against the grid
    int hits = 0;              ///< onsets that voted
};

/// One entry per bar that has at least two hits near its beats.
std::vector<MixHouseBarOffset> measureBarOffsets(const std::vector<double>& onsets, double durationSec,
                                                 double bpm, double barOneSec, int beatsPerBar = 4);

/// A song's grid in a few numbers (milliseconds), from its bar offsets.
struct MixHouseGridFit {
    int bars = 0;              ///< bars with enough hits
    double medianMs = 0.0;     ///< typical offset: the whole grid this much early (+) / late (-) against the hits
    double spreadMs = 0.0;     ///< median distance of a bar from medianMs (how steady the song is on its grid)
    double driftMsPerMin = 0.0;///< slope of a straight line through the offsets: a tempo a hair off
    double startMs = 0.0;      ///< that line at the first measured bar ...
    double endMs = 0.0;        ///< ... and at the last: how far the grid wanders over the song
    double worstWindowMs = 0.0;///< largest |median of 16 bars - medianMs|: a bow or a section off the grid
};
MixHouseGridFit summarizeBarOffsets(const std::vector<MixHouseBarOffset>& offsets);

/// The onset strength over time (spectral flux minus its local average),
/// for measureBarPhase. strength[j] sits at startSec + j * hopSec.
struct MixHouseOnsetCurve {
    double startSec = 0.0;
    double hopSec = 0.0;
    std::vector<float> strength;
};
MixHouseOnsetCurve detectOnsetCurve(const float* mono, size_t frames, int sampleRate);

/// Like measureBarOffsets, but sturdier on syncopated or live music: for each
/// bar, the shift of the grid's beats (that bar and the ones either side)
/// that catches the most onset strength. `hits` is the contrast x10 (best
/// shift against the average one). Bars where no shift stands out are left out.
std::vector<MixHouseBarOffset> measureBarPhase(const MixHouseOnsetCurve& curve, double durationSec, double bpm,
                                               double barOneSec, int beatsPerBar = 4);

/// A constant grid moved onto the song's hits: per bar, how far the hits sit
/// from the grid (measureBarPhase); a straight line through those offsets
/// gives the grid's phase error (the line's height) and tempo error (its
/// slope), and the grid is corrected by both; twice, as the bars measured
/// change with the grid. Only for steady songs: if the bars scatter more than
/// a few ms around the line (a drifting or live song) the grid is returned
/// unchanged, as it is if the result would move it far.
struct MixHouseGridRefinement {
    bool changed = false;
    double bpm = 0.0;
    double barOneSec = 0.0;
    MixHouseGridFit before;   ///< the grid as given, against the hits
    MixHouseGridFit after;    ///< the refined grid (== before when unchanged)
};
MixHouseGridRefinement refineConstantGrid(const MixHouseOnsetCurve& curve, double durationSec, double bpm,
                                          double barOneSec, int beatsPerBar = 4);


/// A beat map from a beat tracker's beats (Beat This!: every beat, in 20 ms
/// frames, with the odd one missed or doubled): doubled beats dropped, missed
/// ones (and beatless stretches) filled at the local tempo, each beat moved
/// onto the onset it belongs to, then smoothed over about two bars so tempo
/// changes stay and jitter goes. `stages` (bits, for measuring): 1 clean,
/// 2 snap, 4 smooth; 7 = all.
std::vector<double> beatMapFromTrackerBeats(const std::vector<float>& beats, const MixHouseOnsetCurve& curve,
                                            double durationSec, int stages = 7);
}

