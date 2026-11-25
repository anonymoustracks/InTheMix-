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
#include "firstlaunchsetupmodel.h"

#include "global/translation.h"
#include "global/async/async.h"
#include "global/settings.h"

namespace {
const char* THEMES_PAGE = "ThemesPage.qml";
[[maybe_unused]] const char* CLIP_VISUALIZATION_PAGE = "ClipVisualizationPage.qml";
const char* WORKSPACE_LAYOUT_PAGE = "WorkspaceLayoutPage.qml";
// GlideMix onboarding (docs/onboarding-plan.md, Chris 2026-11-13).
const char* WAVEFORM_LOOK_PAGE = "WaveformLookPage.qml";
const char* YOUR_MUSIC_PAGE = "YourMusicPage.qml";
const char* STEMS_ENGINE_PAGE = "StemsEnginePage.qml";
const char* SOUND_CLOCK_PAGE = "SoundClockPage.qml";
const char* PLUGINS_PAGE = "PluginsPage.qml";
}

using namespace muse;
using namespace au::appshell;

FirstLaunchSetupModel::FirstLaunchSetupModel(QObject* parent)
    : QObject(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    m_pages = {
        Page { THEMES_PAGE, "audacity://project" },
        // GlideMix: the waveform look replaces Colourful / Classic (CLIP_VISUALIZATION_PAGE).
        Page { WAVEFORM_LOOK_PAGE, "audacity://project" },
        Page { YOUR_MUSIC_PAGE, "audacity://project" },
        Page { STEMS_ENGINE_PAGE, "audacity://project" },
        Page { SOUND_CLOCK_PAGE, "audacity://project" },
        Page { PLUGINS_PAGE, "audacity://project" },
        Page { WORKSPACE_LAYOUT_PAGE, "audacity://project" },
    };
}

void FirstLaunchSetupModel::load()
{
    if (m_loaded) {
        return;
    }

    // GlideMix: after "Restart now" for a new Interface size the wizard goes on
    // where it was (OnboardingSettingsModel::restartToApplySize), once.
    const muse::Settings::Key resumeKey("appshell", "appshell/onboardingResumePage");
    const int resume = muse::settings()->value(resumeKey).toInt();
    muse::settings()->setSharedValue(resumeKey, muse::Val(0));
    setCurrentPageIndex(resume > 0 && resume < int(m_pages.size()) ? resume : 0);
    m_loaded = true;
}

int FirstLaunchSetupModel::numberOfPages() const
{
    return m_pages.size();
}

int FirstLaunchSetupModel::currentPageIndex() const
{
    return m_currentPageIndex;
}

QVariantMap FirstLaunchSetupModel::Page::toMap() const
{
    return {
        { "url", m_url },
        { "properties", m_properties },
    };
}

QVariantMap FirstLaunchSetupModel::currentPage() const
{
    if (m_currentPageIndex < 0 || m_currentPageIndex >= m_pages.size()) {
        return {};
    }

    return m_pages.at(m_currentPageIndex).toMap();
}

bool FirstLaunchSetupModel::canGoBack() const
{
    return m_currentPageIndex > 0;
}

bool FirstLaunchSetupModel::canGoForward() const
{
    return m_currentPageIndex < m_pages.size() - 1;
}

bool FirstLaunchSetupModel::canFinish() const
{
    return m_currentPageIndex == m_pages.size() - 1;
}

void FirstLaunchSetupModel::setCurrentPageIndex(int index)
{
    if (index == m_currentPageIndex || index < 0 || index >= m_pages.size()) {
        return;
    }

    m_currentPageIndex = index;
    emit currentPageChanged();
    emit nextButtonTextChanged();

    async::Async::call(this, [this]() {
        interactive()->open(m_pages.at(m_currentPageIndex).m_backgroundUri);
    });
}

QString FirstLaunchSetupModel::dialogTitle() const
{
    return muse::qtrc("appshell/gettingstarted", "Getting started");
}

QString FirstLaunchSetupModel::backButtonText() const
{
    return muse::qtrc("global", "Back");
}

QString FirstLaunchSetupModel::nextButtonText() const
{
    if (m_currentPageIndex < 0 || m_currentPageIndex >= m_pages.size()) {
        return "";
    }

    return !canFinish() ? muse::qtrc("global", "Next") : muse::qtrc("appshell/gettingstarted", "Finish");
}

QString FirstLaunchSetupModel::formatPageProgress(int current, int total) const
{
    //: %1 is the current page number, %2 is the total number of pages
    return muse::qtrc("appshell/gettingstarted", "%1 of %2").arg(current).arg(total);
}

void FirstLaunchSetupModel::finish()
{
    configuration()->setHasCompletedFirstLaunchSetup(true);
}
