// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WireRunner 1.0

ApplicationWindow {
    id: window
    width: 1360
    height: 820
    minimumWidth: 920
    minimumHeight: 620
    visible: true
    title: "WireRunner"
    color: "#12161a"

    function mediaColor(media) {
        if (media === "video") return "#e6b765"
        if (media === "midi") return "#a893f5"
        if (media === "audio") return "#68d18b"
        return "#89949e"
    }
    function focusFirstRoutingTarget() {
        for (let index = 0; index < cardRepeater.count; ++index) {
            const card = cardRepeater.itemAt(index)
            if (card && card.focusFirstCompatibleInput()) return
        }
    }
    property var liveCard: ({})

    header: Rectangle {
        height: 54
        color: "#1b2025"
        border.color: "#343e47"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 16
            spacing: 13
            Item {
                width: 20; height: 20
                Rectangle { x: 1; y: 3; width: 8; height: 8; radius: 4; color: "transparent"; border.width: 2; border.color: "#68d18b" }
                Rectangle { x: 11; y: 9; width: 8; height: 8; radius: 4; color: "transparent"; border.width: 2; border.color: "#a893f5" }
                Rectangle { x: 8; y: 7; width: 5; height: 2; rotation: 35; color: "#7d8993" }
            }
            Text { text: "WireRunner"; color: "#edf0f2"; font.pixelSize: 15; font.weight: Font.DemiBold }
            Rectangle { width: 1; Layout.fillHeight: true; color: "#354049" }
            Rectangle { width: 7; height: 7; radius: 4; color: graph.connected ? "#68d18b" : "#e6b765" }
            Text { text: graph.statusText; color: "#aab4bc"; font.pixelSize: 10 }
            Item { Layout.fillWidth: true }
            Text { text: graph.remoteSummary; color: "#77838d"; font.pixelSize: 9 }
            Text { text: graph.cardCount + " objects  /  " + graph.linkCount + " links"; color: "#aab4bc"; font.pixelSize: 10 }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#171c20"
            border.color: "#343e47"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 6
                Repeater {
                    model: ["all", "audio", "video", "midi"]
                    delegate: Button {
                        required property string modelData
                        text: modelData === "all" ? "All media" : modelData.charAt(0).toUpperCase() + modelData.slice(1)
                        checked: graph.mediaFilter === modelData
                        checkable: true
                        onClicked: graph.mediaFilter = modelData
                    }
                }
                Rectangle { width: 1; Layout.fillHeight: true; color: "#343e47"; Layout.leftMargin: 6; Layout.rightMargin: 6 }
                Button {
                    visible: graph.focusActive
                    text: "Show full graph"
                    onClicked: graph.clearFocus()
                }
                ToolButton { text: "Undo"; enabled: graph.canUndo; onClicked: graph.undo(); Accessible.name: "Undo routing change" }
                ToolButton { text: "Redo"; enabled: graph.canRedo; onClicked: graph.redo(); Accessible.name: "Redo routing change" }
                Item { Layout.fillWidth: true }
                TextField {
                    id: searchField
                    Layout.preferredWidth: 250
                    placeholderText: "Find an object"
                    selectByMouse: true
                    onAccepted: {
                        const result = graph.findCard(text)
                        if (result.key !== undefined) viewport.reveal(result)
                    }
                    Accessible.description: "Press Enter to select and reveal a matching graph object"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                id: graphPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#12161a"
                border.color: "#303a43"

                Flickable {
                    id: viewport
                    anchors.fill: parent
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    contentWidth: graph.canvasWidth * zoom
                    contentHeight: graph.canvasHeight * zoom
                    property real zoom: 1.0
                    property bool fittedOnce: false

                    function clamp(value, minimum, maximum) { return Math.max(minimum, Math.min(maximum, value)) }
                    function setZoom(next, pointX, pointY) {
                        const old = zoom
                        next = clamp(next, 0.45, 1.8)
                        if (Math.abs(next - old) < 0.001) return
                        contentX = clamp((contentX + pointX) * next / old - pointX, 0, Math.max(0, graph.canvasWidth * next - width))
                        contentY = clamp((contentY + pointY) * next / old - pointY, 0, Math.max(0, graph.canvasHeight * next - height))
                        zoom = next
                    }
                    function fitGraph(showEverything) {
                        if (graph.cardCount === 0) return
                        const fittedZoom = Math.min(width / graph.canvasWidth, height / graph.canvasHeight) * 0.94
                        zoom = clamp(fittedZoom, showEverything ? 0.45 : 0.85, 1.0)
                        contentX = Math.max(0, (graph.canvasWidth * zoom - width) / 2)
                        contentY = Math.max(0, (graph.canvasHeight * zoom - height) / 2)
                        fittedOnce = true
                    }
                    function reveal(item) {
                        const centerX = (item.x + item.width / 2) * zoom
                        const centerY = (item.y + item.height / 2) * zoom
                        contentX = clamp(centerX - width / 2, 0, Math.max(0, contentWidth - width))
                        contentY = clamp(centerY - height / 2, 0, Math.max(0, contentHeight - height))
                    }

                    Item {
                        id: canvas
                        width: graph.canvasWidth
                        height: graph.canvasHeight
                        scale: viewport.zoom
                        transformOrigin: Item.TopLeft

                        MouseArea {
                            anchors.fill: parent
                            z: 0
                            onClicked: graph.clearSelection()
                        }

                        Repeater {
                            id: cardRepeater
                            model: graph.cards
                            delegate: GraphCard {
                                required property var item
                                card: item
                                selected: graph.selectedKey === item.key
                                routing: graph.routing
                                onSelectedRequested: graph.selectCard(item.key)
                                onToggleRequested: graph.toggleCard(item.key)
                                onMoveStarted: (x, y) => window.liveCard = ({key: item.key, baseX: item.x, baseY: item.y, x: x, y: y})
                                onMoving: (x, y) => window.liveCard = ({key: item.key, baseX: item.x, baseY: item.y, x: x, y: y})
                                onMoved: function(x, y) {
                                    graph.moveCard(item.key, x, y)
                                    window.liveCard = ({})
                                }
                                onExpandForRoutingRequested: graph.expandForRouting(item.key)
                                onRouteStarted: function(portId, x, y) {
                                    graph.beginRoute(portId, x, y)
                                    Qt.callLater(window.focusFirstRoutingTarget)
                                }
                                onRouteMoved: (x, y) => graph.updateRoute(x, y)
                                onRouteFinished: (x, y) => graph.finishRoute(x, y)
                                onRouteTargetRequested: portId => graph.finishRouteToPort(portId)
                                onVolumeRequested: (control, percent) => graph.setAudioVolume(control, percent)
                                onMuteRequested: (control, muted) => graph.setAudioMuted(control, muted)
                            }
                        }

                    }

                    PinchHandler {
                        id: pinch
                        target: null
                        property real startingZoom: 1
                        onActiveChanged: if (active) startingZoom = viewport.zoom
                        onActiveScaleChanged: viewport.setZoom(startingZoom * activeScale, centroid.position.x, centroid.position.y)
                    }

                }

                LinkLayer {
                    anchors.fill: parent
                    portAnchors: graph.anchors
                    links: graph.renderedLinks
                    blockers: graph.cardRects
                    viewScale: viewport.zoom
                    contentX: viewport.contentX
                    contentY: viewport.contentY
                    mediaFilter: graph.mediaFilter
                    selectedKey: graph.selectedKey
                    routePreview: graph.routing
                    liveCard: window.liveCard
                    z: 3
                    onLinkActivated: key => graph.selectLink(key)
                }

                WheelHandler {
                    target: null
                    acceptedModifiers: Qt.ControlModifier
                    blocking: true
                    onWheel: function(event) {
                        const delta = event.pixelDelta.y !== 0 ? event.pixelDelta.y * 2 : event.angleDelta.y
                        viewport.setZoom(viewport.zoom * Math.pow(1.0015, delta), event.x, event.y)
                        event.accepted = true
                    }
                }
                WheelHandler {
                    target: null
                    acceptedModifiers: Qt.ShiftModifier
                    blocking: true
                    onWheel: function(event) {
                        const delta = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 2
                        viewport.contentX = viewport.clamp(viewport.contentX - delta, 0,
                            Math.max(0, viewport.contentWidth - viewport.width))
                        event.accepted = true
                    }
                }

                Rectangle {
                    visible: graph.noticeText.length > 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 16
                    width: Math.min(parent.width - 220, noticeLabel.implicitWidth + 34)
                    height: 38; radius: 4
                    color: "#252d33"; border.color: "#59656f"; z: 6
                    Text { id: noticeLabel; anchors.centerIn: parent; text: graph.noticeText; color: "#dce1e4"; font.pixelSize: 10 }
                }

                Rectangle {
                    anchors.left: parent.left; anchors.bottom: parent.bottom
                    anchors.margins: 12
                    width: zoomControls.width + 18; height: 38; radius: 3
                    color: "#1d2328"; border.color: "#46515b"; z: 4
                    Row {
                        id: zoomControls; anchors.centerIn: parent; spacing: 5
                        ToolButton { text: "−"; width: 30; height: 28; onClicked: viewport.setZoom(viewport.zoom / 1.15, viewport.width / 2, viewport.height / 2) }
                        Text { width: 42; anchors.verticalCenter: parent.verticalCenter; horizontalAlignment: Text.AlignHCenter; text: Math.round(viewport.zoom * 100) + "%"; color: "#b8c1c8"; font.pixelSize: 10 }
                        ToolButton { text: "+"; width: 30; height: 28; onClicked: viewport.setZoom(viewport.zoom * 1.15, viewport.width / 2, viewport.height / 2) }
                        Button { text: "Fit"; height: 28; onClicked: viewport.fitGraph(true) }
                    }
                }

                Rectangle {
                    visible: !graph.connected && graph.cardCount === 0
                    anchors.centerIn: parent
                    width: 400; height: 150; radius: 4
                    color: "#20272d"; border.color: "#4b5761"
                    Column { anchors.centerIn: parent; spacing: 12
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PipeWire is unavailable"; color: "#edf0f2"; font.pixelSize: 17; font.weight: Font.DemiBold }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: graph.statusText; color: "#aab4bc" }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Run WireRunner with --demo to explore the graph."; color: "#7da7e8"; font.pixelSize: 10 }
                    }
                }
            }

            InspectorPane {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                selection: graph.selected
                focusActive: graph.focusActive
                onFocusRequested: graph.focusSelected()
                onClearFocusRequested: graph.clearFocus()
                onToggleRequested: key => graph.toggleCard(key)
                onDisconnectRequested: graph.disconnectSelected()
                onVolumeRequested: (control, percent) => typeof control === "number" ? graph.setNodeVolume(control, percent) : graph.setAudioVolume(control, percent)
                onChannelVolumeRequested: (control, channel, percent) => graph.setAudioChannelVolume(control, channel, percent)
                onMuteRequested: (control, muted) => typeof control === "number" ? graph.setNodeMuted(control, muted) : graph.setAudioMuted(control, muted)
                onDefaultRequested: (kind, nodeId) => graph.setDefaultTarget(kind, nodeId)
                onProfileRequested: (deviceId, profileIndex) => graph.setDeviceProfile(deviceId, profileIndex)
                onRouteChoiceRequested: (deviceId, routeIndex, routeDeviceId) => graph.setDeviceRoute(deviceId, routeIndex, routeDeviceId)
            }
        }
    }

    Dialog {
        id: feedbackDialog
        anchors.centerIn: parent
        width: 420
        modal: true
        title: "Create a feedback link?"
        standardButtons: Dialog.Ok | Dialog.Cancel
        closePolicy: Popup.NoAutoClose
        onAccepted: graph.confirmFeedback()
        onRejected: graph.cancelFeedback()
        onOpened: standardButton(Dialog.Ok).text = "Create feedback link"
        contentItem: Text {
            width: 360
            text: "This connection closes a media cycle. PipeWire will delay the return path by one processing cycle. Audio feedback can become loud very quickly."
            color: "#dce1e4"; wrapMode: Text.Wrap
        }
    }

    Shortcut { sequences: [StandardKey.Undo]; onActivated: graph.undo() }
    Shortcut { sequences: [StandardKey.Redo]; onActivated: graph.redo() }
    Shortcut { sequence: "Delete"; enabled: graph.selected.selectionKind === "link"; onActivated: graph.disconnectSelected() }
    Shortcut { sequence: "Escape"; enabled: Boolean(graph.routing.active); onActivated: graph.cancelRoute() }

    Connections {
        target: graph
        function onGraphChanged() {
            if (!viewport.fittedOnce && graph.cardCount > 0) Qt.callLater(function() { viewport.fitGraph(false) })
        }
        function onFeedbackConfirmationChanged() {
            if (graph.feedbackConfirmation) feedbackDialog.open()
            else feedbackDialog.close()
        }
    }
}
