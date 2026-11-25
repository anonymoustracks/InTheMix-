/*
 * MixHouse DJ 2
 */
#include "mixhousecoverart.h"
#include "mixhouse/mixhousenativepath.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QFileInfo>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QPointer>

#include <taglib/tfile.h>
#include <taglib/tvariant.h>
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/xiphcomment.h>
#include <taglib/vorbisfile.h>
#include <taglib/opusfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/wavfile.h>
#include <taglib/aifffile.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4item.h>
#include <taglib/mp4coverart.h>

namespace au::mixhouse {

namespace {

// This module's CMake build concatenates every .cpp in src/mixhouse/ into one
// unity translation unit -- anonymous-namespace/static helper names have
// collided across files here before, so every helper below gets a
// mixhouseCoverArt-prefixed name distinctive to this file.

/// Decodes one embedded picture's raw compressed bytes into a QImage. Real
/// mixxx (trackmetadata_xiph.cpp) passes the picture's own MIME-type string as
/// QImage::fromData's format hint; that string ("image/jpeg") isn't one of
/// Qt's plugin-key format names ("JPG"/"PNG"...), so this instead leaves
/// `format` null and lets QImage::fromData sniff the real magic bytes --
/// equally correct for JPEG/PNG and more robust than trusting a possibly
/// wrong or absent mime-type field in the tag itself.
QImage mixhouseCoverArtDecode(const TagLib::ByteVector& data)
{
    if (data.isEmpty()) {
        return QImage();
    }
    return QImage::fromData(reinterpret_cast<const uchar*>(data.data()),
                             static_cast<int>(data.size()));
}

/// Mixxx's real kPreferredPictureTypes order (trackmetadata_id3v2.cpp /
/// trackmetadata_xiph.cpp): front cover first, then an image of the media
/// itself, then any illustration, falling back to whatever "Other"-typed
/// picture exists rather than showing nothing.
template<typename PictureType>
QImage mixhouseCoverArtPickPreferred(const TagLib::List<PictureType*>& pictures)
{
    const typename PictureType::Type kPreferredOrder[] = {
        PictureType::FrontCover, PictureType::Media, PictureType::Illustration, PictureType::Other
    };
    for (const auto preferredType : kPreferredOrder) {
        for (PictureType* picture : pictures) {
            if (!picture || picture->type() != preferredType) {
                continue;
            }
            QImage image = mixhouseCoverArtDecode(picture->data());
            if (!image.isNull()) {
                return image;
            }
        }
    }
    return QImage();
}

/// ID3v2 APIC extraction (mp3, wav, aiff) -- trackmetadata_id3v2.cpp.
QImage mixhouseCoverArtFromId3v2(TagLib::ID3v2::Tag& tag)
{
    const auto& frameListMap = tag.frameListMap();
    const auto iterApic = frameListMap.find("APIC");
    if (iterApic == frameListMap.end()) {
        return QImage();
    }

    using AttachedPictureFrame = TagLib::ID3v2::AttachedPictureFrame;
    const AttachedPictureFrame::Type kPreferredOrder[] = {
        AttachedPictureFrame::FrontCover, AttachedPictureFrame::Media,
        AttachedPictureFrame::Illustration, AttachedPictureFrame::Other
    };
    for (const auto preferredType : kPreferredOrder) {
        for (const TagLib::ID3v2::Frame* frame : iterApic->second) {
            const auto* apicFrame = dynamic_cast<const AttachedPictureFrame*>(frame);
            if (!apicFrame || apicFrame->type() != preferredType) {
                continue;
            }
            QImage image = mixhouseCoverArtDecode(apicFrame->picture());
            if (!image.isNull()) {
                return image;
            }
        }
    }
    return QImage();
}

/// MP4 'covr' atom extraction -- trackmetadata_mp4.cpp.
QImage mixhouseCoverArtFromMp4(TagLib::MP4::Tag& tag)
{
    if (!tag.contains("covr")) {
        return QImage();
    }
    const TagLib::MP4::CoverArtList coverArtList = tag.item("covr").toCoverArtList();
    for (const auto& coverArt : coverArtList) {
        QImage image = mixhouseCoverArtDecode(coverArt.data());
        if (!image.isNull()) {
            return image;
        }
    }
    return QImage();
}

} // namespace

QImage extractEmbeddedCoverArt(const QString& filePath)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        return QImage();
    }

    TagLib::FileRef fileRef = au::mixhouse::openTagFile(filePath);
    TagLib::File* file = fileRef.file();
    if (!file || !file->isValid()) {
        return QImage();
    }

    // MP3 -- ID3v2 APIC.
    if (auto* mpeg = dynamic_cast<TagLib::MPEG::File*>(file)) {
        if (auto* id3v2 = mpeg->ID3v2Tag()) {
            QImage image = mixhouseCoverArtFromId3v2(*id3v2);
            if (!image.isNull()) {
                return image;
            }
        }
        return QImage();
    }

    // FLAC -- Xiph (VorbisComment) picture comment preferred, native FLAC
    // picture-block list as fallback, matching metadatasourcetaglib.cpp.
    if (auto* flac = dynamic_cast<TagLib::FLAC::File*>(file)) {
        if (auto* xiph = flac->xiphComment()) {
            QImage image = mixhouseCoverArtPickPreferred(xiph->pictureList());
            if (!image.isNull()) {
                return image;
            }
        }
        return mixhouseCoverArtPickPreferred(flac->pictureList());
    }

    // MP4/M4A -- 'covr' atom.
    if (auto* mp4 = dynamic_cast<TagLib::MP4::File*>(file)) {
        if (auto* tag = mp4->tag()) {
            return mixhouseCoverArtFromMp4(*tag);
        }
        return QImage();
    }

    // OGG Vorbis, OGG-FLAC, Opus -- Xiph picture comment.
    if (auto* vorbis = dynamic_cast<TagLib::Ogg::Vorbis::File*>(file)) {
        if (auto* xiph = vorbis->tag()) {
            return mixhouseCoverArtPickPreferred(xiph->pictureList());
        }
        return QImage();
    }
    if (auto* oggFlac = dynamic_cast<TagLib::Ogg::FLAC::File*>(file)) {
        if (auto* xiph = oggFlac->tag()) {
            return mixhouseCoverArtPickPreferred(xiph->pictureList());
        }
        return QImage();
    }
    if (auto* opus = dynamic_cast<TagLib::Ogg::Opus::File*>(file)) {
        if (auto* xiph = opus->tag()) {
            return mixhouseCoverArtPickPreferred(xiph->pictureList());
        }
        return QImage();
    }

    // WAV / AIFF -- ID3v2 chunk.
    if (auto* wav = dynamic_cast<TagLib::RIFF::WAV::File*>(file)) {
        if (auto* id3v2 = wav->ID3v2Tag()) {
            return mixhouseCoverArtFromId3v2(*id3v2);
        }
        return QImage();
    }
    if (auto* aiff = dynamic_cast<TagLib::RIFF::AIFF::File*>(file)) {
        if (auto* id3v2 = aiff->tag()) {
            return mixhouseCoverArtFromId3v2(*id3v2);
        }
        return QImage();
    }

    return QImage();
}

