#include "mixhouseclockdisplays.h"

#include <algorithm>
#include <cstring>

#include <QVariantMap>

#include "global/settings.h"
#include "translation.h"

using namespace au::mixhouse;

const char* MixHouseClockDisplays::kMenuPrefix = "dj-show:";

namespace {
const muse::Settings::Key SHOWN_KEY("mixhouse", "mixhouse/clock/shown");
const muse::Settings::Key SONG_TIME_MODE_KEY("mixhouse", "mixhouse/clock/songTimeMode");
const muse::Settings::Key FLASH_KEY("mixhouse", "mixhouse/clock/flash");

struct Display {
    const char* id;
    const char* title;
};
// In the order they sit in the capsule.
const Display kDisplays[] = {
    { "mixtotal", QT_TRANSLATE_NOOP("mixhouse", "Mix length") },
    { "songtime", QT_TRANSLATE_NOOP("mixhouse", "Song time") },
    { "barbeat", QT_TRANSLATE_NOOP("mixhouse", "Bar and beat") },
    { "phrase", QT_TRANSLATE_NOOP("mixhouse", "Phrase") },
    { "nextmix", QT_TRANSLATE_NOOP("mixhouse", "Next mix countdown") },
    { "tempo", QT_TRANSLATE_NOOP("mixhouse", "Tempo and pitch") },
    { "key", QT_TRANSLATE_NOOP("mixhouse", "Key") },
    { "energy", QT_TRANSLATE_NOOP("mixhouse", "Energy") },
    { "song", QT_TRANSLATE_NOOP("mixhouse", "Song number") },
    { "cpu", QT_TRANSLATE_NOOP("mixhouse", "CPU") },
    { "disk", QT_TRANSLATE_NOOP("mixhouse", "Disk") },
    { "memory", QT_TRANSLATE_NOOP("mixhouse", "Memory") },
};
const char* kDefaultShown = "songtime,barbeat,nextmix,key,song";
const char* kAllId = "complete";
}

MixHouseClockDisplays::MixHouseClockDisplays(QObject* parent)
    : QObject(parent)
{
    muse::settings()->setDefaultValue(SHOWN_KEY, muse::Val(std::string(kDefaultShown)));
    muse::settings()->setDefaultValue(SONG_TIME_MODE_KEY, muse::Val(0));
    muse::settings()->setDefaultValue(FLASH_KEY, muse::Val(true));
    muse::settings()->valueChanged(FLASH_KEY).onReceive(this, [this](const muse::Val&) { emit shownChanged(); });
    muse::settings()->valueChanged(SHOWN_KEY).onReceive(this, [this](const muse::Val&) { emit shownChanged(); });
    muse::settings()->valueChanged(SONG_TIME_MODE_KEY).onReceive(this, [this](const muse::Val&) { emit shownChanged(); });
}

QStringList MixHouseClockDisplays::shown() const
{
    return QString::fromStdString(muse::settings()->value(SHOWN_KEY).toString()).split(',', Qt::SkipEmptyParts);
}

int MixHouseClockDisplays::songTimeMode() const
{
    return std::clamp(muse::settings()->value(SONG_TIME_MODE_KEY).toInt(), 0, 2);
}

QVariantList MixHouseClockDisplays::menuItems() const
{
    const QStringList on = shown();
    QVariantList sub;
    // Complete: every readout at once (again: back to the defaults).
    bool all = true;
    for (const Display& d : kDisplays) {
        all = all && on.contains(QString::fromUtf8(d.id));
    }
    QVariantMap complete;
    complete["id"] = QString::fromUtf8(kMenuPrefix) + QString::fromUtf8(kAllId);
    complete["title"] = muse::qtrc("mixhouse", "Complete");
    complete["checkable"] = true;
    complete["checked"] = all;
    complete["enabled"] = true;
    sub << complete << QVariantMap();
    for (const Display& d : kDisplays) {
        QVariantMap item;
        item["id"] = QString::fromUtf8(kMenuPrefix) + QString::fromUtf8(d.id);
        item["title"] = muse::qtrc("mixhouse", d.title);
        item["checkable"] = true;
        item["checked"] = on.contains(QString::fromUtf8(d.id));
        item["enabled"] = true;
        sub << item;
    }
    QVariantMap show;
    show["id"] = QString::fromUtf8("dj-show");
    show["title"] = muse::qtrc("mixhouse", "Show");
    show["enabled"] = true;
    show["subitems"] = sub;
    return { show, QVariantMap() };   // the empty map is a separator before the time formats
}

bool MixHouseClockDisplays::flash() const
{
    return muse::settings()->value(FLASH_KEY).toBool();
}

QVariantList MixHouseClockDisplays::contextMenuItems() const
{
    QVariantMap item;
    item["id"] = QString::fromUtf8("dj-flash");
    item["title"] = muse::qtrc("mixhouse", "Flash near the end");
    item["checkable"] = true;
    item["checked"] = flash();
    item["enabled"] = true;
    QVariantList list { item, QVariantMap() };
    return list + menuItems().first().toMap().value("subitems").toList();
}

bool MixHouseClockDisplays::handleMenuItem(const QString& itemId)
{
    if (itemId == QString::fromUtf8("dj-flash")) {
        muse::settings()->setSharedValue(FLASH_KEY, muse::Val(!flash()));
        return true;
    }
    if (!itemId.startsWith(QString::fromUtf8(kMenuPrefix))) {
        return itemId == QString::fromUtf8("dj-show");
    }
    toggle(itemId.mid(int(strlen(kMenuPrefix))));
    return true;
}

void MixHouseClockDisplays::toggle(const QString& id)
{
    if (id == QString::fromUtf8(kAllId)) {
        QStringList all;
        bool everyOn = true;
        const QStringList now = shown();
        for (const Display& d : kDisplays) {
            all << QString::fromUtf8(d.id);
            everyOn = everyOn && now.contains(QString::fromUtf8(d.id));
        }
        muse::settings()->setSharedValue(SHOWN_KEY,
                                         muse::Val(everyOn ? std::string(kDefaultShown) : all.join(',').toStdString()));
        return;
    }
    QStringList on = shown();
    if (on.contains(id)) {
        on.removeAll(id);
    } else {
        on << id;
    }
    muse::settings()->setSharedValue(SHOWN_KEY, muse::Val(on.join(',').toStdString()));
}

void MixHouseClockDisplays::cycleSongTimeMode()
{
    muse::settings()->setSharedValue(SONG_TIME_MODE_KEY, muse::Val((songTimeMode() + 1) % 3));
}
