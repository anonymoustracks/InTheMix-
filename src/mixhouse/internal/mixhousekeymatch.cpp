/*
 * GlideMix
 */
#include "mixhousekeymatch.h"

#include <algorithm>
#include <cmath>

#include <QJsonDocument>
#include <QJsonObject>

#include "au3-mixtape-engine/MixtapeArrangement.h"
#include "au3-wave-track/WaveClip.h"
#include "project/iaudacityproject.h"
#include "trackedit/itrackeditproject.h"

#include "mixhousekeypersist.h"

using namespace au::mixhouse;

namespace {
// Mixxx's ChromaticKey ids (proto/keys.proto): 1..12 = C..B major,
// 13..24 = C..B minor.
bool keyIsMajor(int key) { return key > 0 && key < 13; }
int keyToTonic(int key) { return key - (keyIsMajor(key) ? 1 : 13); }
int tonicToKey(int tonic, bool major) { return tonic + (major ? 1 : 13); }
bool keyIsValid(int key) { return key >= 1 && key <= 24; }

// KeyUtils' s_openKeyToKeys: OpenKey number -> {major, minor}.
constexpr int kOpenKeyToKeys[13][2] = {
    { 0, 0 },
    { 1, 22 },   // 1: C major, A minor
    { 8, 17 },   // 2: G, Em
    { 3, 24 },   // 3: D, Bm
    { 10, 19 },  // 4: A, F#m
    { 5, 14 },   // 5: E, C#m
    { 12, 21 },  // 6: B, G#m
    { 7, 16 },   // 7: F#, Ebm
    { 2, 23 },   // 8: Db, Bbm
    { 9, 18 },   // 9: Ab, Fm
    { 4, 13 },   // 10: Eb, Cm
    { 11, 20 },  // 11: Bb, Gm
    { 6, 15 },   // 12: F, Dm
};

int keyToOpenKeyNumber(int key)
{
    for (int n = 1; n <= 12; ++n) {
        if (kOpenKeyToKeys[n][0] == key || kOpenKeyToKeys[n][1] == key) {
            return n;
        }
    }
    return 0;
}

int shortestStepsToKey(int key, int target)
{
    if (!keyIsValid(key) || !keyIsValid(target)) {
        return 0;
    }
    int steps = keyToTonic(target) - keyToTonic(key);
    if (steps > 6) {
        steps -= 12;
    } else if (steps < -6) {
        steps += 12;
    }
    return steps;
}
}

QColor IMixHouseKeyMatch::openKeyColor(int openKeyNumber)
{
    // Copied from mixxx/src/util/color/predefinedcolorpalettes.cpp (kMIKKeyColor1..12),
    // indexed by Open Key number as KeyUtils::keyToColor does.
    static const QColor kMikKeyColors[12] = {
        QColor(0xF1, 0x7E, 0xDB), QColor(0xD1, 0x8B, 0xFD), QColor(0x9E, 0xB4, 0xFD),
        QColor(0x4D, 0xD3, 0xF8), QColor(0x01, 0xEA, 0xEC), QColor(0x00, 0xEE, 0xCB),
        QColor(0x20, 0xEF, 0x7F), QColor(0x7F, 0xF4, 0x48), QColor(0xE0, 0xCA, 0x6D),
        QColor(0xFD, 0xA0, 0x78), QColor(0xFF, 0x86, 0x93), QColor(0xFD, 0x7E, 0xB3)
    };
    return (openKeyNumber >= 1 && openKeyNumber <= 12) ? kMikKeyColors[openKeyNumber - 1] : QColor();
}

QColor IMixHouseKeyMatch::keyColor(int key)
{
    return keyIsValid(key) ? openKeyColor(keyToOpenKeyNumber(key)) : QColor();
}

QString IMixHouseKeyMatch::openKey(int key)
{
    if (!keyIsValid(key)) {
        return QString();
    }
    return QString::number(keyToOpenKeyNumber(key)) + (keyIsMajor(key) ? QStringLiteral("d") : QStringLiteral("m"));
}

