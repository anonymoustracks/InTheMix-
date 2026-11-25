/*
 * GlideMix
 *
 * A StyledSlider in the DJ-fader look, modern version (2026-11-09, after
 * mock A of 2026-10-28): a thin rounded slot with the value in the accent
 * colour, faint ticks, and a light rounded cap with a dark centre line. (A
 * dark cap vanished against the dark track headers.) Behaviour
 * (drag, wheel, keys, accessibility) is StyledSlider's.
 */
import QtQuick
import QtQuick.Controls

import Muse.Ui
import Muse.UiComponents

StyledSlider {
    id: root

    // Ticks above and below the slot (off for short sliders).
    property bool ticks: availableWidth >= 120
    // Where the slot's value colour starts: the left end, or a value (the
    // middle of a bipolar control).
    property real fillOrigin: from
    // GlideMix: the filled line's colour (a track's own colour on its fader); the accent by default.
    property color fillColor: ui.theme.accentColor

    // GlideMix (Chris, 2026-11-21): the reference DAW's fader -- a white dot on an inset pill groove.
    readonly property real capWidth: 14
    readonly property color tickColor: Qt.rgba(ui.theme.fontPrimaryColor.r, ui.theme.fontPrimaryColor.g,
                                               ui.theme.fontPrimaryColor.b, 0.25)
    readonly property real capHeight: 14
    readonly property real grooveHeight: Math.max(6, Math.min(10, availableHeight - 8))

    implicitHeight: 22

    background: Item {
        x: root.leftPadding
        y: root.topPadding
        width: root.availableWidth
        height: root.availableHeight

        NavigationFocusBorder {
            navigationCtrl: root.navigation
        }

        readonly property real travel: width - root.capWidth
        readonly property real originX: root.capWidth / 2
                                        + travel * Math.max(0, Math.min(1, (root.fillOrigin - root.from) / (root.to - root.from)))
        readonly property real valueX: root.capWidth / 2 + travel * root.visualPosition

        Repeater {
            model: root.ticks ? 11 : 0
            delegate: Item {
                required property int index
                readonly property real tx: root.capWidth / 2 + parent.travel * index / 10
                Rectangle {
                    x: tx - 0.5
                    y: 0
                    width: 1
                    height: index % 5 === 0 ? 4 : 2
                    color: root.tickColor
                }
                Rectangle {
                    x: tx - 0.5
                    y: parent.parent.height - height
                    width: 1
                    height: index % 5 === 0 ? 4 : 2
                    color: root.tickColor
                }
            }
        }

        // The groove: a pill a little darker than the panel, its top edge darker still (inset).
        Rectangle {
            id: groove
            anchors.verticalCenter: parent.verticalCenter
            x: 0
            width: parent.width
            height: root.grooveHeight
            radius: height / 2
            color: ui.theme.isDark ? "#1F2024" : Qt.rgba(0, 0, 0, 0.10)
            border.width: 1
            border.color: ui.theme.isDark ? "#2A2B2F" : Qt.rgba(0, 0, 0, 0.14)

            Rectangle {
                x: parent.radius
                y: 0
                width: parent.width - 2 * parent.radius
                height: 1
                color: ui.theme.isDark ? "#16171A" : Qt.rgba(0, 0, 0, 0.22)
            }
        }
        // The value: a line in the track's (or accent) colour along the groove.
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.fillBackground
            x: Math.min(parent.originX, parent.valueX)
            width: Math.abs(parent.valueX - parent.originX)
            height: 3
            radius: 1.5
            color: root.enabled ? root.fillColor : Qt.rgba(ui.theme.fontPrimaryColor.r, ui.theme.fontPrimaryColor.g,
                                                                ui.theme.fontPrimaryColor.b, 0.3)
        }
    }

    // The dot: white, brighter on hover and while held.
    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + (root.availableHeight - height) / 2
        width: root.capWidth
        height: root.capHeight
        radius: width / 2
        color: (root.hovered || root.pressed) ? "#FFFFFF" : "#DDDDDD"
        border.width: ui.theme.isDark ? 0 : 1
        border.color: Qt.rgba(0, 0, 0, 0.25)
    }
}
