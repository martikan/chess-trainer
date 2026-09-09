import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    title: i18n("Chess Trainer")

    signal moduleRequested(string qmlPage)

    Kirigami.CardsListView {
        id: cards

        model: moduleList

        delegate: Kirigami.AbstractCard {
            required property string displayName
            required property string description
            required property string iconName
            required property string qmlPage
            required property bool enabled
            required property int bestScore
            required property int meanResponseMs

            // Disabled modules stay legible but visibly out of reach.
            opacity: enabled ? 1.0 : 0.45

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: iconName
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        level: 4
                        text: displayName
                        Layout.fillWidth: true
                    }

                    Controls.Label {
                        text: description
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        visible: enabled

                        Controls.Button {
                            text: i18n("Start")
                            icon.name: "media-playback-start"
                            onClicked: page.moduleRequested(qmlPage)
                        }

                        Controls.Label {
                            // bestScore is -1 until a round has been completed.
                            visible: bestScore >= 0
                            color: Kirigami.Theme.highlightColor
                            text: meanResponseMs >= 0
                                ? i18nc("best score and mean response time",
                                        "best %1 · avg %2 ms",
                                        bestScore, meanResponseMs)
                                : i18nc("best score", "best %1", bestScore)
                        }
                    }

                    Controls.Label {
                        visible: !enabled
                        text: i18nc("module not implemented yet", "Soon")
                        opacity: 0.7
                    }
                }
            }
        }
    }
}