QString IMixHouseKeyMatch::traditional(int key)
{
    static const char* const names[25] = {
        "",
        "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
        "Cm", "C#m", "Dm", "Ebm", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "Bbm", "Bm"
    };
    return keyIsValid(key) ? QString::fromLatin1(names[key]) : QString();
}

QString IMixHouseKeyMatch::keyText(int key, MixHouseKeyNotation notation)
{
    if (!keyIsValid(key)) {
        return QString();
    }
    switch (notation) {
    case MixHouseKeyNotation::Lancelot: return camelot(key);
    case MixHouseKeyNotation::LancelotTraditional: return camelot(key) + QLatin1Char('/') + traditional(key);
    case MixHouseKeyNotation::OpenKey: return openKey(key);
    case MixHouseKeyNotation::OpenKeyTraditional: return openKey(key) + QLatin1Char('/') + traditional(key);
    case MixHouseKeyNotation::Traditional: return traditional(key);
    }
    return camelot(key);
}

QString IMixHouseKeyMatch::camelot(int key)
{
    if (!keyIsValid(key)) {
        return QString();
    }
    // Lancelot is OpenKey rotated counter-clockwise by 5 (KeyUtils).
    int number = keyToOpenKeyNumber(key) - 5;
    if (number < 1) {
        number += 12;
    }
    return QString::number(number) + (keyIsMajor(key) ? QStringLiteral("B") : QStringLiteral("A"));
}

int IMixHouseKeyMatch::scaleKeySteps(int key, int steps)
{
    if (!keyIsValid(key)) {
        return 0;
    }
    int tonic = (keyToTonic(key) + steps) % 12;
    if (tonic < 0) {
        tonic += 12;
    }
    return tonicToKey(tonic, keyIsMajor(key));
}

int IMixHouseKeyMatch::shortestStepsToCompatibleKey(int key, int target)
{
    // KeyUtils::shortestStepsToCompatibleKey, line for line.
    if (!keyIsValid(key) || !keyIsValid(target) || key == target) {
        return 0;
    }
    // A mode mismatch converts to the relative key first: same notes, so no
    // up-to-6-semitone chipmunk shift to reach the other mode.
    if (keyIsMajor(key) != keyIsMajor(target)) {
        const int openKey = keyToOpenKeyNumber(target);
        target = kOpenKeyToKeys[openKey][keyIsMajor(target) ? 1 : 0];
    }
    // Compatible keys are 0 (tonic) or +-5 (4th / 5th) semitones away.
    const int distance = shortestStepsToKey(key, target);
    if (distance < -2) {
        return 5 + distance;
    }
    if (distance > 2) {
        return -5 + distance;
    }
    return distance;
}

MixHouseKeyMatch::MixHouseKeyMatch(const muse::modularity::ContextPtr& iocCtx)
    : muse::Contextable(iocCtx)
{
}

MixHouseKeyMatch::~MixHouseKeyMatch()
{
    mixtape::SetClipKeylockProvider(nullptr);
}

bool MixHouseKeyMatch::autoKeylocked(double unstretchedBpm, double start, double end) const
{
    const int percent = libraryConfiguration() ? libraryConfiguration()->autoKeylockPercent() : 0;
    if (percent <= 0 || unstretchedBpm <= 0.0 || !tempoMap() || end < start) {
        return false;
    }
    // The line's segments run monotonically between its points, so the
    // farthest tempo is at the song's ends or at a point inside it.
    std::vector<double> times { start, end };
    for (const IMixHouseTempoMap::Point& p : tempoMap()->points()) {
        if (p.time > start && p.time < end) {
            times.push_back(p.time);
        }
    }
    const double limit = percent / 100.0;
    for (const double t : times) {
        const double speed = tempoMap()->bpmAt(t) / unstretchedBpm;
        if (speed > 0.0 && std::abs(speed - 1.0) > limit) {
            return true;
        }
    }
    return false;
}

