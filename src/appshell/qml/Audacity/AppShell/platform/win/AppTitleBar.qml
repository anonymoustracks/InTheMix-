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
import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import Muse.Ui
import Muse.UiComponents

import Audacity.AppShell

Rectangle {
    id: root

    color: ui.theme.backgroundPrimaryColor

    property alias title: titleTextmetrics.text
    property rect titleMoveAreaRect: Qt.rect(titleMoveArea.x, titleMoveArea.y, titleMoveArea.width, titleMoveArea.height)

    property int windowVisibility: Window.Windowed

    property alias appWindow: menu.appWindow

    signal showWindowMinimizedRequested
    signal toggleWindowMaximizedRequested
    signal closeWindowRequested

    height: content.childrenRect.height

    RowLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right

        spacing: 8

        AppMenuBar {
            id: menu

            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            Layout.fillWidth: menu.truncated ? true : false
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight

            availableWidth: root.width - (content.spacing + titleTextmetrics.width + content.spacing + systemButtons.width)
        }

        StyledTextLabel {
            id: titleLabel

            Layout.fillWidth: !menu.truncated ? true : false
            Layout.fillHeight: true
            Layout.minimumWidth: titleTextmetrics.advanceWidth

            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            leftPadding: {
                var parentCenterX = parent.width / 2
                var expectedTextCenterX = parentCenterX - titleTextmetrics.width / 2
                if (expectedTextCenterX > x) {
                    return expectedTextCenterX - x
                }

                return 0
            }

            // InTheMix: the Anonymous Tracks mark, the name, and "by Anonymous Tracks" in small.
            text: "<img src=\"qrc:/resources/atLogo.png\" width=\"16\" height=\"16\" style=\"vertical-align:middle\">&nbsp;&nbsp;InTheMix"
                  + "&nbsp;&nbsp;<span style=\"font-family:'Manrope'; font-size:8pt; font-weight:500; color:#9A968F\">by Anonymous Tracks</span>"
            textFormat: Text.RichText
            // InTheMix: the name in Anonymous Tracks' display face.
            font.family: "Unbounded"
            font.pixelSize: 13   // InTheMix: ui.theme.defaultFont is undefined here (TypeError in the log)
            font.weight: Font.DemiBold

            visible: root.windowVisibility !== Window.FullScreen

            TextMetrics {
                id: titleTextmetrics

                // As wide as the title above (mark, name, "by Anonymous Tracks"), for centring.
                text: "MM  InTheMix  by Anonymous Tr"
                font: titleLabel.font
                elide: Qt.ElideRight
                elideWidth: titleLabel.width
            }
        }

        AppSystemButtons {
            id: systemButtons

            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            Layout.preferredWidth: width
            Layout.preferredHeight: height
            Layout.minimumWidth: width

            windowIsMiximized: root.windowVisibility === Window.Maximized

            visible: root.windowVisibility !== Window.FullScreen

            onShowWindowMinimizedRequested: {
                root.showWindowMinimizedRequested()
            }

            onToggleWindowMaximizedRequested: {
                root.toggleWindowMaximizedRequested()
            }

            onCloseWindowRequested: {
                root.closeWindowRequested()
            }
        }
    }

    Item {
        id: titleMoveArea

        x: titleLabel.x
        y: titleLabel.y
        width: titleLabel.visible ? titleLabel.width : 0
        height: titleLabel.visible ? titleLabel.height : 0
    }
}
