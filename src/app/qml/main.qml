import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Chess Trainer")

    minimumWidth: Kirigami.Units.gridUnit * 22
    minimumHeight: Kirigami.Units.gridUnit * 28
    width: Kirigami.Units.gridUnit * 30
    height: Kirigami.Units.gridUnit * 38

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: true

        actions: [
            Kirigami.Action {
                text: i18n("About Chess Trainer")
                icon.name: "help-about"
                onTriggered: root.pageStack.layers.push(aboutPage)
            },
            Kirigami.Action {
                text: i18n("Quit")
                icon.name: "application-exit"
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        ]
    }

    Component {
        id: aboutPage
        Kirigami.AboutPage {}
    }

    pageStack.initialPage: HomePage {
        onModuleRequested: (qmlPage) => root.pageStack.push(qmlPage)
    }
}