void MixHouseKeyMatch::init()
{
    // keysChanged(): coalesced, a tempo-point drag sends a change per mouse move.
    m_keysTimer.setSingleShot(true);
    m_keysTimer.setInterval(120);
    QObject::connect(&m_keysTimer, &QTimer::timeout, [this]() { m_keysChanged.notify(); });

    m_keylock = libraryConfiguration() ? libraryConfiguration()->keylockEnabled() : true;
    m_autoKeylock = libraryConfiguration() ? libraryConfiguration()->autoKeylockPercent() : 0;
    if (libraryConfiguration()) {
        libraryConfiguration()->settingsChanged().onNotify(this, [this]() {
            const bool keylock = libraryConfiguration()->keylockEnabled();
            const int autoKeylock = libraryConfiguration()->autoKeylockPercent();
            if (keylock != m_keylock || autoKeylock != m_autoKeylock) {
                m_keylock = keylock;
                m_autoKeylock = autoKeylock;
                m_keysTimer.start();
            }
        });
    }
    // Auto-Keylock: the engine asks per clip when it takes a snapshot to play or
    // export (main thread). Decided once for the whole song.
    mixtape::SetClipKeylockProvider([this](const WaveClip& clip) {
        const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
        const double tempo = project ? project->timeSignature().tempo : 0.0;
        return autoKeylocked(tempo * clip.GetStretchRatio(), clip.GetPlayStartTime(), clip.GetPlayEndTime());
    });
    // With Keylock on the speed never moves the key: nothing to tell.
    if (tempoMap()) {
        tempoMap()->tempoMapChanged().onNotify(this, [this]() { keysMayHaveChanged(); });
    }
    if (projectHistory()) {
        projectHistory()->historyChanged().onReceive(this, [this](trackedit::HistoryEvent event) {
            keysMayHaveChanged();
            // Match keys looks again after every edit (a drop, a move), a moment
            // later so the edit is complete; an undo / redo restores a state as
            // it was, so it is left alone.
            if (event == trackedit::HistoryEvent::NewState) {
                m_passTimer.start();
            } else {
                m_passTimer.stop();
            }
        });
    }

    m_passTimer.setSingleShot(true);
    m_passTimer.setInterval(60);
    QObject::connect(&m_passTimer, &QTimer::timeout, [this]() {
        if (projectHistory() && projectHistory()->interactionOngoing()) {
            m_passTimer.start();   // a drag is still going on
            return;
        }
        if (runMatchPass() && projectHistory()) {
            // Part of the edit that caused it: one undo step takes both back.
            projectHistory()->modifyState();
            projectHistory()->markUnsaved();
        }
    });
    m_hintTimer.setSingleShot(true);
    m_hintTimer.setInterval(5000);
    QObject::connect(&m_hintTimer, &QTimer::timeout, [this]() { setHint(false); });

    if (globalContext()) {
        globalContext()->currentProjectChanged().onNotify(this, [this]() { ensureProjectState(); });
    }
}

void MixHouseKeyMatch::ensureProjectState()
{
    const auto project = globalContext() ? globalContext()->currentProject() : nullptr;
    void* au3 = project ? reinterpret_cast<void*>(project->au3ProjectPtr()) : nullptr;
    if (au3 == m_stateFor) {
        return;
    }
    const bool wasOn = m_matchOn;
    const bool hadHint = m_hint;
    m_stateFor = au3;
    m_matchOn = false;
    m_auto.clear();
    m_seenClashes.clear();
    m_hintTimer.stop();
    m_hint = false;
    if (hadHint) {
        m_matchKeysChanged.notify();
    }
    std::string saved;
    if (au3 && loadedKeyState(au3, &saved)) {
        const QJsonObject o = QJsonDocument::fromJson(QByteArray::fromStdString(saved)).object();
        m_matchOn = o.value("on").toBool();
        const QJsonObject songs = o.value("auto").toObject();
        for (auto it = songs.constBegin(); it != songs.constEnd(); ++it) {
            if (it.value().isDouble()) {
                m_auto.insert(it.key(), it.value().toInt());
            }
        }
    }
    if (au3) {
        setKeyStateWriter(au3, [this]() { return savedKeyState(); });
    }
    if (au3 && !m_matchOn) {
        // The clashes already in an opened mix are not news: noted, no flash.
        m_seedClashes = true;
        runMatchPass();
    }
    if (wasOn != m_matchOn) {
        m_matchKeysChanged.notify();
    }
}

