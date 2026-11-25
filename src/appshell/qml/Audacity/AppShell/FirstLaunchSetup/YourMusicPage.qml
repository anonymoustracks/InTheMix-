/*
 * GlideMix onboarding: the music folder, and how hard analysis may work.
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15

import Muse.Ui 1.0
import Muse.UiComponents
import Audacity.AppShell
import Audacity.MixHouse

Page {
    id: root

    title: qsTrc("appshell/gettingstarted", "Where is your music?")
    titleContentSpacing: 16

    property string addedFolder: ""

    OnboardingSettingsModel { id: settings }
    MixHouseTrackListModel { id: tracksModel }
    FilePickerModel {
        id: folderPicker
        title: qsTrc("appshell/gettingstarted", "Choose a music folder to add")
        dir: tracksModel.musicFolder()
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 64
        anchors.rightMargin: 64
        spacing: 10

        RowLayout {
            width: parent.width
            spacing: 12
            FlatButton {
                accentButton: root.addedFolder === ""
                text: root.addedFolder === "" ? qsTrc("appshell/gettingstarted", "Add music folder…")
                                              : qsTrc("appshell/gettingstarted", "Add another folder…")
                onClicked: {
                    var chosen = folderPicker.selectDirectory()
                    if (chosen) {
                        tracksModel.addFolderToLibrary(chosen)
                        root.addedFolder = chosen
                    }
                }
            }
            StyledTextLabel {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft
                wrapMode: Text.WordWrap
                text: root.addedFolder === ""
                      ? qsTrc("appshell/gettingstarted", "InTheMix finds every song in it and its subfolders.")
                      : qsTrc("appshell/gettingstarted", "Adding %1 — this runs on in the background.").arg(root.addedFolder)
            }
        }

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTrc("appshell/gettingstarted", "Using rekordbox, Lexicon, Serato, Traktor, Engine DJ, VirtualDJ or djay? Their libraries, playlists and beat grids show up in the library's sidebar by themselves.")
        }

        Item { width: 1; height: 4 }

        StyledTextLabel {
            font: ui.theme.bodyBoldFont
            text: qsTrc("appshell/gettingstarted", "Analysis speed")
        }

        Repeater {
            model: [
                { mode: 0, title: qsTrc("appshell/gettingstarted", "Performance"),
                  text: qsTrc("appshell/gettingstarted", "All processor cores: the fastest."), rec: false },
                { mode: 1, title: qsTrc("appshell/gettingstarted", "Normal"),
                  text: qsTrc("appshell/gettingstarted", "Fast, and the computer stays easy to use."), rec: true },
                { mode: 2, title: qsTrc("appshell/gettingstarted", "Power saving"),
                  text: qsTrc("appshell/gettingstarted", "Half the cores: for a laptop on battery."), rec: false }
            ]
            delegate: OnboardingOption {
                height: 44
                title: modelData.title
                description: modelData.text
                recommended: modelData.rec
                selected: settings.analysisMode === modelData.mode
                navigationPanel: root.navigationPanel
                navigationRow: index + 1
                onClicked: settings.analysisMode = modelData.mode
            }
        }

        StyledTextLabel {
            width: parent.width
            horizontalAlignment: Text.AlignLeft
            opacity: 0.6
            text: qsTrc("appshell/gettingstarted", "Change it later in Preferences → Music library.")
        }
    }
}
