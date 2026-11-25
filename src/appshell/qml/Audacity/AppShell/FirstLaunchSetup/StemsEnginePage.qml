/*
 * GlideMix onboarding: offer the stems engine (the same install as
 * Preferences > Stems). It downloads in the background; the wizard goes on.
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15

import Muse.Ui 1.0
import Muse.UiComponents
import Audacity.AppShell
import Audacity.Preferences

Page {
    id: root

    title: qsTrc("appshell/gettingstarted", "Split songs into stems")
    titleContentSpacing: 16

    StemsPreferencesModel { id: stemsModel }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 64
        anchors.rightMargin: 64
        spacing: 14

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            text: qsTrc("appshell/gettingstarted", "The stems engine splits a song into drums, bass, vocals and other, so you can take the vocals out of a part, fade the drums in a transition or save a stem as a sample.")
        }
        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTrc("appshell/gettingstarted", "It is free and downloads once, about 1 GB. You can keep going while it downloads.")
        }

        RowLayout {
            width: parent.width
            spacing: 12
            FlatButton {
                visible: !stemsModel.available && !stemsModel.installing
                accentButton: true
                text: qsTrc("appshell/gettingstarted", "Install stems engine")
                onClicked: stemsModel.installEngine()
            }
            FlatButton {
                visible: stemsModel.installing
                text: qsTrc("appshell/gettingstarted", "Cancel")
                onClicked: stemsModel.cancelInstall()
            }
            StyledTextLabel {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft
                wrapMode: Text.WordWrap
                text: stemsModel.available ? qsTrc("appshell/gettingstarted", "✓ The stems engine is installed.")
                                           : stemsModel.installStatus
            }
        }

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            opacity: 0.6
            text: qsTrc("appshell/gettingstarted", "Not now? Install it later in Preferences → Stems.")
        }
    }
}