std::string MixHouseKeyMatch::savedKeyState() const
{
    if (!m_matchOn && m_auto.isEmpty()) {
        return {};
    }
    QJsonObject songs;
    for (auto it = m_auto.constBegin(); it != m_auto.constEnd(); ++it) {
        songs.insert(it.key(), it.value());
    }
    return QJsonDocument(QJsonObject { { "v", 1 }, { "on", m_matchOn }, { "auto", songs } })
           .toJson(QJsonDocument::Compact).toStdString();
}

bool MixHouseKeyMatch::matchKeysOn() const
{
    return m_matchOn;
}

bool MixHouseKeyMatch::clashHint() const
{
    return m_hint;
}

muse::async::Notification MixHouseKeyMatch::matchKeysChanged() const
{
    return m_matchKeysChanged;
}

void MixHouseKeyMatch::setHint(bool hint)
{
    if (hint == m_hint) {
        return;
    }
    m_hint = hint;
    m_matchKeysChanged.notify();
}

void MixHouseKeyMatch::setMatchKeysOn(bool on)
{
    ensureProjectState();
    if (on == m_matchOn || !m_stateFor) {
        return;
    }
    m_matchOn = on;
    m_hintTimer.stop();
    setHint(false);
    bool changed = false;
    const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    if (on) {
        changed = runMatchPass();
    } else if (project && clipsInteraction()) {
        // Back to their own key -- only the songs Match keys transposed and that
        // still have that transpose (one changed by hand since is left as it is).
        for (const trackedit::TrackId trackId : project->trackIdList()) {
            for (const trackedit::Clip& clip : project->clipList(trackId)) {
                const auto it = m_auto.constFind(clip.title.toQString());
                if (it != m_auto.constEnd() && clip.pitch == it.value() && clip.pitch != 0) {
                    changed = clipsInteraction()->resetClipPitch(clip.key) || changed;
                }
            }
        }
        m_auto.clear();
    }
    LOGI() << "GlideMix Match keys " << (on ? "on" : "off") << (changed ? ", songs transposed" : ", no song changed");
    if (projectHistory()) {
        if (changed) {
            projectHistory()->pushHistoryState(on ? muse::trc("mixhouse", "Matched keys")
                                                  : muse::trc("mixhouse", "Put songs back in their own key"),
                                               on ? muse::trc("mixhouse", "Match keys on")
                                                  : muse::trc("mixhouse", "Match keys off"));
        } else {
            projectHistory()->markUnsaved();
        }
    }
    m_matchKeysChanged.notify();
}

