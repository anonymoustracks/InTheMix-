/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "maintoolbarmodel.h"

#include "translation.h"

using namespace au::appshell;

static const QString HOME_PAGE("audacity://home");
static const QString PROJECT_PAGE("audacity://project");
static const QString PUBLISH_PAGE("audacity://publish");

static const QString TITLE_KEY("title");
static const QString URI_KEY("uri");
static const QString IS_TITLE_BOLD_KEY("isTitleBold");
static const QString ENABLED_KEY("enabled");

inline QVariantMap buildItem(const QString& title, const QString& uri, const bool enabled)
{
    QVariantMap item;
    item[TITLE_KEY] = title;
    item[URI_KEY] = uri;
    item[IS_TITLE_BOLD_KEY] = false;
    item[ENABLED_KEY] = enabled;

    return item;
}

MainToolBarModel::MainToolBarModel(QObject* parent)
    : QAbstractListModel(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
}

QVariant MainToolBarModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) {
        return QVariant();
    }

    const QVariantMap& item = m_items.at(index.row());
    switch (role) {
    case TitleRole: return item[TITLE_KEY];
    case UriRole: return item[URI_KEY];
    case IsTitleBoldRole: return item[IS_TITLE_BOLD_KEY];
    case EnabledRole: return item[ENABLED_KEY];
    }

    return QVariant();
}

int MainToolBarModel::rowCount(const QModelIndex&) const
{
    return m_items.size();
}

QHash<int, QByteArray> MainToolBarModel::roleNames() const
{
    static const QHash<int, QByteArray> roles = {
        { TitleRole, TITLE_KEY.toUtf8() },
        { UriRole, URI_KEY.toUtf8() },
        { IsTitleBoldRole, IS_TITLE_BOLD_KEY.toUtf8() },
        { EnabledRole, ENABLED_KEY.toUtf8() },
    };

    return roles;
}

void MainToolBarModel::load()
{
    beginResetModel();

    m_items.clear();
    m_items << buildItem(muse::qtrc("appshell", "Home"), HOME_PAGE, true);
    m_items << buildItem(muse::qtrc("appshell", "Project"), PROJECT_PAGE, false);
    // m_items << buildItem(muse::qtrc("appshell", "Publish"), PUBLISH_PAGE, false);

    //! The developer tools are not a place to end up by mistake while working,
    //! so they are no longer a tab beside Home and Project. Help > Diagnostics
    //! opens them, after asking.

    endResetModel();

    updateNotationPageItem();
    context()->currentProjectChanged().onNotify(this, [this]() {
        //! GlideMix: the tab follows the project's name and its unsaved mark.
        if (auto project = context()->currentProject()) {
            project->displayNameChanged().onNotify(this, [this]() {
                updateNotationPageItem();
            }, muse::async::Asyncable::Mode::SetReplace);
            project->needSave().notification.onNotify(this, [this]() {
                updateNotationPageItem();
            }, muse::async::Asyncable::Mode::SetReplace);
        }
        updateNotationPageItem();
    });
}

//! GlideMix (Chris, 2026-11-14): the Project tab shows the open project's name
//! ("Fluid *" while unsaved) -- that is where the name belongs; the Mac title
//! bar no longer repeats it; a new one shows "Untitled project *". No project: "Project".
QString MainToolBarModel::projectTabTitle() const
{
    const auto project = context()->currentProject();
    if (!project) {
        return muse::qtrc("appshell", "Project");
    }
    // A project never saved has no title yet; its file name is an automatic
    // "New Project <date> N-1.aup4", so it reads "New Mix".
    QString name = project->title().toQString();
    if (name.isEmpty()) {
        name = muse::qtrc("appshell", "New Mix");
    }
    constexpr int kMaxChars = 40;
    if (name.size() > kMaxChars) {
        name = name.left(kMaxChars - 1) + QChar(0x2026);
    }
    return project->hasUnsavedChanges() ? name + QStringLiteral(" *") : name;
}

void MainToolBarModel::updateNotationPageItem()
{
    for (int i = 0; i < m_items.size(); ++i) {
        QVariantMap& item = m_items[i];

        if (item[URI_KEY] == PROJECT_PAGE) {
            item[ENABLED_KEY] = context()->currentProject() != nullptr;
            item[IS_TITLE_BOLD_KEY] = context()->currentProject() != nullptr;
            item[TITLE_KEY] = projectTabTitle();

            QModelIndex modelIndex = index(i);
            emit dataChanged(modelIndex, modelIndex, { TitleRole, IsTitleBoldRole, EnabledRole });

            break;
        }
    }
}

QVariantList MainToolBarModel::projectMenuItems() const
{
    const auto project = context()->currentProject();
    const bool unsaved = project && project->hasUnsavedChanges();
    auto item = [](const QString& id, const QString& title, bool enabled = true) {
        return QVariantMap { { "id", id }, { "title", title }, { "enabled", enabled } };
    };
    return {
        item("file-save", muse::qtrc("appshell", "Save"), unsaved || (project && project->isNewlyCreated())),
        item("file-save-as", muse::qtrc("appshell", "Save as…")),
        item("glidemix-project-rename", muse::qtrc("appshell", "Rename…")),
        QVariantMap {},   // separator
        item("glidemix-project-delete", muse::qtrc("appshell", "Delete…")),
    };
}

void MainToolBarModel::handleProjectMenuItem(const QString& itemId)
{
    if (itemId.isEmpty() || !context()->currentProject()) {
        return;
    }
    // Queued: Rename and Delete close the project, which tears down the page
    // this menu lives in -- not while its click is still being handled.
    const std::string code = itemId.toStdString();
    QMetaObject::invokeMethod(this, [this, code]() {
        dispatcher()->dispatch(code);
    }, Qt::QueuedConnection);
}
