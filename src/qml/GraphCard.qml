// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    required property var card
    required property bool selected
    signal selectedRequested
    signal toggleRequested
    signal moved(real x, real y)

    function mediaColor(media) {
        if (media === "video") return "#e6b765"
        if (media === "midi") return "#a893f5"
        if (media === "audio") return "#68d18b"
        return "#89949e"
    }
    function portLabel(port) {
        if (port.label !== undefined) return port.label
        if (port.channel) return port.channel + "  " + port.name
        return port.name
    }

    width: card.width
    height: card.height
    visible: card.visible
    opacity: card.focused ? 1 : 0.19
    radius: 4
    color: "#242b31"
    border.width: selected ? 2 : 1
    border.color: selected ? "#7da7e8" : "#46515b"
    z: selected ? 4 : 2
    focus: selected
    activeFocusOnTab: true
    Accessible.role: Accessible.ListItem
    Accessible.name: card.title + ", " + card.subtitle + ", " + card.state

    Behavior on opacity { NumberAnimation { duration: 110 } }

    Binding { target: root; property: "x"; value: root.card.x; when: !moveHandler.active }
    Binding { target: root; property: "y"; value: root.card.y; when: !moveHandler.active }

    Rectangle {
        width: 5
        height: parent.height
        radius: 2
        color: root.mediaColor(root.card.media)
    }

    Item {
        id: header
        x: 16; y: 12; width: parent.width - 28; height: 76
        Column {
            width: parent.width - expandButton.width - 8
            spacing: 5
            Text {
                width: parent.width
                text: root.card.title
                color: "#edf0f2"
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: root.card.subtitle
                color: "#9ba6af"
                font.pixelSize: 10
                elide: Text.ElideRight
            }
            Row {
                spacing: 7
                Rectangle { width: 7; height: 7; radius: 4; color: root.card.state === "running" ? "#68d18b" : "#697680"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: root.card.state; color: "#b8c1c8"; font.pixelSize: 10 }
                Text { visible: root.card.nodeCount > 1; text: root.card.nodeCount + " nodes"; color: "#77838d"; font.pixelSize: 10 }
            }
        }
        ToolButton {
            id: expandButton
            anchors.right: parent.right
            anchors.top: parent.top
            width: 72; height: 28
            visible: root.card.inputCount + root.card.outputCount > 0
            text: root.card.expanded ? "Collapse" : (root.card.inputCount + root.card.outputCount) + " ports"
            font.pixelSize: 9
            onClicked: root.toggleRequested()
            Accessible.name: (root.card.expanded ? "Collapse ports for " : "Expand ports for ") + root.card.title
        }
    }

    Repeater {
        model: root.card.expanded ? root.card.inputs : root.card.inputGroups
        delegate: Item {
            required property var modelData
            required property int index
            x: 0; y: 100 + index * 28
            width: root.width / 2 - 8; height: 20
            Rectangle { x: -5; y: 4; width: 11; height: 11; radius: 6; color: "#12161a"; border.width: 2; border.color: root.mediaColor(modelData.media) }
            Text { x: 13; width: parent.width - 16; anchors.verticalCenter: parent.verticalCenter; text: root.portLabel(modelData); color: "#c3cbd1"; font.pixelSize: 9; elide: Text.ElideRight }
        }
    }
    Repeater {
        model: root.card.expanded ? root.card.outputs : root.card.outputGroups
        delegate: Item {
            required property var modelData
            required property int index
            x: root.width / 2 + 8; y: 100 + index * 28
            width: root.width / 2 - 8; height: 20
            Text { width: parent.width - 13; anchors.verticalCenter: parent.verticalCenter; horizontalAlignment: Text.AlignRight; text: root.portLabel(modelData); color: "#c3cbd1"; font.pixelSize: 9; elide: Text.ElideLeft }
            Rectangle { x: parent.width - 6; y: 4; width: 11; height: 11; radius: 6; color: "#12161a"; border.width: 2; border.color: root.mediaColor(modelData.media) }
        }
    }

    TapHandler { onTapped: root.selectedRequested() }
    DragHandler {
        id: moveHandler
        target: root
        acceptedButtons: Qt.LeftButton
        grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType
        onActiveChanged: if (!active) root.moved(root.x, root.y)
    }
    Keys.onReturnPressed: root.selectedRequested()
    Keys.onSpacePressed: root.selectedRequested()
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_E) { root.toggleRequested(); event.accepted = true }
    }
}
