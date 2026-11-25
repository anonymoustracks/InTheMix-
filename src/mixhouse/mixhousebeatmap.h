/*
 * GlideMix
 *
 * Beat maps: a song's beats one by one, for songs whose tempo moves (live
 * drummers, old records, tempo changes) where one BPM and a first beat
 * cannot follow the music. Other apps call this a fluid, dynamic or
 * variable grid (Serato 5's Tempo Map, Lexicon's per-beat markers,
 * rekordbox's Dynamic analysis).
 *
 * The constant grid (mixhousebeats.h, library.beats) stays what the timeline
 * uses; beat maps are kept next to it (library table mixhouse_beatmaps), one
 * per source, so the apps can be compared and the best one chosen later.
 */
#pragma once

#include <QByteArray>
#include <QString>

#include <vector>

namespace au::mixhouse {
struct MixHouseBeatMap {
    QString source;                  ///< "Serato", "Lexicon", "rekordbox", "BeatThis-Map-1", ...
    std::vector<double> beatSecs;    ///< every beat, file seconds (our decoder's clock), strictly increasing

    bool valid() const { return beatSecs.size() >= 2; }
    /// Fractional beat number at `sec` (0 = the first beat); straight lines
    /// between beats, the end segments' tempo beyond them.
    double beatAt(double sec) const;
    /// File seconds of beat number `beat` (fractional allowed).
    double secAt(double beat) const;
    /// The tempo around `sec`, from the gap between the two beats around it.
    double bpmAt(double sec) const;
};

/// A constant grid that fits a beat map best (least squares over every beat).
/// `spreadMs` is the typical distance of a beat from that line (median); a
/// small spread = the map is really constant, a large one = it bends.
struct MixHouseBeatMapFit {
    double bpm = 0.0;
    double firstBeatSec = 0.0;
    double spreadMs = 0.0;
    double worstMs = 0.0;   ///< largest distance of a beat from the line
};
MixHouseBeatMapFit fitConstant(const MixHouseBeatMap& map);

/// Tempo segments, the way Serato 5 stores them: each starts at a beat
/// number and keeps its BPM until the next one. The first beat is at
/// `firstBeatSec`; beats run until `durationSec`.
struct MixHouseTempoSegment {
    double startBeat = 0.0;
    double bpm = 0.0;
};
MixHouseBeatMap beatMapFromSegments(double firstBeatSec, const std::vector<MixHouseTempoSegment>& segments,
                                    double durationSec);

/// Tempo markers (a time and a BPM), the way Lexicon and Traktor store them:
/// a marker is a beat; between two markers lie a whole number of beats at
/// about the first one's BPM (the BPM shown is rounded, the times are not).
/// After the last marker, beats continue at its BPM until `durationSec`.
struct MixHouseTempoMarker {
    double sec = 0.0;
    double bpm = 0.0;
};
MixHouseBeatMap beatMapFromMarkers(const std::vector<MixHouseTempoMarker>& markers, double durationSec);

/// Serato 5's "Serato Tempo Map" tag (ID3 GEOB / the same bytes in other
/// formats): first beat + tempo segments. Times are Serato's clock, before any
/// decoder offset. Empty map when the bytes aren't one.
MixHouseBeatMap seratoTempoMapToBeatMap(const QByteArray& body, double durationSec);

/// mp3guessenc's timing case of an MP3 (EXIT_CODE_CASE_A..D = 1..4; 0 = not
/// an MP3 or unknown). mp3guessenc keeps global state: every caller goes
/// through this one lock.
int mp3TimingShiftCase(const QString& filePath);

/// How much later than our decoder Serato's clock runs for this file, in
/// seconds: 2257 samples (the encoder delay plus one frame) for an MP3 with an
/// Info/Xing header -- measured +51.2 ms on 398 library songs with Serato
/// grids against Lexicon's, cases B and D alike (2026-11-12); 0 otherwise
/// (MP3s without the header: 2 songs measured; other formats: not measured).
double seratoClockOffsetSec(const QString& filePath, int sampleRate);

/// The Serato 5 beat map in a file's "Serato Tempo Map" tag bytes, moved onto
/// our decoder's clock. Empty when there is none.
MixHouseBeatMap seratoBeatMap(const QString& filePath, const QByteArray& tempoMapTag, double durationSec, int sampleRate);

/// Storage blob: little-endian doubles, one per beat.
QByteArray encodeBeatMap(const MixHouseBeatMap& map);
MixHouseBeatMap decodeBeatMap(const QByteArray& blob, const QString& source);
}
