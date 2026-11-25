/*
 * GlideMix onboarding: Keylock, key notation and the DJ clock.
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15

import Muse.Ui 1.0
import Muse.UiComponents
import Audacity.AppShell

Page {
    id: root

    title: qsTrc("appshell/gettingstarted", "Keys, tempo and the DJ clock")
    titleContentSpacing: 16

    OnboardingSettingsModel { id: settings }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 64
        anchors.rightMargin: 64
        spacing: 10

        CheckBox {
            text: qsTrc("appshell/gettingstarted", "Keylock (recommended)")
            checked: settings.keylock
            onClicked: settings.keylock = !checked
        }
        StyledTextLabel {
            width: parent.width
            leftPadding: 28
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTrc("appshell/gettingstarted", "Songs keep their pitch when the tempo changes. Off, they play faster and higher, like a turntable.")
        }

        RowLayout {
            spacing: 12
            StyledTextLabel {
                font: ui.theme.bodyBoldFont
                text: qsTrc("appshell/gettingstarted", "Key notation")
            }
            StyledDropdown {
                Layout.preferredWidth: 220
                model: [
                    { text: qsTrc("appshell/gettingstarted", "Lancelot (8A)"), value: 0 },
                    { text: qsTrc("appshell/gettingstarted", "Lancelot/Traditional (8A/Am)"), value: 1 },
                    { text: qsTrc("appshell/gettingstarted", "OpenKey (1m)"), value: 2 },
                    { text: qsTrc("appshell/gettingstarted", "OpenKey/Traditional (1m/Am)"), value: 3 },
                    { text: qsTrc("appshell/gettingstarted", "Traditional (Am)"), value: 4 }
                ]
                currentIndex: settings.keyNotation
                onActivated: function(index, value) { settings.keyNotation = value }
            }
        }

        Item { width: 1; height: 2 }

        StyledTextLabel {
            font: ui.theme.bodyBoldFont
            text: qsTrc("appshell/gettingstarted", "The DJ clock at the top")
        }
        OnboardingOption {
            height: 44
            title: qsTrc("appshell/gettingstarted", "Standard")
            description: qsTrc("appshell/gettingstarted", "Song time left, bar and beat, countdown to the next mix, key, song number.")
            recommended: true
            selected: !settings.clockComplete
            navigationPanel: root.navigationPanel
            navigationRow: 2
            onClicked: settings.clockComplete = false
        }
        OnboardingOption {
            height: 44
            title: qsTrc("appshell/gettingstarted", "Complete")
            description: qsTrc("appshell/gettingstarted", "Everything: also phrase, tempo, mix length, energy, CPU, disk and memory.")
            selected: settings.clockComplete
            navigationPanel: root.navigationPanel
            navigationRow: 3
            onClicked: settings.clockComplete = true
        }
        CheckBox {
            text: qsTrc("appshell/gettingstarted", "Flash before the end of a song, like a deck")
            checked: settings.clockFlash
            onClicked: settings.clockFlash = !checked
        }

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            opacity: 0.6
            text: qsTrc("appshell/gettingstarted", "Pick your sound card and headphone outputs any time in Audio setup (top right). The clock: right-click it.")
        }
    }
}
