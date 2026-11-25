/*
 * GlideMix: shown once, on the first start after an update
 * (StartupScenario::showStartupDialogsIfNeed). Rewrite the notes for every
 * release, from the same text as the GitHub release.
 */
import QtQuick
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

StyledDialogView {
    id: root

    title: qsTrc("appshell", "What's new in InTheMix")

    contentWidth: 560
    contentHeight: 460

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        StyledFlickable {
            Layout.fillWidth: true
            Layout.fillHeight: true

            contentHeight: notes.implicitHeight

            StyledTextLabel {
                id: notes

                width: parent.width
                horizontalAlignment: Text.AlignLeft
                wrapMode: Text.WordWrap
                textFormat: Text.MarkdownText
                text: "**InTheMix v1.3.13**\n\n"
                      + "InTheMix is now **InTheMix**. Your library, settings and licence stay as they are.\n\n"
                      + "- **Editing is fast again** in big libraries.\n"
                      + "- **A new workspace look**: a studio grid with phrase shading, a ruler in bars, the Charcoal "
                      + "workspace in one colour, rows that light up under the mouse, Inter font, new faders.\n"
                      + "- **Waveform size**: the wave button next to Reset view.\n"
                      + "- **Creative set**: 43 new colours; new tracks get them round the colour wheel.\n"
                      + "- **Search** in Add effect and in Preferences.\n"
                      + "- **No more stray automation points**: add one with a double-click, right-click or V / B / M / T / C.\n"
                      + "- **Undo** for tempo points, volume and pan. **Snap off** means free.\n"
                      + "- Fixes: solo after deleting a track, tracklist times, stars in Related."
            }
        }

        FlatButton {
            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth: 100
            text: qsTrc("global", "OK")
            accentButton: true
            onClicked: root.close()
        }
    }
}
