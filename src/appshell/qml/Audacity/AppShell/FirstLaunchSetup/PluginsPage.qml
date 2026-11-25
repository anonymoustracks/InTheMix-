/*
 * GlideMix onboarding: plug-ins, like Ableton's Plug-In Sources -- only what
 * GlideMix supports: VST3 (Mac and Windows) and Audio Units (Mac). The system
 * folders are always scanned; extra VST3 folders are the same list as
 * Preferences > Effects.
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15

import Muse.Ui 1.0
import Muse.UiComponents
import Audacity.AppShell
import Audacity.Preferences
import Audacity.Effects

Page {
    id: root

    title: qsTrc("appshell/gettingstarted", "Your plug-ins")
    titleContentSpacing: 16

    property bool scanned: false

    PluginPreferencesModel {
        id: pluginModel
        Component.onCompleted: init()
    }
    PluginManagerTableViewModel { id: pluginScan }
    FilePickerModel {
        id: folderPicker
        title: qsTrc("appshell/gettingstarted", "Choose a folder with VST3 plug-ins")
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 64
        anchors.rightMargin: 64
        spacing: 10

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            text: Qt.platform.os === "osx"
                  ? qsTrc("appshell/gettingstarted", "InTheMix uses your VST3 and Audio Unit plug-ins from the usual system folders.")
                  : qsTrc("appshell/gettingstarted", "InTheMix uses your VST3 plug-ins from the usual system folder.")
        }

        StyledTextLabel {
            font: ui.theme.bodyBoldFont
            text: qsTrc("appshell/gettingstarted", "Your own VST3 folders")
        }

        Repeater {
            model: pluginModel.vst3CustomPaths
            delegate: RowLayout {
                width: parent ? parent.width : 400
                spacing: 8
                StyledTextLabel {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignLeft
                    elide: Text.ElideMiddle
                    text: modelData === "" ? qsTrc("appshell/gettingstarted", "(no folder chosen)") : modelData
                }
                FlatButton {
                    text: qsTrc("appshell/gettingstarted", "Remove")
                    onClicked: pluginModel.removeVst3Path(index)
                }
            }
        }

        RowLayout {
            spacing: 12
            FlatButton {
                text: qsTrc("appshell/gettingstarted", "Add folder…")
                onClicked: {
                    var chosen = folderPicker.selectDirectory()
                    if (chosen) {
                        pluginModel.addVst3Path()
                        pluginModel.setVst3Path(pluginModel.vst3CustomPaths.length - 1, chosen)
                    }
                }
            }
            FlatButton {
                accentButton: true
                text: qsTrc("appshell/gettingstarted", "Scan for plug-ins")
                onClicked: {
                    pluginScan.rescanPlugins()
                    root.scanned = true
                }
            }
            StyledTextLabel {
                visible: root.scanned
                text: qsTrc("appshell/gettingstarted", "✓ Scanned")
            }
        }

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            opacity: 0.6
            text: qsTrc("appshell/gettingstarted", "Not now? InTheMix also looks for new plug-ins at every start. Folders: Preferences → Effects.")
        }
    }
}
