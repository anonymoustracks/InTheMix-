/*
* Audacity: A Digital Audio Editor
*/
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

import Audacity.ProjectScene
import Audacity.Automation

import "audio"

TrackItem {
    id: root

    TracksViewStateModel {
        id: automationViewState
        Component.onCompleted: init()
    }

    // GlideMix 1.3.9 (Chris, 2026-11-16): the band knobs' model -- each knob
    // shows its band at the playhead and moves the band's whole line.
    TrackBandKnobsModel {
        id: bandKnobs
        trackId: root.trackViewState.trackId
        Component.onCompleted: init()
    }

    headerTrailingControlsComponent: Component {
        RowLayout {
            opacity: root.collapsed ? 1 : 0
            visible: opacity !== 0

            Behavior on opacity {
                OpacityAnimator {
                    duration: 100
                }
            }

            Loader {
                sourceComponent: trackControlButtons
            }
        }
    }

    extraControlsComponent: Component {
        ColumnLayout {
            spacing: 2

            RowLayout {
                id: topRow

                Layout.fillWidth: true

                spacing: 16

                opacity: root.collapsed ? 0 : 1
                visible: opacity !== 0
                Behavior on opacity {
                    OpacityAnimator {
                        duration: 100
                    }
                }

                PanKnob {
                    id: panKnob

                    // GlideMix (Chris, 2026-11-21): grows and shrinks with the track's height;
                    // 16 at the default height (128), 9 on a short track, 18 on a tall one.
                    radius: Math.max(9, Math.min(18, (root.height - 48) / 5))
                    Layout.alignment: Qt.AlignVCenter

                    value: Boolean(root.item) ? root.item.pan : 0

                    navigation.panel: root.headerNavigationPanel
                    navigation.order: root.extraControlsNavigationStart
                    navigation.enabled: !root.collapsed
                    //: Noun: accessibility name of the pan knob
                    navigation.accessible.name: qsTrc("projectscene", "Pan")

                    onNewPanRequested: function (newValue, completed) {
                        if (Boolean(root.item)) {
                            root.item.setPan(newValue, completed)
                        }
                    }

                    // GlideMix: the same for the pan knob (undo / redo after a turn).
                    Connections {
                        target: root.item
                        function onPanChanged() {
                            if (Boolean(root.item) && panKnob.value !== root.item.pan) {
                                panKnob.value = root.item.pan
                            }
                        }
                    }
                }

                VolumeSlider {
                    id: volumeSlider

                    value: Boolean(root.item) ? root.item.volumeLevel : 0
                    // GlideMix (Chris, 2026-11-21): the fader's line in the track's colour, as in a mixer.
                    fillColor: Boolean(root.item) && root.item.color.a > 0 ? root.item.color : ui.theme.accentColor

                    navigation.panel: root.headerNavigationPanel
                    navigation.order: panKnob.navigation.order + 1
                    navigation.enabled: !root.collapsed
                    navigation.accessible.name: qsTrc("projectscene", "Track volume")

                    onNewVolumeRequested: function (newValue, completed) {
                        if (Boolean(root.item)) {
                            root.item.setVolumeLevel(newValue, completed)
                        }
                    }

                    // GlideMix (2026-11-21): a drag or a typed value sets `value` directly, which
                    // breaks its binding to the track; undo / redo then never moved the fader.
                    Connections {
                        target: root.item
                        function onVolumeLevelChanged() {
                            if (Boolean(root.item) && volumeSlider.value !== root.item.volumeLevel) {
                                volumeSlider.value = root.item.volumeLevel
                            }
                        }
                    }

                    Component.onCompleted: {
                        root.extraControlsNavigationEnd = Qt.binding(function () {
                            return volumeSlider.navigation.order
                        })
                    }
                }

                Loader {
                    sourceComponent: trackControlButtons
                }
            }


            // GlideMix (Chris, MixMeister's per-track lines): the song's own
            // Low / Mid / High line over the whole song, editable anywhere --
            // a quick bass cut on a drop. Again on the lit one hides it.
            RowLayout {
                id: bandButtons
                Layout.fillWidth: true
                // GlideMix 1.3.9 (Chris): room between the fader and the knobs.
                Layout.topMargin: knobsFit && !root.trackViewState.isSampleLine ? knobGap : 0
                spacing: 6
                // Responsive: the dots grow with the room under the fader (track
                // height) and the header's width, from 12 to 26 px; a track too
                // short for even the smallest hides them.
                readonly property real room: root.height - 76
                readonly property real dot: Math.max(14, Math.min(24, room - 2))
                // GlideMix 1.3.9: a knob with its name under it needs knob + label + gap.
                readonly property int labelHeight: 12
                readonly property int knobGap: 5
                // Chris: as big as the song's height allows (a taller song, bigger knobs).
                // (2026-11-21: up to 20 on a tall track, so they grow when the header is enlarged.)
                readonly property real knobRadius: Math.max(8, Math.min(20, (room - knobGap - labelHeight - 2) / 2))
                // Chris: everything stays visible when the header narrows (the FX panel open):
                // below this SAMPLES shows its icon only.
                readonly property bool compact: width < 300
                readonly property real knobRowHeight: knobGap + knobRadius * 2 + labelHeight + 1
                // The row shows as before (SAMPLES, LINES); the knobs only where they AND
                // their names fit whole -- the default song height (128) shows them.
                readonly property bool knobsFit: room >= knobRowHeight
                visible: !root.collapsed && (room >= 14 || root.trackViewState.isSampleLine)
                property int shown: automationViewState.shownBandLine(root.trackViewState.trackId)
                Connections {
                    target: automationViewState
                    function onBandLineChanged() {
                        bandButtons.shown = automationViewState.shownBandLine(root.trackViewState.trackId)
                    }
                }
                // GlideMix 1.3.9 (Chris): the knobs' lines on the song, all four at once and
                // editable, on or off -- the volume automation's own icon, in front of the knobs.
                FlatToggleButton {
                    visible: !root.trackViewState.isSampleLine
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    // Level with the knobs' centres, not the middle of knob + name (Chris: too low).
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: bandButtons.knobsFit ? Math.max(0, bandButtons.knobRadius - 10) : 0
                    icon: IconCode.AUTOMATION
                    checked: bandKnobs.linesShown
                    checkedColor: ui.theme.accentColor
                    toolTipTitle: bandKnobs.linesShown ? qsTrc("projectscene", "Hide the band lines") : qsTrc("projectscene", "Show the band lines")
                    toolTipDescription: qsTrc("projectscene", "Low, Mid, High and Color lines of this song, all at once: drag any of them on the song.")
                    onToggled: {
                        // GlideMix 1.3.10 (Chris): turning the lines on brings all four back, so a
                        // line opened alone from its name closes first.
                        if (!bandKnobs.linesShown && bandButtons.shown >= 0) {
                            automationViewState.toggleBandLine(root.trackViewState.trackId, bandButtons.shown)
                        }
                        bandKnobs.linesShown = !bandKnobs.linesShown
                    }
                }

                Repeater {
                    model: [
                        { text: qsTrc("projectscene", "LOW"), name: qsTrc("projectscene", "Low"), color: "#F0506E" },
                        { text: qsTrc("projectscene", "MID"), name: qsTrc("projectscene", "Mid"), color: "#F5A04A" },
                        { text: qsTrc("projectscene", "HIGH"), name: qsTrc("projectscene", "High"), color: "#4A9BFF" },
                        { text: qsTrc("projectscene", "FILTER"), name: qsTrc("projectscene", "Filter"), color: "#A07CF5" }
                    ]
                    // GlideMix 1.3.9 (Chris): a DJ knob per band, middle = no change.
                    // Turning moves the band's whole line on this track (one undo step
                    // per turn, double-click = back to the middle). The name beside it,
                    // in the lane colour, shows the line to edit by hand, as before.
                    ColumnLayout {
                        id: bandButton
                        required property var modelData
                        required property int index
                        // A sample line has no song bands.
                        visible: !root.trackViewState.isSampleLine && bandButtons.knobsFit
                        readonly property bool shownHere: bandButtons.shown === index
                        readonly property color laneColor: Qt.color(modelData.color)
                        spacing: 1
                        Layout.fillWidth: true
                        // Shrinks with the header down to the knob or its name, whichever is wider.
                        Layout.minimumWidth: Math.max(bandButtons.knobRadius * 2, bandLabel.implicitWidth + 4)
                        Layout.preferredWidth: Layout.minimumWidth
                        Layout.alignment: Qt.AlignTop

                        KnobControl {
                            id: bandKnob
                            Layout.alignment: Qt.AlignHCenter
                            isPanKnob: true   // the ring grows from the middle, both ways
                            djStyle: true
                            radius: bandButtons.knobRadius
                            // Chris: easier to catch -- the knob answers a little outside its ring.
                            hitMargin: 6
                            ringColor: bandButton.laneColor
                            // The wheel scrolls the tracks; it doesn't turn a line's knob by accident.
                            wheelEnabled: false
                            from: -100
                            to: 100
                            stepSize: 1
                            value: bandKnobs.values.length > bandButton.index ? bandKnobs.values[bandButton.index] : 0
                            onMousePressed: bandKnobs.begin(bandButton.index)
                            onNewValueRequested: function (v) { bandKnobs.turn(bandButton.index, v) }
                            onMouseReleased: bandKnobs.end(bandButton.index)
                            // GlideMix 1.3.10: the knob's value while hovered or turned, as MixMeister shows it.
                            ToolTip.visible: mouseArea.pressed || mouseArea.containsMouse
                            ToolTip.delay: 0
                            // ... and its line's key: mouse over the song + the key = a point on the line there.
                            ToolTip.text: bandKnobs.knobText(bandButton.index, value) + "   ·   "
                                          + qsTrc("projectscene", "key %1 adds a point under the mouse").arg(["B", "M", "T", "C"][bandButton.index])
                        }
                        // The band's name under its knob, in the lane colour. The automation button shows
                        // all four lines, editable at once; clicking a name still opens that
                        // one line alone, with its point menu (lit while shown).
                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: bandLabel.implicitWidth + 4
                            implicitHeight: bandButtons.labelHeight
                            radius: height / 2
                            color: Qt.rgba(bandButton.laneColor.r, bandButton.laneColor.g, bandButton.laneColor.b,
                                           bandButton.shownHere ? 0.9 : (labelArea.containsMouse ? 0.35 : 0.0))
                            StyledTextLabel {
                                id: bandLabel
                                anchors.centerIn: parent
                                text: bandButton.modelData.text
                                font.pixelSize: bandButtons.labelHeight - 3
                                font.bold: true
                                color: bandButton.shownHere ? "#14161a" : bandButton.laneColor
                            }
                            MouseArea {
                                id: labelArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    automationViewState.toggleBandLine(root.trackViewState.trackId, bandButton.index)
                                    // GlideMix 1.3.10 (Chris): that line alone -- the other lines (and their button) go off.
                                    if (bandButtons.shown === bandButton.index) {
                                        bandKnobs.linesShown = false
                                    }
                                }
                                ToolTip.visible: containsMouse
                                ToolTip.delay: 600
                                ToolTip.text: qsTrc("projectscene", "%1: the knob moves the whole line up or down (middle: no change). The automation button before LOW shows all the lines to edit on the song.").arg(bandButton.modelData.name)
                            }
                        }
                    }
                }

                // GlideMix Samples (B2, Chris's spot and look): the glossy grey of
                // the header's FX / M / S buttons, lit while the song's sample
                // line shows. First click opens the line, then shows / hides it.
                Item { Layout.fillWidth: true; visible: !root.trackViewState.isSampleLine }
                FlatButton {
                    id: samplesButton
                    readonly property bool shown: root.trackViewState.sampleLineShown
                    // InTheMix: like MixMeister Fusion, no SAMPLES button on a
                    // song; it shows only where a sample line already exists.
                    visible: !root.trackViewState.isSampleLine && root.trackViewState.hasSampleLine
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignVCenter
                    orientation: Qt.Horizontal
                    // The eye is part of the capsule: open = the line shows, closed = hidden.
                    icon: !root.trackViewState.hasSampleLine ? IconCode.WAVEFORM
                          : (shown ? IconCode.EYE_OPEN : IconCode.EYE_CLOSED)
                    text: bandButtons.compact ? "" : qsTrc("projectscene", "SAMPLES")
                    textFont: ui.theme.bodyBoldFont
                    accentButton: shown
                    toolTipTitle: !root.trackViewState.hasSampleLine ? qsTrc("projectscene", "Sample line")
                                  : (shown ? qsTrc("projectscene", "Hide the sample line") : qsTrc("projectscene", "Show the sample line"))
                    toolTipDescription: !root.trackViewState.hasSampleLine
                        ? qsTrc("projectscene", "Opens a line under this song for your own samples: drop them from the Samples tab or Finder.")
                        : (samplesButton.sampleCount > 0
                           ? qsTrc("projectscene", "%n sample(s) or beat(s) on this line. Hidden, the line takes no room: they stay and keep playing.", "", samplesButton.sampleCount)
                           : qsTrc("projectscene", "The line is empty. Hidden, it takes no room."))
                    onClicked: root.trackViewState.toggleSampleLine()

                    // GlideMix (Marc and Chris, 2026-11-21): the button shows that the song's line holds
                    // samples or a beat. Hidden with something on it: an accent outline stays on.
                    // Something new lands while it is hidden: three pulses.
                    readonly property int sampleCount: root.trackViewState.sampleLineClipCount
                    property int lastCount: -1
                    readonly property bool hiddenWithContent: !shown && sampleCount > 0
                    onSampleCountChanged: {
                        if (lastCount >= 0 && sampleCount > lastCount && !shown) {
                            samplesPulse.restart()
                        }
                        lastCount = sampleCount
                    }
                    Component.onCompleted: lastCount = sampleCount

                    Rectangle {
                        id: samplesGlow
                        anchors.fill: parent
                        radius: samplesButton.backgroundRadius
                        color: "transparent"
                        border.width: 1
                        border.color: ui.theme.accentColor
                        visible: samplesButton.hiddenWithContent || samplesPulse.running
                        opacity: 0.9

                        SequentialAnimation {
                            id: samplesPulse
                            loops: 3
                            NumberAnimation { target: samplesGlow; property: "opacity"; to: 0.15; duration: 220; easing.type: Easing.InOutQuad }
                            NumberAnimation { target: samplesGlow; property: "opacity"; to: 1.0; duration: 220; easing.type: Easing.InOutQuad }
                            onStopped: samplesGlow.opacity = 0.9
                        }
                    }
                }

                // GlideMix Samples B8: on a sample line, BEAT sits on this row under
                // FX / M / S (it no longer squeezes the volume fader).
                Item { Layout.fillWidth: true; visible: root.trackViewState.isSampleLine }
                FlatButton {
                    visible: root.trackViewState.isSampleLine
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTrc("projectscene", "BEAT")
                    textFont: ui.theme.bodyBoldFont
                    toolTipTitle: qsTrc("projectscene", "Make a beat")
                    toolTipDescription: qsTrc("projectscene", "A step sequencer with your own one-shots; the beat lands on this sample line as one loop.")
                    onClicked: api.launcher.open("audacity://mixhouse/beat?sampleLine=" + root.trackViewState.trackId)
                }
            }
        }
    }

    rightSideContainerComponent: Component {
        Item {
            width: parent.width
            height: parent.height

            Row {
                id: volumePressureContainer

                property int indicatorWidth: 7

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter

                spacing: 2

                function clearMeters() {
                    leftOrMonoVolumePressureMeter.reset()
                    leftOrMonoVolumePressureMeter.resetClipped()
                    rightVolumePressureMeter.reset()
                    rightVolumePressureMeter.resetClipped()
                }

                TapHandler {
                    onTapped: volumePressureContainer.clearMeters()
                }

                VolumePressureMeter {
                    id: leftOrMonoVolumePressureMeter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    indicatorWidth: parent.indicatorWidth
                    meterModel: trackViewState.meterModel
                    currentVolumePressure: Boolean(root.item) ? root.item.leftChannelPressure : 0
                    currentRMS: Boolean(root.item) ? root.item.leftChannelRMS : 0
                }

                VolumePressureMeter {
                    id: rightVolumePressureMeter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    indicatorWidth: parent.indicatorWidth
                    meterModel: trackViewState.meterModel
                    currentVolumePressure: Boolean(root.item) ? root.item.rightChannelPressure : 0
                    currentRMS: Boolean(root.item) ? root.item.rightChannelRMS : 0
                }

                Connections {
                    target: root.trackViewState

                    function onIsRecordingChanged() {
                        if (trackViewState.isRecording) {
                            leftOrMonoVolumePressureMeter.resetClipped()
                            rightVolumePressureMeter.resetClipped()
                        }

                        leftOrMonoVolumePressureMeter.reset()
                        rightVolumePressureMeter.reset()
                    }

                    function onIsPlayingChanged() {
                        if (trackViewState.isPlaying) {
                            volumePressureContainer.clearMeters()
                        }
                    }
                }

                Connections {
                    target: root

                    function onIsFocusedChanged() {
                        if (!root.isFocused) {
                            volumePressureContainer.clearMeters()
                        }
                    }
                }
            }


            states: [
                State {
                    when: Boolean(root.item) && root.item.channelCount === 1
                    name: "mono"
                    PropertyChanges {
                        target: volumePressureContainer
                        indicatorWidth: 8
                    }
                    PropertyChanges {
                        target: rightVolumePressureMeter
                        visible: false
                    }
                },
                State {
                    when: Boolean(root.item) && root.item.channelCount === 2
                    name: "stereo"
                    PropertyChanges {
                        target: volumePressureContainer
                        indicatorWidth: 7
                    }
                    PropertyChanges {
                        target: rightVolumePressureMeter
                        visible: true
                    }
                }
            ]
        }
    }

    Component {
        id: trackControlButtons

        RowLayout {
            spacing: 4

            // GlideMix: the effects panel, as a small button with the others
            // (it used to be a full-width bar of its own).
            FlatButton {
                id: effectsButton

                Layout.preferredWidth: 26
                Layout.preferredHeight: 20
                text: qsTrc("projectscene", "FX")
                textFont: ui.theme.bodyBoldFont
                toolTipTitle: qsTrc("projectscene", "Effects")
                toolTipDescription: qsTrc("projectscene", "Opens effects panel")

                navigation.panel: root.headerNavigationPanel
                navigation.order: root.headerTrailingControlsNavigationStart
                navigation.accessible.name: qsTrc("projectscene", "Effects")

                onClicked: {
                    root.openEffectsRequested()
                }
            }

            FlatToggleButton {
                id: automationButton

                Layout.preferredWidth: 20
                Layout.preferredHeight: Layout.preferredWidth
                icon: IconCode.AUTOMATION
                checked: automationViewState.clipGainAutomationEnabled
                // Make the track automation state unambiguous, matching the
                // active realtime-effect automation affordance.
                checkedColor: ui.theme.accentColor
                toolTipDescription: qsTrc("projectscene", "Automation")

                navigation.panel: root.headerNavigationPanel
                navigation.order: effectsButton.navigation.order + 1
                navigation.accessible.name: qsTrc("projectscene", "Automation")
                navigation.accessible.role: MUAccessible.CheckBox

                onToggled: {
                    automationViewState.toggleClipGainAutomation()
                }
            }

            FlatToggleButton {
                id: muteButton

                Layout.preferredWidth: 20
                Layout.preferredHeight: Layout.preferredWidth

                icon: IconCode.MUTE
                checked: Boolean(root.item) ? root.item.muted : false

                navigation.panel: root.headerNavigationPanel
                navigation.order: automationButton.navigation.order + 1
                //: Accessibility name of the mute button
                navigation.accessible.name: qsTrc("projectscene", "Mute")
                navigation.accessible.role: MUAccessible.CheckBox

                onToggled: {
                    if (Boolean(root.item)) {
                        root.item.muted = !checked
                    }
                }
            }

            FlatToggleButton {
                id: soloButton

                Layout.preferredWidth: 20
                Layout.preferredHeight: Layout.preferredWidth

                icon: IconCode.SOLO
                checked: Boolean(root.item) ? root.item.solo : false

                navigation.panel: root.headerNavigationPanel
                navigation.order: muteButton.navigation.order + 1
                //: Accessibility name of the solo button
                navigation.accessible.name: qsTrc("projectscene", "Solo")
                navigation.accessible.role: MUAccessible.CheckBox

                onToggled: {
                    if (Boolean(root.item)) {
                        root.item.solo = !checked
                    }
                }

                Component.onCompleted: {
                    root.headerTrailingControlsNavigationEnd = Qt.binding(function () {
                        return soloButton.navigation.order
                    })
                }
            }
        }
    }
}
