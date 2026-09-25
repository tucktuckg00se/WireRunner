// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WireRunner 1.0

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "WireRunner"
    color: "#111418"
    property string mediaFilter: "all"
    property string searchText: ""
    property var visibleNodes: graph.nodes.filter(function(node) {
        return (mediaFilter === "all" || node.media === mediaFilter)
            && (searchText === "" || node.name.toLowerCase().includes(searchText)
                || node.technicalName.toLowerCase().includes(searchText))
    })
    property var selected: ({ kind: "node", name: "Select an object", detail: "Click a node or link on the graph", media: "" })

    function mediaColor(media) {
        if (media === "video") return "#e9bb69"
        if (media === "midi") return "#b49cff"
        if (media === "audio") return "#83dc9a"
        return "#8e98a9"
    }

    header: Rectangle {
        height: 54
        color: "#1c2026"
        border.color: "#303741"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 14
            spacing: 14
            Rectangle { width: 18; height: 18; radius: 9; color: "transparent"; border.width: 2; border.color: "#68c9cf" }
            Label { text: "WireRunner"; color: "#edf1f6"; font.pixelSize: 15; font.bold: true }
            Rectangle { width: 1; Layout.fillHeight: true; color: "#343b45" }
            Label { text: graph.statusText; color: graph.connected ? "#a9dcb5" : "#e9bb69"; font.pixelSize: 11 }
            Item { Layout.fillWidth: true }
            Label { text: graph.remoteSummary; color: "#8f9aa8"; font.pixelSize: 10 }
            Label { text: graph.nodeCount + " nodes  ·  " + graph.linkCount + " links"; color: "#aeb7c2"; font.pixelSize: 10 }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#171b20"
            border.color: "#333b45"
            RowLayout {
                anchors.fill: parent; anchors.margins: 7; spacing: 6
                Repeater {
                    model: ["all", "audio", "video", "midi"]
                    delegate: Button {
                        required property string modelData
                        text: modelData === "all" ? "All media" : modelData.charAt(0).toUpperCase() + modelData.slice(1)
                        checked: window.mediaFilter === modelData
                        checkable: true
                        onClicked: window.mediaFilter = modelData
                    }
                }
                Item { Layout.fillWidth: true }
                TextField {
                    Layout.preferredWidth: 240
                    placeholderText: "Find on graph…"
                    onTextChanged: window.searchText = text.toLowerCase()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: "#11151a"
                border.color: "#303842"
                Flickable {
                    id: viewport
                    anchors.fill: parent
                    clip: true
                    contentWidth: 1120
                    contentHeight: Math.max(700, graph.nodeCount * 90)
                    boundsBehavior: Flickable.StopAtBounds

                    LinkLayer {
                        id: links
                        width: viewport.contentWidth; height: viewport.contentHeight
                        nodes: window.visibleNodes; links: graph.links
                        mediaFilter: window.mediaFilter
                        z: 1
                        onLinkActivated: (id, fromName, toName, media, state) => {
                            window.selected = { kind: "link", name: fromName + " → " + toName,
                                detail: state + " " + media + " link", media: media, id: id }
                        }
                    }

                    Repeater {
                        model: window.visibleNodes
                        delegate: Rectangle {
                            required property var modelData
                            x: modelData.x; y: modelData.y
                            width: 220; height: 112; radius: 5; z: 2
                            color: "#292f37"
                            border.width: window.selected.kind === "node" && window.selected.id === modelData.id ? 2 : 1
                            border.color: window.selected.kind === "node" && window.selected.id === modelData.id ? "#83afff" : "#4a535e"

                            Rectangle { x: 0; y: 0; width: 5; height: parent.height; color: window.mediaColor(modelData.media); radius: 2 }
                            Column {
                                anchors.left: parent.left; anchors.leftMargin: 16; anchors.right: parent.right; anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter; spacing: 7
                                Label { text: modelData.name; color: "#e2e6ec"; font.bold: true; elide: Text.ElideRight; width: parent.width }
                                Label { text: modelData.mediaClass || modelData.media; color: "#929daa"; font.pixelSize: 10; elide: Text.ElideRight; width: parent.width }
                                Row { spacing: 7
                                    Rectangle { width: 7; height: 7; radius: 4; color: modelData.state === "running" ? "#83dc9a" : "#687483" }
                                    Label { text: modelData.state + " · " + modelData.role; color: "#adb6c1"; font.pixelSize: 10 }
                                }
                            }
                            Rectangle { visible: modelData.role !== "source"; x: -6; y: 50; width: 12; height: 12; radius: 6; color: "#11151a"; border.width: 2; border.color: window.mediaColor(modelData.media) }
                            Rectangle { visible: modelData.role !== "destination"; x: parent.width - 6; y: 50; width: 12; height: 12; radius: 6; color: "#11151a"; border.width: 2; border.color: window.mediaColor(modelData.media) }
                            TapHandler { onTapped: window.selected = { kind: "node", name: modelData.name, detail: modelData.mediaClass, media: modelData.media, state: modelData.state, technicalName: modelData.technicalName, stableId: modelData.stableId, id: modelData.id } }
                        }
                    }

                    Label { visible: graph.nodeCount === 0 && graph.connected; anchors.centerIn: parent; text: "No media objects are currently available."; color: "#929daa" }
                }
                Rectangle {
                    visible: !graph.connected && graph.nodeCount === 0
                    anchors.centerIn: parent; width: 390; height: 150; radius: 6
                    color: "#20262d"; border.color: "#49535e"
                    Column { anchors.centerIn: parent; spacing: 12
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "PipeWire is unavailable"; color: "#edf1f6"; font.pixelSize: 17; font.bold: true }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: graph.statusText; color: "#aab4c0" }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Run with --demo to explore the interface."; color: "#83afff"; font.pixelSize: 11 }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 292; Layout.fillHeight: true
                color: "#20252c"; border.color: "#3d4651"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 17; spacing: 14
                    Label { text: "Selected"; color: "#929daa"; font.pixelSize: 10 }
                    Label { text: window.selected.name; color: "#e2e6ec"; font.pixelSize: 18; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Label { text: window.selected.detail; color: "#aab4c0"; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#3b444e" }
                    GridLayout {
                        columns: 2; Layout.fillWidth: true
                        Label { text: "Type"; color: "#929daa"; font.pixelSize: 10 }
                        Label { text: window.selected.kind; color: "#d4dae2"; Layout.alignment: Qt.AlignRight }
                        Label { text: "Media"; color: "#929daa"; font.pixelSize: 10 }
                        Label { text: window.selected.media || "—"; color: window.mediaColor(window.selected.media); Layout.alignment: Qt.AlignRight }
                        Label { visible: window.selected.state !== undefined; text: "State"; color: "#929daa"; font.pixelSize: 10 }
                        Label { visible: window.selected.state !== undefined; text: window.selected.state || "—"; color: "#d4dae2"; Layout.alignment: Qt.AlignRight }
                    }
                    Label { visible: window.selected.technicalName !== undefined; text: window.selected.technicalName || ""; color: "#75808d"; font.family: "monospace"; font.pixelSize: 9; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                    Label { text: "Read-only milestone"; color: "#687483"; font.pixelSize: 10 }
                }
            }
        }
    }
}
