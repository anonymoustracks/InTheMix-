/*
 * GlideMix: the right half of the capsule display at the top of the project
 * window -- key, energy and place in the mix of the song under the playhead,
 * the processor meter and the play light. The clocks (time, BPM, signature)
 * sit to its left in the same capsule (ProjectPage.qml).
 */
import QtQuick

import Muse.Ui
import Muse.UiComponents

import Audacity.MixHouse

Row {
    id: root

    spacing: 0
    height: 35

    MixHouseNowPlayingModel {
        id: np
    }

    // GlideMix: the DJ clock -- which readouts show (the clock's ▾ menu > Show).
    MixHouseClockDisplays {
        id: displays
    }
    ContextMenuLoader {
        id: readoutMenu
        onHandleMenuItem: function (itemId) {
            displays.handleMenuItem(itemId)
        }
    }
    function on(id) { return displays.shown.indexOf(id) >= 0 && !dropped(id) }

    // Room in the header (the window less what sits left and right of the
    // capsule). When the readouts don't fit -- a big Interface size, a laptop
    // screen, Complete -- the least needed ones step aside in this order and
    // come back when there is room again. Song time, bar, next mix, key and
    // song always stay (Chris: the header crowded at 125 %+).
    property real maxWidth: Infinity
    readonly property var dropOrder: ["memory", "disk", "cpu", "energy", "mixtotal", "phrase", "tempo"]
    property int droppedCount: 0
    function dropped(id) {
        const i = dropOrder.indexOf(id)
        return i >= 0 && i < droppedCount
    }
    function fit() {
        if (width > maxWidth && droppedCount < dropOrder.length) {
            droppedCount++
            Qt.callLater(fit)
        }
    }
    function refit() {
        droppedCount = 0
        Qt.callLater(fit)
    }
    onMaxWidthChanged: refit()
    onWidthChanged: Qt.callLater(fit)
    Connections {
        target: displays
        function onShownChanged() { root.refit() }
    }

    function clockText(sec) {
        const s = Math.max(0, Math.floor(sec))
        const h = Math.floor(s / 3600)
        const m = Math.floor((s % 3600) / 60)
        const ss = s % 60
        const two = function (n) { return n < 10 ? "0" + n : "" + n }
        return h > 0 ? h + ":" + two(m) + ":" + two(ss) : m + ":" + two(ss)
    }
    readonly property color warnColor: "#FF5A4E"

    // Steady readouts while they count (Chris: "clocks are shaking"): every
    // digit equally wide (OpenType tabular figures), and each readout only
    // grows while one song plays -- a narrower value or label no longer
    // shifts every readout to its right. The widths start over per song.
    function tabular(f) {
        const spec = { family: f.family, weight: f.weight, letterSpacing: f.letterSpacing, features: { "tnum": 1 } }
        if (f.pixelSize > 0) {
            spec.pixelSize = f.pixelSize
        } else {
            spec.pointSize = f.pointSize
        }
        return Qt.font(spec)
    }
    readonly property font valueFont: tabular(ui.theme.bodyBoldFont)
    readonly property int widthSong: np.songIndex

    // The capsule's colour: the clock field is dark in every theme, so the
    // text follows the capsule's brightness, not the theme.
    property color background: ui.theme.backgroundQuarternaryColor
    readonly property bool darkBackground: 0.299 * background.r + 0.587 * background.g + 0.114 * background.b < 0.5
    readonly property color ink: darkBackground ? "#ECECEF" : "#1A1C1F"
    readonly property color labelColor: Qt.rgba(ink.r, ink.g, ink.b, 0.5)
    readonly property color lineColor: Qt.rgba(ink.r, ink.g, ink.b, 0.15)

    component Divider: Rectangle {
        width: 1
        height: 22
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        color: root.lineColor
    }

    component Segment: Item {
        id: seg
        property string label: ""
        //! Flashes red like a DJ deck near the end of a track.
        property bool flash: false
        default property alias content: valueBox.data
        //! The widest value and label this readout normally shows (digits are
        //! tabular, so "8" stands for any digit): its room is kept from the
        //! start, so INTRO -> CHORUS or 9 -> 10 BARS never pushes the readouts
        //! to its right (Chris, 2026-11-14: the clock still shook on phrases).
        property string reserveValue: ""
        property string reserveLabel: ""
        TextMetrics {
            id: reserveValueMetrics
            font: root.valueFont
            text: seg.reserveValue
        }
        TextMetrics {
            id: reserveLabelMetrics
            font: labelText.font
            text: seg.reserveLabel
        }
        readonly property real reservedWidth: Math.max(seg.reserveValue !== "" ? reserveValueMetrics.advanceWidth : 0,
                                                       seg.reserveLabel !== "" ? reserveLabelMetrics.advanceWidth : 0)
        readonly property real neededWidth: Math.max(valueBox.childrenRect.width, labelText.implicitWidth, reservedWidth) + 28
        property real steadyWidth: 0
        onNeededWidthChanged: if (neededWidth > steadyWidth) steadyWidth = neededWidth
        onVisibleChanged: steadyWidth = 0
        Connections {
            target: root
            function onWidthSongChanged() { seg.steadyWidth = 0 }
        }
        width: Math.max(neededWidth, steadyWidth)
        height: root.height

        // Right-click any readout: Flash near the end, and the Show list.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: function (e) {
                const p = mapToItem(root, e.x, e.y)
                readoutMenu.show(Qt.point(p.x, p.y), displays.contextMenuItems)
            }
        }
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 2
            anchors.bottomMargin: 2
            radius: 4
            color: root.warnColor
            visible: seg.flash
            SequentialAnimation on opacity {
                running: seg.flash
                loops: Animation.Infinite
                NumberAnimation { to: 0.85; duration: 120 }
                PauseAnimation { duration: 330 }
                NumberAnimation { to: 0.15; duration: 120 }
                PauseAnimation { duration: 330 }
            }
        }
        Item {
            id: valueBox
            anchors.horizontalCenter: parent.horizontalCenter
            y: 3
            width: childrenRect.width
            height: 17
        }
        StyledTextLabel {
            id: labelText
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            text: seg.label
            font.pixelSize: 9
            font.bold: true
            font.letterSpacing: 0.8
            font.features: { "tnum": 1 }
            color: seg.flash ? "#FFFFFF" : root.labelColor
        }
    }

    Row {
        visible: root.on("mixtotal")
        Divider {}
        Segment {
            label: qsTrc("mixhouse", "MIX LENGTH")
            reserveValue: "8:88:88"
            StyledTextLabel {
                text: np.mixTotal > 0 ? root.clockText(np.mixTotal) : "—"
                font: root.valueFont
                color: root.ink
            }
        }
    }

    // The song under the playhead: remaining / elapsed / both (click to switch),
    // a progress strip, red in the last 30 s before the next mix.
    Row {
        visible: root.on("songtime")
        Divider {}
        Segment {
            flash: displays.flash && np.endWarning && np.playing
            label: displays.songTimeMode === 1 ? qsTrc("mixhouse", "SONG ELAPSED")
                   : displays.songTimeMode === 2 ? qsTrc("mixhouse", "SONG TIME")
                   : qsTrc("mixhouse", "SONG LEFT")
            reserveLabel: qsTrc("mixhouse", "SONG ELAPSED")
            reserveValue: displays.songTimeMode === 2 ? "88:88 / -88:88" : "-88:88"
            Item {
                width: Math.max(64, songTimeText.implicitWidth)
                height: 17
                StyledTextLabel {
                    id: songTimeText
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: !np.hasSong ? "—"
                          : displays.songTimeMode === 1 ? root.clockText(np.songElapsed)
                          : displays.songTimeMode === 2 ? root.clockText(np.songElapsed) + " / -" + root.clockText(np.songRemaining)
                          : "-" + root.clockText(np.songRemaining)
                    font: root.valueFont
                    color: displays.flash && np.endWarning && np.playing ? "#FFFFFF"
                           : np.endWarning ? root.warnColor : root.ink
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: -2
                    width: parent.width
                    height: 2
                    radius: 1
                    color: root.lineColor
                    Rectangle {
                        width: parent.width * np.songProgress
                        height: parent.height
                        radius: 1
                        color: np.endWarning || np.mixWarning ? root.warnColor
                               : (np.phraseColor.valid ? np.phraseColor : ui.theme.accentColor)
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: displays.cycleSongTimeMode()
                }
            }
        }
    }

    // Bar and beat in the song's own grid, with a dot per beat of the bar.
    Row {
        visible: root.on("barbeat")
        Divider {}
        Segment {
            label: np.bar > 0 ? qsTrc("mixhouse", "BAR %1").arg(np.bar) : qsTrc("mixhouse", "BAR")
            reserveLabel: qsTrc("mixhouse", "BAR %1").arg(888)
            Row {
                y: 5
                spacing: 3
                Repeater {
                    model: Math.min(np.beatsPerBar, 8)
                    Rectangle {
                        width: 7
                        height: 7
                        radius: 2
                        color: np.beat === index + 1 ? (index === 0 ? root.warnColor : ui.theme.accentColor)
                                                     : root.lineColor
                    }
                }
            }
        }
    }

    Row {
        visible: root.on("phrase")
        Divider {}
        Segment {
            label: np.phraseText !== "" ? (np.phraseBarsLeft === 1 ? qsTrc("mixhouse", "1 BAR LEFT") : qsTrc("mixhouse", "%1 BARS LEFT").arg(np.phraseBarsLeft))
                                         : qsTrc("mixhouse", "PHRASE")
            reserveLabel: qsTrc("mixhouse", "%1 BARS LEFT").arg(88)
            reserveValue: "CHORUS 8"
            StyledTextLabel {
                text: np.phraseText !== "" ? np.phraseText.toUpperCase() : "—"
                font: root.valueFont
                color: np.phraseColor.valid && np.phraseText !== ""
                       ? (root.darkBackground ? Qt.lighter(np.phraseColor, 1.3) : Qt.darker(np.phraseColor, 1.6))
                       : root.ink
            }
        }
    }

    // Countdown to the next transition -- the playing song's OUT anchor.
    Row {
        visible: root.on("nextmix")
        Divider {}
        Segment {
            flash: displays.flash && np.mixWarning && np.playing
            label: np.nextMixSec > 0 ? (np.nextMixBars === 1 ? qsTrc("mixhouse", "NEXT MIX · 1 BAR") : qsTrc("mixhouse", "NEXT MIX · %1 BARS").arg(np.nextMixBars))
                                     : qsTrc("mixhouse", "NEXT MIX")
            reserveLabel: qsTrc("mixhouse", "NEXT MIX · %1 BARS").arg(888)
            reserveValue: "-88:88"
            StyledTextLabel {
                text: np.nextMixSec < 0 ? "—" : np.nextMixSec === 0 ? qsTrc("mixhouse", "MIXING")
                                                                    : "-" + root.clockText(np.nextMixSec)
                font: root.valueFont
                color: displays.flash && np.mixWarning && np.playing ? "#FFFFFF"
                       : np.mixWarning || np.nextMixSec === 0 ? root.warnColor : root.ink
            }
        }
    }

    Row {
        visible: root.on("tempo")
        Divider {}
        Segment {
            label: np.songBpm > 0 ? (np.stretchPercent >= 0 ? "+" : "") + np.stretchPercent.toFixed(1) + " %"
                                  : qsTrc("mixhouse", "TEMPO")
            reserveLabel: "+88.8 %"
            reserveValue: "888.8"
            StyledTextLabel {
                text: np.songBpm > 0 ? np.songBpm.toFixed(1) : "—"
                font: root.valueFont
                color: root.ink
            }
        }
    }

    Row {
        visible: root.on("key")
        Divider {}

        Segment {
            label: qsTrc("mixhouse", "KEY")
            reserveValue: "88A"
            StyledTextLabel {
                text: np.keyText !== "" ? np.keyText : "—"
                font: root.valueFont
                color: !np.keyColor.valid || np.keyText === "" ? root.ink
                       : (root.darkBackground ? np.keyColor : Qt.darker(np.keyColor, 1.8))
            }
        }
    }

    Row {
        visible: root.on("energy")
        Divider {}

        Segment {
            label: np.energy > 0 ? qsTrc("mixhouse", "ENERGY %1").arg(np.energy) : qsTrc("mixhouse", "ENERGY")
            reserveLabel: qsTrc("mixhouse", "ENERGY %1").arg(88)
            Item {
                width: 48
                height: 15
                Repeater {
                    model: 10
                    Rectangle {
                        x: index * 5
                        width: 3
                        height: 6 + index
                        y: parent.height - height
                        radius: 1
                        color: index < np.energy ? (index < 7 ? "#5FD068" : "#E7C94A") : root.lineColor
                    }
                }
            }
        }
    }

    Row {
        visible: root.on("song")
        Divider {}

        Segment {
            label: qsTrc("mixhouse", "SONG")
            reserveValue: "88 / 88"
            StyledTextLabel {
                text: np.songCount > 0 ? (np.songIndex > 0 ? np.songIndex : "–") + " / " + np.songCount : "—"
                font: root.valueFont
                color: root.ink
            }
        }
    }

    Row {
        visible: root.on("cpu")
        Divider {}

        Segment {
            label: qsTrc("mixhouse", "CPU %1%").arg(np.cpuPercent)
            reserveLabel: qsTrc("mixhouse", "CPU %1%").arg(188)
            Rectangle {
                y: 6
                width: 46
                height: 4
                radius: 2
                color: root.lineColor
                Rectangle {
                    width: Math.max(2, parent.width * Math.min(np.cpuPercent, 100) / 100)
                    height: parent.height
                    radius: 2
                    color: np.cpuPercent >= 80 ? "#E2574C" : np.cpuPercent >= 50 ? "#E7C94A" : ui.theme.accentColor
                }
            }
        }
    }

    Row {
        visible: root.on("disk")
        Divider {}

        // What GlideMix itself reads from disk; the bar is full at 20 MB/s.
        Segment {
            label: np.diskMBps >= 10 ? qsTrc("mixhouse", "DISK %1 MB/s").arg(Math.round(np.diskMBps))
                                     : qsTrc("mixhouse", "DISK %1 MB/s").arg(np.diskMBps.toFixed(1))
            reserveLabel: qsTrc("mixhouse", "DISK %1 MB/s").arg("88.8")
            Rectangle {
                y: 6
                width: 46
                height: 4
                radius: 2
                color: root.lineColor
                Rectangle {
                    width: Math.max(2, parent.width * Math.min(np.diskMBps / 20, 1))
                    height: parent.height
                    radius: 2
                    color: np.diskMBps >= 16 ? "#E7C94A" : ui.theme.accentColor
                }
            }
        }
    }

    // GlideMix's own memory use; the bar is the share of the computer's memory.
    Row {
        visible: root.on("memory")
        Divider {}
        Segment {
            label: np.memoryGB >= 10 ? qsTrc("mixhouse", "MEMORY %1 GB").arg(Math.round(np.memoryGB))
                                     : qsTrc("mixhouse", "MEMORY %1 GB").arg(np.memoryGB.toFixed(1))
            reserveLabel: qsTrc("mixhouse", "MEMORY %1 GB").arg("88.8")
            Rectangle {
                y: 6
                width: 46
                height: 4
                radius: 2
                color: root.lineColor
                Rectangle {
                    width: Math.max(2, parent.width * Math.min(np.memoryShare, 1))
                    height: parent.height
                    radius: 2
                    color: np.memoryShare >= 0.8 ? "#E2574C" : np.memoryShare >= 0.5 ? "#E7C94A" : ui.theme.accentColor
                }
            }
        }
    }

    Item {
        width: 22
        height: root.height
        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 4
            color: np.playing ? "#53D07C" : root.lineColor
        }
    }
}
