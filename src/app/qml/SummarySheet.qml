import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    id: sheet

    signal againRequested()
    signal homeRequested()

    title: i18n("Round complete")

    // The drill is modal here on purpose: dismissing by clicking away would
    // lose the result the user just earned.
    closePolicy: Controls.Popup.NoAutoClose

    padding: Kirigami.Units.largeSpacing * 2

    // Controls.Dialog, not Kirigami.Dialog: the enum lives on the QtQuick
    // Controls base type.
    standardButtons: Controls.Dialog.NoButton

    customFooterActions: [
        Kirigami.Action {
            text: i18nc("start another round", "Again")
            icon.name: "media-playback-start"
            onTriggered: sheet.againRequested()
        },
        Kirigami.Action {
            text: i18nc("return to the module list", "Home")
            icon.name: "go-home"
            onTriggered: sheet.homeRequested()
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.gridUnit * 2

            ColumnLayout {
                spacing: 0
                Kirigami.Heading {
                    Layout.alignment: Qt.AlignHCenter
                    level: 1
                    text: squareColor.summaryCorrect
                    color: Kirigami.Theme.positiveTextColor
                }
                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: i18n("correct")
                    opacity: 0.7
                }
            }

            ColumnLayout {
                spacing: 0
                Kirigami.Heading {
                    Layout.alignment: Qt.AlignHCenter
                    level: 1
                    text: squareColor.summaryWrong
                    color: Kirigami.Theme.negativeTextColor
                }
                Controls.Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: i18n("wrong")
                    opacity: 0.7
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        GridLayout {
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

            Controls.Label {
                text: i18n("Accuracy")
                opacity: 0.7
            }
            Controls.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: i18nc("percentage", "%1%", squareColor.summaryAccuracyPercent)
            }

            Controls.Label {
                text: i18n("Average response")
                opacity: 0.7
            }
            Controls.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: i18nc("milliseconds", "%1 ms", squareColor.summaryMeanResponseMs)
            }

            Controls.Label {
                visible: squareColor.summarySlowestSquares.length > 0
                text: i18n("Slowest squares")
                opacity: 0.7
            }
            Controls.Label {
                visible: squareColor.summarySlowestSquares.length > 0
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: squareColor.summarySlowestSquares.join("  ")
            }

            Controls.Label {
                // -1 means no completed run has ever been recorded, which
                // happens when storage is unavailable.
                visible: squareColor.bestScore >= 0
                text: i18n("Best ever")
                opacity: 0.7
            }
            Controls.Label {
                visible: squareColor.bestScore >= 0
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                color: Kirigami.Theme.highlightColor
                text: squareColor.bestScore
            }
        }
    }
}
