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
                Button { text: "Fit"; onClicked: viewport.fitGraph() }
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
                    function fitGraph() {
                        if (graph.cardCount === 0) return
                        zoom = clamp(Math.min(width / graph.canvasWidth, height / graph.canvasHeight) * 0.94, 0.45, 1.0)
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
                            model: graph.cards
                            delegate: GraphCard {
                                required property var item
                                card: item
                                selected: graph.selectedKey === item.key
                                onSelectedRequested: graph.selectCard(item.key)
                                onToggleRequested: graph.toggleCard(item.key)
                                onMoved: (x, y) => graph.moveCard(item.key, x, y)
                            }
                        }

                    }

                    WheelHandler {
                        target: null
                        onWheel: function(event) {
                            viewport.setZoom(viewport.zoom * Math.pow(1.0015, event.angleDelta.y), event.x, event.y)
                            event.accepted = true
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
                    z: 3
                    onLinkActivated: key => graph.selectLink(key)
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
            }
        }
    }

    Connections {
        target: graph
        function onGraphChanged() {
            if (!viewport.fittedOnce && graph.cardCount > 0) Qt.callLater(viewport.fitGraph)
        }
    }
}
