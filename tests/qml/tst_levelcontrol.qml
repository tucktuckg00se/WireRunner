// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtTest
import "../../src/qml" as WireRunnerUi

TestCase {
    id: testCase
    name: "LevelControl"
    when: host.visible

    Window {
        id: host
        width: 320; height: 120; visible: true
        WireRunnerUi.LevelControl {
            id: control
            x: 20; y: 30; width: 260
            value: 80; from: 0; to: 150
        }
    }
    SignalSpy { id: commitSpy; target: control; signalName: "committed" }

    function init() { commitSpy.clear() }

    function test_parsesPercentAndDecibels() {
        compare(control.parse("75"), 75)
        compare(control.parse("75%"), 75)
        verify(Math.abs(control.parse("-6 dB") - 79.4328) < 0.001)
        compare(control.parse("0 dB"), 100)
        verify(isNaN(control.parse("loud")))
    }

    function test_resetCommitsUnity() {
        const handle = findChild(control, "levelHandle")
        verify(handle)
        mouseDoubleClickSequence(handle, handle.width / 2, handle.height / 2, Qt.LeftButton)
        compare(commitSpy.count, 1)
        compare(commitSpy.signalArguments[0][0], 100)
    }

    function test_exactTextEntry() {
        const entry = findChild(control, "levelEntry")
        verify(entry)
        host.requestActivate()
        tryCompare(host, "active", true)
        entry.forceActiveFocus()
        tryCompare(entry, "activeFocus", true)
        entry.text = "-6 dB"
        keyClick(Qt.Key_Return)
        tryCompare(commitSpy, "count", 1)
        verify(Math.abs(commitSpy.signalArguments[0][0] - 79.4328) < 0.001)
    }

    function test_keyboardSteps() {
        host.requestActivate()
        tryCompare(host, "active", true)
        control.forceActiveFocus()
        tryCompare(control, "activeFocus", true)
        keyClick(Qt.Key_Right)
        tryCompare(commitSpy, "count", 1)
        compare(commitSpy.signalArguments[0][0], 81)
        commitSpy.clear()
        keyClick(Qt.Key_Left, Qt.ShiftModifier)
        tryCompare(commitSpy, "count", 1)
        compare(commitSpy.signalArguments[0][0], 75)
    }
}
