// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../src/qml" as WireRunnerUi

TestCase {
    id: testCase
    name: "InspectorAudioControls"
    when: host.visible

    property var selectionData: ({
        key: "client:test", selectionKind: "card", title: "Browser", subtitle: "Application",
        state: "running", mediaTypes: ["audio"], nodeCount: 2, inputCount: 0,
        outputCount: 2, connectionCount: 1, members: [], inputs: [], outputs: [],
        audioControls: [
            {nodeId: 12, name: "Music tab", volume: 80, minimum: 0, maximum: 120,
             decibels: -5.8, muted: false, hasVolume: true, hasMute: true, writable: true,
             pending: false, boosted: false},
            {nodeId: 13, name: "Meeting tab", volume: 100, minimum: 0, maximum: 100,
             decibels: 0, muted: true, hasVolume: true, hasMute: true, writable: false,
             pending: false, boosted: false}
        ]
    })

    Window {
        id: host
        width: 360
        height: 720
        visible: true
        WireRunnerUi.InspectorPane {
            id: inspector
            anchors.fill: parent
            selection: testCase.selectionData
            focusActive: false
        }
    }
    SignalSpy { id: volumeSpy; target: inspector; signalName: "volumeRequested" }
    SignalSpy { id: muteSpy; target: inspector; signalName: "muteRequested" }

    function init() {
        volumeSpy.clear()
        muteSpy.clear()
    }

    function test_separateRowsAndPermissions() {
        const first = findChild(inspector, "volumeSlider-12")
        const second = findChild(inspector, "volumeSlider-13")
        verify(first)
        verify(second)
        verify(first.enabled)
        verify(!second.enabled)
        compare(first.to, 120)
    }

    function test_muteUsesExactNode() {
        const button = findChild(inspector, "muteButton-12")
        verify(button)
        mouseClick(button)
        compare(muteSpy.count, 1)
        compare(muteSpy.signalArguments[0][0], 12)
        compare(muteSpy.signalArguments[0][1], true)
    }

    function test_keyboardVolumeUsesExactNode() {
        host.requestActivate()
        tryCompare(host, "active", true)
        const slider = findChild(inspector, "volumeSlider-12")
        slider.forceActiveFocus()
        tryCompare(slider, "activeFocus", true)
        keyClick(Qt.Key_Left)
        tryCompare(volumeSpy, "count", 1, 500)
        compare(volumeSpy.signalArguments[0][0], 12)
        compare(volumeSpy.signalArguments[0][1], 79)
    }
}
