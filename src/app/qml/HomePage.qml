import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    title: i18n("Chess Trainer")

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - (Kirigami.Units.largeSpacing * 4)
        text: i18n("Training modules will appear here.")
    }
}
