import QtQuick
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

StyledDialogView {
    id: root

    property var colors: []
    property int currentColorIndex: 1
    property bool showReset: false
    // GlideMix: a "Custom" button for the system colour picker (clips and
    // tracks; the grid editor's phrase colours do not offer it).
    property bool allowCustom: false
    // GlideMix (Chris, 2026-11-16): stay open after a colour is picked, so the
    // colours can be tried one after another; the red dot closes it.
    property bool keepOpen: false
    // A colour picked in the system picker is saved as this flag + 0xRRGGBB.
    readonly property bool currentIsCustom: (currentColorIndex & 0x40000000) !== 0
    readonly property color currentCustomColor: {
        var hex = (currentColorIndex & 0xFFFFFF).toString(16)
        while (hex.length < 6) {
            hex = "0" + hex
        }
        return "#" + hex
    }

    readonly property int swatchSize: 28
    // GlideMix (Chris, 2026-11-21): the "Creative set" (colours 121..163) is its
    // own block under the 120, in rows of 14 like the reference DAW draws them.
    readonly property var mainColors: colors.filter(function (c) { return c.index <= 120 })
    readonly property var studioColors: colors.filter(function (c) { return c.index > 120 && c.index <= 163 })
    readonly property int studioColumns: 14
    readonly property int studioGap: 14
    readonly property int studioTitleHeight: 16
    readonly property int paletteRows: Math.max(1, Math.ceil(mainColors.length / 24))
    readonly property int studioRows: Math.ceil(studioColors.length / studioColumns)
    readonly property int paletteRowSpacing: 6
    readonly property int paletteHeight: paletteRows * swatchSize
                                         + (paletteRows - 1) * paletteRowSpacing
                                         + (studioRows > 0 ? studioGap + studioTitleHeight + paletteRowSpacing
                                                             + studioRows * swatchSize
                                                             + (studioRows - 1) * paletteRowSpacing : 0)

    signal colorSelected(int colorIndex)
    signal resetRequested
    signal customRequested

    contentWidth: 24 * swatchSize + 23 * 4 + ((showReset || allowCustom) ? 36 : 0)
    // Grid's implicit height is not stable while the Repeater is being
    // populated.  DialogView reads the size as it opens, so relying on it can
    // lock the window at three rows even though all 96 delegates are present.
    contentHeight: titleLabel.implicitHeight + 12 + paletteHeight
    margins: 14
    modal: false
    frameless: true
    resizable: false
    alwaysAboveApp: true
    activateParentOnClose: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        StyledTextLabel {
            id: titleLabel
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
            font.pixelSize: 18
            text: qsTrc("projectscene", "Color")
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: root.paletteHeight
            Layout.minimumHeight: root.paletteHeight
            spacing: 8

            Column {
                Layout.alignment: Qt.AlignTop
                visible: root.showReset || root.allowCustom
                spacing: root.paletteRowSpacing

                FlatButton {
                    width: 28
                    height: 28
                    visible: root.showReset
                    icon: IconCode.UNDO
                    toolTipTitle: qsTrc("projectscene", "Same as track color")
                    onClicked: {
                        root.resetRequested()
                        root.close()
                    }
                }

                // GlideMix: any colour, from the system colour picker. Shows
                // the current colour when it was picked there.
                Rectangle {
                    width: 28
                    height: 28
                    visible: root.allowCustom
                    radius: 5
                    color: root.currentIsCustom ? root.currentCustomColor : "transparent"
                    border.color: root.currentIsCustom ? ui.theme.fontPrimaryColor : ui.theme.strokeColor
                    border.width: root.currentIsCustom ? 3 : 1
                    gradient: root.currentIsCustom ? null : rainbow

                    Gradient {
                        id: rainbow
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#FF4F19" }
                        GradientStop { position: 0.25; color: "#FFFE19" }
                        GradientStop { position: 0.5; color: "#19FF74" }
                        GradientStop { position: 0.75; color: "#1988FF" }
                        GradientStop { position: 1.0; color: "#FE19FF" }
                    }

                    StyledTextLabel {
                        anchors.centerIn: parent
                        visible: !root.currentIsCustom
                        text: "+"
                        font.bold: true
                        font.pixelSize: 18
                        color: "#000000"
                    }

                    MouseArea {
                        id: customArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.close()
                            root.customRequested()
                        }
                    }

                }
            }

            Column {
                Layout.alignment: Qt.AlignTop
                spacing: root.studioGap

            Grid {
                id: palette
                columns: 24
                columnSpacing: 4
                rowSpacing: root.paletteRowSpacing

                Repeater {
                    model: root.mainColors
                    delegate: swatchComponent
                }
            }

            Column {
                visible: root.studioColors.length > 0
                spacing: root.paletteRowSpacing

            StyledTextLabel {
                height: root.studioTitleHeight
                verticalAlignment: Text.AlignVCenter
                //: Name of the second block of colours in the track and clip colour picker
                text: qsTrc("projectscene", "Creative set")
                opacity: 0.75
            }

            Grid {
                id: studioPalette
                columns: root.studioColumns
                columnSpacing: 4
                rowSpacing: root.paletteRowSpacing

                Repeater {
                    model: root.studioColors
                    delegate: swatchComponent
                }
            }
            }
            }
        }
    }

    Component {
        id: swatchComponent

                Item {
                    required property var modelData
                    readonly property bool selected: modelData.index === root.currentColorIndex

                    width: root.swatchSize
                    height: root.swatchSize

                    Rectangle {
                        id: swatch
                        anchors.centerIn: parent
                        width: parent.width - (parent.selected ? 0 : 4)
                        height: width
                        radius: 5
                        color: modelData.color
                        border.color: parent.selected ? ui.theme.fontPrimaryColor : Qt.rgba(0, 0, 0, 0.28)
                        border.width: parent.selected ? 3 : 1

                        Behavior on scale {
                            NumberAnimation { duration: 90 }
                        }
                    }

                    HoverHandler { id: hoverHandler }

                    states: State {
                        when: hoverHandler.hovered && !selected
                        PropertyChanges { target: swatch; scale: 1.12 }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.colorSelected(parent.modelData.index)
                            if (!root.keepOpen) {
                                root.close()
                            }
                        }
                    }
                }
    }

    // These are overlays, not ColumnLayout children. Putting them inside the
    // layout made Qt reserve two extra rows and pushed palette row four below
    // the dialog's clipping boundary.
    MouseArea {
        id: paletteDragArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: titleLabel.height
        z: 10
        cursorShape: Qt.OpenHandCursor

        onPressed: {
            cursorShape = Qt.ClosedHandCursor
            if (root.window) {
                root.window.startSystemMove()
            }
        }
        onReleased: cursorShape = Qt.OpenHandCursor
        onCanceled: cursorShape = Qt.OpenHandCursor
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 1
        anchors.topMargin: 1
        width: 12
        height: 12
        radius: 6
        color: "#FF5F57"
        z: 20

        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            cursorShape: Qt.PointingHandCursor
            onClicked: root.close()
        }
    }
}
