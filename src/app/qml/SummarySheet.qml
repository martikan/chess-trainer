import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    signal againRequested()
    signal homeRequested()
    title: i18n("Round complete")
}
