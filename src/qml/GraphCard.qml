// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    required property var card
    required property bool selected
    property var routing: ({})
    signal selectedRequested
    signal toggleRequested
    signal moved(real x, real y)
    signal moveStarted(real x, real y)
    signal moving(real x, real y)
    signal expandForRoutingRequested
    signal routeStarted(var portId, real graphX, real graphY)
    signal routeMoved(real graphX, real graphY)
    signal routeFinished(real graphX, real graphY)
    signal routeTargetRequested(var portId)
    signal volumeRequested(var control, real percent)
    signal muteRequested(var control, bool muted)

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
    function containsPort(values, id) {
        if (!values) return false
        for (let index = 0; index < values.length; ++index)
            if (Number(values[index]) === Number(id)) return true
        return false
    }
    function focusFirstCompatibleInput() {
        for (let index = 0; index < inputRepeater.count; ++index) {
            const target = inputRepeater.itemAt(index)
            if (target && target.routeCompatible && target.activeFocusOnTab) {
                target.forceActiveFocus()
                return true
            }
        }
        return false
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
    onXChanged: if (moveHandler.active) moving(x, y)
    onYChanged: if (moveHandler.active) moving(x, y)

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
            GraphText {
                width: parent.width
                text: root.card.title
                color: "#edf0f2"
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            GraphText {
                width: parent.width
                text: root.card.subtitle
                color: "#9ba6af"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Row {
                spacing: 7
                Rectangle { width: 7; height: 7; radius: 4; color: root.card.state === "running" ? "#68d18b" : "#697680"; anchors.verticalCenter: parent.verticalCenter }
                GraphText { text: root.card.state; color: "#b8c1c8"; font.pixelSize: 11 }
                GraphText { visible: root.card.nodeCount > 1; text: root.card.nodeCount + " nodes"; color: "#77838d"; font.pixelSize: 11 }
                GraphText {
                    visible: (root.card.defaultBadges || []).length > 0
                    text: (root.card.defaultBadges || []).join(" · ")
                    color: "#7da7e8"; font.pixelSize: 9
                }
            }
        }
        ToolButton {
            id: expandButton
            anchors.right: parent.right
            anchors.top: parent.top
            width: 72; height: 28
            visible: root.card.inputCount + root.card.outputCount > 0
            text: root.card.expanded ? "Collapse" : (root.card.inputCount + root.card.outputCount) + " ports"
            font.pixelSize: 10
            contentItem: GraphText {
                text: expandButton.text
                color: expandButton.enabled ? "#d6dce0" : "#77838d"
                font: expandButton.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            onClicked: root.toggleRequested()
            Accessible.name: (root.card.expanded ? "Collapse ports for " : "Expand ports for ") + root.card.title
        }
    }

    Rectangle {
        id: inlineMixer
        visible: root.card.inlineAudio && root.card.inlineAudio.targetKind !== undefined
        x: 16; y: 91; width: parent.width - 28; height: 46
        radius: 3; color: "#1c2227"; border.color: "#354049"
        property var control: root.card.inlineAudio || ({})
        function commit() { root.volumeRequested(control, inlineVolume.value) }
        Row {
            anchors.fill: parent; anchors.margins: 7; spacing: 7
            GraphText {
                width: 35; anchors.verticalCenter: parent.verticalCenter
                text: inlineMixer.control.direction === "capture" ? "Input" : "Out 1–2"
                color: "#9ba6af"; font.pixelSize: 9; elide: Text.ElideRight
            }
            LevelControl {
                id: inlineVolume
                objectName: "inlineVolume"
                width: 135; height: 28; anchors.verticalCenter: parent.verticalCenter
                from: inlineMixer.control.minimum || 0
                to: inlineMixer.control.maximum || 100
                value: inlineMixer.control.volume || 0
                controlEnabled: Boolean(inlineMixer.control.writable) && Boolean(inlineMixer.control.hasVolume)
                compact: true
                accessibleName: root.card.title + " first two channel volume"
                onCommitted: percent => root.volumeRequested(inlineMixer.control, percent)
            }
            ToolButton {
                width: 28; height: 28; anchors.verticalCenter: parent.verticalCenter
                visible: Boolean(inlineMixer.control.hasMute)
                enabled: Boolean(inlineMixer.control.writable)
                checkable: true; checked: Boolean(inlineMixer.control.muted)
                text: checked ? "M" : "○"
                onClicked: root.muteRequested(inlineMixer.control, checked)
                Accessible.name: (checked ? "Unmute " : "Mute ") + root.card.title
            }
        }
    }

    Repeater {
        id: inputRepeater
        model: root.card.expanded ? root.card.inputs : root.card.inputGroups
        delegate: Item {
            required property var modelData
            required property int index
            x: 0; y: (root.card.portTop || 100) + index * 28
            width: root.width / 2 - 8; height: 20
            property bool routeCompatible: root.containsPort(root.routing.compatibleInputs, modelData.id)
            property bool routeTarget: Boolean(root.routing.active) && Number(root.routing.targetPortId) === Number(modelData.id)
            property string routeReason: !root.routing.active || modelData.id === undefined ? ""
                : routeCompatible ? (root.routing.compatibleNotes[String(modelData.id)] || "")
                : (root.routing.incompatibleInputs[String(modelData.id)] || "This target is incompatible")
            opacity: root.routing.active && modelData.id !== undefined && !routeCompatible ? 0.32 : 1
            activeFocusOnTab: modelData.id !== undefined && (!root.routing.active || routeCompatible)
            Accessible.role: Accessible.Button
            Accessible.name: "Input " + root.portLabel(modelData)
            Accessible.description: root.routing.active ? (routeCompatible ? "Compatible routing target" : "Incompatible routing target") : "Input port"
            HoverHandler { id: routeHover }
            ToolTip.visible: routeHover.hovered && routeReason.length > 0
            ToolTip.text: routeReason
            Rectangle {
                x: -6; y: 3; width: 13; height: 13; radius: 7; color: routeTarget ? root.mediaColor(modelData.media) : "#12161a"
                border.width: routeTarget ? 3 : 2
                border.color: root.routing.active && routeCompatible ? "#edf0f2" : root.mediaColor(modelData.media)
            }
            GraphText { x: 15; width: parent.width - 18; anchors.verticalCenter: parent.verticalCenter; text: root.portLabel(modelData); color: "#d1d7dc"; font.pixelSize: 11; elide: Text.ElideRight }
            MouseArea {
                x: -10; y: -4; width: 32; height: 28
                preventStealing: true
                cursorShape: root.routing.active && parent.routeCompatible ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (root.routing.active && modelData.id !== undefined) root.routeTargetRequested(modelData.id)
                    else if (modelData.count > 1) root.expandForRoutingRequested()
                    else root.selectedRequested()
                }
            }
            Keys.onReturnPressed: if (root.routing.active && modelData.id !== undefined) root.routeTargetRequested(modelData.id)
            Keys.onEscapePressed: root.routeFinished(-10000, -10000)
        }
    }
    Repeater {
        model: root.card.expanded ? root.card.outputs : root.card.outputGroups
        delegate: Item {
            required property var modelData
            required property int index
            x: root.width / 2 + 8; y: (root.card.portTop || 100) + index * 28
            width: root.width / 2 - 8; height: 20
            property bool exactPort: modelData.id !== undefined && modelData.id !== null
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: exactPort ? "Route from output " + root.portLabel(modelData) : "Expand " + root.portLabel(modelData)
            GraphText { width: parent.width - 15; anchors.verticalCenter: parent.verticalCenter; horizontalAlignment: Text.AlignRight; text: root.portLabel(modelData); color: "#d1d7dc"; font.pixelSize: 11; elide: Text.ElideLeft }
            Rectangle {
                x: parent.width - 7; y: 3; width: 13; height: 13; radius: 7
                color: root.routing.active && Number(root.routing.outputPortId) === Number(modelData.id) ? root.mediaColor(modelData.media) : "#12161a"
                border.width: 2; border.color: root.mediaColor(modelData.media)
            }
            MouseArea {
                id: routeMouse
                x: parent.width - 20; y: -4; width: 32; height: 28
                preventStealing: true
                cursorShape: Qt.CrossCursor
                property bool routeInProgress: false
                onPressed: function(mouse) {
                    if (!parent.exactPort) {
                        root.expandForRoutingRequested()
                        return
                    }
                    const point = mapToItem(root.parent, mouse.x, mouse.y)
                    routeInProgress = true
                    root.routeStarted(modelData.id, point.x, point.y)
                }
                onPositionChanged: function(mouse) {
                    if (!routeInProgress) return
                    const point = mapToItem(root.parent, mouse.x, mouse.y)
                    root.routeMoved(point.x, point.y)
                }
                onReleased: function(mouse) {
                    if (!routeInProgress) return
                    const point = mapToItem(root.parent, mouse.x, mouse.y)
                    routeInProgress = false
                    root.routeFinished(point.x, point.y)
                }
                onCanceled: {
                    if (routeInProgress) root.routeFinished(-10000, -10000)
                    routeInProgress = false
                }
            }
            Keys.onReturnPressed: {
                if (!exactPort) root.expandForRoutingRequested()
                else root.routeStarted(modelData.id, root.x + root.width, root.y + (root.card.portTop || 100) + index * 28 + 9)
            }
            Keys.onEscapePressed: root.routeFinished(-10000, -10000)
        }
    }

    TapHandler { onTapped: root.selectedRequested() }
    DragHandler {
        id: moveHandler
        target: root
        acceptedButtons: Qt.LeftButton
        grabPermissions: PointerHandler.CanTakeOverFromHandlersOfSameType | PointerHandler.ApprovesTakeOverByAnything
        onActiveChanged: {
            if (active) root.moveStarted(root.x, root.y)
            else root.moved(root.x, root.y)
        }
    }
    Keys.onReturnPressed: root.selectedRequested()
    Keys.onSpacePressed: root.selectedRequested()
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_E) { root.toggleRequested(); event.accepted = true }
    }
}