MixHouseCoverArtNotifier::MixHouseCoverArtNotifier(QObject* parent)
    : QObject(parent)
{
}

void MixHouseCoverArtNotifier::bump()
{
    if (m_scheduled) {
        return;
    }
    m_scheduled = true;
    QTimer::singleShot(250, this, [this]() {
        m_scheduled = false;
        ++m_revision;
        emit revisionChanged();
    });
}

namespace {
// Off the UI thread: file reading only -- no database (see requestImage's comment).
QImage readEmbeddedOrFolder(const QString& filePath)
{
    QImage result = au::mixhouse::extractEmbeddedCoverArt(filePath);
    if (result.isNull()) {
        const QDir dir = QFileInfo(filePath).absoluteDir();
        for (const char* name : { "cover.jpg", "cover.png", "folder.jpg", "folder.png", "front.jpg", "Cover.jpg",
                                  "Folder.jpg" }) {
            if (dir.exists(QString::fromLatin1(name)) && result.load(dir.filePath(QString::fromLatin1(name)))) {
                break;
            }
        }
    }
    return result;
}
}

MixHouseCoverArtCache::MixHouseCoverArtCache(std::shared_ptr<IMixHouseLibrary> library)
    : m_library(std::move(library))
{
    // GlideMix: two readers -- enough to keep up with scrolling, gentle on a network share.
    m_pool = new QThreadPool();
    m_pool->setMaxThreadCount(2);
    m_notifier = new MixHouseCoverArtNotifier();
}

MixHouseCoverArtCache::~MixHouseCoverArtCache()
{
    // Readers still running drop their result (the state is gone: weak pointer).
    m_state.reset();
    m_pool->clear();
    m_pool->waitForDone(2000);
    delete m_pool;
    m_notifier->deleteLater();
}

void MixHouseCoverArtCache::readLater(const QString& pendingKey, Read what, const QString& path, int trackId)
{
    if (m_state->pending.contains(pendingKey)) {
        return;
    }
    m_state->pending.insert(pendingKey);
    std::weak_ptr<State> weak = m_state;
    QPointer<MixHouseCoverArtNotifier> notifier = m_notifier;
    m_pool->start([weak, notifier, what, path, trackId, pendingKey]() {
        QImage image;
        if (what == Read::Embedded) {
            image = au::mixhouse::extractEmbeddedCoverArt(path);
        } else if (what == Read::Sidecar) {
            image.load(path);
        } else {
            image = readEmbeddedOrFolder(path);
        }
        // Back on the UI thread, where the cache lives.
        QMetaObject::invokeMethod(qApp, [weak, notifier, path, trackId, pendingKey, image]() {
            const auto state = weak.lock();
            if (!state) {
                return;
            }
            state->pending.remove(pendingKey);
            if (trackId >= 0) {
                state->cache.insert(trackId, image);
            } else {
                state->fileCache.insert(path, image);
            }
            if (!image.isNull() && notifier) {
                notifier->bump();
            }
        }, Qt::QueuedConnection);
    });
}

