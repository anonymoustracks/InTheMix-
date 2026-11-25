/*
 * GlideMix onboarding: how songs are drawn, and the size of the whole window.
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15

import Muse.Ui 1.0
import Muse.UiComponents
import Audacity.AppShell

Page {
    id: root

    title: qsTrc("appshell/gettingstarted", "How should your songs look?")
    titleContentSpacing: 16

    OnboardingSettingsModel {
        id: settings
        Component.onCompleted: preselectRecommended()
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 64
        anchors.rightMargin: 64
        spacing: 8

        Repeater {
            model: [
                { look: 0, title: qsTrc("appshell/gettingstarted", "Transparent single wave"),
                  text: qsTrc("appshell/gettingstarted", "One wave per song, no colour box behind it: the beat grid shows right through."), rec: true },
                { look: 1, title: qsTrc("appshell/gettingstarted", "Single wave on colour"),
                  text: qsTrc("appshell/gettingstarted", "One wave per song on the song's colour."), rec: false },
                { look: 2, title: qsTrc("appshell/gettingstarted", "Dual (stereo)"),
                  text: qsTrc("appshell/gettingstarted", "Left and right channel, one above the other."), rec: false },
                { look: 3, title: qsTrc("appshell/gettingstarted", "Colour waveform"),
                  text: qsTrc("appshell/gettingstarted", "DJ-style colours on black: lows, mids and highs in their own colour."), rec: false }
            ]
            delegate: OnboardingOption {
                title: modelData.title
                description: modelData.text
                recommended: modelData.rec
                selected: settings.waveformLook === modelData.look
                navigationPanel: root.navigationPanel
                navigationRow: index
                onClicked: settings.waveformLook = modelData.look
            }
        }

        CheckBox {
            visible: settings.waveformLook === 3
            text: qsTrc("appshell/gettingstarted", "Colour waveform: left and right apart")
            checked: settings.colourStereo
            onClicked: settings.colourStereo = !checked
        }

        Item { width: 1; height: 6 }

        RowLayout {
            width: parent.width
            spacing: 12
            StyledTextLabel {
                font: ui.theme.bodyBoldFont
                text: qsTrc("appshell/gettingstarted", "Interface size")
            }
            // 100 % in the middle: the left half is 50-100 %, the right half 100-200 %.
            StyledSlider {
                Layout.preferredWidth: 160
                from: 0
                to: 100
                stepSize: 0
                value: settings.interfaceSize <= 100 ? settings.interfaceSize - 50 : 50 + (settings.interfaceSize - 100) / 2
                onMoved: {
                    var size = value <= 50 ? 50 + value : 100 + (value - 50) * 2
                    settings.interfaceSize = Math.round(size / 5) * 5
                }
            }
            StyledTextLabel {
                Layout.preferredWidth: 44
                font: ui.theme.bodyBoldFont
                text: settings.interfaceSize + " %"
            }
            FlatButton {
                text: qsTrc("appshell/gettingstarted", "Reset")
                enabled: settings.interfaceSize !== 100
                onClicked: settings.interfaceSize = 100
            }
            // Applies after a restart: GlideMix restarts and this page opens again.
            FlatButton {
                visible: settings.interfaceSize !== settings.runningInterfaceSize
                accentButton: true
                text: qsTrc("appshell/gettingstarted", "Restart now")
                onClicked: settings.restartToApplySize(1)
            }
        }

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: settings.interfaceSize !== settings.runningInterfaceSize
                  ? qsTrc("appshell/gettingstarted", "Restart now to see it at %1 %: InTheMix opens again and setup goes on right here.").arg(settings.interfaceSize)
                  : qsTrc("appshell/gettingstarted", "The whole window bigger or smaller, for small or very sharp screens.")
        }

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            opacity: 0.6
            text: qsTrc("appshell/gettingstarted", "Change it later in the View menu and in Preferences.")
        }
    }
}
