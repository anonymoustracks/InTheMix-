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

#include "appearancepreferencesmodel.h"

#include "ui/internal/themeconverter.h"

#include "log.h"
#include "translation.h"

using namespace au::appshell;
using namespace muse::ui;

static constexpr int INVALID_INDEX = -1;

AppearancePreferencesModel::AppearancePreferencesModel(QObject* parent)
    : QObject(parent)
{
}

void AppearancePreferencesModel::init()
{
    uiConfiguration()->isFollowSystemTheme().notification.onNotify(this, [this]() {
        emit isFollowSystemThemeChanged();
    });

    uiConfiguration()->currentThemeChanged().onNotify(this, [this]() {
        emit themesChanged();
    });

    uiConfiguration()->fontChanged().onNotify(this, [this]() {
        emit currentFontIndexChanged();
        emit bodyTextSizeChanged();
    });

    projectSceneConfiguration()->clipStyleChanged().onReceive(this, [this](const ClipStyles::Style& style){
        Q_UNUSED(style);
        emit clipStyleChanged();
    });
}

bool AppearancePreferencesModel::isFollowSystemThemeAvailable() const
{
    return uiConfiguration()->isFollowSystemThemeAvailable();
}

bool AppearancePreferencesModel::isFollowSystemTheme() const
{
    return uiConfiguration()->isFollowSystemTheme().val;
}

void AppearancePreferencesModel::setFollowSystemTheme(bool enabled)
{
    if (enabled == isFollowSystemTheme()) {
        return;
    }

    uiConfiguration()->setFollowSystemTheme(enabled);
}

bool AppearancePreferencesModel::highContrastEnabled() const
{
    return uiConfiguration()->isHighContrast();
}

QVariantList AppearancePreferencesModel::generalThemes() const
{
    QVariantList result;

    for (const ThemeInfo& theme: allThemes()) {
        if (theme.codeKey == LIGHT_THEME_CODE || theme.codeKey == DARK_THEME_CODE
            || theme.codeKey == CHARCOAL_THEME_CODE) {
            result << ThemeConverter::toMap(theme);
        }
    }

    return result;
}

QVariantList AppearancePreferencesModel::highContrastThemes() const
{
    QVariantList result;

    for (const ThemeInfo& theme : allThemes()) {
        if (theme.codeKey == HIGH_CONTRAST_WHITE_THEME_CODE || theme.codeKey == HIGH_CONTRAST_BLACK_THEME_CODE) {
            result << ThemeConverter::toMap(theme);
        }
    }

    return result;
}

QStringList AppearancePreferencesModel::accentColors() const
{
    // GlideMix (Chris, 2026-11-15: "more vibrant colors"): a bright row on
    // top, the same as the clip colour palette's (projectsceneconfiguration).
    // The accent is saved as a colour, not a place in this list.
    static const QStringList vibrant {
        "#E3121B", "#FF4F19", "#FF7719", "#FFA019", "#FFD019", "#FFFE19", "#D1FF19", "#A2FF19", "#75FF19", "#19FF43", "#19FF74", "#19FFA3", "#19FFD1",
        "#19FEFF", "#19D2FF", "#19B3FF", "#1988FF", "#1957FF", "#1D19FF", "#4519FF", "#7619FF", "#A219FF", "#C919FF", "#FE19FF", "#FF19CA"
    };
    // GlideMix (Chris, 2026-11-21): and the Creative set, as in the clip colour
    // picker (projectsceneconfiguration STUDIO_COLOR_PALETTE), last.
    static const QStringList studio {
        "#FB6840", "#ECA846", "#EDD75E", "#F6F18C", "#EFE3B8", "#D9CDB8", "#CDB6AB", "#EA7930", "#E2AA3A", "#C2CD41", "#9CDF48", "#6FF364", "#B6F9A4", "#DDDDC7",
        "#BEC99D", "#34A93C", "#30C343", "#36D95D", "#4CED8C", "#7BFEBD", "#B4F7E1", "#CBEDED", "#C0D7CC", "#8DC4A4", "#299F94", "#2DB7AC", "#41CBC0", "#5EB7D8",
        "#7D93ED", "#9E88F7", "#C7A5F3", "#D3BCDE", "#ACB8CE", "#9F50F8", "#D16CF9", "#E99BF2", "#F0C2E3", "#D6B5D5", "#FC549F", "#FC8B81", "#FED39D", "#F1DACF",
        "#DEBDC7"
    };
    return vibrant + uiConfiguration()->possibleAccentColors() + studio;
}

