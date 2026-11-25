/*
 * GlideMix
 */
#pragma once

#include <QColor>
#include <QString>

#include "async/notification.h"
#include "modularity/imoduleinterface.h"
#include "trackedit/trackedittypes.h"

#include "imixhouselibraryconfiguration.h"

namespace au::mixhouse {
/// Match Key, from the parked MixHouse DJ app's clip menu: transpose a clip
/// toward the key of the track it overlaps, the shortest way to a harmonically
/// compatible key (Mixxx's KeyUtils::shortestStepsToCompatibleKey). Keys are
/// Mixxx ChromaticKey ids (1..12 major C..B, 13..24 minor C..B), shown in
/// Camelot notation.
class IMixHouseKeyMatch : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IMixHouseKeyMatch)

public:
    virtual ~IMixHouseKeyMatch() = default;

    struct ClipKeyInfo {
        int keyId = 0;             ///< analysed key of the library track, 0 = unknown
        int shiftSemitones = 0;    ///< the clip's current transpose, rounded
        int effectiveKeyId = 0;    ///< the key it sounds in now
        /// Keylock off: the speed moves the pitch too (a song sped up 6 % sounds
        /// ~1 semitone higher). Semitones added by the speed, rounded together
        /// with the transpose; 0 while Keylock is on. Measured where the song
        /// mixes in (the middle of its transition), else at its middle.
        int stretchSteps = 0;
    };

    struct Match {
        bool hasPartner = false;   ///< overlaps a clip on another track
        QString partnerTitle;
        int myKeyId = 0;           ///< this clip's analysed (untransposed) key
        int partnerKeyId = 0;      ///< the key the partner sounds in
        int steps = 0;             ///< transpose (from the analysed key) that matches
    };

    virtual ClipKeyInfo keyInfo(const trackedit::ClipKey& clip) const = 0;
    virtual Match match(const trackedit::ClipKey& clip) const = 0;

    /// Sets the clip's transpose to `semitones` (0 resets). One undo step.
    virtual bool transpose(const trackedit::ClipKey& clip, int semitones) = 0;

    /// The sounding keys may have changed without a transpose: Keylock was
    /// switched, or (Keylock off) the tempo line or a song's stretch moved.
    virtual muse::async::Notification keysChanged() const = 0;

    /// Match keys (Chris, 2026-11-14/16), the key twin of Follow tracks: on, an
    /// incoming song whose key clashes with the song it mixes out of is
    /// transposed the shortest way to a compatible key, if that is at most
    /// matchKeysMaxSemitones(); else it keeps its key. A song transposed by
    /// hand is never touched; turning it off puts the auto-matched songs back.
    /// Per project (saved as `glidemix_keys`), off by default.
    virtual bool matchKeysOn() const = 0;
    virtual void setMatchKeysOn(bool on) = 0;
    /// Off, and a song just placed clashes in a way Match keys would fix: the
    /// buttons flash for a few seconds (never switched on by itself).
    virtual bool clashHint() const = 0;
    /// matchKeysOn() or clashHint() changed.
    virtual muse::async::Notification matchKeysChanged() const = 0;

    static QString camelot(int keyId);
    /// "1d" / "1m".
    static QString openKey(int keyId);
    /// "C", "F#m" -- Mixxx's spellings.
    static QString traditional(int keyId);
    /// The key as the user chose to see keys (Preferences > Music library).
    static QString keyText(int keyId, MixHouseKeyNotation notation);
    /// Mixed In Key's key colours (Mixxx's kMIKKeyColor1..12), by key id;
    /// invalid QColor for an unknown key.
    static QColor keyColor(int keyId);
    /// The same palette by Open Key number (1..12).
    static QColor openKeyColor(int openKeyNumber);
    static int scaleKeySteps(int keyId, int steps);
    static int shortestStepsToCompatibleKey(int keyId, int targetKeyId);
};
}
