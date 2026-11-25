/*
 * GlideMix: the DJ status bar at the bottom right of the project window
 * (Chris 2026-11-13: DJ items instead of Audacity's Selection / Duration
 * clocks) -- the mix's tempo at the playhead, the selection in bars, the mix
 * at a glance and how many transitions are in compatible keys.
 */
import QtQuick

import Muse.Ui
import Muse.UiComponents

import Audacity.MixHouse

Row {
    id: root

    spacing: 0
    height: 24

    MixHouseNowPlayingModel {
        id: np
    }

    readonly property color labelColor: ui.theme.fontSecondaryColor
    readonly property color lineColor: Qt.rgba(ui.theme.fontPrimaryColor.r, ui.theme.fontPrimaryColor.g,
                                               ui.theme.fontPrimaryColor.b, 0.15)

    function tabular(f) {
        const spec = { family: f.family, weight: f.weight, features: { "tnum": 1 } }
        if (f.pixelSize > 0) {
            spec.pixelSize = f.pixelSize
        } else {
            spec.pointSize = f.pointSize
        }
        return Qt.font(spec)
    }
    readonly property font valueFont: tabular(ui.theme.bodyBoldFont)

    function clockText(sec) {
        const s = Math.max(0, Math.round(sec))
        const h = Math.floor(s / 3600)
        const m = Math.floor((s % 3600) / 60)
        const ss = s % 60
        const two = function (n) { return n < 10 ? "0" + n : "" + n }
        return h > 0 ? h + ":" + two(m) + ":" + two(ss) : m + ":" + two(ss)
    }
    function barsText(bars) {
        const per = np.beatsPerBarMix > 0 ? np.beatsPerBarMix : 4
        let beats = Math.round(bars * per)
        const whole = Math.floor(beats / per)
        beats -= whole * per
        let t = qsTrc("mixhouse", "%n bar(s)", "", whole)
        if (beats > 0) {
            t += " " + qsTrc("mixhouse", "%n beat(s)", "", beats)
        }
        return t
    }

    component Item2: Row {
        id: item
        property string label: ""
        property string value: ""
        property color valueColor: ui.theme.fontPrimaryColor
        spacing: 6
        height: root.height
        leftPadding: 12
        rightPadding: 12

        StyledTextLabel {
            anchors.verticalCenter: parent.verticalCenter
            text: item.label
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 0.8
            color: root.labelColor
        }
        StyledTextLabel {
            anchors.verticalCenter: parent.verticalCenter
            text: item.value
            // Tabular figures: the tempo counting along the line doesn't shake.
            font: root.valueFont
            color: item.valueColor
        }
    }
    component Divider: Rectangle {
        width: 1
        height: 14
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        color: root.lineColor
    }

    // InTheMix: MixMeister Fusion's counters -- where the playhead is in the
    // mix, what is left and the whole mix, each with its song count.
    Item2 {
        label: qsTrc("mixhouse", "CURRENT")
        value: np.songCount <= 0 ? "—"
               : root.clockText(np.mixPosition) + " (" + (np.songIndex > 0 ? np.songIndex : 0) + ")"
    }
    Divider {}
    Item2 {
        label: qsTrc("mixhouse", "REMAINING")
        value: np.songCount <= 0 ? "—"
               : root.clockText(Math.max(0, np.mixTotal - np.mixPosition))
                 + " (" + Math.max(0, np.songCount - Math.max(np.songIndex, 0)) + ")"
    }
    Divider {}
    Item2 {
        label: qsTrc("mixhouse", "TOTAL")
        value: np.songCount <= 0 ? "—" : root.clockText(np.mixTotal) + " (" + np.songCount + ")"
    }
    Divider {}
    Item2 {
        label: qsTrc("mixhouse", "TEMPO")
        value: np.mixBpm > 0 ? qsTrc("mixhouse", "%1 BPM").arg(np.mixBpm.toFixed(1)) : "—"
    }
    Divider {}
    Item2 {
        label: qsTrc("mixhouse", "SELECTION")
        value: np.selectionBars >= 0 ? root.barsText(np.selectionBars) + " · " + root.clockText(np.selectionSec) : "—"
    }
    Divider {}
    Item2 {
        label: qsTrc("mixhouse", "MIX")
        value: {
            if (np.songCount <= 0) {
                return "—"
            }
            let t = qsTrc("mixhouse", "%n song(s)", "", np.songCount) + " · " + root.clockText(np.mixTotal)
            if (np.mixMinBpm > 0) {
                t += " · " + (Math.round(np.mixMinBpm) === Math.round(np.mixMaxBpm)
                              ? qsTrc("mixhouse", "%1 BPM").arg(Math.round(np.mixMinBpm))
                              : qsTrc("mixhouse", "%1–%2 BPM").arg(Math.round(np.mixMinBpm)).arg(Math.round(np.mixMaxBpm)))
            }
            return t
        }
    }
    Divider {}
    Item2 {
        label: qsTrc("mixhouse", "HARMONIC")
        value: np.harmonicTotal <= 0 ? "—"
               : np.harmonicClash === "" ? np.harmonicGood + " / " + np.harmonicTotal
               : qsTrc("mixhouse", "%1 / %2 · clash %3").arg(np.harmonicGood).arg(np.harmonicTotal).arg(np.harmonicClash)
        valueColor: np.harmonicTotal <= 0 ? ui.theme.fontPrimaryColor
                    : np.harmonicGood === np.harmonicTotal ? "#5FD068" : "#E7C94A"
    }
}
