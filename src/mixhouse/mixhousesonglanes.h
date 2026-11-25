/*
 * GlideMix Samples: the lanes that hold songs -- every lane but the sample
 * lines. Songs, transitions, the tempo line, the song counter and row picking
 * look at these only: a sample is never taken for a song.
 */
#pragma once

#include <cmath>
#include <vector>

#include "trackedit/itrackeditproject.h"

namespace au::mixhouse {
inline std::vector<trackedit::TrackId> songTrackIds(const trackedit::ITrackeditProject& project)
{
    std::vector<trackedit::TrackId> out;
    for (const trackedit::Track& track : project.trackList()) {
        if (!trackedit::isSampleLine(track)) {
            out.push_back(track.id);
        }
    }
    return out;
}

/// A stem lane made by Separate stems ("Drums — <song>"): part of its song,
/// not a song of its own (the SONG counter skips it).
inline bool isStemLane(const trackedit::Track& track)
{
    static const char16_t* kPrefixes[] = { u"Drums \u2014 ", u"Bass \u2014 ", u"Vocals \u2014 ", u"Other \u2014 " };
    for (const char16_t* prefix : kPrefixes) {
        if (track.title.startsWith(muse::String(prefix))) {
            return true;
        }
    }
    return false;
}

/// The lanes the SONG counter counts: songs only, no sample lines, no stems.
inline std::vector<trackedit::TrackId> countedSongTrackIds(const trackedit::ITrackeditProject& project)
{
    std::vector<trackedit::TrackId> out;
    for (const trackedit::Track& track : project.trackList()) {
        if (!trackedit::isSampleLine(track) && !isStemLane(track)) {
            out.push_back(track.id);
        }
    }
    return out;
}

/// A piece of a song that carries straight on from an earlier piece of the
/// same song on the same lane (it starts where that one ends): the rest of a
/// song after a Cmd+D loop or a split. It was already playing, so it is not a
/// new song and never comes IN as a transition (Chris, 2026-11-16: a loop
/// inside a transition gave a BEATMATCH pill and "6 songs" for 4).
inline bool isContinuationPiece(const trackedit::Clip& clip, const std::vector<trackedit::Clip>& clips)
{
    constexpr double kTouchSec = 1e-3;
    for (const trackedit::Clip& other : clips) {
        if (!(other.key == clip.key) && other.key.trackId == clip.key.trackId && other.title == clip.title
            && other.startTime < clip.startTime && std::abs(other.endTime - clip.startTime) < kTouchSec) {
            return true;
        }
    }
    return false;
}
}
