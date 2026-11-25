/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Audacity-CLA-applies
 *
 * Audacity
 * A Digital Audio Editor
 *
 * Copyright (C) 2024 Audacity BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15

import Muse.Ui 1.0
import Muse.UiComponents

StyledDialogView {
    id: root

    title: qsTrc("preferences", "Preferences")

    // The Appearance page now contains Logic's full 24-column palette. Open
    // wide enough to show it without manual resizing; DialogView will still
    // clamp this to smaller displays.
    contentWidth: 1420
    contentHeight: 640
    resizable: true

    property string currentPageId: ""
    property var params: null

    // GlideMix: closing with unapplied changes (red button, Esc) asks
    // whether to apply them, as Mixxx's preferences do. Our own OK/Cancel
    // have already decided, so they skip the question.
    property bool closeDecided: false

    onAboutToClose: function(closeEvent) {
        if (root.closeDecided || !preferencesModel.hasUnappliedChanges()) {
            return
        }
        const answer = preferencesModel.askApplyBeforeClose()
        if (answer === "cancel" || (answer === "apply" && !root.applyPages())) {
            closeEvent.accepted = false
            return
        }
        if (answer === "apply") {
            preferencesModel.apply()
        } else {
            preferencesModel.cancel()
        }
        root.closeDecided = true
    }

    function applyPages() {
        var ok = true
        var pages = preferencesModel.availablePages()
        for (var i in pages) {
            var obj = root.prv.pagesObjects[pages[i].id]
            if (Boolean(obj)) {
                ok &= obj.apply()
            }
        }
        return ok
    }

    property QtObject prv: QtObject {
        property var pagesObjects: (new Map())

        function resolveStackCurrentIndex() {
            var keys = Object.keys(root.prv.pagesObjects)
            return keys.indexOf(preferencesModel.currentPageId)
        }

        function updateStackCurrentIndex() {
            stack.currentIndex = resolveStackCurrentIndex()
        }
    }

    onNavigationActivateRequested: {
        menu.focusCurrentPage = true
    }

    Component.onCompleted: {
        preferencesModel.load(root.currentPageId)

        initPagesObjects()

        prv.updateStackCurrentIndex()
    }

    function initPagesObjects() {
        var pages = preferencesModel.availablePages()
        for (var i in pages) {
            var pageInfo = pages[i]

            if (!Boolean(pageInfo.path)) {
                continue
            }

            var pageComponent = Qt.createComponent("../" + pageInfo.path)

            if (pageComponent.status === Component.Error) {
                console.error("Error loading page:", pageInfo.path, pageComponent.errorString())
                continue
            }

            var properties = {
                navigationSection: root.navigationSection,
                navigationOrderStart: (i + 1) * 100
            }

            if (root.currentPageId === pageInfo.id) {
                var params = root.params
                for (var key in params) {
                    var value = params[key]
                    properties[key] = value
                }
            }

            var obj = pageComponent.createObject(stack, properties)

            if (!Boolean(obj)) {
                continue
            }

            obj.hideRequested.connect(function () {
                root.hide()
            })

            root.prv.pagesObjects[pageInfo.id] = obj
        }
    }

    //! GlideMix: [{pageId, pageTitle, label, item}] for every label on a page that contains
    //! all the words searched for (any order, any case), the page names first.
    function searchSettings(text) {
        const words = text.toLowerCase().split(/\s+/).filter(function (w) { return w.length > 0 })
        if (words.length === 0) {
            return []
        }
        const matches = function (t) {
            const low = t.toLowerCase()
            return words.every(function (w) { return low.indexOf(w) !== -1 })
        }
        const results = []
        const pages = preferencesModel.availablePages()
        for (let i = 0; i < pages.length; ++i) {
            const title = pages[i].title || ""
            if (title && matches(title)) {
                results.push({ pageId: pages[i].id, pageTitle: title, label: title, item: null })
            }
        }
        for (let i = 0; i < pages.length && results.length < 80; ++i) {
            const page = root.prv.pagesObjects[pages[i].id]
            if (!page) {
                continue
            }
            const seen = {}
            const walk = function (item, depth) {
                if (!item || depth > 40 || results.length >= 80) {
                    return
                }
                const t = (typeof item.text === "string") ? item.text.replace(/<[^>]*>/g, "").trim() : ""
                if (t.length > 1 && t.length < 160 && !seen[t] && matches(t)) {
                    seen[t] = true
                    results.push({ pageId: pages[i].id, pageTitle: pages[i].title || "", label: t, item: item })
                }
                const kids = item.children
                for (let k = 0; kids && k < kids.length; ++k) {
                    walk(kids[k], depth + 1)
                }
            }
            walk(page, 0)
        }
        return results
    }

    function openSearchResult(result) {
        preferencesModel.currentPageId = result.pageId
        const page = root.prv.pagesObjects[result.pageId]
        if (page && result.item && page.revealItem) {
            Qt.callLater(function () { page.revealItem(result.item) })
        }
    }

    PreferencesModel {
        id: preferencesModel

        onCurrentPageIdChanged: function (currentPageId) {
            prv.updateStackCurrentIndex()
        }
    }

    ColumnLayout {
        anchors.fill: parent

        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            spacing: 0

            // GlideMix (Chris, 2026-11-21): a "Search settings" field over the page list, as in
            // the reference DAW. It reads the labels on every page (they are all made when the
            // dialog opens), so it finds a setting by its own name, in the app's language.
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 240
                color: ui.theme.backgroundPrimaryColor

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    SearchField {
                        id: settingsSearch
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        Layout.topMargin: 12
                        Layout.preferredHeight: 30
                        hint: qsTrc("preferences", "Search settings")
                        onSearchTextChanged: searchResults.model = root.searchSettings(searchText)
                    }

                    PreferencesMenu {
                        id: menu

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: settingsSearch.searchText.length === 0

                        navigation.section: root.navigationSection
                        navigation.order: 1

                        model: preferencesModel
                    }

                    StyledListView {
                        id: searchResults

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.topMargin: 10
                        visible: !menu.visible
                        clip: true
                        spacing: 2

                        delegate: ListItemBlank {
                            required property var modelData
                            width: searchResults.width
                            height: 44
                            onClicked: root.openSearchResult(modelData)

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 16
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                StyledTextLabel {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignLeft
                                    elide: Text.ElideRight
                                    text: modelData.label
                                }
                                StyledTextLabel {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignLeft
                                    elide: Text.ElideRight
                                    opacity: 0.6
                                    font.pixelSize: 11
                                    text: modelData.pageTitle
                                }
                            }
                        }

                        StyledTextLabel {
                            anchors.centerIn: parent
                            visible: searchResults.count === 0
                            opacity: 0.6
                            text: qsTrc("preferences", "No settings found")
                        }
                    }
                }
            }

            SeparatorLine {
                orientation: Qt.Vertical
            }

            StackLayout {
                id: stack
            }
        }

        SeparatorLine {}

        PreferencesButtonsPanel {
            id: buttonsPanel

            Layout.fillWidth: true
            Layout.preferredHeight: 70

            navigation.section: root.navigationSection
            navigation.order: 100000

            onRevertFactorySettingsRequested: {
                var pages = preferencesModel.availablePages()

                for (var i in pages) {
                    var page = pages[i]
                    var obj = root.prv.pagesObjects[page.id]
                    if (Boolean(obj)) {
                        obj.reset()
                    }
                }

                preferencesModel.resetFactorySettings()
            }

            onRejectRequested: {
                preferencesModel.cancel()
                root.closeDecided = true
                root.reject()
            }

            onApplyAndContinueRequested: {
                if (root.applyPages()) {
                    preferencesModel.applyAndContinue()
                }
            }

            onApplyRequested: {
                if (root.applyPages()) {
                    preferencesModel.apply()
                    root.closeDecided = true
                    root.hide()
                }
            }
        }
    }
}
