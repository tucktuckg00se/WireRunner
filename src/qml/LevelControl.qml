// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property real value: 100
    property real from: 0
    property real to: 100
    property bool controlEnabled: true
    property string accessibleName: "Volume"
    property bool compact: false
    property bool skipNextBlurCommit: false
    signal committed(real percent)

    implicitHeight: 30
    implicitWidth: 190
    enabled: controlEnabled
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key !== Qt.Key_Left && event.key !== Qt.Key_Right &&
            event.key !== Qt.Key_Up && event.key !== Qt.Key_Down) return
        const direction = event.key === Qt.Key_Right || event.key === Qt.Key_Up ? 1 : -1
        const amount = event.modifiers & Qt.ShiftModifier ? 5 : 1
        root.committed(root.clamp(root.value + direction * amount))
        event.accepted = true
    }

    function clamp(number) { return Math.max(from, Math.min(to, number)) }
    function decibels(percent) {
        return percent <= 0 ? Number.NEGATIVE_INFINITY : 60 * Math.log(percent / 100) / Math.LN10
    }
    function formatted(percent) { return Math.round(percent) + "%" }
    function parse(text) {
        const cleaned = text.trim().toLowerCase().replace("−", "-")
        const db = cleaned.match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*db$/)
        if (db) return clamp(100 * Math.pow(10, Number(db[1]) / 60))
        const percent = cleaned.match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*%?$/)
        return percent ? clamp(Number(percent[1])) : NaN
    }
    function commitText() {
        const parsed = parse(entry.text)
        if (isNaN(parsed)) {
            entry.text = formatted(root.value)
            entry.selectAll()
            return false
        }
        entry.text = formatted(parsed)
        root.committed(parsed)
        return true
    }
    function resetUnity() { if (controlEnabled) root.committed(clamp(100)) }

    RowLayout {
        anchors.fill: parent
        spacing: root.compact ? 5 : 8
        Slider {
            id: slider
            objectName: "levelSlider"
            Layout.fillWidth: true
            from: root.from
            to: root.to
            value: root.value
            enabled: root.controlEnabled
            wheelEnabled: false
            stepSize: 1
            Accessible.name: root.accessibleName
            Accessible.description: root.formatted(value) + ", " + (isFinite(root.decibels(value)) ? root.decibels(value).toFixed(1) + " dB" : "minus infinity dB")
            onMoved: commitTimer.restart()
            onPressedChanged: if (!pressed && commitTimer.running) {
                commitTimer.stop()
                root.committed(value)
            }
            Keys.onPressed: function(event) {
                if (event.key !== Qt.Key_Left && event.key !== Qt.Key_Right &&
                    event.key !== Qt.Key_Up && event.key !== Qt.Key_Down) return
                const direction = event.key === Qt.Key_Right || event.key === Qt.Key_Up ? 1 : -1
                const amount = event.modifiers & Qt.ShiftModifier ? 5 : 1
                value = root.clamp(value + direction * amount)
                root.committed(value)
                event.accepted = true
            }
            handle: Rectangle {
                objectName: "levelHandle"
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 14; height: 20; radius: 3
                color: slider.enabled ? "#edf0f2" : "#77838d"
                border.color: "#59656f"
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true
                    onPositionChanged: function(mouse) {
                        if (!pressed || !root.controlEnabled) return
                        const point = mapToItem(slider, mouse.x, mouse.y)
                        const position = Math.max(0, Math.min(1,
                            (point.x - slider.leftPadding - parent.width / 2) /
                            Math.max(1, slider.availableWidth - parent.width)))
                        slider.value = slider.valueAt(position)
                        commitTimer.restart()
                    }
                    onReleased: if (root.controlEnabled && commitTimer.running) {
                        commitTimer.stop()
                        root.committed(slider.value)
                    }
                    onDoubleClicked: root.resetUnity()
                }
            }
        }
        TextField {
            id: entry
            objectName: "levelEntry"
            Layout.preferredWidth: root.compact ? 46 : 58
            Layout.fillHeight: true
            enabled: root.controlEnabled
            text: root.formatted(root.value)
            horizontalAlignment: Text.AlignRight
            font.pixelSize: root.compact ? 9 : 10
            selectByMouse: true
            Accessible.name: root.accessibleName + " numeric value"
            onActiveFocusChanged: {
                if (activeFocus) selectAll()
                else if (root.skipNextBlurCommit) root.skipNextBlurCommit = false
                else if (text !== root.formatted(root.value)) root.commitText()
            }
            Keys.onReturnPressed: {
                root.skipNextBlurCommit = true
                if (root.commitText()) focus = false
                else root.skipNextBlurCommit = false
            }
            Keys.onEnterPressed: {
                root.skipNextBlurCommit = true
                if (root.commitText()) focus = false
                else root.skipNextBlurCommit = false
            }
            Keys.onEscapePressed: { text = root.formatted(root.value); focus = false }
        }
    }
    onValueChanged: if (!entry.activeFocus) entry.text = formatted(value)
    Timer { id: commitTimer; interval: 100; onTriggered: root.committed(slider.value) }
}
