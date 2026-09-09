import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Controls.Button {
    id: button

    property string shortcutHint: ""

    // Never focusable: keys are handled by DrillPage, and a focused button
    // would let Space or Enter fire an answer alongside the key handler.
    focusPolicy: Qt.NoFocus

    // Buttons only ever grow, so a long round cannot shift the hit targets.
    implicitHeight: Kirigami.Units.gridUnit * 3.5

    // Both answers must look identical at rest: a tinted default would read as
    // a hint, and the drill must not nudge the guess either way.
    highlighted: false

    /// Resting fill, tinted towards the accent colour so the pair belongs to
    /// the countdown ring rather than to the window chrome.
    readonly property color restColor: Kirigami.ColorUtils.tintWithAlpha(
        Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.16)

    /// Tints the button for 110 ms. The next prompt is already on screen by
    /// the time this runs, so the flash never delays the drill.
    function flash(correct) {
        tint.color = correct
            ? Kirigami.Theme.positiveTextColor
            : Kirigami.Theme.negativeTextColor;
        flashAnimation.restart();
    }

    // Replaces the style's background: its hover state is a few percent of
    // lightness, which is invisible against this page's dark backdrop.
    background: Rectangle {
        radius: Kirigami.Units.cornerRadius

        // Pressed darkens as well as tints: a theme whose hoverColor equals
        // its highlightColor would otherwise make press and hover identical.
        color: button.pressed
            ? Qt.darker(Kirigami.ColorUtils.tintWithAlpha(
                button.restColor, Kirigami.Theme.highlightColor, 0.5), 1.3)
            : button.hovered
                ? Kirigami.ColorUtils.tintWithAlpha(
                    button.restColor, Kirigami.Theme.hoverColor, 0.35)
                : button.restColor

        // Under time pressure the pointer lands before the eye confirms, so
        // hover has to be a jump in both fill and outline, not a wash.
        border.width: button.hovered || button.pressed ? 2 : 1
        border.color: button.hovered || button.pressed
            ? Kirigami.Theme.highlightColor
            : Qt.alpha(Kirigami.Theme.textColor, 0.25)

        Behavior on color {
            ColorAnimation { duration: Kirigami.Units.shortDuration }
        }
        Behavior on border.color {
            ColorAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    // An Item wrapper, rather than the layout itself: a Control stretches its
    // contentItem to the whole content rect, which would strand the labels at
    // the top edge instead of centring them.
    contentItem: Item {
        implicitWidth: layout.implicitWidth
        implicitHeight: layout.implicitHeight

        ColumnLayout {
            id: layout

            // Anchors, not a Column: a positioner refuses to lay out anchored
            // children, which silently collapsed both labels onto each other.
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Kirigami.Units.smallSpacing

            Controls.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: button.text
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1.5
                font.weight: Font.DemiBold
            }

            Controls.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                visible: button.shortcutHint.length > 0
                text: button.shortcutHint
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.55
            }
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
