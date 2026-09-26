// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../src/qml" as WireRunnerUi

TestCase {
    id: testCase
    name: "GraphCardInteraction"
    when: host.visible

    property var cardData: ({
        key: "node:test", title: "Test interface", subtitle: "Audio device",
        state: "running", media: "audio", mediaTypes: ["audio"],
        x: 30, y: 30, width: 272, height: 154, expanded: false,
        visible: true, focused: true, nodeCount: 1, inputCount: 2, outputCount: 1,
        inputs: [{id: 11, name: "playback", channel: "FL", media: "audio"}],
        outputs: [{id: 12, name: "capture", channel: "FR", media: "audio"}],
        inputGroups: [{label: "2 audio ports", media: "audio", count: 2}],
        outputGroups: [{id: 12, label: "audio", media: "audio", count: 1}]
    })

    Window {
        id: host
        width: 520
        height: 360
        visible: true
        WireRunnerUi.GraphCard {
            id: card
            card: testCase.cardData
            selected: false
        }
    }
    SignalSpy { id: selectedSpy; target: card; signalName: "selectedRequested" }
    SignalSpy { id: toggleSpy; target: card; signalName: "toggleRequested" }
    SignalSpy { id: movedSpy; target: card; signalName: "moved" }
    SignalSpy { id: movingSpy; target: card; signalName: "moving" }
    SignalSpy { id: expandRouteSpy; target: card; signalName: "expandForRoutingRequested" }
    SignalSpy { id: routeStartedSpy; target: card; signalName: "routeStarted" }
    SignalSpy { id: routeMovedSpy; target: card; signalName: "routeMoved" }
    SignalSpy { id: routeFinishedSpy; target: card; signalName: "routeFinished" }

    function initTestCase() {
        verify(card.visible)
        wait(30)
    }

    function init() {
        selectedSpy.clear()
        toggleSpy.clear()
        movedSpy.clear()
        movingSpy.clear()
        expandRouteSpy.clear()
        routeStartedSpy.clear()
        routeMovedSpy.clear()
        routeFinishedSpy.clear()
    }

    function test_pointerSelectsCanonicalCard() {
        mouseClick(card, 40, 40, Qt.LeftButton)
        wait(20)
        compare(selectedSpy.count, 1)
    }

    function test_keyboardExpandsInPlace() {
        host.requestActivate()
        tryCompare(host, "active", true)
        card.forceActiveFocus()
        tryCompare(card, "activeFocus", true)
        keyClick(Qt.Key_E)
        compare(toggleSpy.count, 1)
    }

    function test_dragReportsOneSavedPosition() {
        mousePress(card, 40, 40, Qt.LeftButton)
        mouseMove(card, 130, 100, 100)
        mouseRelease(card, 130, 100, Qt.LeftButton)
        tryCompare(movedSpy, "count", 1)
        verify(movingSpy.count >= 1)
    }

    function test_singleOutputGroupStartsCanonicalWireGesture() {
        mousePress(card, 264, 110, Qt.LeftButton)
        mouseMove(card, 220, 125, 30)
        mouseRelease(card, 200, 130, Qt.LeftButton)
        compare(routeStartedSpy.count, 1)
        verify(routeMovedSpy.count >= 1)
        compare(routeFinishedSpy.count, 1)
        compare(routeStartedSpy.signalArguments[0][0], 12)
        compare(movedSpy.count, 0)
    }

    function test_draggingPortLabelMovesCardWithoutRouting() {
        mousePress(card, 190, 110, Qt.LeftButton)
        mouseMove(card, 225, 140, 50)
        mouseRelease(card, 225, 140, Qt.LeftButton)
        tryCompare(movedSpy, "count", 1)
        compare(routeStartedSpy.count, 0)
    }

    function test_multiPortGroupExpandsInsteadOfGuessing() {
        mouseClick(card, 8, 110, Qt.LeftButton)
        compare(expandRouteSpy.count, 1)
        compare(routeStartedSpy.count, 0)
    }
}