bool MixHouseKeyMatch::runMatchPass()
{
    ensureProjectState();
    const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    if (!project || !transitions() || !m_stateFor) {
        return false;
    }
    const int maxSteps = libraryConfiguration() ? libraryConfiguration()->matchKeysMaxSemitones() : 1;
    std::vector<IMixHouseTransitions::Zone> zones = transitions()->zones();
    std::sort(zones.begin(), zones.end(), [](const auto& a, const auto& b) { return a.start < b.start; });

    bool changed = false;
    QSet<QString> clashes;   // pairs Match keys would fix (for the hint while off)
    QSet<QString> titlesNow;
    for (const trackedit::TrackId trackId : project->trackIdList()) {
        for (const trackedit::Clip& clip : project->clipList(trackId)) {
            titlesNow.insert(clip.title.toQString());
        }
    }
    for (const IMixHouseTransitions::Zone& zone : zones) {
        if (zone.disabled) {
            continue;
        }
        const trackedit::Clip in = project->clip(zone.incoming);   // read now: earlier passes may have changed it
        const trackedit::Clip out = project->clip(zone.outgoing);
        const QString title = in.title.toQString();
        if (!in.isValid() || !out.isValid() || title.isEmpty()) {
            continue;
        }
        const auto autoIt = m_auto.constFind(title);
        const bool byMatchKeys = autoIt != m_auto.constEnd() && in.pitch == autoIt.value();
        if (in.pitch != 0 && !byMatchKeys) {
            if (m_matchOn && autoIt != m_auto.constEnd()) {
                m_auto.remove(title);   // transposed by hand since: it is the user's now
            }
            continue;
        }
        const double atTime = 0.5 * (zone.start + zone.end);
        const ClipKeyInfo mine = keyInfoAt(in, atTime);
        const ClipKeyInfo theirs = keyInfoAt(out, atTime);
        if (mine.keyId <= 0 || theirs.effectiveKeyId <= 0) {
            continue;
        }
        // The transpose from the song's analysed key that lands on a key
        // compatible with what the outgoing song sounds in there.
        int steps = shortestStepsToCompatibleKey(scaleKeySteps(mine.keyId, mine.stretchSteps), theirs.effectiveKeyId);
        if (std::abs(steps) > maxSteps) {
            steps = 0;   // too far: the song keeps its own key
        }
        if (!m_matchOn) {
            // What it sounds in now (no transpose) against what it would be.
            if (steps != 0 && in.pitch == 0) {
                clashes.insert(out.title.toQString() + QLatin1Char('|') + title);
            }
            continue;
        }
        const int target = steps * 100;
        if (in.pitch != target && clipsInteraction()) {
            // Every piece of the song (a Cmd+D loop, a split) gets the same key.
            for (const trackedit::TrackId trackId : project->trackIdList()) {
                for (const trackedit::Clip& piece : project->clipList(trackId)) {
                    if (piece.title == in.title && piece.pitch == in.pitch) {
                        const bool ok = target == 0 ? clipsInteraction()->resetClipPitch(piece.key)
                                                    : clipsInteraction()->changeClipPitch(piece.key, target);
                        changed = ok || changed;
                    }
                }
            }
            LOGI() << "GlideMix Match keys: an incoming song " << (target / 100 > 0 ? "+" : "") << target / 100
                   << " st (max " << maxSteps << ")";
        }
        if (target != 0) {
            m_auto.insert(title, target);
        } else {
            m_auto.remove(title);
        }
    }
    // Songs no longer in the mix are forgotten.
    for (auto it = m_auto.begin(); it != m_auto.end();) {
        it = titlesNow.contains(it.key()) ? std::next(it) : m_auto.erase(it);
    }

    if (!m_matchOn) {
        const bool seeding = m_seedClashes;
        m_seedClashes = false;
        bool fresh = false;
        for (const QString& pair : clashes) {
            fresh = fresh || !m_seenClashes.contains(pair);
        }
        m_seenClashes = clashes;   // a pair that is fixed and comes back flashes again
        if (fresh && !seeding) {
            setHint(true);
            m_hintTimer.start();
        }
    }
    if (changed) {
        m_keysChanged.notify();
    }
    return changed;
}

void MixHouseKeyMatch::keysMayHaveChanged()
{
    if (keylockOff()) {
        m_keysTimer.start();
    }
}

muse::async::Notification MixHouseKeyMatch::keysChanged() const
{
    return m_keysChanged;
}

bool MixHouseKeyMatch::keylockOff() const
{
    return libraryConfiguration() && !libraryConfiguration()->keylockEnabled();
}

int MixHouseKeyMatch::libraryKeyId(const QString& clipTitle) const
{
    if (clipTitle.isEmpty() || !library() || !library()->isAvailable()) {
        return 0;
    }
    // An imported clip is named after its file (same lookup as the beat grids).
    IMixHouseLibrary::TrackQuery query;
    query.fileBaseName = clipTitle;
    query.limit = 1;
    MixHouseTrackList found = library()->tracks(query);
    if (found.empty()) {
        found = externalLibraryTracks(lexicon().get(), rekordboxXml().get(), query);   // loaded from Lexicon DJ / rekordbox
    }
    return found.empty() ? 0 : found.front().keyId;
}

IMixHouseKeyMatch::ClipKeyInfo MixHouseKeyMatch::keyInfo(const trackedit::ClipKey& key) const
{
    const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    if (!project) {
        return ClipKeyInfo();
    }
    const trackedit::Clip clip = project->clip(key);
    // Keylock off: the key is heard where the song mixes in -- the middle of
    // the transition it comes in on, else the one it goes out on, else its
    // middle (the same partner rule as match()).
    double atTime = 0.5 * (clip.startTime + clip.endTime);
    if (keylockOff() && transitions()) {
        bool found = false;
        for (const IMixHouseTransitions::Zone& zone : transitions()->zones()) {
            if (zone.incoming == key) {
                atTime = 0.5 * (zone.start + zone.end);
                break;
            }
            if (zone.outgoing == key && !found) {
                atTime = 0.5 * (zone.start + zone.end);
                found = true;
            }
        }
    }
    return keyInfoAt(clip, atTime);
}

