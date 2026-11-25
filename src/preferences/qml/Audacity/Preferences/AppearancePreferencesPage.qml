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
import QtQuick 2.15

import Muse.Ui 1.0
import Muse.UiComponents

import Audacity.AppShell
import Audacity.Preferences
import Audacity.UiComponents 1.0

PreferencesPage {
    id: root

    property int navigationOrderStart: 0

    AppearancePreferencesModel {
        id: appearanceModel
    }

    Component.onCompleted: {
        appearanceModel.init()
    }

    // GlideMix: the workspace (lane) background, the same setting as
    // Waveforms > Lane background.
    ArrangementPreferencesModel {
        id: arrangementModel
    }

    Column {
        width: parent.width
        spacing: root.sectionsSpacing

        ThemesSection {
            width: parent.width

            themes: appearanceModel.highContrastEnabled ? appearanceModel.highContrastThemes : appearanceModel.generalThemes
            currentThemeCode: appearanceModel.currentThemeCode
            highContrastEnabled: appearanceModel.highContrastEnabled
            isFollowSystemThemeAvailable: appearanceModel.isFollowSystemThemeAvailable
            isFollowSystemTheme: appearanceModel.isFollowSystemTheme
            accentColors: appearanceModel.accentColors
            currentAccentColorIndex: appearanceModel.currentAccentColorIndex

            navigation.section: root.navigationSection
            navigation.order: root.navigationOrderStart

            onThemeChangeRequested: function (newThemeCode) {
                appearanceModel.currentThemeCode = newThemeCode
            }

            onHighContrastChangeRequested: function (enabled) {
                appearanceModel.highContrastEnabled = enabled
            }

            onSetFollowSystemThemeRequested: function (enabled) {
                appearanceModel.isFollowSystemTheme = enabled
            }

            onAccentColorChangeRequested: function (newColorIndex) {
                appearanceModel.currentAccentColorIndex = newColorIndex
            }

            onCustomAccentColorPicked: function (newColor) {
                appearanceModel.setNewColor(newColor, AppearancePreferencesModel.AccentColor)
            }

            onFocusChanged: {
                if (activeFocus) {
                    root.ensureContentVisibleRequested(Qt.rect(x, y, width, height))
                }
            }

            onEnsureContentVisibleRequested: function (contentRect) {
                root.ensureContentVisibleRequested(contentRect)
            }
        }

        SeparatorLine {}

        BaseSection {
            id: workspaceBackgroundSection

            title: qsTrc("preferences", "Workspace background")

            navigation.section: root.navigationSection
            navigation.order: root.navigationOrderStart + 3

            onFocusChanged: {
                if (activeFocus) {
                    root.ensureContentVisibleRequested(Qt.rect(x, y, width, height))
                }
            }

            ComboBoxWithTitle {
                title: qsTrc("preferences", "Lane background:")

                columnWidth: 280
                currentIndex: arrangementModel.laneBackground
                model: [
                    { text: qsTrc("preferences", "Follow the waveform style"), value: 0 },
                    { text: qsTrc("preferences", "Black"), value: 1 },
                    { text: qsTrc("preferences", "Theme"), value: 2 },
                    { text: qsTrc("preferences", "Charcoal"), value: 3 },
                    { text: qsTrc("preferences", "Grey"), value: 4 }
                ]
                textRole: "text"
                valueRole: "value"

                onValueEdited: function(newIndex, newValue) {
                    arrangementModel.laneBackground = newValue
                }

                navigation.name: "AppearanceLaneBackgroundCombo"
                navigation.panel: workspaceBackgroundSection.navigation
                navigation.row: 0
            }

            StyledTextLabel {
                width: parent.width
                horizontalAlignment: Qt.AlignLeft
                wrapMode: Text.WordWrap
                opacity: 0.7
                text: qsTrc("preferences", "The timeline behind the songs, whatever the theme. Saved with the workspace; also in Waveforms > Lane background.")
            }
        }

        SeparatorLine {
            visible: uiColorsSection.visible
        }

        UiColorsSection {
            id: uiColorsSection

            width: parent.width

            visible: appearanceModel.highContrastEnabled

            navigation.section: root.navigationSection
            navigation.order: root.navigationOrderStart + 1

            onColorChangeRequested: function (newColor, propertyType) {
                appearanceModel.setNewColor(newColor, propertyType)
            }

            onFocusChanged: {
                if (activeFocus) {
                    root.ensureContentVisibleRequested(Qt.rect(x, y, width, height))
                }
            }
        }

        SeparatorLine {}

        ClipStyleSection {
            currentClipStyle: appearanceModel.clipStyle

            navigation.section: root.navigationSection
            navigation.order: root.navigationOrderStart + 2

            onClipStyleChangeRequested: function (clipStyle) {
                appearanceModel.setClipStyle(clipStyle)
            }

            onFocusChanged: {
                if (activeFocus) {
                    root.ensureContentVisibleRequested(Qt.rect(x, y, width, height))
                }
            }
        }

        SeparatorLine {}

        UiFontSection {
            allFonts: appearanceModel.allFonts()
            currentFontIndex: appearanceModel.currentFontIndex
            bodyTextSize: appearanceModel.bodyTextSize

            navigation.section: root.navigationSection
            navigation.order: root.navigationOrderStart + 4

            onFontChangeRequested: function (newFontIndex) {
                appearanceModel.currentFontIndex = newFontIndex
            }

            onBodyTextSizeChangeRequested: function (newBodyTextSize) {
                appearanceModel.bodyTextSize = newBodyTextSize
            }

            onFocusChanged: {
                if (activeFocus) {
                    root.ensureContentVisibleRequested(Qt.rect(x, y, width, height))
                }
            }
        }

        SeparatorLine {}

        // GlideMix: the whole window at 50-200 %, like Ableton's Display Zoom
        // (also View > Interface size and the first-start wizard).
        BaseSection {
            id: interfaceSizeSection
            title: qsTrc("preferences", "Interface size")
            navigation.section: root.navigationSection
            navigation.order: root.navigationOrderStart + 5

            OnboardingSettingsModel { id: interfaceSettings }

            Row {
                spacing: 12
                // 100 % in the middle: the left half is 50-100 %, the right half 100-200 %.
                StyledSlider {
                    width: 220
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0
                    to: 100
                    stepSize: 0
                    value: interfaceSettings.interfaceSize <= 100 ? interfaceSettings.interfaceSize - 50
                                                                  : 50 + (interfaceSettings.interfaceSize - 100) / 2
                    onMoved: {
                        var size = value <= 50 ? 50 + value : 100 + (value - 50) * 2
                        interfaceSettings.interfaceSize = Math.round(size / 5) * 5
                    }
                }
                StyledTextLabel {
                    width: 48
                    anchors.verticalCenter: parent.verticalCenter
                    font: ui.theme.bodyBoldFont
                    text: interfaceSettings.interfaceSize + " %"
                }
                FlatButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrc("preferences", "Reset to 100 %")
                    enabled: interfaceSettings.interfaceSize !== 100
                    navigation.name: "InterfaceSizeReset"
                    navigation.panel: interfaceSizeSection.navigation
                    navigation.row: 0
                    onClicked: interfaceSettings.interfaceSize = 100
                }
                FlatButton {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: interfaceSettings.interfaceSize !== interfaceSettings.runningInterfaceSize
                    accentButton: true
                    text: qsTrc("preferences", "Restart now")
                    onClicked: interfaceSettings.restartToApplySize(0)
                }
            }
            StyledTextLabel {
                width: 420
                horizontalAlignment: Text.AlignLeft
                wrapMode: Text.WordWrap
                opacity: 0.75
                text: interfaceSettings.interfaceSize !== interfaceSettings.runningInterfaceSize
                      ? qsTrc("preferences", "Applies the next time InTheMix starts.")
                      : qsTrc("preferences", "The whole window bigger or smaller. On Windows, the display scaling in Windows settings applies on top.")
            }
        }
    }
}
