/*
 * GlideMix onboarding: one choice as a card -- a radio button, a title and a
 * line of explanation. The whole card is clickable.
 */
import QtQuick 2.15

import Muse.Ui 1.0
import Muse.UiComponents

Rectangle {
    id: root

    property string title: ""
    property string description: ""
    property bool selected: false
    property bool recommended: false
    property NavigationPanel navigationPanel: null
    property int navigationRow: 0
    signal clicked()

    width: parent ? parent.width : 400
    height: 48
    radius: 4
    color: "transparent"
    border.width: 1
    border.color: selected ? ui.theme.accentColor : ui.theme.strokeColor

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        RoundedRadioButton {
            anchors.verticalCenter: parent.verticalCenter
            checked: root.selected
            navigation.name: "OnboardingOption_" + root.title
            navigation.panel: root.navigationPanel
            navigation.row: root.navigationRow
            navigation.accessible.name: root.title
            onToggled: root.clicked()
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            width: parent.width - 40
            StyledTextLabel {
                width: parent.width
                horizontalAlignment: Text.AlignLeft
                font: ui.theme.bodyBoldFont
                text: root.recommended ? qsTrc("appshell/gettingstarted", "%1 (recommended)").arg(root.title) : root.title
            }
            StyledTextLabel {
                width: parent.width
                horizontalAlignment: Text.AlignLeft
                font: ui.theme.bodyFont
                opacity: 0.75
                text: root.description
                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