void AppearancePreferencesModel::resetAppearancePreferencesToDefault()
{
    uiConfiguration()->resetThemes();
    uiConfiguration()->resetFonts();
}

void AppearancePreferencesModel::setNewColor(const QColor& newColor, ColorType colorType)
{
    switch (colorType) {
    case AccentColor:
        uiConfiguration()->setCurrentThemeStyleValue(ThemeStyleKey::ACCENT_COLOR, muse::Val(newColor));
        break;
    case TextAndIconsColor:
        uiConfiguration()->setCurrentThemeStyleValue(ThemeStyleKey::FONT_PRIMARY_COLOR, muse::Val(newColor));
        break;
    case BorderColor:
        uiConfiguration()->setCurrentThemeStyleValue(ThemeStyleKey::STROKE_COLOR, muse::Val(newColor));
        break;
    }

    emit themesChanged();
}

QStringList AppearancePreferencesModel::allFonts() const
{
    return uiConfiguration()->possibleFontFamilies();
}

ClipStyles::Style AppearancePreferencesModel::clipStyle() const
{
    return projectSceneConfiguration()->clipStyle();
}

void AppearancePreferencesModel::setClipStyle(ClipStyles::Style style)
{
    if (projectSceneConfiguration()->clipStyle() == style) {
        return;
    }

    projectSceneConfiguration()->setClipStyle(style);
}

void AppearancePreferencesModel::setHighContrastEnabled(bool enabled)
{
    if (highContrastEnabled() == enabled) {
        return;
    }

    uiConfiguration()->setIsHighContrast(enabled);
}

QString AppearancePreferencesModel::currentThemeCode() const
{
    return QString::fromStdString(currentTheme().codeKey);
}

int AppearancePreferencesModel::currentAccentColorIndex() const
{
    QStringList allColors = accentColors();
    QString color = currentTheme().values[ACCENT_COLOR].toString().toLower();

    for (int i = 0; i < static_cast<int>(allColors.size()); ++i) {
        if (allColors[i].toLower() == color) {
            return i;
        }
    }

    return INVALID_INDEX;
}

ThemeInfo AppearancePreferencesModel::currentTheme() const
{
    return uiConfiguration()->currentTheme();
}

ThemeList AppearancePreferencesModel::allThemes() const
{
    return uiConfiguration()->themes();
}

int AppearancePreferencesModel::currentFontIndex() const
{
    QString currentFont = QString::fromStdString(uiConfiguration()->fontFamily());
    return allFonts().indexOf(currentFont);
}

int AppearancePreferencesModel::bodyTextSize() const
{
    return uiConfiguration()->fontSize(FontSizeType::BODY);
}

void AppearancePreferencesModel::setCurrentThemeCode(const QString& themeCode)
{
    if (themeCode == currentThemeCode() && !isFollowSystemTheme()) {
        return;
    }

    uiConfiguration()->setCurrentTheme(themeCodeFromString(themeCode));
}

void AppearancePreferencesModel::setCurrentAccentColorIndex(int index)
{
    if (index < 0 || index >= accentColors().size()) {
        return;
    }

    if (index == currentAccentColorIndex()) {
        return;
    }

    QColor color = accentColors()[index];
    uiConfiguration()->setCurrentThemeStyleValue(ThemeStyleKey::ACCENT_COLOR, muse::Val(color));
}

void AppearancePreferencesModel::setCurrentFontIndex(int index)
{
    QStringList fonts = allFonts();

    if (index < 0 || index >= fonts.size()) {
        return;
    }

    uiConfiguration()->setFontFamily(fonts[index].toStdString());
    emit currentFontIndexChanged();
}

void AppearancePreferencesModel::setBodyTextSize(int size)
{
    if (size == bodyTextSize() || size <= 0) {
        return;
    }

    uiConfiguration()->setBodyFontSize(size);
    emit bodyTextSizeChanged();
}
