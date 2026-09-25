// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    required property var selection
    required property bool focusActive
    signal focusRequested
    signal clearFocusRequested
    signal toggleRequested(string key)
    signal disconnectRequested

    color: "#1d2328"
    border.color: "#3c4650"

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: 20
                spacing: 10
                Text { text: root.selection.key ? "Selected" : "Nothing selected"; color: "#83909a"; font.pixelSize: 10 }
                Text {
                    Layout.fillWidth: true
                    text: root.selection.selectionKind === "link" ? root.selection.fromName + " to " + root.selection.toName
                        : root.selection.key ? root.selection.title : "Choose a card or wire"
                    color: "#edf0f2"; font.pixelSize: 19; font.weight: Font.DemiBold; wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    text: root.selection.selectionKind === "link" ? root.selection.media + " link, " + root.selection.state
                        : root.selection.key ? root.selection.subtitle : "Details stay attached to the object you select on the graph."
                    color: "#a9b3bb"; font.pixelSize: 11; wrapMode: Text.Wrap
                }
                RowLayout {
                    visible: root.selection.key !== undefined
                    Layout.fillWidth: true
                    Button {
                        Layout.fillWidth: true
                        text: root.focusActive ? "Show full graph" : "Focus signal path"
                        onClicked: root.focusActive ? root.clearFocusRequested() : root.focusRequested()
                    }
                    Button {
                        visible: root.selection.selectionKind === "card"
                        text: root.selection.expanded ? "Collapse" : "Ports"
                        onClicked: root.toggleRequested(root.selection.key)
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#39434c" }

            ColumnLayout {
                visible: root.selection.selectionKind === "card"
                Layout.fillWidth: true; Layout.margins: 20; spacing: 12
                GridLayout {
                    columns: 2; Layout.fillWidth: true; columnSpacing: 16; rowSpacing: 9
                    Text { text: "State"; color: "#83909a"; font.pixelSize: 10 }
                    Text { text: root.selection.state || "Unknown"; color: "#d6dce0"; Layout.alignment: Qt.AlignRight }
                    Text { text: "Media"; color: "#83909a"; font.pixelSize: 10 }
                    Text { text: root.selection.mediaTypes ? root.selection.mediaTypes.join(", ") : "Unknown"; color: "#d6dce0"; Layout.alignment: Qt.AlignRight }
                    Text { text: "Nodes"; color: "#83909a"; font.pixelSize: 10 }
                    Text { text: root.selection.nodeCount || 0; color: "#d6dce0"; Layout.alignment: Qt.AlignRight }
                    Text { text: "Ports"; color: "#83909a"; font.pixelSize: 10 }
                    Text { text: (root.selection.inputCount || 0) + " in, " + (root.selection.outputCount || 0) + " out"; color: "#d6dce0"; Layout.alignment: Qt.AlignRight }
                    Text { text: "Connections"; color: "#83909a"; font.pixelSize: 10 }
                    Text { text: root.selection.connectionCount || 0; color: "#d6dce0"; Layout.alignment: Qt.AlignRight }
                }
                ToolButton { id: nodeDisclosure; text: checked ? "Hide member nodes" : "Show member nodes"; checkable: true }
                ColumnLayout {
                    visible: nodeDisclosure.checked; Layout.fillWidth: true; spacing: 8
                    Repeater {
                        model: root.selection.members || []
                        delegate: Rectangle {
                            required property var modelData
                            Layout.fillWidth: true; Layout.preferredHeight: 55; color: "#252c32"; radius: 3
                            Column { anchors.fill: parent; anchors.margins: 9; spacing: 4
                                Text { width: parent.width; text: modelData.name; color: "#dce1e4"; font.pixelSize: 11; elide: Text.ElideRight }
                                Text { width: parent.width; text: modelData.technicalName; color: "#7f8b95"; font.pixelSize: 9; elide: Text.ElideMiddle }
                            }
                        }
                    }
                }
                ToolButton { id: portDisclosure; text: checked ? "Hide port details" : "Show port details"; checkable: true }
                ColumnLayout {
                    visible: portDisclosure.checked; Layout.fillWidth: true; spacing: 5
                    Repeater {
                        model: (root.selection.inputs || []).concat(root.selection.outputs || [])
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Text { text: modelData.direction === "input" ? "IN" : "OUT"; color: modelData.direction === "input" ? "#7da7e8" : "#68d18b"; font.pixelSize: 9; font.weight: Font.DemiBold }
                            Text { Layout.fillWidth: true; text: modelData.name; color: "#cbd2d7"; font.pixelSize: 10; elide: Text.ElideRight }
                            Text { text: modelData.format || modelData.channel || modelData.media; color: "#7f8b95"; font.pixelSize: 9 }
                        }
                    }
                }
                Text { visible: root.selection.persistent === false; Layout.fillWidth: true; text: "This object has no stable identity, so its position is kept only for this session."; color: "#e6b765"; font.pixelSize: 10; wrapMode: Text.Wrap }
            }

            ColumnLayout {
                visible: root.selection.selectionKind === "link"
                Layout.fillWidth: true; Layout.margins: 20; spacing: 12
                Text { text: "Exact ports"; color: "#83909a"; font.pixelSize: 10 }
                Text { Layout.fillWidth: true; text: root.selection.outputPortName || "Unknown output"; color: "#dce1e4"; wrapMode: Text.Wrap }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#39434c" }
                Text { Layout.fillWidth: true; text: root.selection.inputPortName || "Unknown input"; color: "#dce1e4"; wrapMode: Text.Wrap }
                GridLayout {
                    columns: 2; Layout.fillWidth: true
                    Text { text: "Lifetime"; color: "#83909a"; font.pixelSize: 10 }
                    Text { text: root.selection.linger ? "Until removed" : "Owner session"; color: "#d6dce0"; Layout.alignment: Qt.AlignRight }
                    Text { text: "Feedback"; color: "#83909a"; font.pixelSize: 10 }
                    Text { text: root.selection.feedback ? "One-cycle delay" : "No"; color: "#d6dce0"; Layout.alignment: Qt.AlignRight }
                    Text { text: "Created by"; color: "#83909a"; font.pixelSize: 10 }
                    Text { text: root.selection.createdByWireRunner ? "WireRunner" : "PipeWire client or policy"; color: "#d6dce0"; Layout.alignment: Qt.AlignRight }
                }
                Button {
                    Layout.fillWidth: true
                    text: "Disconnect"
                    enabled: root.selection.canDestroy !== false
                    onClicked: root.disconnectRequested()
                    Accessible.description: "Remove only this exact PipeWire link"
                }
                Text { visible: root.selection.canDestroy === false; Layout.fillWidth: true; text: "PipeWire does not grant permission to remove this link."; color: "#e6b765"; font.pixelSize: 10; wrapMode: Text.Wrap }
            }
            Item { Layout.fillHeight: true; Layout.minimumHeight: 20 }
        }
    }
}
