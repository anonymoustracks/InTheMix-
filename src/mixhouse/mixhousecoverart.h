/*
 * MixHouse DJ 2
 *
 * Roadmap Phase 8: embedded cover-art extraction. Ports real mixxx's own
 * CoverArtUtils::extractEmbeddedCover / SoundSourceProxy::
 * importTrackMetadataAndCoverImageFromFile (src/sources/metadatasourcetaglib.cpp)
 * onto TagLib directly -- this module reads Mixxx's library DATABASE, not its
 * code, but there is no substitute for TagLib itself when the picture lives
 * inside the audio file's own tags, so it's a real, new dependency (see
 * muse_deps/recipes/taglib).
 */
#pragma once

#include <memory>

#include <QImage>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QThreadPool>
#include <QString>
#include <QQuickImageProvider>

#include "imixhouselibrary.h"

namespace au::mixhouse {

/// Real per-format embedded-picture extraction, mirrored from
/// metadatasourcetaglib.cpp's dispatch:
///   MP3          -- ID3v2 APIC (trackmetadata_id3v2.cpp)
///   FLAC         -- Xiph (VorbisComment) picture comment preferred, native
///                   FLAC picture-block list as fallback (trackmetadata_xiph.cpp)
///   OGG Vorbis/Opus, Ogg-FLAC -- Xiph picture comment
///   MP4/M4A      -- 'covr' atom (trackmetadata_mp4.cpp)
///   WAV/AIFF     -- ID3v2 chunk
/// Every format prefers FrontCover, then Media, then Illustration, then Other
/// (TagLib::ID3v2::AttachedPictureFrame::Type / FLAC::Picture::Type), matching
/// mixxx's own kPreferredPictureTypes order. Returns a null QImage if the file
/// can't be opened, has no tag of a supported kind, or has no picture at all --
/// never throws, never logs (a track with no embedded art is the ordinary case,
/// not an error).
QImage extractEmbeddedCoverArt(const QString& filePath);

/// GlideMix: writes a JPEG or PNG file into an audio file as its front cover,
/// in whatever tag that format uses. False when either file can't be used.
bool embedCoverArt(const QString& audioPath, const QString& imagePath);

/// Resolves and decodes one track's cover art the way real mixxx's
/// CoverInfo::loadImage() branches on CoverInfoRelative::Type, and caches the
/// decoded QImage in memory keyed by track id: TagLib extraction + JPEG/PNG
/// decode is exactly the "expensive to decode, cheap to reuse" case already
/// solved for the Waveform Overview column (MixHouseTrackListModel's
/// m_overviewCache) -- repainting the same cell must not re-run TagLib.
///
/// type==NONE (0) never touches disk at all. type==METADATA (1) runs
/// extractEmbeddedCoverArt() on the track's own audio file. type==FILE (2)
/// would load `location` as a sidecar image path (QImage::load), mirroring
/// CoverInfo::loadImage's other branch -- not exercised by this port's initial
/// verification (no FILE-type row was found in the real library), but handled
/// the same way real mixxx does since the column data already carries it.
///
/// A miss (no cover, or extraction failure) is cached as a null QImage too,
/// so a cover-less track is not re-decoded on every scroll/repaint.
/// GlideMix (Marc, 2026-11-19: library scrolling lags on a network share): covers are read from the
/// files on a worker, and `revision` goes up when new ones are in the cache; the cover Images add it
/// to their source ("?r=<revision>") so they ask again. Exposed to QML as `glidemixCovers`.
class MixHouseCoverArtNotifier : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
public:
    explicit MixHouseCoverArtNotifier(QObject* parent = nullptr);
    int revision() const { return m_revision; }
    void bump();   // coalesced: at most a few times a second

signals:
    void revisionChanged();

private:
    int m_revision = 0;
    bool m_scheduled = false;
};

class MixHouseCoverArtCache
{
public:
    explicit MixHouseCoverArtCache(std::shared_ptr<IMixHouseLibrary> library);
    ~MixHouseCoverArtCache();

    /// The decoded cover for `trackId`, or a null QImage if it has none.
    QImage image(int trackId);

    /// Drops trackId's cached entry. Call after a cover-art write (Choose new
    /// cover / Clear cover / Reload from file) so the next request re-decodes
    /// instead of serving what's now a stale image.
    void invalidate(int trackId);

    /// GlideMix: the cover of a file that is not in the library (a rekordbox
    /// or Lexicon track): the picture in it, else a cover image in its folder.
    QImage imageForFile(const QString& filePath);

    MixHouseCoverArtNotifier* notifier() const { return m_notifier; }

private:
    //! What the worker must read for one cover: the picture in `filePath`, a sidecar image file, or
    //! the picture in the file and else a cover image in its folder.
    enum class Read { Embedded, Sidecar, EmbeddedOrFolder };
    struct State {
        QHash<QString, QImage> fileCache;
        QHash<int, QImage> cache;
        QSet<QString> pending;   // "t:<id>" / "f:<path>" being read
    };
    void readLater(const QString& pendingKey, Read what, const QString& path, int trackId);

    std::shared_ptr<State> m_state = std::make_shared<State>();
    std::shared_ptr<IMixHouseLibrary> m_library;
    QThreadPool* m_pool = nullptr;
    MixHouseCoverArtNotifier* m_notifier = nullptr;
};

/// Serves `image://mixhousecovers/<trackId>` for the library table's Cover Art
/// column and the track menu's full-size popup. No existing precedent for
/// QQuickImageProvider/addImageProvider exists anywhere in this codebase (see
/// Phase 8 research) -- this is a standard Qt6 QQuickImageProvider, registered
/// from MixHouseModuleContext::onInit() via IUiEngine::qmlEngine(), the same
/// QQmlEngine-access pattern already used for KDDockWidgets in
/// muse/framework/dockwindow/dockmodule.cpp. Qt takes ownership of the
/// instance once it's handed to QQmlEngine::addImageProvider().
class MixHouseCoverArtImageProvider : public QQuickImageProvider
{
public:
    explicit MixHouseCoverArtImageProvider(std::shared_ptr<IMixHouseLibrary> library);

    /// MAIN-THREAD ONLY. m_library's QSqlQuery/QSqlDatabase are only safe on
    /// the thread that opened them (the GUI thread, same as every other
    /// mixhouse DB access) -- calling this off-thread is undefined behavior,
    /// and is exactly what produced four real EXC_BAD_ACCESS crashes in the
    /// field (root-caused from the actual .ips crash reports: every one showed
    /// requestImage() on a non-main thread, reached via a QML Image with
    /// asynchronous: true). The QML side must never set asynchronous: true on
    /// an Image sourcing image://mixhousecovers/... ; this asserts that
    /// contract at runtime (a qWarning + null image, not Q_ASSERT, since this
    /// build compiles with NDEBUG and Q_ASSERT would silently no-op) so a
    /// future regression fails safely instead of corrupting memory again.
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    MixHouseCoverArtNotifier* notifier() const { return m_cache.notifier(); }

private:
    MixHouseCoverArtCache m_cache;
};

}