QImage MixHouseCoverArtCache::image(int trackId)
{
    auto it = m_state->cache.find(trackId);
    if (it != m_state->cache.end()) {
        return it.value();
    }
    if (m_state->pending.contains(QStringLiteral("t:%1").arg(trackId))) {
        return QImage();   // being read
    }

    // The database is asked here, on the UI thread; the file is read on a worker.
    IMixHouseLibrary::CoverInfo info;
    if (m_library && m_library->coverInfoForTrack(trackId, info)) {
        // CoverInfoRelative::Type: 0=NONE, 1=METADATA, 2=FILE.
        const QString key = QStringLiteral("t:%1").arg(trackId);
        if (info.type == 1) {
            readLater(key, Read::Embedded, info.filePath, trackId);
            return QImage();
        } else if (info.type == 2 && !info.location.isEmpty()) {
            // A standalone sidecar image, relative to the track file's own directory --
            // mirrors CoverInfo::loadImage()'s FILE branch.
            const QFileInfo trackFile(info.filePath);
            readLater(key, Read::Sidecar, trackFile.absoluteDir().filePath(info.location), trackId);
            return QImage();
        } else if (info.type == 0 && !info.filePath.isEmpty()) {
            // GlideMix: the scanner never records artwork, so every track says "none". Look anyway.
            readLater(key, Read::EmbeddedOrFolder, info.filePath, trackId);
            return QImage();
        }
    }
    m_state->cache.insert(trackId, QImage());   // nothing to read: no cover
    return QImage();
}

QImage MixHouseCoverArtCache::imageForFile(const QString& filePath)
{
    if (const auto it = m_state->fileCache.constFind(filePath); it != m_state->fileCache.constEnd()) {
        return it.value();
    }
    readLater(QStringLiteral("f:") + filePath, Read::EmbeddedOrFolder, filePath, -1);
    return QImage();
}

void MixHouseCoverArtCache::invalidate(int trackId)
{
    m_state->cache.remove(trackId);
}

MixHouseCoverArtImageProvider::MixHouseCoverArtImageProvider(std::shared_ptr<IMixHouseLibrary> library)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_cache(std::move(library))
{
}

QImage MixHouseCoverArtImageProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    // See the header doc comment: this must never run off the GUI thread --
    // m_library's SQL connection isn't safe from anywhere else. Fail safe
    // (null image, loud warning) rather than touch the database and risk the
    // exact memory corruption that crashed the app four times before this was
    // fixed by keeping the QML side synchronous.
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        qWarning() << "MixHouseCoverArtImageProvider::requestImage called off the main "
                      "thread for id" << id << "-- refusing to touch the database. "
                      "Check for asynchronous: true on a cover-art Image element.";
        if (size) {
            *size = QSize();
        }
        return QImage();
    }

    // "file/<percent-encoded path>": a track from rekordbox or Lexicon, which
    // is not in the library and so has no library id.
    // GlideMix: "?r=<revision>" only makes QML ask again when covers have arrived.
    const QString key = id.section(QLatin1Char('?'), 0, 0);
    QImage image;
    if (key.startsWith(QStringLiteral("file/"))) {
        image = m_cache.imageForFile(QUrl::fromPercentEncoding(key.mid(5).toUtf8()));
    } else {
        bool ok = false;
        const int trackId = key.toInt(&ok);
        image = ok ? m_cache.image(trackId) : QImage();
    }

    // GlideMix (Marc's log, 2026-11-19: 1,400 "Failed to get image" warnings a session): a song
    // without a cover gets one transparent pixel instead of no image. Every cover Image shows only
    // when ready, so it still looks empty, and Qt no longer writes a warning per row shown.
    if (image.isNull()) {
        image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        if (size) {
            *size = image.size();
        }
        return image;
    }
    if (size) {
        *size = image.size();
    }
    if (requestedSize.isValid() && !requestedSize.isEmpty()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

}

bool au::mixhouse::embedCoverArt(const QString& audioPath, const QString& imagePath)
{
    QFile image(imagePath);
    if (!image.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = image.readAll();

    TagLib::FileRef file = openTagFile(audioPath);
    if (file.isNull()) {
        return false;
    }

    TagLib::VariantMap picture;
    picture["data"] = TagLib::ByteVector(bytes.constData(), static_cast<unsigned int>(bytes.size()));
    picture["mimeType"] = TagLib::String(imagePath.endsWith(".png", Qt::CaseInsensitive) ? "image/png" : "image/jpeg");
    picture["pictureType"] = TagLib::String("Front Cover");
    picture["description"] = TagLib::String("");

    return file.setComplexProperties("PICTURE", TagLib::List<TagLib::VariantMap>({ picture })) && file.save();
}
