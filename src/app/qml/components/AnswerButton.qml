import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Controls.Button {
    id: button

    property string shortcutHint: ""
    property bool primary: false

    // Never focusable: keys are handled by DrillPage, and a focused button
    // would let Space or Enter fire an answer alongside the key handler.
    focusPolicy: Qt.NoFocus

    // Buttons only ever grow, so a long round cannot shift the hit targets.
    implicitHeight: Kirigami.Units.gridUnit * 3.5

    highlighted: primary

    /// Tints the button for 110 ms. The next prompt is already on screen by
    /// the time this runs, so the flash never delays the drill.
    function flash(correct) {
        tint.color = correct
            ? Kirigami.Theme.positiveTextColor
            : Kirigami.Theme.negativeTextColor;
        flashAnimation.restart();
    }

    contentItem: Column {
        spacing: 0

        Controls.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: button.text
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.5
        }

        Controls.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: button.shortcutHint.length > 0
            text: button.shortcutHint
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.6
        }
    }

    Rectangle {
        id: tint
        anchors.fill: parent
        radius: Kirigami.Units.cornerRadius
        opacity: 0
        z: 1

        // Clicks must reach the button underneath.
        visible: opacity > 0
    }

    SequentialAnimation {
        id: flashAnimation

        PropertyAction { target: tint; property: "opacity"; value: 0.45 }
        PauseAnimation { duration: 110 }
        NumberAnimation {
            target: tint
            property: "opacity"
            to: 0
            duration: 90
        }
    }
}