IMixHouseKeyMatch::ClipKeyInfo MixHouseKeyMatch::keyInfoAt(const trackedit::Clip& clip, double atTime) const
{
    ClipKeyInfo info;
    if (!clip.isValid()) {
        return info;
    }
    info.keyId = libraryKeyId(clip.title.toQString());
    info.shiftSemitones = static_cast<int>(std::lround(clip.pitch / 100.0));
    double semitones = clip.pitch / 100.0;
    // Keylock off plays like a turntable: the engine's pitch is cents / ratio
    // (MixtapeClipVoice::PitchFor), so the speed adds 12 * log2(speed). The
    // speed is the tempo line over the BPM at which the clip plays unstretched
    // (MixHouseBeatGrids::clipBpm: project tempo x the clip's stretch ratio).
    const double unstretchedBpm = keylockOff() && beatGrids() ? beatGrids()->clipBpm(clip.key) : 0.0;
    if (keylockOff() && tempoMap() && !autoKeylocked(unstretchedBpm, clip.startTime, clip.endTime)) {
        const double t = std::clamp(atTime, clip.startTime, std::max(clip.startTime, clip.endTime));
        const double lineBpm = tempoMap()->bpmAt(t);
        if (unstretchedBpm > 0.0 && lineBpm > 0.0) {
            semitones += 12.0 * std::log2(lineBpm / unstretchedBpm);
        }
    }
    info.stretchSteps = static_cast<int>(std::lround(semitones)) - info.shiftSemitones;
    info.effectiveKeyId = scaleKeySteps(info.keyId, info.shiftSemitones + info.stretchSteps);
    return info;
}

IMixHouseKeyMatch::Match MixHouseKeyMatch::match(const trackedit::ClipKey& key) const
{
    Match result;
    const auto project = globalContext() ? globalContext()->currentTrackeditProject() : nullptr;
    if (!project || !transitions()) {
        return result;
    }

    // The partner is the clip this one mixes with. Prefer the track it mixes
    // INTO (it is the incoming side), else the one mixing into it.
    trackedit::ClipKey partner;
    for (const IMixHouseTransitions::Zone& zone : transitions()->zones()) {
        if (zone.incoming == key) {
            partner = zone.outgoing;
            break;
        }
        if (zone.outgoing == key && !partner.isValid()) {
            partner = zone.incoming;
        }
    }
    if (!partner.isValid()) {
        result.myKeyId = keyInfo(key).keyId;
        return result;
    }

    // Both keys as heard together, in the middle of the transition: with
    // Keylock off each song's speed there moves its key (0 with Keylock on).
    const ClipKeyInfo mine = keyInfo(key);
    double atTime = 0.0;
    for (const IMixHouseTransitions::Zone& zone : transitions()->zones()) {
        if ((zone.incoming == key && zone.outgoing == partner) || (zone.outgoing == key && zone.incoming == partner)) {
            atTime = 0.5 * (zone.start + zone.end);
            break;
        }
    }
    const ClipKeyInfo theirs = keyInfoAt(project->clip(partner), atTime);
    result.hasPartner = true;
    result.partnerTitle = project->clip(partner).title.toQString();
    result.myKeyId = mine.keyId;
    result.partnerKeyId = theirs.effectiveKeyId;
    // The transpose (from the analysed key) that lands on a compatible key,
    // counting the speed's own shift.
    result.steps = shortestStepsToCompatibleKey(scaleKeySteps(mine.keyId, mine.stretchSteps), theirs.effectiveKeyId);
    return result;
}

bool MixHouseKeyMatch::transpose(const trackedit::ClipKey& key, int semitones)
{
    if (!trackeditInteraction()) {
        return false;
    }
    return semitones == 0 ? trackeditInteraction()->resetClipPitch(key)
                          : trackeditInteraction()->changeClipPitch(key, semitones * 100);
}
