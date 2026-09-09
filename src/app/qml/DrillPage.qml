import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

import "components"

Kirigami.Page {
    id: page

    // Immersive: the round gets the whole window. Chrome returns with the
    // summary because leaving this page pops it.
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
    padding: Kirigami.Units.largeSpacing * 2

    // The page owns every key: buttons are not focusable, so no other item
    // can consume or duplicate an answer.
    focus: true

    Component.onCompleted: squareColor.start()

    Keys.onPressed: (event) => {
        // A held-down arrow key must not spray answers.
        if (event.isAutoRepeat) {
            event.accepted = true;
            return;
        }

        switch (event.key) {
        case Qt.Key_Left:
        case Qt.Key_L:
            page.submit(false);
            event.accepted = true;
            break;
        case Qt.Key_Right:
        case Qt.Key_D:
            page.submit(true);
            event.accepted = true;
            break;
        case Qt.Key_Escape:
            squareColor.abort();
            event.accepted = true;
            break;
        default:
            event.accepted = false;
        }
    }

    /// Single entry point for both mouse and keyboard, so one interaction can
    /// never produce two answers.
    function submit(dark) {
        if (!squareColor.running) {
            return;
        }
        const wasCorrect = squareColor.answer(dark);
        const button = dark ? darkButton : lightButton;
        button.flash(wasCorrect);
    }

    Connections {
        target: squareColor
        function onRoundFinished() {
            summarySheet.open();
        }
    }

    SummarySheet {
        id: summarySheet

        onAgainRequested: {
            close();
            squareColor.start();
            page.forceActiveFocus();
        }
        onHomeRequested: {
            close();
            // applicationWindow() is Kirigami's accessor for the root window;
            // the ColumnView attached property is not available on a Page.
            applicationWindow().pageStack.pop();
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width, Kirigami.Units.gridUnit * 22)
        spacing: Kirigami.Units.largeSpacing

        CountdownRing {
            Layout.alignment: Qt.AlignHCenter
            remainingMs: squareColor.remainingMs
            totalMs: squareColor.roundLengthMs
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: squareColor.promptText
            // Deliberately larger than any heading level: this is the one
            // thing the user reads under time pressure.
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 4
            font.weight: Font.Light
            font.letterSpacing: 4
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.largeSpacing

            Controls.Label {
                text: i18nc("correct answer count", "✓ %1", squareColor.correct)
                color: Kirigami.Theme.positiveTextColor
            }
            Controls.Label {
                text: i18nc("wrong answer count", "✗ %1", squareColor.wrong)
                color: Kirigami.Theme.negativeTextColor
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            AnswerButton {
                id: lightButton
                Layout.fillWidth: true
                primary: true
                text: i18nc("light coloured square", "Light")
                shortcutHint: i18nc("keyboard shortcuts", "← / L")
                onClicked: page.submit(false)
            }

            AnswerButton {
                id: darkButton
                Layout.fillWidth: true
                text: i18nc("dark coloured square", "Dark")
                shortcutHint: i18nc("keyboard shortcuts", "→ / D")
                onClicked: page.submit(true)
            }
        }

        Controls.Label {
            Layout.alignment: Qt.AlignHCenter
            text: i18nc("keyboard hint", "Esc  end round")
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.45
        }
    }
}
