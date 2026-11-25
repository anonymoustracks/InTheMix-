/*
 * GlideMix: which readouts the header clock capsule shows (the DJ clock,
 * Chris 2026-11-13), as ticks in a "Show" submenu of the clock's format
 * menu. An app setting, so every window and the next launch agree.
 */
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQmlIntegration>

#include "async/asyncable.h"

namespace au::mixhouse {
class MixHouseClockDisplays : public QObject, public muse::async::Asyncable
{
    Q_OBJECT
    QML_ELEMENT

    //! Ids of the readouts shown: songtime, barbeat, phrase, nextmix, mixtotal,
    //! tempo, key, energy, song, cpu, disk.
    Q_PROPERTY(QStringList shown READ shown NOTIFY shownChanged)
    //! The song time readout: 0 remaining, 1 elapsed, 2 both.
    Q_PROPERTY(int songTimeMode READ songTimeMode NOTIFY shownChanged)
    //! The "Show" submenu and a separator, for the front of the clock's menu.
    Q_PROPERTY(QVariantList menuItems READ menuItems NOTIFY shownChanged)
    //! SONG LEFT / NEXT MIX flash near the end (right-click the readouts to switch).
    Q_PROPERTY(bool flash READ flash NOTIFY shownChanged)
    //! The right-click menu of the readouts.
    Q_PROPERTY(QVariantList contextMenuItems READ contextMenuItems NOTIFY shownChanged)

public:
    explicit MixHouseClockDisplays(QObject* parent = nullptr);

    QStringList shown() const;
    int songTimeMode() const;
    QVariantList menuItems() const;
    bool flash() const;
    QVariantList contextMenuItems() const;

    //! A menu item id from menuItems ("dj-show:<id>"); false when not one of ours.
    Q_INVOKABLE bool handleMenuItem(const QString& itemId);
    Q_INVOKABLE void toggle(const QString& id);
    Q_INVOKABLE void cycleSongTimeMode();

    static const char* kMenuPrefix;

signals:
    void shownChanged();
};
}
